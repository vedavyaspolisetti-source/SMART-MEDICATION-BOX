# 💊 Smart Medication Box

A responsive web application and ESP32 firmware for managing medication schedules for elderly patients and people with memory loss.

## 📋 Features

- **Caregiver Web App**: Easy-to-use interface for scheduling medications.
- **Physical Layout Mirroring**: Vertical UI matches the 4-compartment hardware stack (Box 1 Top, Box 4 Bottom).
- **Daily Recurring Schedules**: Set once, runs forever. The system automatically resets daily at midnight.
- **Real-Time Sync**: Firebase Realtime Database for instant updates to the hardware.
- **Hardware Integration**: ESP32 with LEDs, buzzer, LCD, and IR sensors.

## 🛠️ Setup

### 1. Firebase Setup

1. Go to [Firebase Console](https://console.firebase.google.com).
2. Create a new project.
3. Enable **Realtime Database** (start in test mode for development).
4. Copy your config from Project Settings → Your apps.

### 2. Web App Setup

1. Open `app.js`.
2. Replace the Firebase config object with your own credentials.
   ```javascript
   const firebaseConfig = {
       apiKey: "YOUR_API_KEY",
       // ... other config ...
   };
   ```
3. Open `index.html` in a browser.

### 3. ESP32 Setup

1. Install [Arduino IDE](https://www.arduino.cc/en/software).
2. Install required libraries:
   - `Firebase ESP Client`
   - `LiquidCrystal I2C`
   - `NTPClient`
   - `ArduinoJson`
3. Open `esp32_code/medication_box.ino`.
4. Update:
   - `WIFI_SSID` / `WIFI_PASSWORD`
   - `API_KEY` / `DATABASE_URL`
5. Upload to ESP32.

## 🔌 Hardware Wiring

| ESP32 Pin | Component | Description |
|-----------|-----------|-------------|
| GPIO 25 | LED 1 | Compartment 1 (Top) Indicator |
| GPIO 26 | LED 2 | Compartment 2 Indicator |
| GPIO 27 | LED 3 | Compartment 3 Indicator |
| GPIO 14 | LED 4 | Compartment 4 (Bottom) Indicator |
| GPIO 32 | Buzzer | Audio Alert |
| GPIO 33 | IR Sensor 1 | Detects hand in Box 1 |
| GPIO 34 | IR Sensor 2 | Detects hand in Box 2 |
| GPIO 35 | IR Sensor 3 | Detects hand in Box 3 |
| GPIO 36 | IR Sensor 4 | Detects hand in Box 4 |
| GPIO 21 | LCD SDA | Display Data |
| GPIO 22 | LCD SCL | Display Clock |

## 📁 Project Structure

```
medication-box/
├── index.html          # Web app UI (Vertical Layout)
├── style.css           # Responsive Styles
├── app.js              # Logic & Firebase Integration
├── esp32_code/
│   └── medication_box.ino  # Firmware
└── README.md
```

## 📊 Firebase Data Structure

The system uses a strictly **compartment-wise** structure. Each compartment is an independent node.
Times are stored in **12-hour format (HH:MM AM/PM)**.

```json
{
  "medication_box": {
    "compartment_1": {
      "time": "08:30 AM",
      "buzzer": true,
      "medicine_taken": false,
      "medicines": [
        { "name": "Aspirin", "tablets": 1 }
      ]
    },
    "compartment_2": {
      "time": "01:00 PM",
      "buzzer": true,
      "medicine_taken": false,
      "medicines": []
    },
    "compartment_3": { ... },
    "compartment_4": { ... }
  }
}
```

## 🧠 System Logic

### Daily Recurring Schedule
- **Set Once, Run Forever**: The schedule persists in Firebase.
- **Midnight Reset**: The ESP32 sends a write request to Firebase at **00:00** every night, resetting the `medicine_taken` flag to `false`. This ensures the alarm triggers again the next day without manual intervention.
- **Real-Time Updates**: Changing time or medicines in the app updates the ESP32 instantly.

### Smart Hardware Behavior
- **Alarm**: When `current_time == compartment_time`, the specific LED and Buzzer turn ON.
- **LCD Display**: Shows "Take Medicine" and cycles through the list of medicines (Name & Count).
- **Intelligent Stop**: The IR sensor detects when the user puts their hand in the box.
  - **Action**: Stops Buzzer, turns OFF LED, marks `medicine_taken = true` in Firebase, and displays "Thank You".

## 📄 License

MIT License.
