#pragma once
#include "config.h"
#include <TFT_eSPI.h>
#include <WiFi.h>
#include "clock_faces.h"   // drawPageDots, COL_* palette

// ── Helpers ───────────────────────────────────────────────────────────────────

// RSSI → 0-5 bars
static int rssiBars(int rssi) {
  if (rssi > -50) return 5;
  if (rssi > -60) return 4;
  if (rssi > -70) return 3;
  if (rssi > -80) return 2;
  return 1;
}

// 5 increasing-height bars centred at (cx, baseY)
static void drawBars(TFT_eSprite& spr, int cx, int baseY, int filled) {
  int totalW = 5*5 + 4*2;  // 5 bars × 5px wide + 4 gaps × 2px = 33px
  int x = cx - totalW/2;
  for (int i=0; i<5; i++) {
    int bh = 5 + i*3;  // heights 5,8,11,14,17
    uint16_t col = (i < filled) ? TFT_GREEN : COL_DGRAY;
    spr.fillRect(x, baseY-bh, 5, bh, col);
    x += 7;
  }
}

// Centered label (dim) + value (bright) on same row, font 2
// maxLabelChars controls how much of the 240px width the label takes
static void diagRow(TFT_eSprite& spr, int y,
                    const char* label, const char* val,
                    uint16_t valCol = TFT_WHITE) {
  // Label right-aligned at x=118, value left-aligned at x=122 → centred pair
  spr.setTextColor(COL_MGRAY, TFT_BLACK);
  spr.setTextDatum(MR_DATUM);
  spr.drawString(label, 118, y, 2);
  spr.setTextColor(valCol, TFT_BLACK);
  spr.setTextDatum(ML_DATUM);
  spr.drawString(val, 122, y, 2);
}

// Thin horizontal divider line clipped to the round display boundary.
// Calculates the chord half-width at the given y using the circle equation
// (hw = sqrt(119^2 - (y-120)^2)), subtracts an 8px safety margin so the
// line doesn't bleed into the anti-aliased bezel, then draws a grey hLine.
static void diagDivider(TFT_eSprite& spr, int y) {
  // half-chord at this y: sqrt(119^2 - (y-120)^2) with margin
  int dy = abs(y-120);
  if (dy >= 119) return;
  int hw = (int)sqrtf(119.f*119.f - (float)(dy*dy)) - 8;  // 8 px safety margin
  if (hw < 10) return;
  spr.drawFastHLine(120-hw, y, hw*2, COL_DGRAY);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Diagnostics page — circular-display-aware layout
// ═════════════════════════════════════════════════════════════════════════════
void drawDiagnosticsPage(TFT_eSprite& spr, bool ntpSynced, uint8_t page) {
  char buf[40];
  spr.fillSprite(TFT_BLACK);

  // ── Title (y=18 — tight on the circle, ~138px safe width) ────────────────
  spr.fillSmoothCircle(120,120,119, C565(10,12,28), TFT_BLACK);  // dark blue bg
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(COL_GOLD2, C565(10,12,28));
  spr.drawString("DIAGNOSTICS", 120, 18, 2);

  diagDivider(spr, 30);

  // ── WiFi ──────────────────────────────────────────────────────────────────
  bool conn = (WiFi.status() == WL_CONNECTED);
  int  rssi = conn ? WiFi.RSSI() : 0;

  // Row 1: WiFi status + signal bars
  {
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(COL_MGRAY, TFT_BLACK);
    spr.drawString("WiFi", 120, 44, 2);
    // bars to the right of centre
    if (conn) {
      drawBars(spr, 168, 52, rssiBars(rssi));
    }
    spr.setTextColor(conn ? TFT_GREEN : TFT_RED, TFT_BLACK);
    spr.setTextDatum(MR_DATUM);
    spr.drawString(conn ? "Online" : "Offline", 114, 44, 2);
  }

  // dBm below bars (safe circle zone)
  if (conn) {
    snprintf(buf, sizeof(buf), "%ddBm", rssi);
    spr.setTextColor(COL_LGRAY, TFT_BLACK);
    spr.setTextDatum(MC_DATUM);
    spr.drawString(buf, 168, 62, 1);
  }

  // Row 2: SSID
  {
    String ssid = conn ? WiFi.SSID() : "--";
    if (ssid.length() > 14) ssid = ssid.substring(0,13) + "..";
    diagRow(spr, 76, "SSID", ssid.c_str());
  }

  // Row 3: IP
  diagRow(spr, 91, "IP", conn ? WiFi.localIP().toString().c_str() : "--");

  // Row 4: MAC — font 1 (smaller) to avoid overflow
  {
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(COL_MGRAY, TFT_BLACK);
    spr.drawString("MAC", 120, 106, 1);
    spr.setTextColor(C565(90,215,235), TFT_BLACK);
    spr.drawString(WiFi.macAddress().c_str(), 120, 116, 1);
  }

  // Row 5: NTP
  diagRow(spr, 130, "NTP",
          ntpSynced ? "Synced" : "Waiting",
          ntpSynced ? TFT_GREEN : TFT_YELLOW);

  diagDivider(spr, 141);

  // ── System ────────────────────────────────────────────────────────────────

  // Row 6: Heap
  snprintf(buf, sizeof(buf), "%u/%u KB",
           ESP.getFreeHeap()/1024, ESP.getHeapSize()/1024);
  diagRow(spr, 153, "Heap", buf);

  // Row 7: PSRAM
  {
    uint32_t fp=ESP.getFreePsram(), tp=ESP.getPsramSize();
    if (tp) snprintf(buf,sizeof(buf),"%.1f/%.1fMB", fp/1048576.f, tp/1048576.f);
    else    strcpy(buf,"None");
    diagRow(spr, 169, "PSRAM", buf);
  }

  // Row 8: Uptime
  {
    unsigned long up=millis()/1000UL;
    snprintf(buf,sizeof(buf),"%02lu:%02lu:%02lu", up/3600,(up%3600)/60,up%60);
    diagRow(spr, 185, "Up", buf);
  }

  // Row 9: Battery
  {
    float vbat = analogReadMilliVolts(PIN_BAT_ADC) * 3.f / 1000.f;
    uint16_t vc = (vbat>3.70f) ? TFT_GREEN : (vbat>3.40f ? TFT_YELLOW : TFT_RED);
    snprintf(buf,sizeof(buf),"%.2fV", vbat);
    diagRow(spr, 199, "Batt", buf, vc);
  }

  // Row 10: Chip summary — font 1, centred, above page dots
  snprintf(buf,sizeof(buf),"%s  %dMHz  %dMB",
           ESP.getChipModel(), ESP.getCpuFreqMHz(),
           (int)(ESP.getFlashChipSize()/(1024*1024)));
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(COL_MGRAY, TFT_BLACK);
  spr.drawString(buf, 120, 208, 1);

}
