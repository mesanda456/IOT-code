#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <ArduinoJson.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// LDR pin (GPIO 36 / VP)
#define LDR_PIN 36  

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
  int lightValue = analogRead(LDR_PIN);
  String lightDesc;

  // Your original light-level logic
  if (lightValue >= 0 && lightValue <= 500) {
    lightDesc = "Very bright light 🌞";
  } 
  else if (lightValue > 500 && lightValue <= 2000) {
    lightDesc = "Normal indoor light 💡";
  } 
  else if (lightValue > 2000 && lightValue <= 4095) {
    lightDesc = "Dark / Night 🌑";
  } 
  else {
    lightDesc = "Out of range";
  }

  // Print to Serial Monitor
  Serial.print("LDR Value: ");
  Serial.print(lightValue);
  Serial.print(" → ");
  Serial.println(lightDesc);

  // Push to Firebase (overwrite same node)
  if (Firebase.ready()) {
    String path = "/LDR_Data";
    FirebaseJson json;
    json.set("analog", lightValue);
    json.set("description", lightDesc);

    if (Firebase.RTDB.setJSON(&fbdo, path.c_str(), &json)) {
      Serial.println("Data updated to Firebase ✅");
    } else {
      Serial.print("Firebase update failed: ");
      Serial.println(fbdo.errorReason());
    }
  }

  delay(500); // half-second delay
}