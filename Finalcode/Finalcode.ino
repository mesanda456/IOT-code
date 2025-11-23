#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <ArduinoJson.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// Rain sensor AO pin
#define RAIN_AO 39  // VN pin on ESP32

// WiFi credentials
const char* ssid = "Dulmina";
const char* password = "dula1790";

// Firebase credentials
#define FIREBASE_HOST  "https://all-in-one-a5cbb-default-rtdb.asia-southeast1.firebasedatabase.app"
#define FIREBASE_AUTH "AIzaSyBJSGM5NsCNh6VrenkBmF58zKgJ49JxYc8"

// Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

void setup() {
  Serial.begin(115200);

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to Wi-Fi!");

  // Firebase setup
  config.database_url = FIREBASE_HOST;
  auth.token.uid = "";
  config.signer.tokens.legacy_token = FIREBASE_AUTH;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void loop() {
  int rainAnalog = analogRead(RAIN_AO);  // 0–4095

  // Determine status based on analog value
  String status;
  if (rainAnalog > 2000) {
    status = "Dry";
  } else if (rainAnalog > 1000) {
    status = "Light Rain";
  } else {
    status = "Heavy Rain";
  }

  // Print to Serial Monitor
  Serial.print("Rain Analog: ");
  Serial.print(rainAnalog);
  Serial.print(" → ");
  Serial.println(status);

  // Push to Firebase
  if (Firebase.ready()) {
    String path = "/Rain_Data";
    FirebaseJson json;
    json.set("analog", rainAnalog);
    json.set("status", status);

    if (Firebase.RTDB.setJSON(&fbdo, path.c_str(), &json)) {
      Serial.println("Data updated to Firebase ✅");
    } else {
      Serial.print("Firebase update failed: ");
      Serial.println(fbdo.errorReason());
    }
  }

  delay(1000); // 1-second delay
}