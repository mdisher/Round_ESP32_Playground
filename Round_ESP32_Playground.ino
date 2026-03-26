/*
 * ╔══════════════════════════════════════════════════════════════════════════╗
 * ║  ESP32-S3-Touch-LCD-1.28  –  Round Display Playground                    ║
 * ║  Waveshare  |  240×240 IPS round display                                 ║
 * ╠══════════════════════════════════════════════════════════════════════════╣
 * ║  Hardware                                                                ║
 * ║    Display  : GC9A01A (SPI)     Touch : CST816S (I2C)                    ║
 * ║    IMU      : QMI8658 (I2C)     Flash : 16 MB   PSRAM: 2 MB OPI          ║
 * ╠══════════════════════════════════════════════════════════════════════════╣
 * ║  Pages  (swipe left / right to navigate)                                 ║
 * ║    0 – Phantom Diver  (dark navy dial, lume markers)                     ║
 * ║    1 – Ocean          (deep blue gradient, gold accents)                 ║
 * ║    2 – Moon Analog    (real moon photo + terminator shading)             ║
 * ║    3 – Moon Digital   (same background, 12-hr HH:MM:SS)                 ║
 * ║    4 – Diag: Network  (WiFi, IP, MAC, NTP, GW, DNS, Ch, TX)             ║
 * ║    5 – Diag: System   (heap, flash, CPU temp, chip, battery, IMU tilt)  ║
 * ╠══════════════════════════════════════════════════════════════════════════╣
 * ║  Required libraries (Arduino Library Manager)                            ║
 * ║    • TFT_eSPI  by Bodmer                                                 ║
 * ╠══════════════════════════════════════════════════════════════════════════╣
 * ║  Arduino IDE board settings                                              ║
 * ║    Board   → ESP32S3 Dev Module                                          ║
 * ║    PSRAM   → OPI PSRAM                                                   ║
 * ║    USB CDC → Enabled                                                     ║
 * ║    Upload  → USB-OTG (or UART via GPIO43/44)                             ║
 * ╚══════════════════════════════════════════════════════════════════════════╝
 *
 * IMPORTANT: config.h MUST be included first — it defines USER_SETUP_LOADED
 * so TFT_eSPI skips its own User_Setup.h and uses our pin/driver defines.
 */

#include "config.h"           // ← FIRST: TFT_eSPI setup defines
#include <TFT_eSPI.h>
#include <Wire.h>
#include <WiFi.h>
#include <time.h>
#include <esp_system.h>       // esp_reset_reason()

#include "secrets.h"
#include "touch_cst816s.h"
#include "clock_faces.h"
#include "page_diagnostics.h"
#include "clock_face_moon.h"

// ── Globals ───────────────────────────────────────────────────────────────────
TFT_eSPI    tft    = TFT_eSPI();
TFT_eSprite canvas = TFT_eSprite(&tft);
CST816S     touch(PIN_TOUCH_RST, PIN_TOUCH_INT);

uint8_t currentPage = PAGE_CLOCK1;
bool    needsRedraw = true;
bool    ntpSynced   = false;
uint8_t lastSecond  = 255;

// ── WiFi connection ───────────────────────────────────────────────────────────
static void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.printf("  Connecting to \"%s\"", WIFI_SSID);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) {
    delay(500);
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("  IP    : %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("  SSID  : %s\n", WiFi.SSID().c_str());
    Serial.printf("  RSSI  : %d dBm\n", WiFi.RSSI());
    Serial.printf("  MAC   : %s\n", WiFi.macAddress().c_str());
  } else {
    Serial.println("  WiFi connection failed — running offline.");
  }
}

// ── NTP sync ──────────────────────────────────────────────────────────────────
static void syncNTP() {
  // Configure timezone (Eastern: EST-5 / EDT-4 with auto-DST)
  configTzTime(TZ_STRING, NTP_SERVER1, NTP_SERVER2);

  Serial.print("  Waiting for NTP");
  struct tm t;
  for (int i = 0; i < 8; i++) {
    if (getLocalTime(&t, 600)) {   // 600 ms per attempt
      ntpSynced = true;
      break;
    }
    Serial.print('.');
  }
  if (ntpSynced)
    Serial.printf("\n  NTP synced: %02d:%02d:%02d %02d/%02d/%04d\n",
                  t.tm_hour, t.tm_min, t.tm_sec,
                  t.tm_mon + 1, t.tm_mday, t.tm_year + 1900);
  else
    Serial.println("\n  NTP sync failed — clock will show 00:00:00.");
}

// ── Splash screen shown during boot ──────────────────────────────────────────
static void showSplash() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Connecting...", 120, 110, 4);
  tft.setTextColor(COL_LGRAY, TFT_BLACK);
  tft.drawString(WIFI_SSID, 120, 145, 2);
}

// ── setup ─────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n╔══ ESP32-S3 Round Display ════════════════════╗");
  // Print reset reason — helps diagnose unexpected reboots / watchdog hits
  static const char* RR[] = {
    "?","PowerOn","ExtPin","SW","Panic","IntWDT","TaskWDT","WDT","DeepSleep","Brownout","SDIO"
  };
  esp_reset_reason_t rr = esp_reset_reason();
  Serial.printf("  Reset : %s\n", rr < 11 ? RR[rr] : "?");

  // Backlight on
  pinMode(PIN_LCD_BL, OUTPUT);
  digitalWrite(PIN_LCD_BL, HIGH);

  // Display init
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  showSplash();

  // Double-buffer sprite — ESP32-S3 with OPI PSRAM uses PSRAM for large allocs
  canvas.setColorDepth(16);
  if (!canvas.createSprite(240, 240)) {
    Serial.println("  ERROR: sprite allocation failed (check PSRAM setting)");
  } else {
    Serial.printf("  Sprite OK  (%u bytes free heap)\n", ESP.getFreeHeap());
  }

  // I2C bus (touch + IMU share the same bus)
  Wire.begin(PIN_SDA, PIN_SCL, 400000);

  // Touch controller
  Serial.print("  Touch: ");
  touch.begin();

  // Battery ADC — GPIO1 through a 200k/100k voltage divider
  analogSetPinAttenuation(PIN_BAT_ADC, ADC_11db);
  analogReadResolution(12);

  // WiFi
  Serial.println("  WiFi:");
  connectWiFi();

  // NTP (only worthwhile if online)
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("  NTP:");
    syncNTP();
  }

  Serial.println("╚═══════════════════════════════════════════════╝\n");
  needsRedraw = true;
}

// ── loop ──────────────────────────────────────────────────────────────────────
void loop() {

  // ── Touch / gesture ───────────────────────────────────────────────────────
  static uint32_t lastGestureMs = 0;
  TouchData td = touch.read();

  if (td.touched && td.gesture != GEST_NONE) {
    uint32_t now = millis();
    if (now - lastGestureMs > 500) {    // 500 ms de-bounce
      lastGestureMs = now;

      if (td.gesture == GEST_SWIPE_LEFT) {
        currentPage = (currentPage + 1) % NUM_PAGES;
        needsRedraw = true;
        Serial.printf("→ Page %d\n", currentPage);
      } else if (td.gesture == GEST_SWIPE_RIGHT) {
        currentPage = (currentPage + NUM_PAGES - 1) % NUM_PAGES;
        needsRedraw = true;
        Serial.printf("← Page %d\n", currentPage);
      }
    }
  }

  // ── Time-driven redraws ───────────────────────────────────────────────────
  struct tm timeinfo;
  bool haveTime = getLocalTime(&timeinfo, 0);   // non-blocking

  // Clock pages: redraw once per second
  if (currentPage < PAGE_DIAG_NET && haveTime) {
    if (timeinfo.tm_sec != lastSecond) {
      lastSecond  = timeinfo.tm_sec;
      needsRedraw = true;
    }
  }

  // Diagnostics pages: refresh every 2 s
  static uint32_t lastDiagMs = 0;
  if (currentPage >= PAGE_DIAG_NET && millis() - lastDiagMs >= 2000) {
    lastDiagMs  = millis();
    needsRedraw = true;
  }

  // ── Debug heartbeat (every 5 s) ───────────────────────────────────────────
  static uint32_t lastDbgMs = 0;
  if (millis() - lastDbgMs >= 5000) {
    lastDbgMs = millis();
    bool wok = (WiFi.status() == WL_CONNECTED);
    Serial.printf("[%5lus] page=%d  heap=%u  wifi=%s(%ddBm)\n",
                  millis() / 1000UL,
                  currentPage,
                  ESP.getFreeHeap(),
                  wok ? "OK" : "off",
                  wok ? (int)WiFi.RSSI() : 0);
  }

  // ── WiFi reconnect (every 30 s when offline) ──────────────────────────────
  static uint32_t lastWifiMs = 0;
  if (millis() - lastWifiMs >= 30000) {
    lastWifiMs = millis();
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[WiFi] offline — reconnecting");
      WiFi.reconnect();
    }
  }

  // ── NTP daily refresh (86400000 ms = 24 h) ─────────────────────────────
  static uint32_t lastNtpMs = 0;
  if (WiFi.status() == WL_CONNECTED && millis() - lastNtpMs >= 86400000UL) {
    lastNtpMs = millis();
    configTzTime(TZ_STRING, NTP_SERVER1, NTP_SERVER2);
    Serial.println("[NTP] daily refresh triggered");
  }

  // ── Render ────────────────────────────────────────────────────────────────
  if (needsRedraw) {
    needsRedraw = false;

    int h     = haveTime ? timeinfo.tm_hour        : 0;
    int m     = haveTime ? timeinfo.tm_min         : 0;
    int s     = haveTime ? timeinfo.tm_sec         : 0;
    int day   = haveTime ? timeinfo.tm_mday        : 1;
    int month = haveTime ? (timeinfo.tm_mon  + 1)  : 1;
    int year  = haveTime ? (timeinfo.tm_year + 1900) : 2025;

    switch (currentPage) {
      case PAGE_CLOCK1:    drawClockFace1(canvas, h, m, s, day, currentPage); break;
      case PAGE_CLOCK2:    drawClockFace2(canvas, h, m, s, day, currentPage); break;
      case PAGE_MOON:      drawClockFace3(canvas, h, m, s, day, year, month, currentPage); break;
      case PAGE_MOON_DIG:  drawClockFace3Digital(canvas, h, m, s, day, year, month, currentPage); break;
      case PAGE_DIAG_NET:  drawNetworkPage(canvas, ntpSynced, currentPage); break;
      case PAGE_DIAG_SYS:  drawSystemPage(canvas, currentPage); break;
    }

    canvas.pushSprite(0, 0);
  }

  delay(20);    // ~50 Hz poll rate
}
