#include <Arduino.h>
#include <math.h>
#include "driver/twai.h"

static const gpio_num_t CAN_TX = GPIO_NUM_4;  // D3
static const gpio_num_t CAN_RX = GPIO_NUM_5;  // D4

static constexpr uint8_t ADC_PIN = GPIO_NUM_8;  // D9 or A9

static constexpr uint8_t MUX_ENBLE = GPIO_NUM_40;  // D13
static constexpr uint8_t MUX_S3 = GPIO_NUM_3;  // D2
static constexpr uint8_t MUX_S2 = GPIO_NUM_39;  // D12
static constexpr uint8_t MUX_S1 = GPIO_NUM_2;  // D1
static constexpr uint8_t MUX_S0 = GPIO_NUM_38;  // D11

static constexpr uint8_t FAULT_PIN = GPIO_NUM_6;  // D5
static constexpr float FAULT_TEMP = 60.0f;

static constexpr uint32_t SEND_DELAY_MS = 5;

static constexpr float SH_A = 1.1395e-3f;
static constexpr float SH_B = 2.3230e-4f;
static constexpr float SH_C = 9.5816e-8f;
static constexpr float R_FIXED = 10000.0f;
static constexpr float ADC_MAX = 4095.0f;

static const int subpackID = 1;

static twai_message_t CAN_outMsg1;
static twai_message_t CAN_outMsg2;
static float adcValue = 0;
static float temperature = 0;

static bool twai_initialized = false;

static bool init_can() {
  twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX, CAN_RX, TWAI_MODE_NO_ACK);
  g.tx_queue_len = 25;
  g.rx_queue_len = 25;

  twai_timing_config_t t = TWAI_TIMING_CONFIG_500KBITS();
  twai_filter_config_t f = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  twai_driver_uninstall();

  esp_err_t err = twai_driver_install(&g, &t, &f);
  Serial.printf("twai_driver_install: %s\n", esp_err_to_name(err));
  if (err != ESP_OK) return false;

  err = twai_start();
  Serial.printf("twai_start: %s\n", esp_err_to_name(err));
  if (err != ESP_OK) return false;

  twai_initialized = true;

  return true;
}

static float read_temperature() {
  analogRead(ADC_PIN);
  delayMicroseconds(20);

  int32_t raw = 0;
  for (int j = 0; j < 16; j++) raw += analogRead(ADC_PIN);
  adcValue = raw / 16.0f;

  if (adcValue <= 0 || adcValue >= ADC_MAX) return 0.0f;

  float R = R_FIXED * adcValue / (ADC_MAX - adcValue);
  float lnR = logf(R);
  float inv_T = SH_A + SH_B * lnR + SH_C * lnR * lnR * lnR;
  return 1.0f / inv_T - 273.15f;
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(MUX_ENBLE, OUTPUT);
  pinMode(MUX_S3, OUTPUT);
  pinMode(MUX_S2, OUTPUT);
  pinMode(MUX_S1, OUTPUT);
  pinMode(MUX_S0, OUTPUT);
  pinMode(FAULT_PIN, OUTPUT_OPEN_DRAIN);
  digitalWrite(FAULT_PIN, LOW);
  digitalWrite(MUX_ENBLE, LOW);

  Serial.begin(115200);
  delay(500);
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  if (!init_can()) {
    Serial.println("CAN init failed!");
    while (1) {}
  }

  memset(&CAN_outMsg1, 0, sizeof(CAN_outMsg1));
  memset(&CAN_outMsg2, 0, sizeof(CAN_outMsg2));

  switch (subpackID) {
    case 1: CAN_outMsg1.identifier = 0x11; CAN_outMsg2.identifier = 0x12; break;
    case 2: CAN_outMsg1.identifier = 0x21; CAN_outMsg2.identifier = 0x22; break;
    case 3: CAN_outMsg1.identifier = 0x31; CAN_outMsg2.identifier = 0x32; break;
    case 4: CAN_outMsg1.identifier = 0x41; CAN_outMsg2.identifier = 0x42; break;
    case 5: CAN_outMsg1.identifier = 0x51; CAN_outMsg2.identifier = 0x52; break;
    case 6: CAN_outMsg1.identifier = 0x61; CAN_outMsg2.identifier = 0x62; break;
  }
  CAN_outMsg1.data_length_code = 6;
  CAN_outMsg2.data_length_code = 6;
}

void loop() {
  digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));

  twai_status_info_t st;
  if (twai_get_status_info(&st) == ESP_OK && st.state == TWAI_STATE_BUS_OFF) {
    twai_stop();
    twai_driver_uninstall();
    delay(50);
    init_can();
    delay(10);
    return;
  }

  uint8_t temp[12] = {0};
  for (int i = 0; i < 12; i++) {
    int channel = (i + 1) % 12;  // T12 has binary b0000, others are fine
    // selecting the mux channel
    digitalWrite(MUX_S0, channel % 2);
    digitalWrite(MUX_S1, (channel / 2) % 2);
    digitalWrite(MUX_S2, (channel / 4) % 2);
    digitalWrite(MUX_S3, (channel / 8) % 2);
    delayMicroseconds(20);

    temperature = read_temperature();
    if (temperature > FAULT_TEMP) digitalWrite(FAULT_PIN, HIGH);
    temp[i] = (uint8_t)temperature;
  }

  for (int i = 0; i < 6; i++) {
    CAN_outMsg1.data[i] = temp[i];
    CAN_outMsg2.data[i] = temp[i + 6];
  }
  
  if(twai_initialized) {
    esp_err_t err1 = twai_transmit(&CAN_outMsg1, pdMS_TO_TICKS(10));
    esp_err_t err2 = twai_transmit(&CAN_outMsg2, pdMS_TO_TICKS(10));
    // twai_status_info_t st;
    // if (twai_get_status_info(&st) == ESP_OK && st.state == TWAI_STATE_BUS_OFF) {
    if(err1 == ESP_ERR_INVALID_STATE || err2 == ESP_ERR_INVALID_STATE) {
      Serial.println("Bus off detected before transmission, resetting TWAI...");
      twai_initialized = false;
      twai_stop();
      twai_driver_uninstall();
      delay(10);
      init_can();
    } else {
      Serial.println(err1 == ESP_OK ? "MSG1 sent" : "MSG1 FAILED");
      Serial.println(err2 == ESP_OK ? "MSG2 sent" : "MSG2 FAILED");
    }
  }
  // esp_err_t err1 = twai_transmit((twai_message_t*)&CAN_outMsg1, pdMS_TO_TICKS(10));
  // esp_err_t err2 = twai_transmit((twai_message_t*)&CAN_outMsg2, pdMS_TO_TICKS(10));

  

  Serial.print("MSG1 (0x");
  Serial.print(CAN_outMsg1.identifier, HEX);
  Serial.print("): ");
  for (int i = 0; i < CAN_outMsg1.data_length_code; i++) {
      Serial.print(CAN_outMsg1.data[i]);
      Serial.print(" ");
  }
  Serial.println();

  Serial.print("MSG2 (0x");
  Serial.print(CAN_outMsg2.identifier, HEX);
  Serial.print("): ");
  for (int i = 0; i < CAN_outMsg2.data_length_code; i++) {
      Serial.print(CAN_outMsg2.data[i]);
      Serial.print(" ");
  }
  Serial.println();

  delay(SEND_DELAY_MS);
}
