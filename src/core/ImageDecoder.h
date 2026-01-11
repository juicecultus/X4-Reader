#ifndef IMAGE_DECODER_H
#define IMAGE_DECODER_H

#include <Arduino.h>
#include <SD.h>

#include <JPEGDEC.h>
#undef MOTOSHORT
#undef MOTOLONG
#undef INTELSHORT
#undef INTELLONG
#include <PNGdec.h>

class ImageDecoder {
public:
    struct DecodeContext {
        uint8_t* frameBuffer;
        uint8_t* grayscaleLsbBuffer;
        uint8_t* grayscaleMsbBuffer;
        bool planeMaskMode;
        uint8_t planeMask;
        uint16_t targetWidth;
        uint16_t targetHeight;
        int16_t offsetX;
        int16_t offsetY;
        uint16_t decodedWidth;
        uint16_t decodedHeight;
        uint16_t renderWidth;
        uint16_t renderHeight;
        bool rotateSource90;
        bool scaleToWidth;
        int16_t* errorBuf;
        uint16_t* pngLineBuf;
        size_t pngLineBufPixels;
        bool success;
    };
    /**
     * @brief Decodes a JPEG or PNG file from SD card using BBEPAPER driver.
     * 
     * @param path Path to the image file on SD card.
     * @param bbep Pointer to BBEPAPER driver instance.
     * @param frameBuffer Pointer to raw 1-bit framebuffer (800x480).
     * @param targetWidth Target width (800 for current display).
     * @param targetHeight Target height (480 for current display).
     * @return true if decoding was successful.
     */
    static bool decodeToDisplay(const char* path, uint8_t* frameBuffer, uint16_t targetWidth, uint16_t targetHeight,
                                uint8_t* grayscaleLsbBuffer = nullptr, uint8_t* grayscaleMsbBuffer = nullptr);

    static bool decodeToDisplayFitWidth(const char* path, uint8_t* frameBuffer, uint16_t targetWidth, uint16_t targetHeight,
                                        uint8_t* grayscaleLsbBuffer = nullptr, uint8_t* grayscaleMsbBuffer = nullptr);

    static bool decodeBmpPlaneFitWidth(const char* path, uint8_t* planeBuffer, uint16_t targetWidth, uint16_t targetHeight,
                                       uint8_t planeMask);

    static bool decodePlaneFitWidth(const char* path, uint8_t* planeBuffer, uint16_t targetWidth, uint16_t targetHeight,
                                    uint8_t planeMask);

private:
    static bool decodeBMPToDisplay(const char* path, DecodeContext* ctx);
    static bool decodeBMPToPlaneFitWidth(const char* path, uint8_t* planeBuffer, uint16_t targetWidth, uint16_t targetHeight,
                                         uint8_t planeMask);
    static PNG* currentPNG;
    static int JPEGDraw(JPEGDRAW *pDraw);
    
    // PNGdec callbacks
    static void PNGDraw(PNGDRAW *pDraw);
};

#endif // IMAGE_DECODER_H
