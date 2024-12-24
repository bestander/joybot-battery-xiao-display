#pragma once
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>
#include <SPI.h>

// Display pins for Hardware SPI
#define TFT_CS    1    // Chip select can be any digital pin
#define TFT_DC    3    // Data/Command can be any digital pin
#define TFT_RST   2    // Reset can be any digital pin

extern Adafruit_GC9A01A tft;
