#include <Arduino.h>
#include "ESP32_CAN.h"
#include <math.h>

// CAN messages
static CAN_message_t CAN_outMsg1;
static CAN_message_t CAN_outMsg2;

// ESP32-S3 pins
#define ADC_PIN      A0

#define LED_PIN      LED_BUILTIN
#define SEND_DELAY   1000    // ms

// Steinhart-Hart coeefficients
// new ones
#define SH_A 1.1395e-3f
#define SH_B 2.3230e-4f
#define SH_C 9.5816e-8f
// old ones
// #define SH_A 8.7741e-4f
// #define SH_B 2.5323e-4f
// #define SH_C 1.8451e-7f
#define R_FIXED 10000.0f
#define ADC_MAX 4095.0f

const int subpackID = 1;

float adcValue = 0;
float temperature = 0;

ESP32_CAN Can;

void setup() {
  pinMode(LED_PIN, OUTPUT);
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

  // reduce noise by taking average
  int32_t raw =0;
  for (int i=0;i<16;i++) {
    raw += analogRead(ADC_PIN);
  }
  adcValue = raw/16.0f;

  if (adcValue<=0 || adcValue>=ADC_MAX) {
    Serial.println("Error: thermistor open or short circuit!");
    delay(SEND_DELAY);
    return;
  }

  float R = R_FIXED *adcValue/(ADC_MAX - adcValue);
  float lnR = logf(R);
  float inv_T = SH_A + SH_B*lnR + SH_C*lnR*lnR*lnR;
  temperature = 1.0f/inv_T - 273.15f;

  for (int i = 0; i < 6; i++) {
    CAN_outMsg1.buf[i] = (uint8_t)temperature;
  }

  Serial.print("ADC Raw: ");
  Serial.print(adcValue, 0);
  Serial.print("  |  R: ");
  Serial.print(R, 1);
  Serial.print(" ohm  |  Temp: ");
  Serial.print(temperature, 2);
  Serial.println(" C");

  Can.write(CAN_outMsg1);
  delay(SEND_DELAY);
}

