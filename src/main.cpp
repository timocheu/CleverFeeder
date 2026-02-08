#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>
#include <WiFi.h>

// Sensor Functions headers
#include "motion_functions.h"
#include "ultrasonic_functions.h"
#include "lcd_functions.h"
#include "servo_functions.h"
#include "config.h"

// HARDWARE CONIGURATION
#define MOTION_PIN 8
#define ULTRASONIC_TRIG 2
#define ULTRASONIC_ECHO 4
#define SERVO_PIN 12

// [ WIFI CONFIGURATION ]
const char* ssid = SSID;
const char* password = PASSWORD;

// [ NETWORK TIME PROTOCOL CONFIGURATION ]
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 28800;
const long dayLightOffset_sec = 0;

// [ FEEDING SETTINGS ]
#define FEED_INTERVAL 4 // Starts at 08:00, 12:00, 16:00
#define FOOD_CAPACITY_HEIGHT 10 // (12cm - 2cm), 2cm for allowance, 
unsigned long catNearbyWindow = 3 * 1000UL; // 3 Seconds

// [ OBJECTS ]
LiquidCrystal_I2C LCD_SCREEN(0x27, 16, 2);
Servo feederServo;

// [ GLOBAL STATES ]
struct tm timeinfo; // Local time, ntp must be sync before using
bool canFeed = false;
int foodLevelPercentage = 0; 

int lastFeed = 8; // To prevent double feeding, if time is close for the next feeding.
int allowanceFeed = 1; // hour allowance for the cat to get food
int feedAttempt = 3;

volatile bool catNearby = false; // Motion variables
volatile unsigned long lastSeen = 0;

void updateFoodLevel() {
  long distance = 0.01723 * readUltrasonicDistance(ULTRASONIC_TRIG, ULTRASONIC_ECHO);

  // Limit the max value distance, to avoid negative percentage
  distance = constrain(distance, 0, FOOD_CAPACITY_HEIGHT);

  foodLevelPercentage = ((FOOD_CAPACITY_HEIGHT - distance) * 100) / FOOD_CAPACITY_HEIGHT;
}

// Run in internal ram for speed, instead of flash
void ARDUINO_ISR_ATTR motionChange() {
  catNearby = true;
  lastSeen = millis();
}

void setup()
{
  // BAUD = 115200 for ESP32
  // BAUD = 9200 for Arduino Uno R3
  Serial.begin(115200);

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
  // Stop the program if unable to get time
  if (getLocalTime(&timeinfo)) {
    Serial.println("[NTP] Successful in syncing the time.");
  } else {
    Serial.println("[NTP:ERROR] Unable to obtain time.");
    return;
  }

  // Setup Pins
  pinMode(ULTRASONIC_TRIG, OUTPUT);
  pinMode(ULTRASONIC_ECHO, INPUT);
  
  // Setup servo and display
  setupLCD(LCD_SCREEN);
  feederServo.attach(SERVO_PIN);

  attachInterrupt(MOTION_PIN, motionChange, CHANGE);
}

void loop()
{
  // 08:00 to 20:00
  if (8 <= timeinfo.tm_hour && timeinfo.tm_hour < 19) {
    unsigned long now = millis();
    if (now - lastSeen > catNearbyWindow) {
      catNearby = false;
    }

    // Allow feed if its within interval, run only once per interval
    if (timeinfo.tm_hour - lastFeed >= FEED_INTERVAL) {
      canFeed = true;
      lastFeed = timeinfo.tm_hour;
      feedAttempt--;

      /* 
        if its the last attempt, which meant 16:00 time, have feed allowance
        of 3hours, instead of 1hour
      */ 
      if (feedAttempt == 1) {
        allowanceFeed = 3;
      }
    }

    // The second condition calculates if the current time is less than
    // time allowance.
    if (canFeed && timeinfo.tm_hour - lastFeed <= allowanceFeed) {
      if (catNearby) {
        openFeeder(feederServo);
        closeFeeder(feederServo);


        // block feeding
        canFeed = false;
      }
    }
  } else if ((feedAttempt != 3 || allowanceFeed != 1) && timeinfo.tm_hour > 19) {
    // Reset if current time is greater than 19:00
    feedAttempt = 3;
    allowanceFeed = 1;
  }

  updateMovement(LCD_SCREEN, detectMotion(MOTION_PIN));
  updateFoodLevel(LCD_SCREEN, foodLevelPercentage);

  delay(1000);
}
