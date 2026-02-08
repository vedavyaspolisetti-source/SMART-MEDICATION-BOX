/**
 * =====================================================
 * SMART MEDICATION BOX - ESP32 FIRMWARE v4.1 (REFINED)
 * =====================================================
 *
 * FEATURES:
 * - Monitors 4 fixed compartments: /compartment_1, _2, _3, _4
 * - Daily Recurring Schedule (Midnight Reset)
 * - Independent Alarms per box
 */

#include "addons/RTDBHelper.h"
#include "addons/TokenHelper.h"
#include <ArduinoJson.h>
#include <Firebase_ESP_Client.h>
#include <LiquidCrystal_I2C.h>
#include <NTPClient.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <Wire.h>

// =====================================================
// 1. CONFIGURATION
// =====================================================

// ⚠️ UPDATE THESE
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#define API_KEY "AIzaSyBfoXX_E1568WwAR6sCls_5o9L5h1FgZqc"
#define DATABASE_URL                                                           \
  "https://smart-medication-box-e8e5b-default-rtdb.firebaseio.com"

// Hardware Pins
const int LED_PINS[] = {25, 26, 27, 14}; // Box 1-4
const int IR_PINS[] = {33, 34, 35, 36};  // Box 1-4
const int BUZZER_PIN = 32;

#define LCD_ADDRESS 0x27
#define LCD_COLS 16
#define LCD_ROWS 2

// =====================================================
// 2. GLOBAL OBJECTS
// =====================================================

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLS, LCD_ROWS);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 19800, 60000);

bool signupOK = false;
unsigned long lastCheck = 0;
const int CHECK_INTERVAL = 3000;
bool midnightResetDone = false; // Flag to prevent multiple resets

// Alarm State for EACH compartment
struct CompartmentState {
  bool active;
  String time;
  bool buzzer;
  bool taken;
  // Medicine Display Data
  String medNames[5];
  int medCounts[5];
  int totalMeds;
};

// Indices 0-3 map to Box 1-4
CompartmentState boxes[4];
int activeAlarmIndex = -1; // -1 = None, 0-3 = Active Box ID

// =====================================================
// 3. SETUP
// =====================================================

void setup() {
  Serial.begin(115200);

  // Init Hardware
  for (int i = 0; i < 4; i++) {
    pinMode(LED_PINS[i], OUTPUT);
    digitalWrite(LED_PINS[i], LOW);
    pinMode(IR_PINS[i], INPUT);
    boxes[i].active = false;
    boxes[i].taken = true; // Assume taken at boot to prevent instant alarms
  }
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // Init LCD
  lcd.init();
  lcd.backlight();
  lcd.print("System Boot...");

  // WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  lcd.clear();
  lcd.print("WiFi Connected");

  // Firebase
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  if (Firebase.signUp(&config, &auth, "", "")) {
    signupOK = true;
  }
  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // Time
  timeClient.begin();
}

// =====================================================
// 4. MAIN LOOP
// =====================================================

void loop() {
  timeClient.update();

  // Midnight Reset Logic
  checkMidnightReset();

  // Priority: Handle Active Alarm
  if (activeAlarmIndex != -1) {
    handleAlarm(activeAlarmIndex);
  }
  // Otherwise: Check for New Alarms
  else {
    if (millis() - lastCheck > CHECK_INTERVAL) {
      syncData();
      lastCheck = millis();
    }
    lcdStatus();
  }
}

// =====================================================
// 5. CORE LOGIC
// =====================================================

void checkMidnightReset() {
  int h = timeClient.getHours();
  int m = timeClient.getMinutes();

  // If it is 00:00 (Midnight)
  if (h == 0 && m == 0) {
    if (!midnightResetDone) {
      Serial.println("Performing Midnight Reset...");
      if (Firebase.ready() && signupOK) {
        // Reset all compartments to medicine_taken = false
        for (int i = 1; i <= 4; i++) {
          String path =
              "/medication_box/compartment_" + String(i) + "/medicine_taken";
          Firebase.RTDB.setBool(&fbdo, path.c_str(), false);
        }
        midnightResetDone = true;
      }
    }
  } else {
    // Reset flag once we move past 00:00
    midnightResetDone = false;
  }
}

void syncData() {
  if (!Firebase.ready() || !signupOK)
    return;

  String currentTime = getFormattedTime();

  if (Firebase.RTDB.getJSON(&fbdo, "/medication_box")) {
    FirebaseJson &json = fbdo.jsonObject();

    for (int i = 0; i < 4; i++) {
      String compKey = "compartment_" + String(i + 1);
      FirebaseJsonData result;
      json.get(result, compKey);

      if (result.success) {
        FirebaseJson compJson;
        compJson.setJsonData(result.stringValue);

        FirebaseJsonData t, b, taken, meds;
        compJson.get(t, "time");
        compJson.get(b, "buzzer");
        compJson.get(taken, "medicine_taken");
        compJson.get(meds, "medicines");

        // Trigger only if Time Matches AND Not Taken
        if (t.stringValue == currentTime && !taken.boolValue) {
          triggerAlarm(i, t.stringValue, b.boolValue, meds);
          return;
        }
      }
    }
  }
}

void triggerAlarm(int boxIndex, String time, bool buzzer,
                  FirebaseJsonData &meds) {
  activeAlarmIndex = boxIndex;
  boxes[boxIndex].active = true;
  boxes[boxIndex].time = time;
  boxes[boxIndex].buzzer = buzzer;
  boxes[boxIndex].totalMeds = 0;

  // Parse Medicines Array
  FirebaseJsonArray arr;
  arr.setJsonArrayData(meds.stringValue);
  for (size_t k = 0; k < arr.size(); k++) {
    if (k >= 5)
      break;
    FirebaseJsonData itemData;
    arr.get(itemData, k);
    FirebaseJson item;
    item.setJsonData(itemData.stringValue);
    FirebaseJsonData n, c;
    item.get(n, "name");
    item.get(c, "tablets");
    boxes[boxIndex].medNames[k] = n.stringValue;
    boxes[boxIndex].medCounts[k] = c.intValue;
    boxes[boxIndex].totalMeds++;
  }

  // Turn ON LED
  digitalWrite(LED_PINS[boxIndex], HIGH);
  Serial.println("ALARM! Box " + String(boxIndex + 1));
}

void handleAlarm(int boxIndex) {
  if (boxes[boxIndex].buzzer) {
    pulseBuzzer();
  }

  static unsigned long lastUpdate = 0;
  static int cycle = -1;
  if (millis() - lastUpdate > 2000) {
    lcd.clear();
    if (cycle == -1) {
      lcd.setCursor(0, 0);
      lcd.print("TAKE MEDS!");
      lcd.setCursor(0, 1);
      lcd.print("Open Box " + String(boxIndex + 1));
      cycle = 0;
      if (boxes[boxIndex].totalMeds == 0)
        cycle = -1;
    } else {
      lcd.setCursor(0, 0);
      lcd.print(boxes[boxIndex].medNames[cycle]);
      lcd.setCursor(0, 1);
      lcd.print(String(boxes[boxIndex].medCounts[cycle]) + " pills");
      cycle++;
      if (cycle >= boxes[boxIndex].totalMeds)
        cycle = -1;
    }
    lastUpdate = millis();
  }

  if (digitalRead(IR_PINS[boxIndex]) == LOW) {
    completeAlarm(boxIndex);
  }
}

void completeAlarm(int boxIndex) {
  Serial.println("Taken! Box " + String(boxIndex + 1));
  digitalWrite(LED_PINS[boxIndex], LOW);
  digitalWrite(BUZZER_PIN, LOW);

  String path =
      "/medication_box/compartment_" + String(boxIndex + 1) + "/medicine_taken";
  Firebase.RTDB.setBool(&fbdo, path.c_str(), true);

  lcd.clear();
  lcd.print("Thank You!");
  delay(2000);

  activeAlarmIndex = -1;
  boxes[boxIndex].active = false;
}

void lcdStatus() {
  static unsigned long last = 0;
  if (millis() - last > 5000) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("System Active");
    lcd.setCursor(0, 1);
    lcd.print(getFormattedTime());
    last = millis();
  }
}

String getFormattedTime() {
  int h = timeClient.getHours();
  int m = timeClient.getMinutes();
  String suffix = (h >= 12) ? "PM" : "AM";
  int h12 = (h > 12) ? h - 12 : h;
  if (h12 == 0)
    h12 = 12;
  char buf[10];
  sprintf(buf, "%02d:%02d %s", h12, m, suffix.c_str());
  return String(buf);
}

void pulseBuzzer() {
  static bool state = false;
  static unsigned long last = 0;
  if (millis() - last > 300) {
    state = !state;
    digitalWrite(BUZZER_PIN, state);
    last = millis();
  }
}
