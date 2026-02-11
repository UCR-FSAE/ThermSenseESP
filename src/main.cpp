#include <Arduino.h>
#include "ESP32_CAN.h"
#include <math.h>

// CAN messages
static CAN_message_t CAN_outMsg1;
static CAN_message_t CAN_outMsg2;

// ESP32-S3 pins
#define ADC_PIN      A0

#define MUX_ENBLE   D8
#define MUX_S3      D5
#define MUX_S2      D4
#define MUX_S1      D3
#define MUX_S0      D2

#define LED_PIN      LED_BUILTIN
#define SEND_DELAY   1000    // ms

const int subpackID = 1;

float adcValue = 0;
float temperature = 0;

ESP32_CAN Can;

void setup() {
  pinMode(LED_PIN, OUTPUT);
  pinMode(MUX_ENBLE, OUTPUT);
  pinMode(MUX_S3, OUTPUT);
  pinMode(MUX_S2, OUTPUT);
  pinMode(MUX_S1, OUTPUT);
  pinMode(MUX_S0, OUTPUT);
  digitalWrite(MUX_ENBLE, LOW);  // Enables the multiplexer
  digitalWrite(MUX_S3, LOW);
  digitalWrite(MUX_S2, LOW);
  digitalWrite(MUX_S1, LOW);
  digitalWrite(MUX_S0, LOW);
  Serial.begin(115200);
  delay(500);
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  Serial.println("Starting ESP32-S3 Thermistor CAN node...");

  if (!Can.begin()) {
    Serial.println("CAN init failed!");
    while (1);
  }

  switch (subpackID) {
    case 1: CAN_outMsg1.id = 0x11; CAN_outMsg2.id = 0x12; break;
    case 2: CAN_outMsg1.id = 0x21; CAN_outMsg2.id = 0x22; break;
    case 3: CAN_outMsg1.id = 0x31; CAN_outMsg2.id = 0x32; break;
    case 4: CAN_outMsg1.id = 0x41; CAN_outMsg2.id = 0x42; break;
    case 5: CAN_outMsg1.id = 0x51; CAN_outMsg2.id = 0x52; break;
    case 6: CAN_outMsg1.id = 0x61; CAN_outMsg2.id = 0x62; break;
  }

  CAN_outMsg1.len = CAN_outMsg2.len = 6;

  Serial.println("Setup complete. Sending thermistor data over CAN.");
}

void loop() {
  digitalWrite(LED_PIN, !digitalRead(LED_PIN));

  adcValue = analogRead(ADC_PIN) * (3.3 / 4095.0);

  // for (int i = 0; i < 6; i++) {  
    temperature =
      (1.0 / ((1.0 / 298.15) +
      (1.0 / 3435.0) *
      log(((10000.0 * (3.3 / adcValue)) - 10000.0) / 10000.0))) - 273.15;

  //   CAN_outMsg1.buf[i] = (uint8_t)temperature;
  // }
  CAN_outMsg1.buf[0] = (uint8_t)temperature; 

  // Serial.print("ADC Voltage: ");
  // Serial.println(adcValue, 3);
  Serial.print("Temperature (C): ");
  Serial.println(temperature, 2);
  Serial.print("Raw ADC: ");
  Serial.println(adcValue, 3);

  Can.write(CAN_outMsg1);
  delay(SEND_DELAY);
}

