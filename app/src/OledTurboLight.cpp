#include "./OledTurboLight.h"
#include <Arduino.h>
#include "../settings.h"
#include "./FreeDeck.h"

// --- OLED framebuffer constants ---
#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#define PAGE_HEIGHT 8
#define NUM_PAGES (OLED_HEIGHT / PAGE_HEIGHT)

static uint8_t oled_addr;
static uint8_t framebuffer[OLED_WIDTH * NUM_PAGES];    // 1-bit per pixel, 1024 bytes
static bool dirty_pages[NUM_PAGES];                    // track which pages need flushing
static uint8_t oled_delay = 1;                         // default delay for I2C

// --- low-level I2C functions (unchanged from your previous code) ---
static void i2cByteOut(uint8_t b);
void i2cBegin(uint8_t addr);
void i2cWrite(uint8_t *pData, uint8_t bLen);
void i2cEnd();
static void I2CWrite(int iAddr, unsigned char *pData, int iLen);

// --- OLED commands ---
static void oledWriteCommand(uint8_t c);
static void oledWriteCommand2(uint8_t c, uint8_t d);
static void oledSetPosition(int x, int page);

// --- high-level API ---
void oledInit(uint8_t bAddr, uint8_t pre_charge_period, uint8_t refresh_frequency) {
    oled_addr = bAddr;

    // Initialize I2C lines
    I2CDDR &= ~((1 << BB_SDA) | (1 << BB_SCL));
    I2CPORT |= ((1 << BB_SDA) | (1 << BB_SCL));

    unsigned char oled_initbuf[] = {
        0x00, 0xae, 0xa8, 0x3f, 0xd3, 0x00, 0x40, 0xa1, 0xc8, 0xda, 0x12,
        0x81, 0xff, 0xa4, 0xa6, 0xd5, refresh_frequency, 0x8d, 0x14, 0xaf,
        0x20, 0x00, 0xd9, pre_charge_period, 0xdb, MINIMUM_BRIGHTNESS
    };
    I2CWrite(oled_addr, oled_initbuf, sizeof(oled_initbuf));

    // Clear framebuffer
    memset(framebuffer, 0, sizeof(framebuffer));
    memset(dirty_pages, true, sizeof(dirty_pages)); // mark all pages dirty
}

// Clear display (framebuffer + OLED)
void oledFill(uint8_t color) {
    memset(framebuffer, color, sizeof(framebuffer));
    for (int i = 0; i < NUM_PAGES; i++) dirty_pages[i] = true;
}

// Set a pixel in the framebuffer
void oledSetPixel(int x, int y, uint8_t color) {
    if (x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT) return;
    int page = y / PAGE_HEIGHT;
    int index = page * OLED_WIDTH + x;
    uint8_t mask = 1 << (y % PAGE_HEIGHT);

    if (color)
        framebuffer[index] |= mask;
    else
        framebuffer[index] &= ~mask;

    dirty_pages[page] = true;  // mark page dirty
}

// Push the framebuffer to the OLED (only dirty pages)
void oledFlush() {
    for (int page = 0; page < NUM_PAGES; page++) {
        if (!dirty_pages[page]) continue;
        oledSetPosition(0, page);
        I2CWrite(oled_addr, &framebuffer[page * OLED_WIDTH], OLED_WIDTH);
        dirty_pages[page] = false;
    }
}

// Load a 128x64 BMP part into framebuffer
void oledLoadBMPPart(uint8_t *pBMP, int bytes = 1024, int offset = 0) {
    int page_offset = offset / 8;
    for (int i = 0; i < bytes; i++) {
        int page = (i / OLED_WIDTH) + page_offset;
        int col = i % OLED_WIDTH;
        if (page < NUM_PAGES)
            framebuffer[page * OLED_WIDTH + col] = pBMP[i];
    }
    for (int i = page_offset; i < page_offset + (bytes / OLED_WIDTH); i++)
        dirty_pages[i] = true;
}

// Clear screen wrapper
void oled_clear() {
    oledFill(0x00);
    oledFlush();
}

// Turn display on/off
void oled_power(uint8_t state) {
    oledWriteCommand(state ? 0xAF : 0xAE);
}

// Write text placeholder (can still update via framebuffer)
void oled_write_line() {
    // Your previous text-drawing code can modify framebuffer
    // then call oledFlush() at the end
}

// Set contrast
void oledSetContrast(uint8_t val) {
    oledWriteCommand2(0x81, val);
}

// --- low-level helpers ---
static void oledSetPosition(int x, int page) {
    oledWriteCommand(0xB0 | page);
    oledWriteCommand(0x00 | (x & 0x0F));
    oledWriteCommand(0x10 | ((x >> 4) & 0x0F));
}
