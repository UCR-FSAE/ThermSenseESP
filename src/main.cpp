#include <Arduino.h>
#include <math.h>
#include "driver/twai.h"

static const gpio_num_t CAN_TX = GPIO_NUM_43;  // D6
static const gpio_num_t CAN_RX = GPIO_NUM_44;  // D7

static constexpr uint8_t ADC_PIN = GPIO_NUM_4;  // D3/A3

static constexpr uint8_t MUX_EN = GPIO_NUM_40;  // D13
static constexpr uint8_t MUX_S3 = GPIO_NUM_41;  // D14
static constexpr uint8_t MUX_S2 = GPIO_NUM_39;  // D12
static constexpr uint8_t MUX_S1 = GPIO_NUM_2;   // D1
static constexpr uint8_t MUX_S0 = GPIO_NUM_38;  // D11

static constexpr uint8_t FAULT_PIN = GPIO_NUM_6;  // D5
static constexpr float FAULT_TEMP_C = 60.0f;

static constexpr uint8_t NUM_SUBPACKS = 6;
static constexpr uint8_t TEMPS_PER_SUBPACK = 12;
static constexpr uint8_t TOTAL_TEMPS = NUM_SUBPACKS * TEMPS_PER_SUBPACK;

static constexpr uint32_t SUBPACK_TX_PERIOD_MS = 100;
static constexpr uint32_t ORION_ADDRESS_PERIOD_MS = 200;
static constexpr uint32_t ORION_SUMMARY_PERIOD_MS = 100;
static constexpr uint32_t ORION_GENERAL_PERIOD_MS = 100;
static constexpr uint32_t STALE_TEMP_MS = 2000;

static constexpr uint32_t ORION_ADDRESS_ID = 0x18EEFF80;
static constexpr uint32_t ORION_SUMMARY_ID = 0x1839F380;
static constexpr uint32_t ORION_GENERAL_ID = 0x1838F380;
static constexpr uint8_t ORION_MODULE_NUMBER = 0;
static constexpr uint8_t ORION_SOURCE_ADDRESS = 0x80;

static constexpr float SH_A = 1.1395e-3f;
static constexpr float SH_B = 2.3230e-4f;
static constexpr float SH_C = 9.5816e-8f;
static constexpr float R_FIXED = 10000.0f;
static constexpr float ADC_MAX = 4095.0f;

static uint8_t subpackId = 1;
static bool isMaster = false;
static bool canOk = false;

static int8_t localTemps[TEMPS_PER_SUBPACK];
static int8_t allTemps[TOTAL_TEMPS];
static bool tempValid[TOTAL_TEMPS];
static uint32_t tempLastSeen[TOTAL_TEMPS];

static uint32_t lastSubpackTx = 0;
static uint32_t lastOrionAddressTx = 0;
static uint32_t lastOrionSummaryTx = 0;
static uint32_t lastOrionGeneralTx = 0;
static uint8_t nextGeneralIndex = 0;

struct TempStats {
  bool valid;
  int8_t minTemp;
  int8_t maxTemp;
  int8_t avgTemp;
  uint8_t minId;
  uint8_t maxId;
  uint8_t count;
};

static bool init_can() {
  twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX, CAN_RX, TWAI_MODE_NORMAL);
  g.tx_queue_len = 25;
  g.rx_queue_len = 75;

  twai_timing_config_t t = TWAI_TIMING_CONFIG_500KBITS();
  twai_filter_config_t f = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  twai_driver_uninstall();

  esp_err_t err = twai_driver_install(&g, &t, &f);
  if (err != ESP_OK) {
    Serial.printf("twai_driver_install failed: %s\n", esp_err_to_name(err));
    canOk = false;
    return false;
  }

  err = twai_start();
  if (err != ESP_OK) {
    Serial.printf("twai_start failed: %s\n", esp_err_to_name(err));
    canOk = false;
    return false;
  }

  canOk = true;
  return true;
}

static void restart_can_if_bus_off() {
  if (!canOk) return;

  twai_status_info_t st;
  if (twai_get_status_info(&st) == ESP_OK && st.state == TWAI_STATE_BUS_OFF) {
    canOk = false;
    twai_stop();
    twai_driver_uninstall();
    delay(50);
    init_can();
  }
}

static bool send_can(const twai_message_t& msg) {
  if (!canOk) return false;

  esp_err_t err = twai_transmit(&msg, pdMS_TO_TICKS(10));
  if (err == ESP_OK) return true;

  if (err == ESP_ERR_INVALID_STATE) {
    canOk = false;
    init_can();
  }

  return false;
}

static int8_t clamp_temp(float tempC) {
  if (tempC > 127.0f) return 127;
  if (tempC < -128.0f) return -128;
  return (int8_t)lroundf(tempC);
}

static float read_temperature_c() {
  analogRead(ADC_PIN);
  delayMicroseconds(20);

  int32_t rawSum = 0;
  for (uint8_t i = 0; i < 16; i++) rawSum += analogRead(ADC_PIN);

  float adc = rawSum / 16.0f;
  if (adc <= 0.0f || adc >= ADC_MAX) return 0.0f;

  float resistance = R_FIXED * adc / (ADC_MAX - adc);
  float lnR = logf(resistance);
  float invK = SH_A + SH_B * lnR + SH_C * lnR * lnR * lnR;
  return (1.0f / invK) - 273.15f;
}

static void read_local_temps() {
  bool fault = false;

  for (uint8_t i = 0; i < TEMPS_PER_SUBPACK; i++) {
    uint8_t muxChannel = (i + 1) % TEMPS_PER_SUBPACK;  // T12 is mux channel 0

    digitalWrite(MUX_S0, muxChannel & 0x01);
    digitalWrite(MUX_S1, (muxChannel >> 1) & 0x01);
    digitalWrite(MUX_S2, (muxChannel >> 2) & 0x01);
    digitalWrite(MUX_S3, (muxChannel >> 3) & 0x01);
    delayMicroseconds(20);

    float tempC = read_temperature_c();
    if (tempC > FAULT_TEMP_C) fault = true;
    localTemps[i] = clamp_temp(tempC);
  }

  digitalWrite(FAULT_PIN, fault ? HIGH : LOW);
}

static uint8_t read_subpack_id() {
  pinMode(GPIO_NUM_11, INPUT);  // subpack 1
  pinMode(GPIO_NUM_7, INPUT);   // subpack 2
  pinMode(GPIO_NUM_12, INPUT);  // subpack 3
  pinMode(GPIO_NUM_8, INPUT);   // subpack 4
  pinMode(GPIO_NUM_13, INPUT);  // subpack 5
  pinMode(GPIO_NUM_9, INPUT);   // subpack 6

  if (digitalRead(GPIO_NUM_11) == HIGH) return 1;
  if (digitalRead(GPIO_NUM_7) == HIGH) return 2;
  if (digitalRead(GPIO_NUM_12) == HIGH) return 3;
  if (digitalRead(GPIO_NUM_8) == HIGH) return 4;
  if (digitalRead(GPIO_NUM_13) == HIGH) return 5;
  if (digitalRead(GPIO_NUM_9) == HIGH) return 6;
  return 1;
}

static void save_temps(uint8_t pack, uint8_t firstTemp, const uint8_t* data, uint8_t len) {
  if (pack < 1 || pack > NUM_SUBPACKS) return;

  uint8_t base = (pack - 1) * TEMPS_PER_SUBPACK + firstTemp;
  uint32_t now = millis();

  for (uint8_t i = 0; i < len; i++) {
    uint8_t index = base + i;
    if (index >= TOTAL_TEMPS) return;

    allTemps[index] = (int8_t)data[i];
    tempValid[index] = true;
    tempLastSeen[index] = now;
  }
}

static void save_local_master_temps() {
  uint32_t now = millis();

  for (uint8_t i = 0; i < TEMPS_PER_SUBPACK; i++) {
    allTemps[i] = localTemps[i];
    tempValid[i] = true;
    tempLastSeen[i] = now;
  }
}

static void receive_subpack_messages() {
  twai_message_t msg;

  while (twai_receive(&msg, 0) == ESP_OK) {
    if (msg.extd || msg.data_length_code < 6) continue;

    uint8_t pack = (msg.identifier >> 4) & 0x0F;
    uint8_t half = msg.identifier & 0x0F;

    if (pack < 2 || pack > NUM_SUBPACKS) continue;
    if (half == 1) save_temps(pack, 0, msg.data, 6);
    if (half == 2) save_temps(pack, 6, msg.data, 6);
  }
}

static void mark_stale_temps() {
  uint32_t now = millis();

  for (uint8_t i = 0; i < TOTAL_TEMPS; i++) {
    if (tempValid[i] && now - tempLastSeen[i] > STALE_TEMP_MS) {
      tempValid[i] = false;
    }
  }
}

static TempStats get_stats() {
  TempStats stats = {};
  int32_t sum = 0;

  for (uint8_t i = 0; i < TOTAL_TEMPS; i++) {
    if (!tempValid[i]) continue;

    int8_t temp = allTemps[i];

    if (!stats.valid) {
      stats.valid = true;
      stats.minTemp = temp;
      stats.maxTemp = temp;
      stats.minId = i;
      stats.maxId = i;
    }

    if (temp < stats.minTemp) {
      stats.minTemp = temp;
      stats.minId = i;
    }

    if (temp > stats.maxTemp) {
      stats.maxTemp = temp;
      stats.maxId = i;
    }

    sum += temp;
    stats.count++;
  }

  if (stats.valid) {
    stats.avgTemp = (int8_t)lroundf((float)sum / stats.count);
  }

  return stats;
}

static uint8_t orion_summary_checksum(const uint8_t data[8]) {
  uint16_t sum = 0;
  for (uint8_t i = 0; i < 7; i++) sum += data[i];
  sum += 0x39;
  sum += 8;
  return (uint8_t)sum;
}

static void send_subpack_temps() {
  twai_message_t msg = {};
  msg.extd = 0;
  msg.data_length_code = 6;

  msg.identifier = (subpackId << 4) | 0x01;
  for (uint8_t i = 0; i < 6; i++) msg.data[i] = (uint8_t)localTemps[i];
  send_can(msg);

  msg.identifier = (subpackId << 4) | 0x02;
  for (uint8_t i = 0; i < 6; i++) msg.data[i] = (uint8_t)localTemps[i + 6];
  send_can(msg);
}

static void send_orion_address_claim() {
  twai_message_t msg = {};
  msg.identifier = ORION_ADDRESS_ID;
  msg.extd = 1;
  msg.data_length_code = 8;
  msg.data[0] = 0xF3;
  msg.data[1] = 0x00;
  msg.data[2] = ORION_SOURCE_ADDRESS;
  msg.data[3] = 0xF3;
  msg.data[4] = ORION_MODULE_NUMBER << 3;
  msg.data[5] = 0x40;
  msg.data[6] = 0x1E;
  msg.data[7] = 0x90;
  send_can(msg);
}

static void send_orion_summary(const TempStats& stats) {
  if (!stats.valid) return;

  twai_message_t msg = {};
  msg.identifier = ORION_SUMMARY_ID;
  msg.extd = 1;
  msg.data_length_code = 8;
  msg.data[0] = ORION_MODULE_NUMBER;
  msg.data[1] = (uint8_t)stats.minTemp;
  msg.data[2] = (uint8_t)stats.maxTemp;
  msg.data[3] = (uint8_t)stats.avgTemp;
  msg.data[4] = TOTAL_TEMPS;
  msg.data[5] = stats.maxId;
  msg.data[6] = stats.minId;
  msg.data[7] = orion_summary_checksum(msg.data);
  send_can(msg);
}

static void send_orion_general(const TempStats& stats) {
  if (!stats.valid) return;

  for (uint8_t tries = 0; tries < TOTAL_TEMPS; tries++) {
    uint8_t id = nextGeneralIndex++;
    if (nextGeneralIndex >= TOTAL_TEMPS) nextGeneralIndex = 0;
    if (!tempValid[id]) continue;

    twai_message_t msg = {};
    msg.identifier = ORION_GENERAL_ID;
    msg.extd = 1;
    msg.data_length_code = 8;
    msg.data[0] = id;
    msg.data[1] = (uint8_t)allTemps[id];
    msg.data[2] = id;
    msg.data[3] = (uint8_t)stats.minTemp;
    msg.data[4] = (uint8_t)stats.maxTemp;
    msg.data[5] = stats.maxId;
    msg.data[6] = stats.minId;
    msg.data[7] = 0x80;
    send_can(msg);
    return;
  }
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(MUX_EN, OUTPUT);
  pinMode(MUX_S0, OUTPUT);
  pinMode(MUX_S1, OUTPUT);
  pinMode(MUX_S2, OUTPUT);
  pinMode(MUX_S3, OUTPUT);
  pinMode(FAULT_PIN, OUTPUT_OPEN_DRAIN);

  digitalWrite(MUX_EN, LOW);
  digitalWrite(FAULT_PIN, LOW);

  Serial.begin(115200);
  delay(500);

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  subpackId = read_subpack_id();
  isMaster = subpackId == 1;

  for (uint8_t i = 0; i < TOTAL_TEMPS; i++) {
    tempValid[i] = false;
    tempLastSeen[i] = 0;
  }

  if (!init_can()) {
    while (true) delay(500);
  }

  Serial.printf("Subpack %u, mode: %s\n", subpackId, isMaster ? "master to Orion" : "sender to master");
}

void loop() {
  digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  restart_can_if_bus_off();
  read_local_temps();

  uint32_t now = millis();

  if (!isMaster) {
    if (now - lastSubpackTx >= SUBPACK_TX_PERIOD_MS) {
      lastSubpackTx = now;
      send_subpack_temps();
    }
    return;
  }

  save_local_master_temps();
  receive_subpack_messages();
  mark_stale_temps();

  TempStats stats = get_stats();

  if (now - lastOrionAddressTx >= ORION_ADDRESS_PERIOD_MS) {
    lastOrionAddressTx = now;
    send_orion_address_claim();
  }

  if (now - lastOrionSummaryTx >= ORION_SUMMARY_PERIOD_MS) {
    lastOrionSummaryTx = now;
    send_orion_summary(stats);
  }

  if (now - lastOrionGeneralTx >= ORION_GENERAL_PERIOD_MS) {
    lastOrionGeneralTx = now;
    send_orion_general(stats);
  }
}
