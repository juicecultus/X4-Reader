#include "ImageDecoder.h"

static ImageDecoder::DecodeContext* g_ctx = nullptr;
PNG* ImageDecoder::currentPNG = nullptr;

bool ImageDecoder::decodeToBW(const char* path, uint8_t* outBuffer, uint16_t targetWidth, uint16_t targetHeight) {
    String p = String(path);
    p.toLowerCase();

    DecodeContext ctx;
    ctx.outBuffer = outBuffer;
    ctx.targetWidth = targetWidth;
    ctx.targetHeight = targetHeight;
    ctx.success = false;
    g_ctx = &ctx;

    // Initialize buffer to white (0xFF)
    memset(outBuffer, 0xFF, (targetWidth * targetHeight) / 8);

    if (p.endsWith(".jpg") || p.endsWith(".jpeg")) {
        JPEGDEC jpeg;
        File f = SD.open(path);
        if (!f) return false;

        // Use the simpler open() with callbacks to avoid linkage issues with File-based open
        int rc = jpeg.open((void *)&f, (int)f.size(), [](void *p) { /* close */ }, 
                       [](JPEGFILE *pfn, uint8_t *pBuf, int32_t iLen) -> int32_t {
                           if (!pfn->fHandle) return -1;
                           return ((File *)pfn->fHandle)->read(pBuf, (size_t)iLen);
                       },
                       [](JPEGFILE *pfn, int32_t iPos) -> int32_t {
                           if (!pfn->fHandle) return -1;
                           return ((File *)pfn->fHandle)->seek((uint32_t)iPos) ? iPos : -1;
                       }, JPEGDraw);

        if (rc) {
            jpeg.setUserPointer(&ctx);
            // We want to fit the image to the screen
            int scale = 0;
            if (jpeg.getWidth() > targetWidth || jpeg.getHeight() > targetHeight) {
                // Simplified scaling (JPEGDEC supports 1/2, 1/4, 1/8)
                if (jpeg.getWidth() > targetWidth * 4) scale = JPEG_SCALE_EIGHTH;
                else if (jpeg.getWidth() > targetWidth * 2) scale = JPEG_SCALE_QUARTER;
                else scale = JPEG_SCALE_HALF;
            }
            
            if (jpeg.decode(0, 0, scale)) {
                ctx.success = true;
            }
            jpeg.close();
        }
        f.close();
    } else if (p.endsWith(".png")) {
        PNG png;
        currentPNG = &png;
        File f = SD.open(path);
        if (!f) {
            currentPNG = nullptr;
            return false;
        }
        
        int rc = png.open(path, [](const char *szFilename, int32_t *pFileSize) -> void * {
            File *file = new File(SD.open(szFilename));
            if (file && *file) {
                *pFileSize = (int32_t)file->size();
                return (void *)file;
            }
            if (file) delete file;
            return NULL;
        }, [](void *pHandle) {
            File *file = (File *)pHandle;
            if (file) {
                file->close();
                delete file;
            }
        }, [](PNGFILE *pfn, uint8_t *pBuffer, int32_t iLength) -> int32_t {
            if (!pfn->fHandle) return -1;
            File *file = (File *)pfn->fHandle;
            return (int32_t)file->read(pBuffer, (size_t)iLength);
        }, [](PNGFILE *pfn, int32_t iPos) -> int32_t {
            if (!pfn->fHandle) return -1;
            File *file = (File *)pfn->fHandle;
            return file->seek((uint32_t)iPos) ? iPos : -1;
        }, [](PNGDRAW *pDraw) -> int {
            PNGDraw(pDraw);
            return 1;
        });
        
        if (rc == PNG_SUCCESS) {
            rc = png.decode(&ctx, 0);
            if (rc == PNG_SUCCESS) {
                ctx.success = true;
            }
            png.close();
        }
        f.close();
        currentPNG = nullptr;
    }


    g_ctx = nullptr;
    return ctx.success;
}

int ImageDecoder::JPEGDraw(JPEGDRAW *pDraw) {
    DecodeContext *ctx = (DecodeContext *)pDraw->pUser;
    
    // Bytes per line in the destination buffer (Width/8)
    const int destStride = (ctx->targetWidth + 7) / 8;

    for (int y = 0; y < pDraw->iHeight; y++) {
        int targetY = pDraw->y + y;
        if (targetY >= ctx->targetHeight) break;

        for (int x = 0; x < pDraw->iWidth; x++) {
            int targetX = pDraw->x + x;
            if (targetX >= ctx->targetWidth) break;

            uint16_t pixel = pDraw->pPixels[y * pDraw->iWidth + x];
            
            // Extract RGB565 components
            uint8_t r = (pixel >> 11) & 0x1F; // 5 bits
            uint8_t g = (pixel >> 5) & 0x3F;  // 6 bits
            uint8_t b = pixel & 0x1F;         // 5 bits
            
            // Convert to 0-255 luminance
            // r * 255/31 = 8.22
            // g * 255/63 = 4.04
            // b * 255/31 = 8.22
            float lum = (r * 8.22f * 0.299f) + (g * 4.04f * 0.587f) + (b * 8.22f * 0.114f);
            
            if (lum < 128) {
                // Black pixel (0 in E-Ink)
                int byteIdx = (targetY * destStride) + (targetX / 8);
                int bitIdx = 7 - (targetX % 8);
                ctx->outBuffer[byteIdx] &= ~(1 << bitIdx);
            }
        }
    }
    return 1;
}

void ImageDecoder::PNGDraw(PNGDRAW *pDraw) {
    DecodeContext *ctx = (DecodeContext *)pDraw->pUser;
    uint16_t usPixels[800]; 
    
    if (!currentPNG) return;
    
    currentPNG->getLineAsRGB565(pDraw, usPixels, PNG_RGB565_LITTLE_ENDIAN, 0xffffffff);

    // Bytes per line in the destination buffer (Width/8)
    const int destStride = (ctx->targetWidth + 7) / 8;

    int targetY = pDraw->y;
    if (targetY >= ctx->targetHeight) return;

    for (int x = 0; x < pDraw->iWidth; x++) {
        int targetX = x;
        if (targetX >= ctx->targetWidth) break;

        uint16_t pixel = usPixels[x];
        uint8_t r = (pixel >> 11) & 0x1F; // 5 bits
        uint8_t g = (pixel >> 5) & 0x3F;  // 6 bits
        uint8_t b = pixel & 0x1F;         // 5 bits
        
        // Convert to 0-255 luminance
        float lum = (r * 8.22f * 0.299f) + (g * 4.04f * 0.587f) + (b * 8.22f * 0.114f);

        if (lum < 128) {
            int byteIdx = (targetY * destStride) + (targetX / 8);
            int bitIdx = 7 - (targetX % 8);
            ctx->outBuffer[byteIdx] &= ~(1 << bitIdx);
        }
    }
}

