#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <HX711_ADC.h>
#include <ArduinoJson.h>

// Sensor Functions headers
#include "ultrasonic_functions.h"
#include "lcd_functions.h"
#include "config.h"

// HARDWARE CONFIGURATION
#define MOTION_PIN1 12
#define MOTION_PIN2 13
#define ULTRASONIC_TRIG 25
#define ULTRASONIC_ECHO 33
#define SERVO_PIN 23

// HX711 PINS
#define HX711_SCK  2
#define HX711_DOUT 4
// Calibration with tape = 244.30

// [ WIFI CONFIGURATION ]
const char* ssid = SSID;
const char* password = PASSWORD;

// [ SUPABASE CONFIGURATION ]
// Define SUPABASE_URL and SUPABASE_KEY in config.h
const char* supabaseUrl = SUPABASE_URL;
const char* supabaseKey = SUPABASE_KEY;

// [ NETWORK TIME PROTOCOL CONFIGURATION ]
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 28800;
const long dayLightOffset_sec = 0;

// [ FEEDING SETTINGS ]
#define FEED_INTERVAL 4 // Starts at 08:00, 12:00, 16:00
#define FOOD_CAPACITY_HEIGHT 100.0 // (120mm - 20mm), 2mm for allowance
unsigned long catNearbyWindow = 3 * 1000UL; // 3 Seconds

// [ SUPABASE INTERVALS ]
#define WEIGHT_REPORT_INTERVAL   30000UL // 30 seconds
#define FOOD_REPORT_INTERVAL     20000UL // 20 seconds
#define FEED_CMD_POLL_INTERVAL    1000UL// 5 seconds
unsigned long lastWeightReport = 0;
unsigned long lastFoodReport = 0;
unsigned long lastFeedCmdPoll = 0;

// [ OBJECTS ]
LiquidCrystal_I2C LCD_SCREEN(0x27, 16, 2);
Servo feederServo;
HX711_ADC LoadCell(HX711_DOUT, HX711_SCK);

// [ GLOBAL STATES ]

// Mutual Exclusion, to prevent crashing due to multiple hardware running at the same time.
SemaphoreHandle_t i2cMutex = xSemaphoreCreateMutex();
bool isFeedingInProgress = false;

struct tm timeinfo; // Local time, ntp must be sync before using
bool canFeed = false;
int foodLevelPercentage = 0;
float currentWeightGrams = 0.0;

int lastFeed = 8;
int allowanceFeed = 1;
int feedAttempt = 3;

volatile bool catNearby = false;
volatile unsigned long lastSeen = 0;
unsigned long lastUpdateFood = 0;
long distance = 0.1723 * readUltrasonicDistance(ULTRASONIC_TRIG, ULTRASONIC_ECHO);

// [ DAILY LOG STATE ]
int dailyMotionCount = 0;       // Incremented on every motion trigger
bool dailyLogSent = false;      // Guard: only send once per midnight window
int lastLogDay = -1;            // Tracks which day the log was last sent

// [ FUNCTION DECLARATION ]
void executeFeeding(int portions = 1);
void updateFoodLevel();
void reportWeightToSupabase(float grams);
void reportFoodLevelToSupabase(int percentage);
void reportMovementToSupabase();
void checkFeedCommand();
void sendDailyLog();
String getDateString();
void addSupabaseHeaders(HTTPClient& http);
void postToSupabase(const String& endpoint, const String& body, const char* label);
void ARDUINO_ISR_ATTR motionFound();

void setup()
{
  Serial.begin(115200);

  // [SET PIN MODES]
  pinMode(ULTRASONIC_TRIG, OUTPUT);
  pinMode(ULTRASONIC_ECHO, INPUT);

  feederServo.attach(SERVO_PIN);

  attachInterrupt(MOTION_PIN1, motionFound, CHANGE);
  attachInterrupt(MOTION_PIN2, motionFound, CHANGE);

  // -- [SETUP LCD]
  LCD_SCREEN.init();
  LCD_SCREEN.backlight();
  setupLCD(LCD_SCREEN);
  delay(200);

  // -- [SETUP HX711] --
  LoadCell.begin();
  LoadCell.start(2000, true); // stabilizing time 2s, tare on startup
  if (LoadCell.getTareTimeoutFlag()) {
    Serial.println("[HX711] Timeout - check wiring and pin designations");
  } else {
    LoadCell.setCalFactor(244.30);
    Serial.println("[HX711] Startup complete");
  }

  // -- WIFI --
  WiFi.begin(ssid, password);
  Serial.println("[WiFi] Connecting...");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("[WiFi] Connected to WiFi Network: ");
  Serial.println(ssid);

  // Sync local time
  configTime(gmtOffset_sec, dayLightOffset_sec, ntpServer);
  
  updateFoodLevel();
}

void loop()
{
  // Return the program if unable to get time
  if (!getLocalTime(&timeinfo)) return;

  // Update HX711 continuously (non-blocking)
  if (LoadCell.update()) {
    currentWeightGrams = LoadCell.getData();
  }

  unsigned long now = millis();

  if (catNearby && (now - lastSeen >= catNearbyWindow)) {
    reportMovementToSupabase();
    catNearby = false;
  }

  // Report weight to Supabase every 30 seconds
  if (now - lastWeightReport >= WEIGHT_REPORT_INTERVAL) {
    lastWeightReport = now;
    reportWeightToSupabase(currentWeightGrams);
  }

  // Report food level to Supabase every 20 seconds
  if (now - lastFoodReport >= FOOD_REPORT_INTERVAL) {
    lastFoodReport = now;
    reportFoodLevelToSupabase(foodLevelPercentage);
  }

  // Poll Supabase for remote feed commands every 5 seconds
  if (now - lastFeedCmdPoll >= FEED_CMD_POLL_INTERVAL) {
    lastFeedCmdPoll = now;
    checkFeedCommand();
  }

  // Send daily log snapshot at midnight (00:00)
  if (timeinfo.tm_hour == 0 && timeinfo.tm_min == 0) {
    if (!dailyLogSent || lastLogDay != timeinfo.tm_mday) {
      sendDailyLog();
      dailyLogSent = true;
      lastLogDay = timeinfo.tm_mday;
      dailyMotionCount = 0; // Reset motion counter for the new day
    }
  } else {
    dailyLogSent = false; // Allow log to fire again next midnight
  }

  // 08:00 to 19:00
  if (8 <= timeinfo.tm_hour && timeinfo.tm_hour < 19) {
    if (!canFeed && timeinfo.tm_hour - lastFeed >= FEED_INTERVAL) {
      canFeed = true;
      feedAttempt--;
      allowanceFeed = (feedAttempt == 1) ? 3 : 1;
    }

    if (canFeed && timeinfo.tm_hour - lastFeed <= allowanceFeed) {
      if (catNearby) {
        lastFeed = timeinfo.tm_hour;
        executeFeeding();
        canFeed = false;
      }
    }
  } else if ((feedAttempt != 3 || allowanceFeed != 1) && timeinfo.tm_hour > 19) {
    feedAttempt = 3;
    allowanceFeed = 1;
  }


  if (now - lastUpdateFood >= 1000) {
    updateFoodLevel();

    if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
      lcdUpdateAll(LCD_SCREEN, catNearby, foodLevelPercentage);
      xSemaphoreGive(i2cMutex);
    }

    lastUpdateFood = millis();
  }
}

void executeFeeding(int portions) {
  if (isFeedingInProgress) return;
  isFeedingInProgress = true;

  Serial.printf("[FEEDING] feeding... portions: %d\n", portions);

  for (int i = 0; i < portions; i++) {
    feederServo.write(120);
    delay(1000);
    feederServo.write(0);
    if (i < portions - 1) delay(800); // brief pause between portions
  }

  if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    updateFoodLevel();
    lcdUpdateAll(LCD_SCREEN, catNearby, foodLevelPercentage);
    xSemaphoreGive(i2cMutex);
  } else {
    Serial.println("[ERROR] I2C Bus busy: could not update level");
  }

  // Settle briefly then report updated weight after feeding
  delay(1000);
  if (LoadCell.update()) {
    currentWeightGrams = LoadCell.getData();
  }
  reportWeightToSupabase(currentWeightGrams);

  // Log feeding event to Supabase
  String eventBody = "{\"portions\":" + String(portions) + "}";
  postToSupabase("/rest/v1/feeding_events", eventBody, "feeding_events POST");

  isFeedingInProgress = false;
  Serial.println("[FEEDER] Cycle complete");
}

void updateFoodLevel() {
  distance = 0.1723 * readUltrasonicDistance(ULTRASONIC_TRIG, ULTRASONIC_ECHO);
  distance = constrain(distance, 0, FOOD_CAPACITY_HEIGHT);
  foodLevelPercentage = ((FOOD_CAPACITY_HEIGHT - distance) * 100) / FOOD_CAPACITY_HEIGHT;
}

void addSupabaseHeaders(HTTPClient& http) {
  http.addHeader("apikey", supabaseKey);
  http.addHeader("Authorization", String("Bearer ") + supabaseKey);
  http.addHeader("Content-Type", "application/json");
}

// Generic POST helper for Supabase
void postToSupabase(const String& endpoint, const String& body, const char* label) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.printf("[Supabase] WiFi not connected, skipping %s\n", label);
    return;
  }
  HTTPClient http;
  http.begin(String(supabaseUrl) + endpoint);
  addSupabaseHeaders(http);
  int code = http.POST(body);
  if (code > 0) {
    Serial.printf("[Supabase] %s | HTTP %d\n", label, code);
  } else {
    Serial.printf("[Supabase] %s failed: %s\n", label, http.errorToString(code).c_str());
  }
  http.end();
}

void reportWeightToSupabase(float grams) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[Supabase] WiFi not connected, skipping weight report");
    return;
  }

  HTTPClient http;
  String url = String(supabaseUrl) + "/rest/v1/sensor_states?sensor=eq.weight";
  http.begin(url);
  addSupabaseHeaders(http);

  String body = "{\"weight_grams\":" + String(grams, 1) + "}";
  int httpCode = http.sendRequest("PATCH", body);

  if (httpCode > 0) {
    Serial.printf("[Supabase] Weight reported: %.1fg | HTTP %d\n", grams, httpCode);
  } else {
    Serial.printf("[Supabase] Weight report failed: %s\n", http.errorToString(httpCode).c_str());
  }

  http.end();
}

void reportFoodLevelToSupabase(int percentage) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[Supabase] WiFi not connected, skipping food level report");
    return;
  }

  HTTPClient http;
  String url = String(supabaseUrl) + "/rest/v1/sensor_states?sensor=eq.food_level";
  http.begin(url);
  addSupabaseHeaders(http);

  String body = "{\"distance_cm\":" + String(foodLevelPercentage) + "}";
  int httpCode = http.sendRequest("PATCH", body);

  if (httpCode > 0) {
    Serial.printf("[Supabase] Food level reported: %d%%| HTTP %d\n", foodLevelPercentage, httpCode);
  } else {
    Serial.printf("[Supabase] Food level report failed: %s\n", http.errorToString(httpCode).c_str());
  }

  http.end();
}

void reportMovementToSupabase() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[Supabase] WiFi not connected, skipping movement report");
    return;
  }

  HTTPClient http;
  String url = String(supabaseUrl) + "/rest/v1/sensor_states?sensor=eq.motion";
  http.begin(url);
  addSupabaseHeaders(http);

  struct tm currtime = timeinfo; // Local time, ntp must be sync before using
  currtime.tm_hour += 4;
  char buffer[40]; // Buffer to hold the formatted time string
  // Format the time as ISO 8601 with milliseconds (fixed at .000 for standard library) and Z for UTC
  strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S.000Z", &currtime);

  String body = "{\"last_motion_at\":\"" + String(buffer) + "\"}";
  int httpCode = http.sendRequest("PATCH", body);

  if (httpCode > 0) {
    Serial.printf("[Supabase] Movement reported: %s | HTTP %d\n", buffer, httpCode);
  } else {
    Serial.printf("[Supabase] Movement report failed: %s\n", http.errorToString(httpCode).c_str());
  }

  http.end();
}

// Returns current date as "YYYY-MM-DD" using NTP-synced timeinfo
String getDateString() {
  char buf[11];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02d",
    timeinfo.tm_year + 1900,
    timeinfo.tm_mon + 1,
    timeinfo.tm_mday);
  return String(buf);
}

void sendDailyLog() {
  String date = getDateString();

  // --- food_level snapshot ---
  String foodBody = "{\"sensor\":\"food_level\",\"log_date\":\"" + date +
                    "\",\"distance_cm\":" + String(foodLevelPercentage) + "}";
  postToSupabase("/rest/v1/sensor_daily_log", foodBody, "Daily log: food_level");

  // --- weight snapshot ---
  String weightBody = "{\"sensor\":\"weight\",\"log_date\":\"" + date +
                      "\",\"weight_grams\":" + String(currentWeightGrams, 1) + "}";
  postToSupabase("/rest/v1/sensor_daily_log", weightBody, "Daily log: weight");

  // --- motion count snapshot ---
  String motionBody = "{\"sensor\":\"motion\",\"log_date\":\"" + date +
                      "\",\"motion_count\":" + String(dailyMotionCount) + "}";
  postToSupabase("/rest/v1/sensor_daily_log", motionBody, "Daily log: motion");

  Serial.println("[Supabase] Daily logs sent");
}

void checkFeedCommand() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  String url = String(supabaseUrl) + "/rest/v1/feed_commands?order=created_at.asc&limit=1";
  http.begin(url);
  addSupabaseHeaders(http);

  int httpCode = http.GET();
  if (httpCode != 200) {
    http.end();
    return;
  }

  String json = http.getString();
  http.end();

  // Parse JSON array — expect [{"id": "...", "portions": 1}] or []
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err || !doc.is<JsonArray>() || doc.as<JsonArray>().size() == 0) return;

  JsonObject cmd = doc[0];
  String cmdId = cmd["id"].as<String>();
  int portions = cmd["portions"] | 1; // default 1 if missing

  Serial.printf("[FeedCmd] Remote command received — id: %s, portions: %d\n", cmdId.c_str(), portions);

  // Run the feeder
  executeFeeding(portions);

  // DELETE the command so it doesn't fire again
  HTTPClient delHttp;
  String delUrl = String(supabaseUrl) + "/rest/v1/feed_commands?id=eq." + cmdId;
  delHttp.begin(delUrl);
  addSupabaseHeaders(delHttp);
  delHttp.sendRequest("DELETE");
  delHttp.end();

  Serial.println("[FeedCmd] Command deleted");
}

void ARDUINO_ISR_ATTR motionFound() {
  catNearby = true;
  lastSeen = millis();
  dailyMotionCount++;
}