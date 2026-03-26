#pragma once

// ╔══════════════════════════════════════════════════════════════════════════╗
// ║  TFT_eSPI configuration for Waveshare ESP32-S3-Touch-LCD-1.28           ║
// ║  Display driver : GC9A01A  (SPI)                                        ║
// ║  IMPORTANT: this file MUST be #included before <TFT_eSPI.h>             ║
// ╚══════════════════════════════════════════════════════════════════════════╝

#define USER_SETUP_LOADED
#define USER_SETUP_ID  302

#define GC9A01_DRIVER

#define TFT_WIDTH   240
#define TFT_HEIGHT  240

// ── SPI display pins ──────────────────────────────────────────────────────────
#define TFT_MOSI   11
#define TFT_SCLK   10
#define TFT_MISO   12
#define TFT_CS      9
#define TFT_DC      8
#define TFT_RST    14
#define TFT_BL      2

#define TFT_BACKLIGHT_ON HIGH

// Required to prevent SPI bus conflicts on ESP32-S3
#define USE_HSPI_PORT

// ── Fonts to load ─────────────────────────────────────────────────────────────
#define LOAD_GLCD    // 8-px system font
#define LOAD_FONT2   // 16-px
#define LOAD_FONT4   // 26-px
#define LOAD_FONT6   // 48-px (digits)
#define LOAD_FONT7   // 7-segment 48-px
#define LOAD_GFXFF   // GFX free fonts
#define SMOOTH_FONT

// ── SPI clock speeds ─────────────────────────────────────────────────────────
#define SPI_FREQUENCY       40000000
#define SPI_READ_FREQUENCY  20000000

// ╔══════════════════════════════════════════════════════════════════════════╗
// ║  Board pin map                                                           ║
// ╚══════════════════════════════════════════════════════════════════════════╝

// ── Touch (CST816S via I2C) ───────────────────────────────────────────────────
#define PIN_SDA          6
#define PIN_SCL          7
#define PIN_TOUCH_RST   13
#define PIN_TOUCH_INT    5

// ── Battery ADC ───────────────────────────────────────────────────────────────
// Voltage divider: 200 kΩ (top) + 100 kΩ (bottom)  →  V_bat = V_adc × 3
#define PIN_BAT_ADC      1

// ── Backlight (alias, same as TFT_BL) ────────────────────────────────────────
#define PIN_LCD_BL       2

// ╔══════════════════════════════════════════════════════════════════════════╗
// ║  NTP / timezone                                                          ║
// ╚══════════════════════════════════════════════════════════════════════════╝
#define NTP_SERVER1  "pool.ntp.org"
#define NTP_SERVER2  "time.nist.gov"
// POSIX tz string: Eastern (EST-5 / EDT-4, DST auto-switches)
#define TZ_STRING    "EST5EDT,M3.2.0,M11.1.0"

// ╔══════════════════════════════════════════════════════════════════════════╗
// ║  Page IDs                                                                ║
// ╚══════════════════════════════════════════════════════════════════════════╝
#define PAGE_CLOCK1    0
#define PAGE_CLOCK2    1
#define PAGE_MOON      2   // analog moon clock
#define PAGE_MOON_DIG  3   // digital moon clock
#define PAGE_DIAG_NET  4   // diagnostics — network (WiFi, IP, MAC, NTP, Gateway, DNS)
#define PAGE_DIAG_SYS  5   // diagnostics — system  (heap, flash, CPU temp, IMU tilt)
#define NUM_PAGES      6
