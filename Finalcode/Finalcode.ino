#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <ArduinoJson.h>
#include "addons/TokenHelper.h"
#include <TinyGPS++.h>
#include <HardwareSerial.h>
#include "DHT.h"
#include <ESP32Servo.h>

// ---------------- WIFI ----------------
const char* ssid = "Dulmina";
const char* password = "dula1790";

// ---------------- FIREBASE ----------------
#define FIREBASE_HOST "https://iot-forest-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define FIREBASE_API_KEY "AIzaSyBmEE-Bm7HICrRigTgzZLSKjwllMTJP_-s"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// ---------------- DHT11 ----------------
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// ---------------- MQ-9 ----------------
#define MQ9_PIN 32
int SMOKE_THRESHOLD = 1500;

// ---------------- Flame Sensor ----------------
#define FLAME_DO 34
#define FLAME_AO 33

// ---------------- Rain & LDR ----------------
#define RAIN_AO 39
#define LDR_PIN 36

// ---------------- Servo & Buzzer ----------------
Servo servo;
#define SERVO_PIN 13
#define BUZZER_PIN 18
int STOP_VAL = 90;
int FORWARD_SLOW = 80;
int BACKWARD_SLOW = 100;

// ---------------- GPS ----------------
#define RXD2 16
#define TXD2 17
#define GPS_BAUD 9600
HardwareSerial gpsSerial(2);
TinyGPSPlus gps;

// ---------------- Timers ----------------
unsigned long lastUpload = 0;
const unsigned long UPLOAD_INTERVAL = 10000; // 10s
const char* DEVICE_PATH = "forest_devices/device_01";

// ---------------- Firebase Setup ----------------
void setupFirebase() {
  config.api_key = FIREBASE_API_KEY;
  config.database_url = FIREBASE_HOST;
  auth.user.email = "kulasekaradulmina95@gmail.com";
  auth.user.password = "dula1790";
  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

// ---------------- Upload Snapshot ----------------
void uploadSnapshot(bool pushAlert = false) {
  FirebaseJson json;

  float temp = dht.readTemperature();
  float hum  = dht.readHumidity();
  int gasValue = analogRead(MQ9_PIN);
  int flameAnalog = analogRead(FLAME_AO);
  int flameDigital = digitalRead(FLAME_DO);

  int rainAnalog = analogRead(RAIN_AO);
  float rainPercent = (1.0 - ((float)rainAnalog / 4095.0)) * 100.0;
  String rainStatus = (rainPercent < 33) ? "Dry" : (rainPercent < 66) ? "Light Rain" : "Heavy Rain";

  int ldrValue = analogRead(LDR_PIN);
  String lightDesc = (ldrValue <= 500) ? "Very bright light 🌞" : (ldrValue <= 2000) ? "Normal indoor light 💡" : "Dark / Night 🌑";

  bool isNight = (ldrValue > 2000);
  bool fireDetected = (!isNight && (gasValue >= SMOKE_THRESHOLD || (!isnan(temp) && temp >= 45))) ||
                      (isNight && (flameDigital == LOW || flameAnalog > 2000 || gasValue >= SMOKE_THRESHOLD || (!isnan(temp) && temp >= 45)));

  // GPS coordinates
  double latitude = gps.location.isValid() ? gps.location.lat() : 0.0;
  double longitude = gps.location.isValid() ? gps.location.lng() : 0.0;

  // JSON payload
  json.set("timestamp", String(millis()));
  json.set("temperature", isnan(temp)? 0 : temp);
  json.set("humidity", isnan(hum)? 0 : hum);
  json.set("gas", gasValue);
  json.set("flameAnalog", flameAnalog);
  json.set("flameDigital", flameDigital);
  json.set("rainAnalog", rainAnalog);
  json.set("rainPercent", rainPercent);
  json.set("rainStatus", rainStatus);
  json.set("ldrAnalog", ldrValue);
  json.set("lightDescription", lightDesc);
  json.set("fireDetected", fireDetected ? "true" : "false");
  json.set("latitude", latitude);
  json.set("longitude", longitude);

  String path = String(DEVICE_PATH) + "/last";
  Firebase.RTDB.setJSON(&fbdo, path.c_str(), &json);

  if (pushAlert && fireDetected) {
    Firebase.RTDB.pushJSON(&fbdo, "/alerts", &json);
  }
}

// ---------------- MAIN SETUP ----------------
void setup() {
  dht.begin();

  pinMode(FLAME_DO, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  servo.setPeriodHertz(50);
  servo.attach(SERVO_PIN, 500, 2500);
  servo.write(STOP_VAL);

  gpsSerial.begin(GPS_BAUD, SERIAL_8N1, RXD2, TXD2);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(300);

  setupFirebase();
}

// ---------------- MAIN LOOP ----------------
void loop() {
  while (gpsSerial.available() > 0) gps.encode(gpsSerial.read());

  float temp = dht.readTemperature();
  float hum  = dht.readHumidity();
  int gasValue = analogRead(MQ9_PIN);
  int flameAnalog = analogRead(FLAME_AO);
  int flameDigital = digitalRead(FLAME_DO);

  int rainAnalog = analogRead(RAIN_AO);
  float rainPercent = (1.0 - ((float)rainAnalog / 4095.0)) * 100.0;
  String rainStatus = (rainPercent < 33) ? "Dry" : (rainPercent < 66) ? "Light Rain" : "Heavy Rain";

  int ldrValue = analogRead(LDR_PIN);
  bool isNight = (ldrValue > 2000);

  bool fireDetected = (!isNight && (gasValue >= SMOKE_THRESHOLD || (!isnan(temp) && temp >= 45))) ||
                      (isNight && (flameDigital == LOW || flameAnalog > 2000 || gasValue >= SMOKE_THRESHOLD || (!isnan(temp) && temp >= 45)));

  if (fireDetected) {
    servo.write(STOP_VAL);
    digitalWrite(BUZZER_PIN, HIGH);
    uploadSnapshot(true);
    delay(3000);
    digitalWrite(BUZZER_PIN, LOW);
  } else if (isNight) {
    servo.write(FORWARD_SLOW); delay(1200);
    servo.write(STOP_VAL); delay(300);
    servo.write(BACKWARD_SLOW); delay(1200);
    servo.write(STOP_VAL); delay(300);
  }

  if (millis() - lastUpload > UPLOAD_INTERVAL) {
    uploadSnapshot(false);
    lastUpload = millis();
  }

  delay(2000);
}