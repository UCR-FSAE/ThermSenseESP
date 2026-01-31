#include "ESP32_CAN.h"
#include <math.h>

// CAN messages
static CAN_message_t CAN_outMsg1;
static CAN_message_t CAN_outMsg2;

// ESP32-S3 pins
#define ADC_PIN      1      // ADC1_CH0
#define LED_PIN      LED_BUILTIN
#define SEND_DELAY   100    // ms

const char subpackID = 'S1';

float adcValue = 0;
float temperature = 0;

ESP32_CAN Can;

void setup() {
  pinMode(LED_PIN, OUTPUT);
  Serial.begin(115200);
  delay(500);

  Serial.println("Starting ESP32-S3 Thermistor CAN node...");

  if (!Can.begin()) {
    Serial.println("CAN init failed!");
    while (1);
  }

  switch (subpackID) {
    case 'S1': CAN_outMsg1.id = 0x11; CAN_outMsg2.id = 0x12; break;
    case 'S2': CAN_outMsg1.id = 0x21; CAN_outMsg2.id = 0x22; break;
    case 'S3': CAN_outMsg1.id = 0x31; CAN_outMsg2.id = 0x32; break;
    case 'S4': CAN_outMsg1.id = 0x41; CAN_outMsg2.id = 0x42; break;
    case 'S5': CAN_outMsg1.id = 0x51; CAN_outMsg2.id = 0x52; break;
    case 'S6': CAN_outMsg1.id = 0x61; CAN_outMsg2.id = 0x62; break;
  }

  CAN_outMsg1.len = CAN_outMsg2.len = 6;

  Serial.println("Setup complete. Sending thermistor data over CAN.");
}

void loop() {
  digitalWrite(LED_PIN, !digitalRead(LED_PIN));

  adcValue = analogRead(ADC_PIN) * (3.3 / 4095.0);

  temperature =
      (1.0 / ((1.0 / 298.15) +
      (1.0 / 3435.0) *
      log(((10000.0 * (3.3 / adcValue)) - 10000.0) / 10000.0))) - 273.15;

  for (int i = 0; i < 6; i++) {
    CAN_outMsg1.buf[i] = (uint8_t)temperature;
  }

  Serial.print("ADC Voltage: ");
  Serial.println(adcValue, 3);
  Serial.print("Temperature (C): ");
  Serial.println(temperature, 2);

  Can.write(CAN_outMsg1);
  delay(SEND_DELAY);
}
