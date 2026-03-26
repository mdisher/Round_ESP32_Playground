#pragma once
#include "config.h"
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <Wire.h>
#include <math.h>
#include <esp_system.h>
#include "clock_faces.h"   // drawPageDots, COL_* palette

// ── Circle boundary reference (r=119, centre 120,120, 8px safety margin) ──────
//   y=18  → safe width 107px    y=46  → 170px    y=62  → 192px
//   y=78  → 206px               y=94  → 218px    y=109 → 222px
//   y=137 → 220px               y=154 → 212px    y=170 → 200px
//   y=185 → 183px               y=196 → 167px    y=218 → 119px (dots)

static const uint16_t DIAG_BG = C565(10, 12, 28);   // dark navy background

// ─── QMI8658 IMU (I2C 0x6B) ─────────────────────────────────────────────────
// Minimal accel-only init; returns tilt angle from horizontal, or -999 if absent.
#define QMI8658_ADDR 0x6B
static bool _imuOk = false, _imuInit = false;

static void imuSetup() {
  if (_imuInit) return;
  _imuInit = true;
  Wire.beginTransmission(QMI8658_ADDR);
  Wire.write(0x00);                                    // WHO_AM_I
  if (Wire.endTransmission(false) != 0) return;
  Wire.requestFrom((uint8_t)QMI8658_ADDR, (uint8_t)1);
  if (!Wire.available() || Wire.read() != 0x05) return;
  // CTRL1 = 0x60: address auto-increment, little-endian
  Wire.beginTransmission(QMI8658_ADDR); Wire.write(0x02); Wire.write(0x60); Wire.endTransmission(true);
  // CTRL2 = 0x78: ±8 g, 61 Hz  (aODR=0111, aFS=10)
  Wire.beginTransmission(QMI8658_ADDR); Wire.write(0x03); Wire.write(0x78); Wire.endTransmission(true);
  // CTRL7 = 0x01: enable accelerometer
  Wire.beginTransmission(QMI8658_ADDR); Wire.write(0x08); Wire.write(0x01); Wire.endTransmission(true);
  _imuOk = true;
}

static float imuTiltDeg() {
  if (!_imuOk) return -999.f;
  Wire.beginTransmission(QMI8658_ADDR);
  Wire.write(0x35);                                    // AX_L
  if (Wire.endTransmission(false) != 0) return -999.f;
  Wire.requestFrom((uint8_t)QMI8658_ADDR, (uint8_t)6);
  if (Wire.available() < 6) return -999.f;
  uint8_t b[6]; for (int i = 0; i < 6; i++) b[i] = Wire.read();
  float ax = (int16_t)(b[0] | (b[1] << 8)) / 4096.f;
  float ay = (int16_t)(b[2] | (b[3] << 8)) / 4096.f;
  float az = (int16_t)(b[4] | (b[5] << 8)) / 4096.f;
  return atan2f(sqrtf(ax*ax + ay*ay), fabsf(az)) * (180.f / M_PI);
}

// ─── Layout helpers ──────────────────────────────────────────────────────────

// Chord-clipped divider line
static void diagDivider(TFT_eSprite& spr, int y) {
  int dy = abs(y - 120);
  if (dy >= 119) return;
  int hw = (int)sqrtf(119.f*119.f - (float)(dy*dy)) - 8;
  if (hw < 10) return;
  spr.drawFastHLine(120 - hw, y, hw * 2, COL_DGRAY);
}

// Centre the "label  value" pair as a unit using textWidth for exact placement.
// Both pieces share y (vertically centred); label is dim, value uses valCol.
static void diagRow(TFT_eSprite& spr, int y,
                    const char* label, const char* val,
                    uint16_t valCol = TFT_WHITE) {
  const int GAP = 6;
  int lw = spr.textWidth(label, 2);
  int vw = spr.textWidth(val,   2);
  int sx = 120 - (lw + GAP + vw) / 2;
  spr.setTextDatum(ML_DATUM);
  spr.setTextColor(COL_MGRAY, DIAG_BG);
  spr.drawString(label, sx, y, 2);
  spr.setTextColor(valCol, DIAG_BG);
  spr.drawString(val, sx + lw + GAP, y, 2);
}

// Centred string helper (MC_DATUM), font 2
static void diagCentre(TFT_eSprite& spr, int y, const char* str,
                        uint16_t col = COL_LGRAY) {
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(col, DIAG_BG);
  spr.drawString(str, 120, y, 2);
}

// 5 increasing-height signal bars centred at (cx, baseY)
static int rssiBars(int rssi) {
  if (rssi > -50) return 5;
  if (rssi > -60) return 4;
  if (rssi > -70) return 3;
  if (rssi > -80) return 2;
  return 1;
}
static void drawBars(TFT_eSprite& spr, int cx, int baseY, int filled) {
  int x = cx - (5*5 + 4*2) / 2;
  for (int i = 0; i < 5; i++) {
    int bh = 5 + i*3;
    spr.fillRect(x, baseY - bh, 5, bh, (i < filled) ? TFT_GREEN : COL_DGRAY);
    x += 7;
  }
}

// ═════════════════════════════════════════════════════════════════════════════
//  PAGE_DIAG_NET — network diagnostics
//  Safe widths at used rows: 46→170  62→192  78→206  94→218  109→222
//                            137→220  154→212  170→200  185→183  218→119
// ═════════════════════════════════════════════════════════════════════════════
void drawNetworkPage(TFT_eSprite& spr, bool ntpSynced, uint8_t page) {
  char buf[40];
  spr.fillSprite(TFT_BLACK);
  spr.fillSmoothCircle(120, 120, 119, DIAG_BG, TFT_BLACK);

  // Title — safe width 107px; "NETWORK" ≈ 82px ✓
  diagCentre(spr, 18, "NETWORK", COL_GOLD2);
  diagDivider(spr, 30);

  // WiFi status — safe 170px
  bool conn = (WiFi.status() == WL_CONNECTED);
  int  rssi  = conn ? WiFi.RSSI() : 0;
  if (conn) {
    snprintf(buf, sizeof(buf), "Online %d dBm", rssi);
    diagRow(spr, 46, "WiFi", buf, TFT_GREEN);
    // Signal bars centred, base at y=68 (tallest bar top ≈ y=51, no text overlap)
    drawBars(spr, 120, 68, rssiBars(rssi));
  } else {
    diagRow(spr, 46, "WiFi", "Offline", TFT_RED);
  }

  // SSID — safe 206px
  {
    String ssid = conn ? WiFi.SSID() : "--";
    if (ssid.length() > 16) ssid = ssid.substring(0, 15) + "..";
    diagRow(spr, 83, "SSID", ssid.c_str());
  }

  // IP — safe 218px; longest IP "255.255.255.255" ≈ 120px; "IP" ≈ 18px → 144px total ✓
  diagRow(spr, 98, "IP", conn ? WiFi.localIP().toString().c_str() : "--");

  // MAC — centred pair; "MAC" 33px + 6 + "AA:BB:CC:DD:EE:FF" ≈ 162px = 201px → safe at 109 (222px) ✓
  diagRow(spr, 113, "MAC", WiFi.macAddress().c_str(), C565(90, 215, 235));

  diagDivider(spr, 126);

  // NTP — safe 220px
  diagRow(spr, 139, "NTP",
          ntpSynced ? "Synced" : "Waiting",
          ntpSynced ? TFT_GREEN : TFT_YELLOW);

  // Gateway — safe 212px; "GW" + long IP ≈ 145px ✓
  diagRow(spr, 154, "GW", conn ? WiFi.gatewayIP().toString().c_str() : "--");

  // DNS — safe 200px
  diagRow(spr, 170, "DNS", conn ? WiFi.dnsIP().toString().c_str() : "--");

  // Channel + TX Power — one centred string; safe 183px
  // Longest realistic: "Ch 11   TX 20 dBm" ≈ 155px ✓
  if (conn) {
    snprintf(buf, sizeof(buf), "Ch %d   TX %d dBm", WiFi.channel(), WiFi.getTxPower());
  } else {
    snprintf(buf, sizeof(buf), "Ch --   TX -- dBm");
  }
  diagCentre(spr, 185, buf, COL_MGRAY);

  drawPageDots(spr, page, NUM_PAGES, DIAG_BG);
}

// ═════════════════════════════════════════════════════════════════════════════
//  PAGE_DIAG_SYS — system diagnostics
//  Safe widths: 46→170  62→192  78→206  94→218  121→222
//               137→220  152→214  180→190  196→167  218→119
// ═════════════════════════════════════════════════════════════════════════════
void drawSystemPage(TFT_eSprite& spr, uint8_t page) {
  char buf[40];
  spr.fillSprite(TFT_BLACK);
  spr.fillSmoothCircle(120, 120, 119, DIAG_BG, TFT_BLACK);

  // Title — safe 107px; "SYSTEM" ≈ 70px ✓
  diagCentre(spr, 18, "SYSTEM", COL_GOLD2);
  diagDivider(spr, 30);

  // Heap — safe 170px
  snprintf(buf, sizeof(buf), "%u / %u KB",
           ESP.getFreeHeap() / 1024, ESP.getHeapSize() / 1024);
  diagRow(spr, 46, "Heap", buf);

  // PSRAM — safe 192px
  {
    uint32_t fp = ESP.getFreePsram(), tp = ESP.getPsramSize();
    if (tp) snprintf(buf, sizeof(buf), "%.1f / %.1f MB", fp / 1048576.f, tp / 1048576.f);
    else    strcpy(buf, "None");
    diagRow(spr, 62, "PSRAM", buf);
  }

  // Flash: sketch size vs total — safe 206px
  // "Flash" + "234KB / 16MB" ≈ 48+6+120 = 174px ✓
  snprintf(buf, sizeof(buf), "%uKB / %uMB",
           ESP.getSketchSize() / 1024,
           ESP.getFlashChipSize() / (1024 * 1024));
  diagRow(spr, 78, "Flash", buf);

  // Uptime — safe 218px
  {
    unsigned long up = millis() / 1000UL;
    snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu", up / 3600, (up % 3600) / 60, up % 60);
    diagRow(spr, 94, "Up", buf);
  }

  diagDivider(spr, 108);

  // CPU temp — safe 222px; temperatureRead() available on ESP32 Arduino core
  {
    float tempF = temperatureRead() * 9.f / 5.f + 32.f;
    uint16_t tc = (tempF < 140.f) ? TFT_GREEN : (tempF < 158.f ? TFT_YELLOW : TFT_RED);
    snprintf(buf, sizeof(buf), "%.1f F", tempF);
    diagRow(spr, 121, "CPU Temp", buf, tc);
  }

  // Chip model + clock — centred single string — safe 220px
  snprintf(buf, sizeof(buf), "%s  %d MHz", ESP.getChipModel(), ESP.getCpuFreqMHz());
  diagCentre(spr, 137, buf);

  // Cores + revision — centred single string — safe 214px; "Cores 2   Rev 0" ≈ 130px ✓
  snprintf(buf, sizeof(buf), "Cores %d   Rev %d", ESP.getChipCores(), ESP.getChipRevision());
  diagCentre(spr, 152, buf);

  diagDivider(spr, 164);

  // Battery — safe 190px
  // "Batt" + "3.87V 72%" ≈ 42+6+90 = 138px ✓
  {
    float vbat = analogReadMilliVolts(PIN_BAT_ADC) * 3.f / 1000.f;
    uint16_t vc = (vbat > 3.70f) ? TFT_GREEN : (vbat > 3.40f ? TFT_YELLOW : TFT_RED);
    int pct = (int)constrain((vbat - 3.2f) / 1.0f * 100.f, 0.f, 100.f);
    snprintf(buf, sizeof(buf), "%.2fV  %d%%", vbat, pct);
    diagRow(spr, 180, "Batt", buf, vc);
  }

  // IMU / tilt — safe 167px; "IMU" + "12.3 deg" ≈ 33+6+80 = 119px ✓
  imuSetup();
  {
    float tilt = imuTiltDeg();
    if (tilt < -900.f) {
      diagRow(spr, 196, "IMU", "Offline", COL_MGRAY);
    } else {
      snprintf(buf, sizeof(buf), "%.1f deg", tilt);
      diagRow(spr, 196, "IMU", buf, C565(90, 215, 235));
    }
  }

  drawPageDots(spr, page, NUM_PAGES, DIAG_BG);
}
