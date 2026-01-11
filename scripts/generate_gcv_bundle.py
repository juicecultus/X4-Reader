#!/usr/bin/env python3

from __future__ import annotations

import argparse
import os
from dataclasses import dataclass
from pathlib import Path
from typing import Tuple

from PIL import Image, ImageOps


DISPLAY_W = 480
DISPLAY_H = 800
BUFFER_SIZE = (DISPLAY_W * DISPLAY_H) // 8  # 48000


def fnv1a32(data: bytes) -> int:
    h = 2166136261
    for b in data:
        h ^= b
        h = (h * 16777619) & 0xFFFFFFFF
    return h


def quantize4(lum: int) -> int:
    # Must match firmware ImageDecoder::quantize4
    # 0 = black, 1 = dark gray, 2 = light gray, 3 = white
    if lum < 43:
        return 0
    if lum < 128:
        return 1
    if lum < 213:
        return 2
    return 3


def _set_packed_bit(buf: bytearray, byte_idx: int, bit_idx: int, value: bool) -> None:
    if value:
        buf[byte_idx] |= (1 << bit_idx)
    else:
        buf[byte_idx] &= ~(1 << bit_idx)


def _map_xy_to_buf_indices(px: int, py: int) -> Tuple[int, int]:
    # Must match firmware mapping used in ImageDecoder:
    # fx = py
    # fy = 479 - px
    # byteIdx = (fy * 100) + (fx / 8)
    # bitIdx  = 7 - (fx % 8)
    fx = py
    fy = 479 - px
    if fx < 0 or fx >= 800 or fy < 0 or fy >= 480:
        raise ValueError("Mapped coords out of bounds")
    byte_idx = (fy * 100) + (fx // 8)
    bit_idx = 7 - (fx % 8)
    return byte_idx, bit_idx


def render_fit_width(img: Image.Image) -> Image.Image:
    # Firmware decodeToDisplayFitWidth(): scale image to width=480, keep aspect, render height <= 800
    img = ImageOps.exif_transpose(img)
    img = img.convert("RGB")

    src_w, src_h = img.size
    if src_w == 0 or src_h == 0:
        raise ValueError("Invalid image")

    scale = DISPLAY_W / float(src_w)
    out_w = DISPLAY_W
    out_h = int(round(src_h * scale))
    if out_h <= 0:
        out_h = 1

    # Pillow resize
    resized = img.resize((out_w, out_h), Image.BILINEAR)

    # Place at top-left in a 480x800 canvas, white background
    canvas = Image.new("RGB", (DISPLAY_W, DISPLAY_H), (255, 255, 255))
    paste_h = min(DISPLAY_H, out_h)
    if paste_h > 0:
        crop = resized.crop((0, 0, out_w, paste_h))
        canvas.paste(crop, (0, 0))

    return canvas


@dataclass
class GcvBundle:
    bw: bytes
    lsb: bytes
    msb: bytes


def build_bundle(canvas_rgb: Image.Image) -> GcvBundle:
    if canvas_rgb.size != (DISPLAY_W, DISPLAY_H):
        raise ValueError("Canvas must be 480x800")

    bw = bytearray(BUFFER_SIZE)
    lsb = bytearray(BUFFER_SIZE)
    msb = bytearray(BUFFER_SIZE)

    pix = canvas_rgb.load()

    for py in range(DISPLAY_H):
        for px in range(DISPLAY_W):
            r, g, b = pix[px, py]
            lum = (r * 306 + g * 601 + b * 117) >> 10
            if lum < 0:
                lum = 0
            if lum > 255:
                lum = 255
            q = quantize4(lum)

            # Firmware BW: 1 means white (color != 0). It uses lum >= 128 as white.
            color_white = lum >= 128

            # Firmware grayscale mask semantics:
            # LSB active when q == 1 (dark gray only)
            # MSB active when q == 1 or q == 2 (dark + light)
            lsb_on = q == 1
            msb_on = (q == 1) or (q == 2)

            byte_idx, bit_idx = _map_xy_to_buf_indices(px, py)
            _set_packed_bit(bw, byte_idx, bit_idx, color_white)
            _set_packed_bit(lsb, byte_idx, bit_idx, lsb_on)
            _set_packed_bit(msb, byte_idx, bit_idx, msb_on)

    return GcvBundle(bw=bytes(bw), lsb=bytes(lsb), msb=bytes(msb))


def write_bundle(out_dir: Path, key_hex: str, bundle: GcvBundle) -> None:
    out_dir.mkdir(parents=True, exist_ok=True)

    base = out_dir / key_hex
    bw_path = base.with_suffix(".bw")
    lsb_path = base.with_suffix(".lsb")
    msb_path = base.with_suffix(".msb")
    gcv_path = base.with_suffix(".gcv")

    bw_path.write_bytes(bundle.bw)
    lsb_path.write_bytes(bundle.lsb)
    msb_path.write_bytes(bundle.msb)
    # Marker file (empty)
    gcv_path.write_bytes(b"")


def main() -> None:
    ap = argparse.ArgumentParser(description="Generate MicroReader .gcv grayscale cover bundle (.bw/.lsb/.msb + .gcv)")
    ap.add_argument("image", help="Input image (jpg/png/etc)")
    ap.add_argument(
        "--epub",
        help="EPUB path string used for stable cache key (recommended). If omitted, key uses image path.",
        default=None,
    )
    ap.add_argument(
        "--out-dir",
        help="Output directory for cover bundle (e.g. /Volumes/MICROREADER/microreader/epub_covers)",
        required=True,
    )
    ap.add_argument("--key", help="Override key hex (no 0x prefix)", default=None)
    ap.add_argument("--preview", help="Write preview PNG next to outputs", action="store_true")

    args = ap.parse_args()

    img_path = Path(args.image)
    out_dir = Path(args.out_dir)

    if not img_path.exists():
        raise SystemExit(f"Input image not found: {img_path}")

    if args.key:
        key_hex = args.key.lower()
    else:
        key_src = (args.epub if args.epub else str(img_path)).encode("utf-8")
        key_hex = f"{fnv1a32(key_src):08x}"

    img = Image.open(img_path)
    canvas = render_fit_width(img)
    bundle = build_bundle(canvas)

    if len(bundle.bw) != BUFFER_SIZE or len(bundle.lsb) != BUFFER_SIZE or len(bundle.msb) != BUFFER_SIZE:
        raise SystemExit("Internal error: buffer sizes incorrect")

    write_bundle(out_dir, key_hex, bundle)

    if args.preview:
        # Visualize as 4-level grayscale based on plane masks
        preview = Image.new("L", (DISPLAY_W, DISPLAY_H), 255)
        pv = preview.load()

        # reconstruct q from masks approximately
        # q==1 -> lsb=1,msb=1 (dark)
        # q==2 -> lsb=0,msb=1 (light)
        # q==0 or 3 -> msb=0 (white/black depends on bw)
        # Use BW to distinguish q==0 (black) vs q==3 (white)
        bw = bundle.bw
        lsb = bundle.lsb
        msb = bundle.msb
        for py in range(DISPLAY_H):
            for px in range(DISPLAY_W):
                byte_idx, bit_idx = _map_xy_to_buf_indices(px, py)
                bw_bit = (bw[byte_idx] >> bit_idx) & 1
                lsb_bit = (lsb[byte_idx] >> bit_idx) & 1
                msb_bit = (msb[byte_idx] >> bit_idx) & 1

                if msb_bit == 0:
                    # black or white from bw
                    pv[px, py] = 255 if bw_bit else 0
                else:
                    pv[px, py] = 60 if lsb_bit else 180

        base = out_dir / key_hex
        preview_path = base.with_suffix(".preview.png")
        preview.save(preview_path)

    print("Generated bundle:")
    print(f"  key: {key_hex}")
    print(f"  out: {out_dir}")
    print(f"  files: {key_hex}.bw/.lsb/.msb/.gcv")


if __name__ == "__main__":
    main()
