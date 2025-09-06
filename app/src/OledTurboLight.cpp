#include "OledTurboLight.h"
#include "../settings.h"
#include "./FreeDeck.h"

static uint8_t oled_addr;
static uint8_t framebuffer[MAX_CACHE] = {0};
static int iScreenOffset;

// Low-level I²C routines
static inline void i2cByteOut(uint8_t b) {
    static uint8_t bOld;
    bOld &= ~((1 << BB_SDA) | (1 << BB_SCL));
    for (uint8_t i = 0; i < 8; i++) {
        bOld &= ~(1 << BB_SDA);
        if (b & 0x80) bOld |= (1 << BB_SDA);
        I2CPORT = bOld;
        delayMicroseconds(oled_delay);
        I2CPORT |= (1 << BB_SCL);
        delayMicroseconds(oled_delay);
        I2C_CLK_LOW();
        b <<= 1;
    }
    // ACK bit
    I2CPORT = bOld & ~(1 << BB_SDA);
    delayMicroseconds(oled_delay);
    I2CPORT |= (1 << BB_SCL);
    delayMicroseconds(oled_delay);
    I2C_CLK_LOW();
}

void i2cBegin(uint8_t addr) {
    I2CPORT |= ((1 << BB_SDA) | (1 << BB_SCL));
    I2CDDR |= ((1 << BB_SDA) | (1 << BB_SCL));
    I2CPORT &= ~(1 << BB_SDA);
    delayMicroseconds((oled_delay + 1) * 2);
    I2CPORT &= ~(1 << BB_SCL);
    i2cByteOut(addr << 1);
}

void i2cWrite(uint8_t *pData, uint8_t bLen) {
    uint8_t i, b;
    uint8_t bOld = I2CPORT & ~((1 << BB_SDA) | (1 << BB_SCL));

    while (bLen--) {
        b = *pData++;
        for (i = 0; i < 8; i++) {
            bOld &= ~(1 << BB_SDA);
            if (b & 0x80) bOld |= (1 << BB_SDA);
            I2CPORT = bOld;
            delayMicroseconds(oled_delay);
            I2CPORT |= (1 << BB_SCL);
            delayMicroseconds(oled_delay);
            I2C_CLK_LOW();
            b <<= 1;
        }
        I2CPORT &= ~(1 << BB_SDA);
        I2CPORT |= (1 << BB_SCL);
        I2CPORT &= ~(1 << BB_SCL);
    }
}

void i2cEnd() {
    I2CPORT &= ~(1 << BB_SDA);
    I2CPORT |= (1 << BB_SCL);
    I2CPORT |= (1 << BB_SDA);
    I2CDDR &= ~((1 << BB_SDA) | (1 << BB_SCL));
}

static void I2CWrite(int iAddr, unsigned char *pData, int iLen) {
    i2cBegin(oled_addr);
    i2cWrite(pData, iLen);
    i2cEnd();
}

// OLED commands
static void oledWriteCommand(unsigned char c) {
    unsigned char buf[2] = {0x00, c};
    I2CWrite(oled_addr, buf, 2);
}

static void oledWriteCommand2(unsigned char c, unsigned char d) {
    unsigned char buf[3] = {0x00, c, d};
    I2CWrite(oled_addr, buf, 3);
}

void oledInit(uint8_t bAddr, uint8_t pre_charge_period, uint8_t refresh_frequency) {
    oled_addr = bAddr;
    I2CPORT |= (1 << BB_SDA) | (1 << BB_SCL);
    I2CDDR &= ~((1 << BB_SDA) | (1 << BB_SCL));

    unsigned char init_buf[] = {
        0x00, 0xae, 0xa8, 0x3f, 0xd3, 0x00, 0x40, 0xa1,
        0xc8, 0xda, 0x12, 0x81, 0xff, 0xa4, 0xa6, 0xd5,
        refresh_frequency, 0x8d, 0x14, 0xaf, 0x20, 0x00,
        0xd9, pre_charge_period, 0xdb, MINIMUM_BRIGHTNESS
    };
    I2CWrite(oled_addr, init_buf, sizeof(init_buf));
}

void oledShutdown() { oledWriteCommand(0xae); }
void oledTurnOn() { oledWriteCommand(0xaf); }
void oledSetContrast(unsigned char ucContrast) { oledWriteCommand2(0x81, ucContrast); }

static void oledSetPosition(int x, int y) {
    oledWriteCommand(0xb0 | y);
    oledWriteCommand(0x00 | (x & 0xf));
    oledWriteCommand(0x10 | ((x >> 4) & 0xf));
    iScreenOffset = y * 128 + x;
}

static void oledWriteDataBlock(unsigned char *ucBuf, int iLen) {
    unsigned char temp[iLen + 1];
    temp[0] = 0x40;
    memcpy(&temp[1], ucBuf, iLen);
    I2CWrite(oled_addr, temp, iLen + 1);
}

int oledSetPixel(int x, int y, unsigned char ucColor) {
    if (x < 0 || x >= 128 || y < 0 || y >= 64) return -1;
    int idx = (y / 8) * 128 + x;
    uint8_t bit = 1 << (y % 8);
    if (ucColor) framebuffer[idx] |= bit;
    else framebuffer[idx] &= ~bit;

    static uint8_t last_val[MAX_CACHE] = {0};
    if (framebuffer[idx] != last_val[idx]) {
        oledSetPosition(x, y / 8);
        oledWriteDataBlock(&framebuffer[idx], 1);
        last_val[idx] = framebuffer[idx];
    }
    return 0;
}

void oledLoadBMPPart(uint8_t *pBMP, int bytes, int offset) {
    int lines = bytes / 128;
    oledSetPosition(0, offset / 8);
    for (int y = 0; y < lines; y++) {
        oledWriteDataBlock(&pBMP[y * 128], 128);
        memcpy(&framebuffer[(offset/8 + y) * 128], &pBMP[y * 128], 128);
    }
}

void oledFill(unsigned char ucData) {
    memset(framebuffer, ucData, MAX_CACHE);
    uint8_t temp[16];
    memset(temp, ucData, 16);
    for (int y = 0; y < 8; y++) {
        oledSetPosition(0, y);
        for (int x = 0; x < 8; x++) {
            oledWriteDataBlock(temp, 16);
        }
    }
}
