#include <Arduino.h>

#define LED_PIN 21   // XIAO ESP32-S3 onboard LED

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("ESP32-S3 is alive!");

  pinMode(LED_PIN, OUTPUT);
}

void loop() {
  digitalWrite(LED_PIN, HIGH);
  Serial.println("LED ON");
  delay(500);

  digitalWrite(LED_PIN, LOW);
  Serial.println("LED OFF");
  delay(500);
}
