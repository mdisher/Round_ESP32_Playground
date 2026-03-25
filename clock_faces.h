#pragma once
#include "config.h"
#include <TFT_eSPI.h>
#include <math.h>

// ── RGB → RGB565 ──────────────────────────────────────────────────────────────
#define C565(r,g,b) ((uint16_t)(((uint16_t)((r)&0xF8)<<8)|((uint16_t)((g)&0xFC)<<3)|((b)>>3)))

// ── Shared palette ────────────────────────────────────────────────────────────
static const uint16_t COL_LUME_LO  = C565( 50, 90, 30);   // lume glow halo
static const uint16_t COL_LUME     = C565(195,240,155);   // main lume
static const uint16_t COL_LUME_HI  = C565(228,255,205);   // bright lume centre
static const uint16_t COL_SILVER   = C565(192,192,200);
static const uint16_t COL_SILVER2  = C565(222,222,230);
static const uint16_t COL_DGRAY    = C565( 38, 38, 46);
static const uint16_t COL_MGRAY    = C565( 98, 98,108);
static const uint16_t COL_LGRAY    = C565(158,158,168);
static const uint16_t COL_GOLD     = C565(208,172, 66);
static const uint16_t COL_GOLD2    = C565(232,198, 92);
static const uint16_t COL_DRED     = C565(208, 18, 18);
static const uint16_t COL_ORANGE   = C565(238, 76,  0);
static const uint16_t FACE1_BG     = C565(  5,  7, 18);   // near-black navy

// ── Page-dot indicator (defined but not currently used) ───────────────────────
inline void drawPageDots(TFT_eSprite& spr, uint8_t cur, uint8_t total,
                          uint16_t bgColor) {
  const int SP=14, Y=218, R=4;
  int x0 = 120 - ((total-1)*SP)/2;
  for (uint8_t i = 0; i < total; i++) {
    int x = x0 + i*SP;
    if (i==cur) spr.fillSmoothCircle(x,Y,R, TFT_WHITE, bgColor);
    else        spr.drawSmoothCircle(x,Y,R, COL_MGRAY,  bgColor);
  }
}

// ── Tapered quadrilateral hand ────────────────────────────────────────────────
//  cx,cy     — pivot point (float for sub-pixel accuracy)
//  angleDeg  — angle in degrees, 0=12 o'clock, clockwise
//  fLen      — length forward from pivot (tip side)
//  bLen      — length back from pivot (tail/counterweight side)
//  fW        — half-width at the tip
//  bW        — half-width at the tail
//  col       — fill colour
static void drawHand(TFT_eSprite& spr,
                     float cx, float cy, float angleDeg,
                     float fLen, float bLen, float fW, float bW,
                     uint16_t col) {
  float r=angleDeg*DEG_TO_RAD, hx=sinf(r), hy=-cosf(r), px=cosf(r), py=sinf(r);
  int16_t x0=roundf(cx-hx*bLen-px*bW), y0=roundf(cy-hy*bLen-py*bW);
  int16_t x1=roundf(cx-hx*bLen+px*bW), y1=roundf(cy-hy*bLen+py*bW);
  int16_t x2=roundf(cx+hx*fLen+px*fW), y2=roundf(cy+hy*fLen+py*fW);
  int16_t x3=roundf(cx+hx*fLen-px*fW), y3=roundf(cy+hy*fLen-py*fW);
  spr.fillTriangle(x0,y0,x1,y1,x2,y2,col);
  spr.fillTriangle(x0,y0,x2,y2,x3,y3,col);
}

// ── Dauphine / leaf hand ──────────────────────────────────────────────────────
//  cx,cy     — pivot point
//  angleDeg  — 0=12 o'clock, clockwise
//  fLen      — tip length from pivot
//  bLen      — tail length from pivot
//  maxW      — maximum half-width (at ~42% of fLen from pivot)
//  col       — fill colour
static void drawDauphineHand(TFT_eSprite& spr,
                              float cx, float cy, float angleDeg,
                              float fLen, float bLen, float maxW,
                              uint16_t col) {
  float r=angleDeg*DEG_TO_RAD, hx=sinf(r), hy=-cosf(r), px=cosf(r), py=sinf(r);
  float wPos=fLen*0.42f, bHW=maxW*0.20f;
  int16_t tx =roundf(cx+hx*fLen),         ty =roundf(cy+hy*fLen);
  int16_t wrx=roundf(cx+hx*wPos+px*maxW), wry=roundf(cy+hy*wPos+py*maxW);
  int16_t wlx=roundf(cx+hx*wPos-px*maxW), wly=roundf(cy+hy*wPos-py*maxW);
  int16_t brx=roundf(cx-hx*bLen+px*bHW),  bry=roundf(cy-hy*bLen+py*bHW);
  int16_t blx=roundf(cx-hx*bLen-px*bHW),  bly=roundf(cy-hy*bLen-py*bHW);
  spr.fillTriangle(tx,ty, wrx,wry, brx,bry, col);
  spr.fillTriangle(tx,ty, brx,bry, blx,bly, col);
  spr.fillTriangle(tx,ty, blx,bly, wlx,wly, col);
}

// ── Date window ───────────────────────────────────────────────────────────────
static void drawDateWindow(TFT_eSprite& spr, int x, int y, int day,
                            uint16_t border, uint16_t bg, uint16_t fg) {
  spr.fillRect(x-14, y-11, 28, 22, border);
  spr.fillRect(x-12, y- 9, 24, 18, bg);
  char buf[4]; snprintf(buf,sizeof(buf),"%d",day);
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(fg, bg);
  spr.drawString(buf, x, y, 2);
}

// ═════════════════════════════════════════════════════════════════════════════
//  FACE 1 – PHANTOM DIVER
//
//  Near-black navy dial · Two-piece metallic bezel with 60 pip track
//  Lume triangle at 12, wide bars at 3/6/9, round dots elsewhere
//  5-layer silver/lume hands · Red second with counterweight · Date @ 3
// ═════════════════════════════════════════════════════════════════════════════
void drawClockFace1(TFT_eSprite& spr, int h, int m, int s, int day, uint8_t page) {
  const float    CX = 120, CY = 120;
  const uint16_t BG    = FACE1_BG;
  const uint16_t BEZEL = C565(24, 26, 35);   // bezel track

  spr.fillSprite(TFT_BLACK);

  // ── Bezel: dark track R=111-119 ───────────────────────────────────────────
  spr.fillSmoothCircle(CX,CY, 119, BEZEL, TFT_BLACK);
  spr.fillSmoothCircle(CX,CY, 111, BG,    BEZEL);
  spr.drawSmoothCircle(CX,CY, 119, COL_SILVER2, TFT_BLACK);  // outer rim
  spr.drawSmoothCircle(CX,CY, 111, COL_SILVER,  BG);         // inner rim

  // ── Bezel pip marks ───────────────────────────────────────────────────────
  for (int i = 0; i < 60; i++) {
    float a = i*6.0f*DEG_TO_RAD, sa = sinf(a), ca = cosf(a);
    bool  h5 = (i%5 == 0);
    spr.drawWideLine(
      CX + sa*(h5 ? 112.f : 116.f), CY - ca*(h5 ? 112.f : 116.f),
      CX + sa*118.f,                 CY - ca*118.f,
      h5 ? 3.0f : 1.1f,
      h5 ? TFT_WHITE : COL_MGRAY,
      BEZEL);
  }

  // ── Hour markers ─────────────────────────────────────────────────────────
  for (int i = 0; i < 12; i++) {
    float a = i*30.0f*DEG_TO_RAD, sa = sinf(a), ca = cosf(a);
    if (i == 0) {
      // 12 o'clock: inverted triangle (tip toward 12, base inward)
      spr.fillTriangle(120, 22, 110, 47, 130, 47, COL_LUME_LO);  // glow
      spr.fillTriangle(120, 24, 113, 45, 127, 45, COL_LUME);      // body
    } else if (i%3 == 0) {
      // 3, 6, 9: wide lume bars (R=76 to R=96)
      float x1=CX+sa*76, y1=CY-ca*76, x2=CX+sa*96, y2=CY-ca*96;
      spr.drawWideLine(x1,y1,x2,y2, 15.f, COL_LUME_LO, BG);
      spr.drawWideLine(x1,y1,x2,y2, 10.f, COL_LUME,    BG);
      spr.drawWideLine(x1,y1,x2,y2,  4.f, COL_LUME_HI, COL_LUME);
    } else {
      // Others: round lume dots at R=83
      int mx=roundf(CX+sa*83), my=roundf(CY-ca*83);
      spr.fillSmoothCircle(mx,my, 8, COL_LUME_LO, BG);
      spr.fillSmoothCircle(mx,my, 6, COL_LUME,    COL_LUME_LO);
      spr.fillSmoothCircle(mx,my, 2, COL_LUME_HI, COL_LUME);
    }
  }

  // ── Angles ────────────────────────────────────────────────────────────────
  float secA=s*6.0f, minA=m*6.0f+s*0.1f, hrA=(h%12)*30.0f+m*0.5f;

  // ── Date window at 3 o'clock (clear of 3-o'clock bar at R=76) ─────────────
  drawDateWindow(spr, roundf(CX+60), roundf(CY), day, COL_SILVER, TFT_WHITE, TFT_BLACK);

  // ── Minute hand (5 layers) ────────────────────────────────────────────────
  drawHand(spr, CX+1,CY+2, minA, 87,20, 3.2f, 7.0f, C565(2, 3, 8));
  drawHand(spr, CX,  CY,   minA, 87,20, 3.6f, 7.5f, COL_DGRAY);
  drawHand(spr, CX,  CY,   minA, 86,19, 2.6f, 6.0f, COL_SILVER2);
  drawHand(spr, CX,  CY,   minA, 80,13, 1.0f, 2.8f, COL_LUME);
  drawHand(spr, CX,  CY,   minA, 74, 8, 0.3f, 0.9f, COL_LUME_HI);

  // ── Hour hand (5 layers) ─────────────────────────────────────────────────
  drawHand(spr, CX+1,CY+2, hrA, 56,16, 5.0f,11.5f, C565(2, 3, 8));
  drawHand(spr, CX,  CY,   hrA, 56,16, 5.5f,12.0f, COL_DGRAY);
  drawHand(spr, CX,  CY,   hrA, 55,15, 4.2f,10.0f, COL_SILVER2);
  drawHand(spr, CX,  CY,   hrA, 48,10, 1.3f, 4.0f, COL_LUME);
  drawHand(spr, CX,  CY,   hrA, 42, 6, 0.4f, 1.4f, COL_LUME_HI);

  // ── Second hand: red needle + round counterweight ─────────────────────────
  float sr=secA*DEG_TO_RAD, sx=sinf(sr), sy=cosf(sr);
  spr.drawWideLine(CX-sx*24+1,CY+sy*24+2, CX+sx*94+1,CY-sy*94+2, 1.5f, C565(2,3,8), BG);
  spr.drawWideLine(CX-sx*24,  CY+sy*24,   CX+sx*94,  CY-sy*94,   2.2f, COL_DRED,    BG);
  spr.drawWideLine(CX-sx*5,   CY+sy*5,    CX+sx*88,  CY-sy*88,   0.7f, COL_ORANGE,  COL_DRED);
  spr.fillSmoothCircle(roundf(CX-sx*22), roundf(CY+sy*22), 9, COL_DRED,   BG);
  spr.fillSmoothCircle(roundf(CX-sx*22), roundf(CY+sy*22), 4, COL_ORANGE, COL_DRED);

  // ── Centre jewel (5 rings) ────────────────────────────────────────────────
  spr.fillSmoothCircle(CX,CY, 11, COL_SILVER,  BG);
  spr.fillSmoothCircle(CX,CY,  9, COL_DGRAY,   COL_SILVER);
  spr.fillSmoothCircle(CX,CY,  6, COL_SILVER2, COL_DGRAY);
  spr.fillSmoothCircle(CX,CY,  3, TFT_WHITE,   COL_SILVER2);
  spr.fillSmoothCircle(CX,CY,  1, COL_LGRAY,   TFT_WHITE);

}

// ═════════════════════════════════════════════════════════════════════════════
//  FACE 2 – OCEAN
//
//  Deep blue radial gradient · Gold border + chapter ring
//  White baton markers (12 longest, cardinals medium, others short)
//  Slim white hands · Gold second hand + centre jewel · Date @ 3
// ═════════════════════════════════════════════════════════════════════════════
void drawClockFace2(TFT_eSprite& spr, int h, int m, int s, int day, uint8_t page) {
  const float CX=120, CY=120;

  // Gradient steps: dark navy at rim → steel blue at centre
  const uint16_t G0 = C565(  5, 10, 42);
  const uint16_t G1 = C565(  8, 17, 58);
  const uint16_t G2 = C565( 12, 25, 78);
  const uint16_t G3 = C565( 16, 34, 98);
  const uint16_t G4 = C565( 20, 44,118);
  const uint16_t GC = C565( 24, 54,138);

  spr.fillSprite(TFT_BLACK);
  spr.fillSmoothCircle(CX,CY, 119, G0, TFT_BLACK);
  spr.fillSmoothCircle(CX,CY, 100, G1, G0);
  spr.fillSmoothCircle(CX,CY,  80, G2, G1);
  spr.fillSmoothCircle(CX,CY,  60, G3, G2);
  spr.fillSmoothCircle(CX,CY,  38, G4, G3);
  spr.fillSmoothCircle(CX,CY,  18, GC, G4);

  // ── Gold outer border (3 rings) ───────────────────────────────────────────
  spr.drawSmoothCircle(CX,CY, 119, C565(55, 42, 18), TFT_BLACK);
  spr.drawSmoothCircle(CX,CY, 117, COL_GOLD2,         G0);
  spr.drawSmoothCircle(CX,CY, 115, COL_GOLD,           G0);
  spr.drawSmoothCircle(CX,CY, 113, C565(90, 72, 28),   G0);

  // ── Chapter ring ─────────────────────────────────────────────────────────
  spr.drawSmoothCircle(CX,CY, 108, C565(100,115,165), G0);

  // ── Minute pips (non-hour positions) ──────────────────────────────────────
  for (int i = 0; i < 60; i++) {
    if (i%5 == 0) continue;
    float a=i*6.0f*DEG_TO_RAD, sa=sinf(a), ca=cosf(a);
    spr.drawWideLine(CX+sa*104, CY-ca*104, CX+sa*108, CY-ca*108,
                     1.0f, C565(160,170,210), G0);
  }

  // ── Hour baton markers ────────────────────────────────────────────────────
  // 12 is longest/widest; 3,6,9 medium; others short
  for (int i = 0; i < 12; i++) {
    float a=i*30.0f*DEG_TO_RAD, sa=sinf(a), ca=cosf(a);
    float ri  = (i==0) ? 81.f : (i%3==0 ? 86.f : 91.f);
    float w   = (i==0) ? 6.0f : (i%3==0 ? 5.0f : 3.5f);
    uint16_t mc = (i==0 || i%3==0) ? TFT_WHITE : C565(210,218,242);
    // Drop shadow
    spr.drawWideLine(CX+sa*ri+0.8f, CY-ca*ri+0.8f, CX+sa*105.8f, CY-ca*105.8f,
                     w+0.6f, C565(2,6,28), G0);
    // Marker
    spr.drawWideLine(CX+sa*ri, CY-ca*ri, CX+sa*106, CY-ca*106, w, mc, G0);
  }

  // ── Angles ────────────────────────────────────────────────────────────────
  float secA=s*6.0f, minA=m*6.0f+s*0.1f, hrA=(h%12)*30.0f+m*0.5f;

  // ── Date window at 3 o'clock (clear of 3-o'clock bar at R=86) ─────────────
  drawDateWindow(spr, roundf(CX+67), roundf(CY), day, COL_GOLD, C565(230,236,255), G0);

  // ── Minute hand (4 layers, slim white) ───────────────────────────────────
  drawHand(spr, CX+0.8f,CY+1.5f, minA, 85,18, 2.5f, 6.5f, C565(2,6,28));
  drawHand(spr, CX,     CY,      minA, 86,19, 2.9f, 7.0f, C565(40,56,104));
  drawHand(spr, CX,     CY,      minA, 85,18, 1.9f, 5.2f, TFT_WHITE);
  drawHand(spr, CX,     CY,      minA, 79,12, 0.5f, 1.8f, C565(200,210,242));

  // ── Hour hand (4 layers, slim white) ─────────────────────────────────────
  drawHand(spr, CX+0.8f,CY+1.5f, hrA, 53,14, 4.0f,10.0f, C565(2,6,28));
  drawHand(spr, CX,     CY,      hrA, 54,15, 4.5f,10.5f, C565(40,56,104));
  drawHand(spr, CX,     CY,      hrA, 53,14, 3.2f, 8.5f, TFT_WHITE);
  drawHand(spr, CX,     CY,      hrA, 47, 9, 1.0f, 3.0f, C565(200,210,242));

  // ── Second hand (gold needle + counterweight) ─────────────────────────────
  float sr=secA*DEG_TO_RAD, sx=sinf(sr), sy=cosf(sr);
  spr.drawWideLine(CX-sx*20+0.8f,CY+sy*20+1.5f, CX+sx*91+0.8f,CY-sy*91+1.5f,
                   1.5f, C565(2,6,28), GC);
  spr.drawWideLine(CX-sx*20, CY+sy*20, CX+sx*91, CY-sy*91, 1.8f, COL_GOLD,  GC);
  spr.drawWideLine(CX-sx*4,  CY+sy*4,  CX+sx*85, CY-sy*85, 0.5f, COL_GOLD2, COL_GOLD);
  spr.fillSmoothCircle(roundf(CX-sx*18), roundf(CY+sy*18), 5, COL_GOLD,  GC);
  spr.fillSmoothCircle(roundf(CX-sx*18), roundf(CY+sy*18), 2, COL_GOLD2, COL_GOLD);

  // ── Centre cap (gold jewel) ───────────────────────────────────────────────
  spr.fillSmoothCircle(CX,CY, 10, C565(80, 62, 22), GC);
  spr.fillSmoothCircle(CX,CY,  8, COL_GOLD,          C565(80,62,22));
  spr.fillSmoothCircle(CX,CY,  5, COL_GOLD2,         COL_GOLD);
  spr.fillSmoothCircle(CX,CY,  3, GC,                COL_GOLD2);
  spr.fillSmoothCircle(CX,CY,  1, COL_GOLD2,         GC);

}
