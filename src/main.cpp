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
  pinMode(MUX_ENBLE, OUTPUT);
  pinMode(MUX_S3, OUTPUT);
  pinMode(MUX_S2, OUTPUT);
  pinMode(MUX_S1, OUTPUT);
  pinMode(MUX_S0, OUTPUT);
  digitalWrite(MUX_ENBLE, LOW);  // Enables the multiplexer
  // digitalWrite(MUX_S3, HIGH);
  // digitalWrite(MUX_S2, LOW);
  // digitalWrite(MUX_S1, HIGH);
  // digitalWrite(MUX_S0, HIGH);
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
  uint8_t temp[12] = {0};

  for (int i=0; i<12;i++) {
    int channel = (i+1)%12;  // T12 has binary b0000, others are fine
    // selecting the mux channel
    digitalWrite(MUX_S0, channel%2);
    digitalWrite(MUX_S1, (channel/2)%2);
    digitalWrite(MUX_S2, (channel/4)%2);
    digitalWrite(MUX_S3, (channel/8)%2);

    // this delay is needed for the mux to settle after switching channels, otherwise the ADC reading will be unstable and inaccurate
    // there might be channel bleeding
    // without this, the channel numbers were mismatching
    delayMicroseconds(20);
    
    // // discard the first reading after switching channels, it is usually inaccurate due to the residual charge in the ADC sample and hold capacitor from the previous channel
    analogRead(ADC_PIN);
    delayMicroseconds(20);
    // reduce noise by taking average
    int32_t raw =0;
    for (int j=0;j<16;j++) {
      raw += analogRead(ADC_PIN);
    }
    adcValue = raw/16.0f;

    if (adcValue<=0 || adcValue>=ADC_MAX) {
      Serial.println("Error: thermistor open or short circuit!");
      delay(SEND_DELAY);
      temp[i] = 0;
      continue;
    }

    float R = R_FIXED *adcValue/(ADC_MAX - adcValue);
    float lnR = logf(R);
    float inv_T = SH_A + SH_B*lnR + SH_C*lnR*lnR*lnR;
    temperature = 1.0f/inv_T - 273.15f;
    temp[i] = (uint8_t)(temperature);

    Serial.print("Channel: ");
    Serial.print(i+1);
    Serial.print(" | Temp: ");
    Serial.print(temperature, 2);
    Serial.println(" C");
    delay (100);
  }

  //   CAN_outMsg1.buf[i] = (uint8_t)temperature;
  // }
  // CAN_outMsg1.buf[0] = (uint8_t)temperature; 

  // Serial.print("ADC Raw: ");
  // Serial.print(adcValue, 0);
  // Serial.print("  |  R: ");
  // Serial.print(R, 1);
  // Serial.print(" ohm  |  Temp: ");
  // Serial.print(temperature, 2);
  // Serial.println(" C");

  // Can.write(CAN_outMsg1);

  for (int i = 0; i < 6; i++) {
      CAN_outMsg1.buf[i] = temp[i];
      CAN_outMsg2.buf[i] = temp[i + 6];
  }

  Serial.println(Can.write(CAN_outMsg1) ? "MSG1 sent" : "MSG1 FAILED");
  Serial.println(Can.write(CAN_outMsg2) ? "MSG2 sent" : "MSG2 FAILED");

  Serial.print("MSG1 (0x");
  Serial.print(CAN_outMsg1.id, HEX);
  Serial.print("): ");
  for (int i = 0; i < CAN_outMsg1.len; i++) {
      Serial.print(CAN_outMsg1.buf[i]);
      Serial.print(" ");
  }
  Serial.println();

  Serial.print("MSG2 (0x");
  Serial.print(CAN_outMsg2.id, HEX);
  Serial.print("): ");
  for (int i = 0; i < CAN_outMsg2.len; i++) {
      Serial.print(CAN_outMsg2.buf[i]);
      Serial.print(" ");
  }
  Serial.println();
  delay(SEND_DELAY);
  
}

