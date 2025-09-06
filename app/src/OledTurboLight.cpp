#include "./OledTurboLight.h"
#include <Arduino.h>
#include "../settings.h"
#include "./FreeDeck.h"

// some globals
static int iScreenOffset;  // current write offset of screen data
static uint8_t oled_addr;
static uint8_t bCache[MAX_CACHE] = {0x40};  // for faster character drawing
static uint8_t bEnd = 1;
static void oledWriteCommand(unsigned char c);

// I²C low-level functions
static inline void i2cByteOut(uint8_t b) {
    uint8_t i;
    uint8_t bOld = I2CPORT & ~((1 << BB_SDA) | (1 << BB_SCL));
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
    // ack bit
    I2CPORT = bOld & ~(1 << BB_SDA);
    delayMicroseconds(oled_delay);
    I2CPORT |= (1 << BB_SCL);
    delayMicroseconds(oled_delay);
    I2C_CLK_LOW();
}

void i2cBegin(uint8_t addr) {
    I2CPORT |= ((1 << BB_SDA) + (1 << BB_SCL));
    I2CDDR |= ((1 << BB_SDA) + (1 << BB_SCL));
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
        if (b == 0 || b == 0xff) {
            bOld &= ~(1 << BB_SDA);
            if (b & 0x80) bOld |= (1 << BB_SDA);
            I2CPORT = bOld;
            delayMicroseconds(oled_delay);
            for (i = 0; i < 8; i++) {
                I2CPORT |= (1 << BB_SCL);
                delayMicroseconds(oled_delay);
                I2C_CLK_LOW();
            }
        } else {
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
        }
        // ack bit
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

// OLED high-level functions
void oledInit(uint8_t bAddr, uint8_t pre_charge_period, uint8_t refresh_frequency) {
    unsigned char oled_initbuf[] = {
        0x00, 0xae, 0xa8, 0x3f, 0xd3, 0x00, 0x40, 0xa1, 0xc8, 0xda, 0x12,
        0x81, 0xff, 0xa4, 0xa6, 0xd5, refresh_frequency, 0x8d, 0x14, 0xaf,
        0x20, 0x00, 0xd9, pre_charge_period, 0xdb, MINIMUM_BRIGHTNESS
    };

    oled_addr = bAddr;
    I2CDDR &= ~(1 << BB_SDA);
    I2CDDR &= ~(1 << BB_SCL);
    I2CPORT |= (1 << BB_SDA);
    I2CPORT |= (1 << BB_SCL);

    I2CWrite(oled_addr, oled_initbuf, sizeof(oled_initbuf));
}

void oledShutdown() { oledWriteCommand(0xae); }
void oledTurnOn() { oledWriteCommand(0xaf); }

static void oledWriteCommand(unsigned char c) {
    unsigned char buf[2];
    buf[0] = 0x00;
    buf[1] = c;
    I2CWrite(oled_addr, buf, 2);
}

static void oledWriteCommand2(unsigned char c, unsigned char d) {
    unsigned char buf[3];
    buf[0] = 0x00;
    buf[1] = c;
    buf[2] = d;
    I2CWrite(oled_addr, buf, 3);
}

void oledSetContrast(unsigned char ucContrast) {
    oledWriteCommand2(0x81, ucContrast);
}

static void oledSetPosition(int x, int y) {
    oledWriteCommand(0xb0 | y);
    oledWriteCommand(0x00 | (x & 0xf));
    oledWriteCommand(0x10 | ((x >> 4) & 0xf));
    iScreenOffset = (y * 128) + x;
}

static void oledWriteDataBlock(unsigned char *ucBuf, int iLen) {
    unsigned char ucTemp[iLen + 1];
    ucTemp[0] = 0x40;
    memcpy(&ucTemp[1], ucBuf, iLen);
    I2CWrite(oled_addr, ucTemp, iLen + 1);
}

int oledSetPixel(int x, int y, unsigned char ucColor) {
    int i = ((y >> 3) * 128) + x;
    if (i < 0 || i > 1023) return -1;

    unsigned char uc = 0;
    if (ucColor) uc |= (0x1 << (y & 7));
    oledSetPosition(x, y >> 3);
    oledWriteDataBlock(&uc, 1);
    return 0;
}

void oledLoadBMPPart(uint8_t *pBMP, int bytes = 1024, int offset = 0) {
    int iPitch = 128;
    int rows = bytes / iPitch;
    oledSetPosition(0, offset / 16 / 8);
    for (int y = 0; y < rows; y++) {
        oledWriteDataBlock(&pBMP[y * iPitch], iPitch);
    }
}

// ------------------ NEW: streaming serial image ------------------
void oled_write_data() {
    last_data_received = millis();
    uint8_t display = readSerialBinary();
    setMuxAddress(display, DISPLAY);

    uint16_t received = 0;
    uint32_t ellapsed = millis();
    unsigned char buffer[IMG_CACHE_SIZE];

    while (received < 1024) {
        while (!Serial.available()) {
            if (millis() - ellapsed > 1000) return;
        }
        ellapsed = millis();
        size_t len = Serial.readBytes(buffer, IMG_CACHE_SIZE);
        oledLoadBMPPart(buffer, len, received);
        received += len;
    }
}

// Fill display with a pattern
void oledFill(unsigned char ucData) {
    unsigned char temp[16];
    memset(temp, ucData, 16);
    for (int y = 0; y < 8; y++) {
        oledSetPosition(0, y);
        for (int x = 0; x < 8; x++) {
            oledWriteDataBlock(temp, 16);
        }
    }
}
