#include <TinyGPS++.h>
#include <HardwareSerial.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "DHT.h"
#include <ESP32Servo.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"

// ---------------- WIFI ----------------
const char* ssid = "Dulmina";
const char* password = "dula1790";

// ---------------- FIREBASE ----------------
#define FIREBASE_HOST "https://iot-forest-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define FIREBASE_API_KEY "AIzaSyBmEE-Bm7HICrRigTgzZLSKjwllMTJP_-s"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// ---------------- GPS ----------------
#define RXD2 16
#define TXD2 17
#define GPS_BAUD 9600
HardwareSerial gpsSerial(2);
TinyGPSPlus gps;

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

// ---------------- Rain Sensor ----------------
#define POWER_PIN 14
#define DO_PIN 25
#define AO_PIN 26  // Analog output

// ---------------- LDR ----------------
#define LDR_AO 27
#define LDR_DO 5

// ---------------- Servo & Buzzer ----------------
Servo servo;
#define SERVO_PIN 13
#define BUZZER_PIN 18
int STOP_VAL = 90;
int FORWARD_SLOW = 80;
int BACKWARD_SLOW = 100;

// ---------------- Timers ----------------
unsigned long lastUpload = 0;
const unsigned long UPLOAD_INTERVAL = 10000; // 10s
unsigned long lastAddressTime = 0;
const unsigned long ADDRESS_INTERVAL = 10000; // 10s

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
  Serial.println("Firebase Auth + RTDB Initialized!");
}

// ---------------- Helper: Fetch Address ----------------
String fetchAddress(double lat, double lon) {
  if (WiFi.status() != WL_CONNECTED) return "";
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

// ---------------- Upload Snapshot ----------------
void uploadSnapshot(bool pushAlert = false) {
  FirebaseJson json;

  // Read DHT
  float temp = dht.readTemperature();
  float hum  = dht.readHumidity();

  // Read sensors
  int gasValue = analogRead(MQ9_PIN);
  int flameAnalog = analogRead(FLAME_AO);
  int flameDigital = digitalRead(FLAME_DO);

  // Rain sensor
  digitalWrite(POWER_PIN, HIGH);
  delay(10);
  int rainAnalog = analogRead(AO_PIN);
  int rainDigital = digitalRead(DO_PIN);
  digitalWrite(POWER_PIN, LOW);

  // Correct Rain Intensity (inverted)
  float rainPercent = (1.0 - ((float)rainAnalog / 4095.0)) * 100.0;
  String rainStatus = (rainDigital == LOW ? "Rain Detected" : "No Rain");

  int ldrAnalog = analogRead(LDR_AO);
  int ldrDigital = digitalRead(LDR_DO);
  int satellites = gps.satellites.value();

  // JSON data
  json.set("timestamp", String(millis()));
  if (!isnan(temp)) json.set("temperature", temp);
  if (!isnan(hum))  json.set("humidity", hum);
  json.set("gas", gasValue);
  json.set("flameAnalog", flameAnalog);
  json.set("flameDigital", flameDigital);
  json.set("rainAnalog", rainAnalog);
  json.set("rainDigital", rainStatus);
  json.set("rainPercent", rainPercent);
  json.set("ldrAnalog", ldrAnalog);
  json.set("ldrDigital", ldrDigital);
  json.set("satellites", satellites);

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

  // Fire detection
  bool isNight = (ldrAnalog < 1500);
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
    Serial.println(fbdo.errorReason());
  }

  // Push alert if fire
  if (isFire || pushAlert) {
    FirebaseJson alert;
    alert.set("timestamp", String(millis()));
    alert.set("fireDetected", "true");
    alert.set("temperature", !isnan(temp)? temp : 0);
    alert.set("humidity", !isnan(hum)? hum : 0);
    alert.set("gas", gasValue);
    alert.set("flameAnalog", flameAnalog);
    alert.set("flameDigital", flameDigital);
    alert.set("rainAnalog", rainAnalog);
    alert.set("rainDigital", rainStatus);
    alert.set("rainPercent", rainPercent);
    if (gps.location.isValid()) {
      alert.set("latitude", gps.location.lat());
      alert.set("longitude", gps.location.lng());
    }
    Firebase.RTDB.pushJSON(&fbdo, "/alerts", &alert);
    Serial.println("Alert pushed to /alerts");
  }
}

// ---------------- MAIN SETUP ----------------
void setup() {
  Serial.begin(115200);

  gpsSerial.begin(GPS_BAUD, SERIAL_8N1, RXD2, TXD2);
  dht.begin();

  pinMode(FLAME_DO, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  pinMode(POWER_PIN, OUTPUT);
  pinMode(DO_PIN, INPUT);

  pinMode(LDR_DO, INPUT);

  servo.setPeriodHertz(50);
  servo.attach(SERVO_PIN, 500, 2500);
  servo.write(STOP_VAL);

  Serial.print("Connecting WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println("\nWiFi connected.");

  setupFirebase();
  delay(500);
}

// ---------------- MAIN LOOP ----------------
void loop() {
  while (gpsSerial.available() > 0) gps.encode(gpsSerial.read());

  // Read sensors
  float temp = dht.readTemperature();
  float hum  = dht.readHumidity();
  int gasValue = analogRead(MQ9_PIN);
  int flameAnalog = analogRead(FLAME_AO);
  int flameDigital = digitalRead(FLAME_DO);

  digitalWrite(POWER_PIN, HIGH);
  delay(10);
  int rainAnalog = analogRead(AO_PIN);
  int rainDigital = digitalRead(DO_PIN);
  digitalWrite(POWER_PIN, LOW);

  float rainPercent = (1.0 - ((float)rainAnalog / 4095.0)) * 100.0;
  String rainStatus = (rainDigital == LOW ? "Rain Detected" : "No Rain");

  int ldrAnalog = analogRead(LDR_AO);
  int ldrDigital = digitalRead(LDR_DO);

  // Debug prints
  Serial.print("Temp: "); if (!isnan(temp)) Serial.print(temp); else Serial.print("nan");
  Serial.print(" C  Humidity: "); if (!isnan(hum)) Serial.print(hum); else Serial.print("nan");
  Serial.print(" %  Gas: "); Serial.print(gasValue);
  Serial.print("  Rain Level: "); Serial.print(rainAnalog);
  Serial.print("  Rain Intensity: "); Serial.print(rainPercent); Serial.print("%");
  Serial.print("  Rain Status: "); Serial.println(rainStatus);
  Serial.print("  LDR Analog: "); Serial.print(ldrAnalog);
  Serial.print("  LDR Digital: "); Serial.println(ldrDigital);

  bool isNight = (ldrAnalog < 1500);
  bool fireDetected = false;
  if (!isNight) {
    if (gasValue >= SMOKE_THRESHOLD || (!isnan(temp) && temp >= 45)) fireDetected = true;
  } else {
    if (flameDigital == LOW || flameAnalog > 2000 || gasValue >= SMOKE_THRESHOLD || (!isnan(temp) && temp >= 45))
      fireDetected = true;
  }

  if (fireDetected) {
    Serial.println("!!! FIRE DETECTED - STOP SERVO + BUZZER ON !!!");
    servo.write(STOP_VAL);
    digitalWrite(BUZZER_PIN, HIGH);
    uploadSnapshot(true);
    delay(3000);
    digitalWrite(BUZZER_PIN, LOW);
  } else {
    digitalWrite(BUZZER_PIN, LOW);
    if (!isNight) {
      servo.write(STOP_VAL);
    } else {
      servo.write(FORWARD_SLOW); delay(1200);
      servo.write(STOP_VAL); delay(300);
      servo.write(BACKWARD_SLOW); delay(1200);
      servo.write(STOP_VAL); delay(300);
    }
  }

  if (millis() - lastUpload > UPLOAD_INTERVAL) {
    uploadSnapshot(false);
    lastUpload = millis();
  }

  delay(2000); // safe delay for DHT11
}