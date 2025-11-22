#include <ESP32Servo.h>

Servo servo;

// ----- Servo Pins & Values -----
const int servoPin = 13;
int STOP_VAL = 90;         // stop position
int FORWARD_SLOW = 80;     // slow forward
int BACKWARD_SLOW = 100;   // slow backward

// ----- Flame Sensor & Buzzer -----
const int flameDigital = 34;  // D0 pin
const int flameAnalog  = 35;  // A0 pin
const int buzzerPin    = 14;  // buzzer pin

void setup() {
  Serial.begin(115200);

  // Servo
  servo.attach(servoPin);

  // Flame sensor & buzzer
  pinMode(flameDigital, INPUT);
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, LOW); // buzzer off
}

void loop() {

  int flameStatus = digitalRead(flameDigital); // 0 = fire detected
  int flameValue = analogRead(flameAnalog);    // intensity (0–4095)

  Serial.print("Analog Value: ");
  Serial.println(flameValue);

  // ---------------------------------------------------------
  // 🔥 FIRE DETECTED → STOP SERVO + BUZZER ON
  // ---------------------------------------------------------
  if (flameStatus == 0) {
    Serial.println("🔥 FLAME DETECTED! STOPPING SERVO!");
    
    servo.write(STOP_VAL);        // stop servo
    digitalWrite(buzzerPin, HIGH); // turn buzzer ON
    
    delay(200); // small delay only
    return;     // exit loop → prevent servo from rotating
  }

  // ---------------------------------------------------------
  // ✅ NO FIRE → RUN SERVO + BUZZER OFF
  // ---------------------------------------------------------
  digitalWrite(buzzerPin, LOW);  // buzzer off
  Serial.println("No Flame — Servo Running");

  // ----- Rotate Forward (like 0 → 360) -----
  servo.write(FORWARD_SLOW);
  delay(2000);

  // Stop
  servo.write(STOP_VAL);
  delay(1000);

  // ----- Rotate Backward (360 → 0) -----
  servo.write(BACKWARD_SLOW);
  delay(2000);

  // Stop
  servo.write(STOP_VAL);
  delay(1000);
}
