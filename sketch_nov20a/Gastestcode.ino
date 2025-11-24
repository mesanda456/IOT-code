#define MQ2_AO 34   // Analog output from MQ-2

void setup() {
  Serial.begin(115200);
  pinMode(MQ2_AO, INPUT);
}

void loop() {
  int gasValue = analogRead(MQ2_AO);
  Serial.print("Gas Value = ");
  Serial.println(gasValue); // 0–4095

  delay(500);
}
