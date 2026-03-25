#pragma once
#include <Wire.h>

// ── CST816S capacitive touch controller driver (polling mode) ─────────────────
// I2C address
#define CST816S_ADDR  0x15

// Gesture codes from register 0x01 (GestureID)
enum GestureID : uint8_t {
  GEST_NONE         = 0x00,
  GEST_SWIPE_UP     = 0x01,
  GEST_SWIPE_DOWN   = 0x02,
  GEST_SWIPE_LEFT   = 0x03,
  GEST_SWIPE_RIGHT  = 0x04,
  GEST_CLICK        = 0x05,
  GEST_DOUBLE_CLICK = 0x0B,
  GEST_LONG_PRESS   = 0x0C,
};

struct TouchData {
  bool      touched;
  GestureID gesture;
  uint16_t  x;
  uint16_t  y;
};

class CST816S {
public:
  // rst: GPIO connected to TP_RST (GPIO 13 on this board)
  // irq: GPIO connected to TP_INT (GPIO 5  on this board)
  CST816S(uint8_t rst = 13, uint8_t irq = 5) : _rst(rst), _irq(irq) {}

  void begin() {
    // Hardware reset sequence
    pinMode(_rst, OUTPUT);
    digitalWrite(_rst, LOW);
    delay(100);
    digitalWrite(_rst, HIGH);
    delay(100);

    // Disable auto-sleep so the chip stays responsive in polling mode.
    // Register 0xFE = REG_DIS_AUTO_SLEEP, write 0x01 to disable.
    _writeReg(0xFE, 0x01);

    // Log chip ID for debugging
    uint8_t chipId = _readReg(0xA7);
    Serial.printf("  CST816S chip ID: 0x%02X\n", chipId);
  }

  // Read current touch state by polling the I2C registers directly.
  // The INT pin is wired to PIN_TOUCH_INT but not used here; polling is
  // simpler and reliable enough at the ~50 Hz loop rate.
  // Call every loop() iteration; returns immediately with touched=false
  // when no finger is detected (FingerNum register == 0).
  TouchData read() {
    TouchData d = { false, GEST_NONE, 0, 0 };

    // Read 7 bytes starting at register 0x00 (REG_DATA)
    // Layout: [0]=data  [1]=GestureID  [2]=FingerNum
    //         [3]=XH    [4]=XL         [5]=YH  [6]=YL
    Wire.beginTransmission(CST816S_ADDR);
    Wire.write(0x00);
    if (Wire.endTransmission(false) != 0) return d;

    Wire.requestFrom((uint8_t)CST816S_ADDR, (uint8_t)7);
    if (Wire.available() < 7) return d;

    uint8_t buf[7];
    for (int i = 0; i < 7; i++) buf[i] = Wire.read();

    if (buf[2] == 0) return d;   // FingerNum == 0 → no touch

    d.touched = true;
    d.gesture = (GestureID)buf[1];
    d.x       = ((uint16_t)(buf[3] & 0x0F) << 8) | buf[4];
    d.y       = ((uint16_t)(buf[5] & 0x0F) << 8) | buf[6];
    return d;
  }

private:
  uint8_t _rst, _irq;

  void _writeReg(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(CST816S_ADDR);
    Wire.write(reg);
    Wire.write(val);
    Wire.endTransmission(true);
  }

  uint8_t _readReg(uint8_t reg) {
    Wire.beginTransmission(CST816S_ADDR);
    Wire.write(reg);
    Wire.endTransmission(false);
    Wire.requestFrom((uint8_t)CST816S_ADDR, (uint8_t)1);
    return Wire.available() ? Wire.read() : 0xFF;
  }
};
