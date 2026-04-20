#pragma once
#include <Arduino.h>

// ── Noise floors ─────────────────────────────────────────────
const float CURRENT_NOISE_FLOOR_MA = 0.5f;
const float VOLTAGE_NOISE_FLOOR_V  = 0.005f;

// ── Auto-range formatters ─────────────────────────────────────

String formatVoltage(float V) {
  if (V < 0) V = 0;
  if (V < VOLTAGE_NOISE_FLOOR_V) V = 0;
  if (V >= 10.0f)   return String(V, 2) + " V";
  if (V >=  1.0f)   return String(V, 3) + " V";
  float mV = V * 1000.0f;
  if (mV >= 100.0f) return String(mV, 1) + " mV";
  if (mV >=  10.0f) return String(mV, 2) + " mV";
  return              String(mV, 3) + " mV";
}

String formatCurrent(float mA_val) {
  if (fabsf(mA_val) < CURRENT_NOISE_FLOOR_MA) mA_val = 0;
  bool  neg    = (mA_val < 0);
  float abs_mA = fabsf(mA_val);
  String sign  = neg ? "-" : "";
  if (abs_mA >= 1000.0f) {
    float A = abs_mA / 1000.0f;
    if (A >= 10.0f) return sign + String(A, 2) + " A";
                    return sign + String(A, 3) + " A";
  }
  if (abs_mA >= 100.0f) return sign + String(abs_mA, 2) + " mA";
  if (abs_mA >=  10.0f) return sign + String(abs_mA, 3) + " mA";
  if (abs_mA >=   1.0f) return sign + String(abs_mA, 3) + " mA";
  float uA = abs_mA * 1000.0f;
  return sign + String(uA, 1) + " µA";
}

String formatPower(float mW) {
  if (mW < 0) mW = 0;
  if (mW >= 1000.0f) {
    float W = mW / 1000.0f;
    if (W >= 10.0f) return String(W, 2) + " W";
                    return String(W, 3) + " W";
  }
  if (mW >= 100.0f) return String(mW, 2) + " mW";
  if (mW >=  10.0f) return String(mW, 3) + " mW";
  return              String(mW, 3) + " mW";
}

// Auto-range: kWh / Wh / mWh
String formatEnergy(float Wh) {
  if (Wh >= 1000.0f) return String(Wh / 1000.0f, 3) + " kWh";
  if (Wh >=    1.0f) return String(Wh, 3) + " Wh";
  return               String(Wh * 1000.0f, 1) + " mWh";
}

// 12-hour label for a 0–23 hour, e.g. "12 AM", "1 PM", "11 PM"
String formatHour12(uint8_t h) {
  const char* period = (h >= 12) ? "PM" : "AM";
  uint8_t h12 = h % 12;
  if (h12 == 0) h12 = 12;
  return String(h12) + " " + period;
}
