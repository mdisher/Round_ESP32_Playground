#pragma once
#include "config.h"
#include <TFT_eSPI.h>
#include <math.h>
#include "clock_faces.h"   // drawHand, drawPageDots, COL_* palette
#include "moon_image.h"    // 240x240 RGB565 full-moon PROGMEM array

// ─────────────────────────────────────────────────────────────────────────────
//  Moon phase  0.0=new  0.25=first-quarter  0.5=full  0.75=last-quarter
// ─────────────────────────────────────────────────────────────────────────────
static float calcMoonPhase(int year, int month, int day) {
  int  a  = (14 - month) / 12;
  int  y  = year - a;
  int  m  = month + 12*a - 2;
  long JD = (long)day + (long)(y+4800)*365L + y/4 - y/100 + y/400
            + (153*m+2)/5 - 32083L;
  float age = fmodf((float)(JD - 2451549L), 29.53059f);
  if (age < 0) age += 29.53059f;
  return age / 29.53059f;
}

static const char* moonPhaseName(float p) {
  if (p < 0.035f || p >= 0.965f) return "New Moon";
  if (p < 0.215f)                return "Wax Crescent";
  if (p < 0.285f)                return "First Quarter";
  if (p < 0.465f)                return "Wax Gibbous";
  if (p < 0.535f)                return "Full Moon";
  if (p < 0.715f)                return "Wan Gibbous";
  if (p < 0.785f)                return "Last Quarter";
  return                                "Wan Crescent";
}

// ── Filled ellipse (TFT_eSPI has no native filled-ellipse) ───────────────────
static void fillEllipse(TFT_eSprite& spr, int cx, int cy, int rx, int ry,
                         uint16_t col) {
  if (rx <= 0 || ry <= 0) return;
  float ry2 = (float)ry * ry;
  for (int dy = -ry; dy <= ry; dy++) {
    int xw = (int)(rx * sqrtf(fmaxf(0.f, 1.f - (float)(dy*dy)/ry2)));
    spr.drawFastHLine(cx-xw, cy+dy, 2*xw+1, col);
  }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Moon surface — blit the real photo from PROGMEM, clip to circle
// ─────────────────────────────────────────────────────────────────────────────
static void drawMoonSurface(TFT_eSprite& spr, int CX, int CY, int R) {
  // Push the 240×240 RGB565 image row by row, clipped to the moon circle
  int R2 = R * R;
  for (int y = 0; y < 240; y++) {
    int dy = y - CY;
    int dx2 = R2 - dy*dy;
    if (dx2 < 0) continue;           // row entirely outside circle
    int hw = (int)sqrtf((float)dx2); // half-chord width
    int x0 = CX - hw;
    int x1 = CX + hw;
    // Copy the in-circle portion of this row from PROGMEM
    const uint16_t* rowPtr = MOON_IMAGE + y * 240 + x0;
    for (int x = x0; x <= x1; x++) {
      spr.drawPixel(x, y, pgm_read_word(rowPtr++));
    }
  }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Terminator — transparent pixel-level darkening with soft penumbra
//
//  Convention (Northern-hemisphere): waxing = lit on right
//    phase 0.00 = new  (all dark)    phase 0.25 = first quarter (right lit)
//    phase 0.50 = full (all lit)     phase 0.75 = last quarter  (left lit)
//
//  Each in-circle pixel is read, its RGB components multiplied by a factor
//  that goes from EARTHSHINE (very dim) deep in shadow to 1.0 (unchanged)
//  in full light, with a 24-pixel-wide smooth (smoothstep) transition.
//  This preserves the actual photo detail on both sides of the terminator.
// ─────────────────────────────────────────────────────────────────────────────
static void applyTerminator(TFT_eSprite& spr, int CX, int CY, int R,
                              float phase) {
  const float EARTHSHINE = 0.07f;   // shadow brightness (earthshine glow)
  const float PENUMBRA   = 12.f;    // soft-edge half-width in pixels

  float cos_p  = (phase <= 0.5f) ?  cosf(2.f*PI*phase)
                                  : -cosf(2.f*PI*phase);
  float x_term = cos_p * R;

  int R2 = R * R;
  for (int y = 0; y < 240; y++) {
    int dy  = y - CY;
    int hw2 = R2 - dy*dy;
    if (hw2 < 0) continue;
    int hw = (int)sqrtf((float)hw2);

    for (int px = CX - hw; px <= CX + hw; px++) {
      float xf = (float)(px - CX);
      // dist > 0 = lit side,  dist < 0 = shadow side
      float dist = (phase <= 0.5f) ? (xf - x_term) : (x_term - xf);

      if (dist >= PENUMBRA) continue;    // fully lit — skip

      float factor;
      if (dist <= -PENUMBRA) {
        factor = EARTHSHINE;             // deep shadow: just earthshine
      } else {
        // Smoothstep blend across [-PENUMBRA, +PENUMBRA]
        float t = (dist + PENUMBRA) / (PENUMBRA * 2.f);
        factor  = EARTHSHINE + (1.f - EARTHSHINE) * t * t * (3.f - 2.f * t);
      }

      uint16_t c = spr.readPixel(px, y);
      uint8_t  r = (uint8_t)(((c >> 11) & 0x1F) * factor);
      uint8_t  g = (uint8_t)(((c >>  5) & 0x3F) * factor);
      uint8_t  b = (uint8_t)(( c        & 0x1F) * factor);
      spr.drawPixel(px, y, (uint16_t)((r << 11) | (g << 5) | b));
    }
  }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Transparent outlined text — floats over the moon without background boxes
//
//  TFT_eSPI skips the background fill when textcolor == textbgcolor, giving
//  true per-pixel transparency.  We draw a 1px dark outline first (8 dirs),
//  then the foreground on top — readable against any lighting condition.
// ─────────────────────────────────────────────────────────────────────────────
static void drawMoonText(TFT_eSprite& spr, const char* str,
                          int x, int y, uint8_t font,
                          uint16_t fg = TFT_WHITE) {
  const uint16_t DARK = C565(0, 0, 0);
  spr.setTextColor(DARK, DARK);   // bg == fg → no background fill (transparent)
  for (int8_t dx = -1; dx <= 1; dx++)
    for (int8_t dy = -1; dy <= 1; dy++)
      if (dx || dy) spr.drawString(str, x + dx, y + dy, font);
  spr.setTextColor(fg, fg);
  spr.drawString(str, x, y, font);
}

// ═════════════════════════════════════════════════════════════════════════════
//  FACE 3 – MOON PHASE CLOCK  (Analog)
//
//  Full-disc moon surface fills the display.  Terminator driven by real date.
//  No hour markers — just clean white hands for maximum moon visibility.
// ═════════════════════════════════════════════════════════════════════════════
void drawClockFace3(TFT_eSprite& spr,
                    int h, int m, int s,
                    int day, int year, int month,
                    uint8_t page) {
  const int CX=120, CY=120, R=118;
  spr.fillSprite(TFT_BLACK);

  float phase = calcMoonPhase(year, month, day);
  drawMoonSurface(spr, CX, CY, R);
  if (phase < 0.48f || phase > 0.52f)
    applyTerminator(spr, CX, CY, R, phase);

  // Thin bezel frame
  spr.drawSmoothCircle(CX, CY, 119, C565(22,22,28), TFT_BLACK);
  spr.drawSmoothCircle(CX, CY, 118, C565(46,44,52), TFT_BLACK);

  // Phase name (top, transparent outlined)
  spr.setTextDatum(MC_DATUM);
  drawMoonText(spr, moonPhaseName(phase), 120, 21, 1, C565(245,243,237));

  // Hand angles
  float hrA  = (h%12)*30.0f + m*0.5f;
  float minA = m*6.0f + s*0.1f;
  float secA = s*6.0f;

  // Hour hand — white, 3-layer (shadow / outline / face)
  drawHand(spr, CX+1, CY+2, hrA, 52,15, 5.0f,11.5f, C565(3, 5,12));
  drawHand(spr, CX,   CY,   hrA, 53,16, 5.5f,12.0f, C565(18,20,28));
  drawHand(spr, CX,   CY,   hrA, 52,15, 3.8f, 9.8f, TFT_WHITE);

  // Minute hand — white, 3-layer
  drawHand(spr, CX+1, CY+2, minA, 83,18, 3.2f, 7.5f, C565(3, 5,12));
  drawHand(spr, CX,   CY,   minA, 84,19, 3.6f, 8.0f, C565(18,20,28));
  drawHand(spr, CX,   CY,   minA, 83,18, 2.6f, 6.2f, TFT_WHITE);

  // Second hand — thin red needle
  float sr=secA*DEG_TO_RAD, sx=sinf(sr), sy=cosf(sr);
  spr.drawWideLine(CX-sx*18+1, CY+sy*18+1, CX+sx*86+1, CY-sy*86+1,
                   1.2f, C565(3,5,12), C565(120,118,112));
  spr.drawWideLine(CX-sx*18,   CY+sy*18,   CX+sx*86,   CY-sy*86,
                   1.6f, COL_DRED, C565(120,118,112));
  spr.fillSmoothCircle(roundf(CX-sx*16), roundf(CY+sy*16), 5, COL_DRED, C565(120,118,112));

  // Date window at 3 o'clock
  {
    int dx=CX+68, dy=CY;
    spr.fillRect(dx-14, dy-11, 28, 22, C565(28,30,38));
    spr.fillRect(dx-12, dy- 9, 24, 18, C565(232,230,224));
    char buf[4]; snprintf(buf, sizeof(buf), "%d", day);
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(C565(18,20,30), C565(232,230,224));
    spr.drawString(buf, dx, dy, 2);
  }

  // Centre cap
  spr.fillSmoothCircle(CX, CY, 10, C565(22,24,32), C565(120,118,112));
  spr.fillSmoothCircle(CX, CY,  8, COL_SILVER,      C565(22,24,32));
  spr.fillSmoothCircle(CX, CY,  5, COL_SILVER2,     COL_SILVER);
  spr.fillSmoothCircle(CX, CY,  2, TFT_WHITE,       COL_SILVER2);

  // Moon age (bottom, transparent outlined)
  char agebuf[18];
  snprintf(agebuf, sizeof(agebuf), "%.1f day moon", phase * 29.53f);
  drawMoonText(spr, agebuf, 120, 208, 1, C565(185,183,177));

}

// ═════════════════════════════════════════════════════════════════════════════
//  FACE 4 – MOON PHASE CLOCK  (Digital)
//
//  Same moon background + terminator.  All text uses drawMoonText() so it
//  floats transparently over the image with no background boxes.
// ═════════════════════════════════════════════════════════════════════════════
void drawClockFace3Digital(TFT_eSprite& spr,
                            int h, int m, int s,
                            int day, int year, int month,
                            uint8_t page) {
  const int CX=120, CY=120, R=118;

  spr.fillSprite(TFT_BLACK);
  float phase = calcMoonPhase(year, month, day);
  drawMoonSurface(spr, CX, CY, R);
  if (phase < 0.48f || phase > 0.52f)
    applyTerminator(spr, CX, CY, R, phase);

  spr.drawSmoothCircle(CX, CY, 119, C565(22,22,28), TFT_BLACK);
  spr.setTextDatum(MC_DATUM);

  // Phase name
  drawMoonText(spr, moonPhaseName(phase), 120, 28, 2, C565(240,238,232));

  // HH:MM:SS — 12-hour, Font 7 (7-segment 48px), centred
  int h12 = h % 12; if (h12 == 0) h12 = 12;
  char tbuf[9];
  snprintf(tbuf, sizeof(tbuf), "%02d:%02d:%02d", h12, m, s);
  drawMoonText(spr, tbuf, 120, 120, 7, TFT_WHITE);

  // Date DD Mon YYYY
  static const char* MONS[] = {
    "Jan","Feb","Mar","Apr","May","Jun",
    "Jul","Aug","Sep","Oct","Nov","Dec"
  };
  char dbuf[14];
  snprintf(dbuf, sizeof(dbuf), "%d %s %d",
           day, (month>=1&&month<=12) ? MONS[month-1] : "---", year);
  drawMoonText(spr, dbuf, 120, 178, 2, C565(220,218,212));

  // Moon age
  char abuf[18];
  snprintf(abuf, sizeof(abuf), "%.1f day moon", phase * 29.53f);
  drawMoonText(spr, abuf, 120, 208, 1, C565(185,183,177));

}
