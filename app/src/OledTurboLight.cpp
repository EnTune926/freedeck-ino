#include "./OledTurboLight.h"
#include <Arduino.h>
#include "../settings.h"
#include "./FreeDeck.h"

// -------------------- FRAMEBUFFER --------------------
static uint8_t framebuffer[8][128]; // 8 pages, 128 columns

// Track which pages are dirty and need to be sent to OLED
static bool dirty_pages[8] = {true, true, true, true, true, true, true, true};

// -------------------- LOW-LEVEL I2C --------------------
// (keep your existing i2cByteOut, i2cBegin, i2cWrite, i2cEnd functions)

// -------------------- OLED COMMANDS --------------------
static void oledWriteCommand(uint8_t c) {
  uint8_t buf[2] = {0x00, c};
  I2CWrite(oled_addr, buf, 2);
}

static void oledSetPosition(int x, int y) {
  oledWriteCommand(0xb0 | y);
  oledWriteCommand(0x00 | (x & 0xf));
  oledWriteCommand(0x10 | ((x >> 4) & 0xf));
}

// -------------------- OPTIMIZED PIXEL FUNCTIONS --------------------
int oledSetPixel(int x, int y, uint8_t color) {
  if (x < 0 || x >= 128 || y < 0 || y >= 64) return -1;
  int page = y / 8;
  uint8_t mask = 1 << (y % 8);

  if (color)
    framebuffer[page][x] |= mask;
  else
    framebuffer[page][x] &= ~mask;

  dirty_pages[page] = true;
  return 0;
}

// Flush a single page to the OLED if dirty
static void flushPage(int page) {
  if (!dirty_pages[page]) return;
  oledSetPosition(0, page);
  uint8_t buf[129];
  buf[0] = 0x40; // data command
  memcpy(&buf[1], framebuffer[page], 128);
  I2CWrite(oled_addr, buf, 129);
  dirty_pages[page] = false;
}

// Flush all dirty pages
void oledFlush() {
  for (int i = 0; i < 8; i++) {
    flushPage(i);
  }
}

// -------------------- BMP DATA --------------------
void oledLoadBMPPart(uint8_t *pBMP, int bytes, int offset) {
  int page_start = offset / 128;
  int page_count = bytes / 128;

  for (int p = 0; p < page_count; p++) {
    memcpy(framebuffer[page_start + p], &pBMP[p * 128], 128);
    dirty_pages[page_start + p] = true;
  }
  oledFlush();
}

// -------------------- FILL --------------------
void oledFill(uint8_t data) {
  for (int p = 0; p < 8; p++) {
    memset(framebuffer[p], data, 128);
    dirty_pages[p] = true;
  }
  oledFlush();
}
