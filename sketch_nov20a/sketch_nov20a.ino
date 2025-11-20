/* Final Forest-Fire Tracker + Firebase Realtime DB uploader
   - Uploads sensor snapshot every 10s
   - Immediate upload on fire detection (push to /alerts)
   - Uses Firebase_ESP_Client (Mobizt)
*/

#include <TinyGPS++.h>
#include <HardwareSerial.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "DHT.h"
#include <ESP32Servo.h>

// Firebase (Mobizt library)
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h" // for token generation

// -------------------- YOUR WIFI --------------------
const char* ssid = "Dulmina";
const char* password ="dula1790";

// -------------------- FIREBASE --------------------
#define FIREBASE_HOST "iot-forest-default-rtdb.asia-southeast1.firebasedatabase.app"
#define FIREBASE_API_KEY "AIzaSyBmEE-Bm7HICrRigTgzZLSKjwllMTJP_-s"

// Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// -------------------- GPS --------------------
#define RXD2 16
#define TXD2 17
#define GPS_BAUD 9600
HardwareSerial gpsSerial(2);
TinyGPSPlus gps;

// -------------------- DHT11 --------------------
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// -------------------- MQ-9 Gas Sensor --------------------
#define MQ9_PIN 32
int SMOKE_THRESHOLD = 1500;

// -------------------- Flame Sensor --------------------
#define FLAME_DO 34   // digital output (DO)
#define FLAME_AO 33   // analog AO

// -------------------- Rain Sensor --------------------
#define RAIN_PIN 25   // digital DO

// -------------------- Servo & Buzzer --------------------
Servo s;
#define SERVO_PIN 18
#define BUZZER_PIN 26
int STOP_PWM = 90;

// -------------------- Timers --------------------
unsigned long lastUpload = 0;
const unsigned long UPLOAD_INTERVAL = 300000; // 10 seconds
unsigned long lastAddressTime = 0;
const unsigned long ADDRESS_INTERVAL = 10000; // 10 seconds

// device id/path in DB
const char* DEVICE_PATH = "forest_devices/device_01";


// -------------------- FIREBASE SETUP WITH AUTH --------------------
void setupFirebase() {
  config.api_key = FIREBASE_API_KEY;
  config.database_url = FIREBASE_HOST;

  // YOU MUST CREATE THIS USER IN FIREBASE AUTH
  auth.user.email = "kulasekaradulmina95@gmail.com";
  auth.user.password = "dula1790";

  // IMPORTANT for token generation
  config.token_status_callback = tokenStatusCallback;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  Serial.println("Firebase Auth + RTDB Initialized!");
}


// ----------------------------------------------------------------------------
// Helper: fetch address via OpenStreetMap Reverse Geocoding
// ----------------------------------------------------------------------------
String fetchAddress(double lat, double lon) {
  if (WiFi.status() != WL_CONNECTED) return String("");

  HTTPClient http;
  String url = "https://nominatim.openstreetmap.org/reverse?format=json&lat=" +
               String(lat, 6) + "&lon=" + String(lon, 6) +
               "&zoom=18&addressdetails=1";

  http.begin(url);
  http.addHeader("User-Agent", "ESP32-Forest-Trk");

  int code = http.GET();
  String address = "";

  if (code > 0) {
    String payload = http.getString();
    int i1 = payload.indexOf("\"display_name\":\"");
    if (i1 != -1) {
      i1 += 16;
      int i2 = payload.indexOf("\"", i1);
      address = payload.substring(i1, i2);
    }
  }

  http.end();
  return address;
}


// ----------------------------------------------------------------------------
// Upload a snapshot OR an alert to Firebase
// ----------------------------------------------------------------------------
void uploadSnapshot(bool pushAlert = false) {
  FirebaseJson json;

  float temp = dht.readTemperature();
  int gasValue = analogRead(MQ9_PIN);
  int flameAnalog = analogRead(FLAME_AO);
  int flameDigital = digitalRead(FLAME_DO);
  int rainState = digitalRead(RAIN_PIN);
  int satellites = gps.satellites.value();

  json.set("timestamp", String(millis()));
  if (!isnan(temp)) json.set("temperature", temp);
  json.set("gas", gasValue);
  json.set("flameAnalog", flameAnalog);
  json.set("flameDigital", flameDigital);
  json.set("rain", (rainState == LOW) ? "true" : "false");
  json.set("satellites", satellites);

  // Add GPS data
  if (gps.location.isValid()) {
    double lat = gps.location.lat();
    double lon = gps.location.lng();
    json.set("latitude", lat);
    json.set("longitude", lon);

    if (millis() - lastAddressTime > ADDRESS_INTERVAL) {
      String address = fetchAddress(lat, lon);
      if (address.length()) json.set("address", address);
      lastAddressTime = millis();
    }
  }

  // FIRE detection logic
  bool isNight = (flameAnalog > 2000);
  bool isFire = false;

  if (!isNight) {
    if (gasValue >= SMOKE_THRESHOLD || (!isnan(temp) && temp >= 45)) isFire = true;
  } else {
    if (flameDigital == LOW || flameAnalog > 2000 || gasValue >= SMOKE_THRESHOLD || (!isnan(temp) && temp >= 45))
      isFire = true;
  }

  json.set("fireDetected", isFire ? "true" : "false");

  // Upload snapshot
  String path = String(DEVICE_PATH) + "/last";

  if (Firebase.RTDB.setJSON(&fbdo, path.c_str(), &json)) {
    Serial.println("Uploaded snapshot to " + path);
  } else {
    Serial.print("Firebase error: ");
    Serial.println(fbdo.errorReason());
  }

  // Push alert to /alerts
  if (isFire || pushAlert) {
    FirebaseJson alert;

    alert.set("timestamp", String(millis()));
    alert.set("fireDetected", "true");
    alert.set("temperature", !isnan(temp) ? temp : 0);
    alert.set("gas", gasValue);
    alert.set("flameAnalog", flameAnalog);
    alert.set("flameDigital", flameDigital);

    if (gps.location.isValid()) {
      alert.set("latitude", gps.location.lat());
      alert.set("longitude", gps.location.lng());
    }

    String alertsPath = "/alerts";

    if (Firebase.RTDB.pushJSON(&fbdo, alertsPath.c_str(), &alert)) {
      Serial.println("Alert pushed to " + alertsPath);
    } else {
      Serial.print("Alert push failed: ");
      Serial.println(fbdo.errorReason());
    }
  }
}


// ----------------------------------------------------------------------------
// MAIN SETUP
// ----------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);

  gpsSerial.begin(GPS_BAUD, SERIAL_8N1, RXD2, TXD2);
  dht.begin();

  pinMode(FLAME_DO, INPUT);
  pinMode(FLAME_AO, INPUT);
  pinMode(RAIN_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  s.setPeriodHertz(50);
  s.attach(SERVO_PIN);
  s.write(STOP_PWM);

  Serial.print("Connecting WiFi");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }

  Serial.println("\nWiFi connected.");

  setupFirebase();
}


// ----------------------------------------------------------------------------
// LOOP
// ----------------------------------------------------------------------------
void loop() {
  while (gpsSerial.available() > 0) gps.encode(gpsSerial.read());

  float temp = dht.readTemperature();
  int gasValue = analogRead(MQ9_PIN);
  int flameAnalog = analogRead(FLAME_AO);
  int flameDigital = digitalRead(FLAME_DO);

  bool isNight = (flameAnalog > 2000);
  bool fireDetected = false;

  if (!isNight) {
    if (gasValue >= SMOKE_THRESHOLD || temp >= 45) fireDetected = true;
  } else {
    if (flameDigital == LOW || flameAnalog > 2000 || gasValue >= SMOKE_THRESHOLD || temp >= 45)
      fireDetected = true;
  }

  if (fireDetected) {
    Serial.println("!!! FIRE DETECTED - IMMEDIATE ACTION !!!");
    s.write(STOP_PWM);
    digitalWrite(BUZZER_PIN, HIGH);

    uploadSnapshot(true);

    delay(3000);
    digitalWrite(BUZZER_PIN, LOW);
  } else {
    if (!isNight) {
      s.write(STOP_PWM);
      Serial.println("Daytime - Servo stopped");
    } else {
      s.write(100); delay(1200);
      s.write(STOP_PWM); delay(300);
      s.write(80); delay(1200);
      s.write(STOP_PWM); delay(300);
    }
  }

  if (millis() - lastUpload > UPLOAD_INTERVAL) {
    uploadSnapshot(false);
    lastUpload = millis();
  }

  delay(200);
}
