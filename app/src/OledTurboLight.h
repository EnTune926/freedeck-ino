#ifndef OLED_TURBO_LIGHT_H
#define OLED_TURBO_LIGHT_H

#include <Arduino.h>

#define I2CPORT PORTD
#define I2CDDR DDRD
#define I2C_CLK_LOW() I2CPORT = bOld

#define MAX_CACHE 1024

// I²C low-level helpers
static inline void i2cByteOut(uint8_t b);
void i2cBegin(uint8_t addr);
void i2cWrite(uint8_t *pData, uint8_t bLen);
void i2cEnd();
static void I2CWrite(int iAddr, unsigned char *pData, int iLen);

// OLED API
void oledInit(uint8_t bAddr, uint8_t pre_charge_period, uint8_t refresh_frequency);
void oledShutdown();
void oledTurnOn();
void oledSetContrast(unsigned char ucContrast);

// Pixel & framebuffer
int oledSetPixel(int x, int y, unsigned char ucColor);
void oledLoadBMPPart(uint8_t *pBMP, int bytes = 1024, int offset = 0);
void oledFill(unsigned char ucData);

#endif
