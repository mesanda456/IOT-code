#define POWER_PIN 14
#define DO_PIN 25
#define AO_PIN 26  // Connect AO from sensor here

void setup() {
  Serial.begin(9600);
  delay(1000);
  Serial.println("Rain Sensor Test with Limit");

  pinMode(POWER_PIN, OUTPUT);
  pinMode(DO_PIN, INPUT);
}

void loop() {
  digitalWrite(POWER_PIN, HIGH);
  delay(10);

  int rain_state = digitalRead(DO_PIN);
  int rain_value = analogRead(AO_PIN);  // read analog value (0-4095)

  digitalWrite(POWER_PIN, LOW);

  // Digital rain detection
  if (rain_state == HIGH)
    Serial.println("No Rain");
  else
    Serial.println("Rain Detected");

  // Rain intensity / limit
  Serial.print("Rain Level (0-4095) = ");
  Serial.println(rain_value);

  // Optional: convert to percentage
  float rain_percent = (rain_value / 4095.0) * 100;
  Serial.print("Rain Intensity = ");
  Serial.print(rain_percent);
  Serial.println("%");

  Serial.println("----------------------");
  delay(1000);
}