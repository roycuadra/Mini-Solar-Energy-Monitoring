/*
 * Solar Energy Monitoring System
 * ================================
 * Hardware : ESP8266 (NodeMCU), INA226, DS1302
 *
 * New in this version:
 *   Peak power/voltage/current tracked per hour (0-23)
 *   Automatic midnight reset of all 24-hour peaks
 *   12-hour AM/PM timestamp format throughout
 *   /peaks endpoint — 24-bar chart data + best-hour summary
 *   /resetpeaks endpoint — manual peak reset button
 *
 * File layout (same sketch folder = one Arduino project):
 *   solar_monitor.ino  <- this file
 *   formatters.h       <- auto-range format functions + formatHour12()
 *   webpage.h          <- full HTML/CSS/JS dashboard (PROGMEM)
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <Wire.h>
#include <INA226_WE.h>
#include <ThreeWire.h>
#include <RtcDS1302.h>
#include <EEPROM.h>

#include "formatters.h"
#include "webpage.h"

// -- Debug flag -----------------------------------------------
const bool DEBUG_MODE = true;
#define DLOG(fmt, ...) \
  do { if (DEBUG_MODE) Serial.printf("[DBG] " fmt "\n", ##__VA_ARGS__); } while (0)

// -- INA226 --------------------------------------------------
#define I2C_ADDRESS 0x40
#define SDA_PIN D2
#define SCL_PIN D1
INA226_WE ina226 = INA226_WE(I2C_ADDRESS);
bool inaOK = false;

// -- DS1302 --------------------------------------------------
#define RTC_CLK D5
#define RTC_DAT D6
#define RTC_RST D7
ThreeWire myWire(RTC_DAT, RTC_CLK, RTC_RST);
RtcDS1302<ThreeWire> Rtc(myWire);

// -- LED -----------------------------------------------------
#define STATUS_LED LED_BUILTIN   // active-LOW on NodeMCU

// -- AP ------------------------------------------------------
const char* AP_SSID = "Mini_Energy_Monitoring";
const char* AP_PASS = "cuadra12345";
DNSServer        dnsServer;
ESP8266WebServer server(80);

// ============================================================
//  SENSOR CONSTANTS
// ============================================================
const uint8_t  BUF_SIZE        = 8;
const uint8_t  HISTORY_SIZE    = 60;

const float CAL_FACTOR    = 1.0f;   // tune once vs multimeter
const float MIN_VOLTAGE    = 1.0f;
const float MAX_VOLTAGE    = 30.5f;
const float MAX_CURRENT_MA = 1300.0f;
const float EMA_ALPHA      = 0.12f;

const unsigned long SENSOR_INTERVAL    = 1100UL;
const unsigned long INA_RETRY_INTERVAL = 10000UL;
const unsigned long EEPROM_SAVE_PERIOD = 600000UL;
const unsigned long LED_SLOW           = 800UL;
const unsigned long LED_FAST           = 250UL;

const float WH_PER_MW_READING =
    (1.0f / 1000.0f) * (float(SENSOR_INTERVAL) / 1000.0f / 3600.0f);

// -- EEPROM layout -------------------------------------------
const int      EEPROM_SIZE  = 16;
const int      ADDR_MAGIC   = 0;
const int      ADDR_ENERGY  = 4;
const uint32_t EEPROM_MAGIC = 0xCAFEBABE;

// ============================================================
//  RUNTIME STATE
// ============================================================
float   voltageBuf[BUF_SIZE] = {0};
float   currentBuf[BUF_SIZE] = {0};
uint8_t bufIdx  = 0;
bool    bufFull = false;

float currentZeroOffset = 0.0f;
float filteredVoltage   = 0.0f;
float filteredCurrent   = 0.0f;
float loadVoltage       = 0.0f;
float power_mW          = 0.0f;

struct Reading { float voltage; float current_mA; float power_mW; };
Reading history[HISTORY_SIZE];
uint8_t histIdx   = 0;
uint8_t histCount = 0;

float energyWh = 0.0f;

// -- Hourly peak tracking ------------------------------------
// Index 0-23 = hour of day. Auto-reset at midnight.
struct HourPeak {
  float power_mW;
  float voltage_V;
  float current_mA;
};
HourPeak hourlyPeaks[24];
uint8_t  lastRtcDay = 255;   // 255 = uninitialized

volatile uint8_t clientCount = 0;

unsigned long lastSensorUpdate = 0;
unsigned long lastInaRetry     = 0;
unsigned long lastEepromSave   = 0;
unsigned long lastLedToggle    = 0;
bool          ledState         = false;
bool          systemReady      = false;

// ============================================================
//  HELPERS
// ============================================================
float bufAverage(float* buf) {
  uint8_t n = bufFull ? BUF_SIZE : bufIdx;
  if (n == 0) return 0.0f;
  float s = 0;
  for (uint8_t i = 0; i < n; i++) s += buf[i];
  return s / n;
}

void resetAllPeaks() {
  for (uint8_t h = 0; h < 24; h++) hourlyPeaks[h] = {0.0f, 0.0f, 0.0f};
  DLOG("Hourly peaks reset");
}

// 12-hour format: YYYY-MM-DD HH:MM:SS AM/PM
String getTimeString() {
  RtcDateTime now = Rtc.GetDateTime();
  if (!now.IsValid()) return "RTC Error";
  uint8_t h = now.Hour();
  const char* period = (h >= 12) ? "PM" : "AM";
  uint8_t h12 = h % 12;
  if (h12 == 0) h12 = 12;
  char buf[30];
  snprintf(buf, sizeof(buf), "%04u-%02u-%02u %02u:%02u:%02u %s",
           now.Year(), now.Month(), now.Day(),
           h12, now.Minute(), now.Second(), period);
  return String(buf);
}

uint8_t getCurrentHour() {
  RtcDateTime now = Rtc.GetDateTime();
  return now.IsValid() ? now.Hour() : 0;
}

uint8_t getCurrentDay() {
  RtcDateTime now = Rtc.GetDateTime();
  return now.IsValid() ? now.Day() : 0;
}

void configureINA226() {
  ina226.setConversionTime(INA226_CONV_TIME_8244);
  ina226.setAverage(INA226_AVERAGE_64);
  ina226.setMeasureMode(INA226_CONTINUOUS);
  ina226.setResistorRange(0.1f, 1.3f);
}

void loadEEPROM() {
  uint32_t magic;
  EEPROM.get(ADDR_MAGIC, magic);
  if (magic == EEPROM_MAGIC) {
    EEPROM.get(ADDR_ENERGY, energyWh);
    if (isnan(energyWh) || energyWh < 0.0f) energyWh = 0.0f;
    DLOG("EEPROM loaded: %.4f Wh", energyWh);
  } else {
    energyWh = 0.0f;
    DLOG("EEPROM fresh");
  }
}

void saveEEPROM() {
  EEPROM.put(ADDR_MAGIC,  EEPROM_MAGIC);
  EEPROM.put(ADDR_ENERGY, energyWh);
  EEPROM.commit();
  DLOG("EEPROM saved: %.4f Wh", energyWh);
}

void epochToDatetime(uint32_t epoch,
                     uint16_t& yr, uint8_t& mo, uint8_t& dy,
                     uint8_t& hr, uint8_t& mn, uint8_t& sc) {
  sc = epoch % 60; epoch /= 60;
  mn = epoch % 60; epoch /= 60;
  hr = epoch % 24; epoch /= 24;
  yr = 1970;
  while (true) {
    uint32_t diy = (yr%4==0 && (yr%100!=0 || yr%400==0)) ? 366 : 365;
    if (epoch < diy) break;
    epoch -= diy; yr++;
  }
  static const uint8_t dim[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  bool leap = (yr%4==0 && (yr%100!=0 || yr%400==0));
  mo = 1;
  for (uint8_t m = 0; m < 12; m++) {
    uint8_t d = dim[m] + (m==1 && leap ? 1 : 0);
    if (epoch < d) break;
    epoch -= d; mo++;
  }
  dy = uint8_t(epoch + 1);
}

// ============================================================
//  HTTP HANDLERS
// ============================================================
void handleRoot() {
  server.send_P(200, "text/html", WEBPAGE);
}

void handleData() {
  String vStr, cStr, pStr, eStr;
  if (!inaOK) {
    vStr = "\"Sensor Error\""; cStr = "\"Sensor Error\"";
    pStr = "\"Sensor Error\""; eStr = "\"--\"";
  } else {
    vStr = "\"" + formatVoltage(loadVoltage)     + "\"";
    cStr = "\"" + formatCurrent(filteredCurrent) + "\"";
    pStr = "\"" + formatPower(power_mW)          + "\"";
    eStr = "\"" + formatEnergy(energyWh)         + "\"";
  }
  String json = "{";
  json += "\"v\":"  + vStr + ",";
  json += "\"c\":"  + cStr + ",";
  json += "\"p\":"  + pStr + ",";
  json += "\"e\":"  + eStr + ",";
  json += "\"t\":\"" + getTimeString() + "\",";
  json += "\"s\":\"" + String(clientCount > 0 ? "CLIENT CONNECTED" : "IDLE") + "\"";
  json += "}";
  server.sendHeader("Cache-Control", "no-cache, no-store");
  server.send(200, "application/json", json);
}

void handleHistory() {
  auto appendArray = [&](String& json, const char* key, float Reading::* field) {
    json += "\""; json += key; json += "\":[";
    for (uint8_t i = 0; i < histCount; i++) {
      uint8_t idx = (histCount < HISTORY_SIZE) ? i : (histIdx + i) % HISTORY_SIZE;
      if (i > 0) json += ",";
      json += String(history[idx].*field, 2);
    }
    json += "]";
  };
  String json = "{";
  json += "\"count\":" + String(histCount) + ",";
  appendArray(json, "v", &Reading::voltage);    json += ",";
  appendArray(json, "c", &Reading::current_mA); json += ",";
  appendArray(json, "p", &Reading::power_mW);
  json += "}";
  server.sendHeader("Cache-Control", "no-cache, no-store");
  server.send(200, "application/json", json);
}

// /peaks — 24-hour bar chart data + best-hour summary
void handlePeaks() {
  uint8_t bestHour = 0;
  float   bestPow  = 0.0f;
  for (uint8_t h = 0; h < 24; h++) {
    if (hourlyPeaks[h].power_mW > bestPow) {
      bestPow  = hourlyPeaks[h].power_mW;
      bestHour = h;
    }
  }
  uint8_t currentHour = getCurrentHour();

  String json = "{";
  json += "\"current_hour\":" + String(currentHour) + ",";

  json += "\"power\":[";
  for (uint8_t h = 0; h < 24; h++) {
    if (h > 0) json += ",";
    json += String(hourlyPeaks[h].power_mW, 2);
  }
  json += "],\"voltage\":[";
  for (uint8_t h = 0; h < 24; h++) {
    if (h > 0) json += ",";
    json += String(hourlyPeaks[h].voltage_V, 3);
  }
  json += "],\"current\":[";
  for (uint8_t h = 0; h < 24; h++) {
    if (h > 0) json += ",";
    json += String(hourlyPeaks[h].current_mA, 2);
  }
  json += "],";

  json += "\"best_power\":\""       + (bestPow > 0 ? formatPower(bestPow)                           : "--") + "\",";
  json += "\"best_voltage\":\""     + (bestPow > 0 ? formatVoltage(hourlyPeaks[bestHour].voltage_V) : "--") + "\",";
  json += "\"best_current\":\""     + (bestPow > 0 ? formatCurrent(hourlyPeaks[bestHour].current_mA): "--") + "\",";
  json += "\"best_hour_label\":\"" + (bestPow > 0 ? formatHour12(bestHour)                          : "--") + "\"";
  json += "}";

  server.sendHeader("Cache-Control", "no-cache, no-store");
  server.send(200, "application/json", json);
}

void handleZeroCal() {
  if (!inaOK) {
    server.send(503, "application/json", "{\"error\":\"INA226 not ready\"}");
    return;
  }
  ina226.readAndClearFlags();
  currentZeroOffset = ina226.getCurrent_mA();
  DLOG("Zero-cal: offset = %.3f mA", currentZeroOffset);
  server.send(200, "application/json",
              "{\"status\":\"ok\",\"offset\":" + String(currentZeroOffset, 3) + "}");
}

void handleSetTime() {
  if (!server.hasArg("epoch")) {
    server.send(400, "application/json", "{\"error\":\"missing epoch param\"}");
    return;
  }
  uint32_t epoch = (uint32_t)server.arg("epoch").toInt();
  uint16_t yr; uint8_t mo, dy, hr, mn, sc;
  epochToDatetime(epoch, yr, mo, dy, hr, mn, sc);
  Rtc.SetDateTime(RtcDateTime(yr, mo, dy, hr, mn, sc));
  lastRtcDay = dy;  // prevent spurious midnight reset
  DLOG("RTC set to %04u-%02u-%02u %02u:%02u:%02u", yr, mo, dy, hr, mn, sc);
  server.send(200, "application/json",
              "{\"status\":\"ok\",\"time\":\"" + getTimeString() + "\"}");
}

void handleResetEnergy() {
  energyWh = 0.0f;
  saveEEPROM();
  server.send(200, "application/json", "{\"status\":\"ok\"}");
}

void handleResetPeaks() {
  resetAllPeaks();
  server.send(200, "application/json", "{\"status\":\"ok\"}");
}

// ============================================================
//  WI-FI EVENTS
// ============================================================
void onStationConnected(const WiFiEventSoftAPModeStationConnected&)
    { clientCount++; }
void onStationDisconnected(const WiFiEventSoftAPModeStationDisconnected&)
    { if (clientCount > 0) clientCount--; }

// ============================================================
//  SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(100);

  pinMode(STATUS_LED, OUTPUT);
  digitalWrite(STATUS_LED, HIGH);

  EEPROM.begin(EEPROM_SIZE);
  loadEEPROM();

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);
  inaOK = ina226.init();
  if (inaOK) { configureINA226(); Serial.println("[OK] INA226 ready"); }
  else        { Serial.println("[WARN] INA226 not found — retrying every 10 s"); }

  Rtc.Begin();
  if (!Rtc.IsDateTimeValid()) {
    Serial.println("[WARN] RTC invalid — using compile time");
    Rtc.SetDateTime(RtcDateTime(__DATE__, __TIME__));
  }
  if (!Rtc.GetIsRunning()) Rtc.SetIsRunning(true);
  Serial.print("[OK] RTC: "); Serial.println(getTimeString());

  lastRtcDay = getCurrentDay();
  resetAllPeaks();

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  IPAddress ip = WiFi.softAPIP();
  Serial.print("[OK] AP IP: "); Serial.println(ip);

  dnsServer.start(53, "*", ip);

  server.on("/",            handleRoot);
  server.on("/data",        handleData);
  server.on("/history",     handleHistory);
  server.on("/peaks",       handlePeaks);
  server.on("/zerocal",     handleZeroCal);
  server.on("/settime",     handleSetTime);
  server.on("/resetenergy", handleResetEnergy);
  server.on("/resetpeaks",  handleResetPeaks);
  server.onNotFound(handleRoot);
  server.begin();

  WiFi.onSoftAPModeStationConnected(onStationConnected);
  WiFi.onSoftAPModeStationDisconnected(onStationDisconnected);

  Serial.println("[OK] Server ready");
  systemReady = true;
}

// ============================================================
//  LOOP
// ============================================================
void loop() {
  ESP.wdtFeed();
  dnsServer.processNextRequest();
  server.handleClient();

  unsigned long now = millis();

  // -- INA226 auto-recovery ----------------------------------
  if (!inaOK && (now - lastInaRetry >= INA_RETRY_INTERVAL)) {
    lastInaRetry = now;
    inaOK = ina226.init();
    if (inaOK) { configureINA226(); Serial.println("[OK] INA226 recovered"); }
  }

  // -- Midnight reset ----------------------------------------
  // Compares RTC day number each loop. When it changes -> new day.
  {
    uint8_t today = getCurrentDay();
    if (lastRtcDay != 255 && today != lastRtcDay) {
      Serial.println("[INFO] Midnight — resetting hourly peaks");
      resetAllPeaks();
    }
    lastRtcDay = today;
  }

  // -- Sensor read -------------------------------------------
  if (inaOK && (now - lastSensorUpdate >= SENSOR_INTERVAL)) {
    lastSensorUpdate = now;

    ina226.readAndClearFlags();
    float rawV = ina226.getBusVoltage_V();
    float rawI = ina226.getCurrent_mA() - currentZeroOffset;

    if (rawV >= MIN_VOLTAGE && rawV <= MAX_VOLTAGE &&
        fabsf(rawI) <= MAX_CURRENT_MA) {

      // 1. EMA
      filteredVoltage = EMA_ALPHA * rawV + (1.0f - EMA_ALPHA) * filteredVoltage;
      filteredCurrent = EMA_ALPHA * rawI + (1.0f - EMA_ALPHA) * filteredCurrent;

      // 2. Circular buffer
      voltageBuf[bufIdx] = filteredVoltage;
      currentBuf[bufIdx] = filteredCurrent;
      bufIdx = (bufIdx + 1) % BUF_SIZE;
      if (bufIdx == 0) bufFull = true;

      float avgV = bufAverage(voltageBuf);
      float avgI = bufAverage(currentBuf);

      // 3. Calibrate
      loadVoltage     = avgV * CAL_FACTOR;
      power_mW        = loadVoltage * avgI;
      filteredCurrent = avgI;

      // 4. Energy
      if (power_mW > 0.0f) energyWh += power_mW * WH_PER_MW_READING;

      // 5. Hourly peak — update slot for current RTC hour
      {
        uint8_t h = getCurrentHour();
        if (power_mW > hourlyPeaks[h].power_mW) {
          hourlyPeaks[h] = { power_mW, loadVoltage, avgI };
          DLOG("Peak @ %s: P=%s V=%s I=%s",
               formatHour12(h).c_str(),
               formatPower(power_mW).c_str(),
               formatVoltage(loadVoltage).c_str(),
               formatCurrent(avgI).c_str());
        }
      }

      // 6. Trend history
      history[histIdx] = { loadVoltage, avgI, power_mW };
      histIdx = (histIdx + 1) % HISTORY_SIZE;
      if (histCount < HISTORY_SIZE) histCount++;

      DLOG("V=%-10s I=%-12s P=%-10s E=%s",
           formatVoltage(loadVoltage).c_str(),
           formatCurrent(filteredCurrent).c_str(),
           formatPower(power_mW).c_str(),
           formatEnergy(energyWh).c_str());
    }
  }

  // -- EEPROM periodic save ----------------------------------
  if (now - lastEepromSave >= EEPROM_SAVE_PERIOD) {
    lastEepromSave = now;
    saveEEPROM();
  }

  // -- LED heartbeat -----------------------------------------
  //   Slow 800 ms = running, no client
  //   Fast 250 ms = client connected
  if (systemReady) {
    unsigned long interval = (clientCount > 0) ? LED_FAST : LED_SLOW;
    if (now - lastLedToggle >= interval) {
      lastLedToggle = now;
      ledState = !ledState;
      digitalWrite(STATUS_LED, ledState ? LOW : HIGH);
    }
  }
}
