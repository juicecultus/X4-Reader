#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include "soc/soc_caps.h"
#include <stdint.h>

#define USB_VID 0x303a
#define USB_PID 0x82D4

#define HAS_KEYBOARD     // has keyboard to use
#define HAS_KEYBOARD_HID // has keyboard to use

#define KB_I2C_ADDRESS 0x34
#define BQ25896_I2C_ADDRESS 0x6B

static const uint8_t TX = 43;
static const uint8_t RX = 44;

static const uint8_t SDA = 3;
static const uint8_t SCL = 2;

static const uint8_t SS = 21;
static const uint8_t MOSI = 34;
static const uint8_t MISO = 33;
static const uint8_t SCK = 35;

#define CAPS_LOCK 0x00
#define SHIFT 0x1c
#define KEY_LEFT_SHIFT 0x1c
#define KEY_FN 0x14
#define KEY_BACKSPACE 0x1d
#define KEY_ENTER 0x13

#endif /* Pins_Arduino_h */
