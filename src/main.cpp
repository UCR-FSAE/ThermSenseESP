#include <Arduino.h>
#include <math.h>
#include "driver/twai.h"

static const gpio_num_t CAN_TX = GPIO_NUM_43;  // D6
static const gpio_num_t CAN_RX = GPIO_NUM_44;  // D7

static constexpr uint8_t ADC_PIN = GPIO_NUM_4;  // D3 or A3

static constexpr uint8_t MUX_ENBLE = GPIO_NUM_40;  // D13
<<<<<<< ours
static constexpr uint8_t MUX_S3 = GPIO_NUM_41;  // D14
static constexpr uint8_t MUX_S2 = GPIO_NUM_39;  // D12
static constexpr uint8_t MUX_S1 = GPIO_NUM_2;  // D1
static constexpr uint8_t MUX_S0 = GPIO_NUM_38;  // D11
=======
static constexpr uint8_t MUX_S3 = GPIO_NUM_41;     // D14
static constexpr uint8_t MUX_S2 = GPIO_NUM_39;     // D12
static constexpr uint8_t MUX_S1 = GPIO_NUM_2;      // D1
static constexpr uint8_t MUX_S0 = GPIO_NUM_38;     // D11
>>>>>>> theirs

static constexpr uint8_t FAULT_PIN = GPIO_NUM_6;  // D5
static constexpr float FAULT_TEMP = 60.0f;

<<<<<<< ours
static constexpr uint32_t SEND_DELAY_MS = 5;
=======
static constexpr uint8_t NUM_SUBPACKS = 6;
static constexpr uint8_t THERMISTORS_PER_SUBPACK = 12;
static constexpr uint8_t TOTAL_THERMISTORS = NUM_SUBPACKS * THERMISTORS_PER_SUBPACK;

static constexpr uint32_t SUBPACK_SEND_PERIOD_MS = 100;
static constexpr uint32_t ORION_ADDR_CLAIM_PERIOD_MS = 200;
static constexpr uint32_t ORION_SUMMARY_PERIOD_MS = 100;
static constexpr uint32_t ORION_GENERAL_PERIOD_MS = 100;
static constexpr uint32_t DEBUG_PRINT_PERIOD_MS = 1000;
static constexpr uint32_t THERMISTOR_STALE_MS = 2000;

static constexpr uint8_t ORION_MODULE_NUMBER = 0x00;
static constexpr uint8_t ORION_SOURCE_ADDRESS = 0x80;
static constexpr uint32_t ORION_ADDR_CLAIM_ID = 0x18EEFF80;
static constexpr uint32_t ORION_SUMMARY_ID = 0x1839F380;
static constexpr uint32_t ORION_GENERAL_ID = 0x1838F380;
>>>>>>> theirs

static constexpr float SH_A = 1.1395e-3f;
static constexpr float SH_B = 2.3230e-4f;
static constexpr float SH_C = 9.5816e-8f;
static constexpr float R_FIXED = 10000.0f;
static constexpr float ADC_MAX = 4095.0f;

<<<<<<< ours
static int subpackID = 1;

static twai_message_t CAN_outMsg1;
static twai_message_t CAN_outMsg2;
static float adcValue = 0;
static float temperature = 0;

static bool twai_initialized = false;

static bool init_can() {
  twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX, CAN_RX, TWAI_MODE_NO_ACK);
  g.tx_queue_len = 25;
  g.rx_queue_len = 25;
=======
static uint8_t subpackID = 1;
static bool isMaster = false;
static bool twai_initialized = false;

static int8_t localTemps[THERMISTORS_PER_SUBPACK];
static int8_t thermistorTemps[TOTAL_THERMISTORS];
static bool thermistorValid[TOTAL_THERMISTORS];
static uint32_t thermistorLastSeen[TOTAL_THERMISTORS];

static uint32_t lastSubpackTx = 0;
static uint32_t lastAddrClaimTx = 0;
static uint32_t lastSummaryTx = 0;
static uint32_t lastGeneralTx = 0;
static uint32_t lastDebugPrint = 0;
static uint8_t nextGeneralThermistorIndex = 0;

struct TempStats {
  bool valid;
  int8_t lowTemp;
  int8_t highTemp;
  int8_t avgTemp;
  uint8_t lowId;
  uint8_t highId;
  uint8_t count;
};

static bool init_can() {
  twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX, CAN_RX, TWAI_MODE_NO_ACK);
  g.tx_queue_len = 25;
  g.rx_queue_len = 75;
>>>>>>> theirs

  twai_timing_config_t t = TWAI_TIMING_CONFIG_500KBITS();
  twai_filter_config_t f = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  twai_driver_uninstall();

  esp_err_t err = twai_driver_install(&g, &t, &f);
  Serial.printf("twai_driver_install: %s\n", esp_err_to_name(err));
<<<<<<< ours
  if (err != ESP_OK) return false;

  err = twai_start();
  Serial.printf("twai_start: %s\n", esp_err_to_name(err));
  if (err != ESP_OK) return false;

  twai_initialized = true;

  return true;
}

=======
  if (err != ESP_OK) {
    twai_initialized = false;
    return false;
  }

  err = twai_start();
  Serial.printf("twai_start: %s\n", esp_err_to_name(err));
  if (err != ESP_OK) {
    twai_initialized = false;
    return false;
  }

  twai_initialized = true;
  return true;
}

static void restart_can() {
  twai_initialized = false;
  twai_stop();
  twai_driver_uninstall();
  delay(50);
  init_can();
}

static void check_bus_off() {
  if (!twai_initialized) return;

  twai_status_info_t st;
  if (twai_get_status_info(&st) == ESP_OK && st.state == TWAI_STATE_BUS_OFF) {
    Serial.println("CAN bus-off, restarting TWAI");
    restart_can();
  }
}

static bool transmit_msg(const twai_message_t& msg, const char* label) {
  if (!twai_initialized) return false;

  esp_err_t err = twai_transmit(&msg, pdMS_TO_TICKS(10));
  if (err == ESP_OK) return true;

  Serial.printf("%s TX failed: %s\n", label, esp_err_to_name(err));
  if (err == ESP_ERR_INVALID_STATE) restart_can();
  return false;
}

>>>>>>> theirs
static float read_temperature() {
  analogRead(ADC_PIN);
  delayMicroseconds(20);

  int32_t raw = 0;
  for (int j = 0; j < 16; j++) raw += analogRead(ADC_PIN);
<<<<<<< ours
  adcValue = raw / 16.0f;

  if (adcValue <= 0 || adcValue >= ADC_MAX) return 0.0f;

  float R = R_FIXED * adcValue / (ADC_MAX - adcValue);
  float lnR = logf(R);
  float inv_T = SH_A + SH_B * lnR + SH_C * lnR * lnR * lnR;
  return 1.0f / inv_T - 273.15f;
=======

  float adcValue = raw / 16.0f;
  if (adcValue <= 0 || adcValue >= ADC_MAX) return 0.0f;

  float rThermistor = R_FIXED * adcValue / (ADC_MAX - adcValue);
  float lnR = logf(rThermistor);
  float invT = SH_A + SH_B * lnR + SH_C * lnR * lnR * lnR;
  return 1.0f / invT - 273.15f;
}

static int8_t clamp_temp(float tempC) {
  if (tempC > 127.0f) return 127;
  if (tempC < -128.0f) return -128;
  return (int8_t)lroundf(tempC);
}

static void read_local_thermistors() {
  bool fault = false;

  for (uint8_t i = 0; i < THERMISTORS_PER_SUBPACK; i++) {
    uint8_t channel = (i + 1) % THERMISTORS_PER_SUBPACK;  // T12 is mux channel 0

    digitalWrite(MUX_S0, channel & 0x01);
    digitalWrite(MUX_S1, (channel >> 1) & 0x01);
    digitalWrite(MUX_S2, (channel >> 2) & 0x01);
    digitalWrite(MUX_S3, (channel >> 3) & 0x01);
    delayMicroseconds(20);

    float tempC = read_temperature();
    if (tempC > FAULT_TEMP) fault = true;
    localTemps[i] = clamp_temp(tempC);
  }

  digitalWrite(FAULT_PIN, fault ? HIGH : LOW);
}

static void store_subpack_temps(uint8_t subpack, const int8_t temps[THERMISTORS_PER_SUBPACK]) {
  if (subpack < 1 || subpack > NUM_SUBPACKS) return;

  uint8_t base = (subpack - 1) * THERMISTORS_PER_SUBPACK;
  uint32_t now = millis();

  for (uint8_t i = 0; i < THERMISTORS_PER_SUBPACK; i++) {
    uint8_t idx = base + i;
    thermistorTemps[idx] = temps[i];
    thermistorValid[idx] = true;
    thermistorLastSeen[idx] = now;
  }
}

static void clear_thermistor_data() {
  for (uint8_t i = 0; i < TOTAL_THERMISTORS; i++) {
    thermistorTemps[i] = 0;
    thermistorValid[i] = false;
    thermistorLastSeen[i] = 0;
  }
}

static void mark_stale_thermistors() {
  uint32_t now = millis();

  for (uint8_t i = 0; i < TOTAL_THERMISTORS; i++) {
    if (thermistorValid[i] && now - thermistorLastSeen[i] > THERMISTOR_STALE_MS) {
      thermistorValid[i] = false;
    }
  }
}

static bool is_subpack_temp_id(uint32_t id) {
  uint8_t subpack = (id >> 4) & 0x0F;
  uint8_t half = id & 0x0F;
  return subpack >= 1 && subpack <= NUM_SUBPACKS && (half == 1 || half == 2);
}

static void parse_subpack_temp_message(const twai_message_t& msg) {
  if (msg.extd || msg.data_length_code < 6 || !is_subpack_temp_id(msg.identifier)) return;

  uint8_t subpack = (msg.identifier >> 4) & 0x0F;
  if (subpack == 1) return;  // master gets subpack 1 from its own ADCs

  uint8_t half = msg.identifier & 0x0F;
  uint8_t baseInSubpack = (half == 1) ? 0 : 6;
  uint8_t globalBase = (subpack - 1) * THERMISTORS_PER_SUBPACK + baseInSubpack;
  uint32_t now = millis();

  for (uint8_t i = 0; i < 6; i++) {
    uint8_t idx = globalBase + i;
    thermistorTemps[idx] = (int8_t)msg.data[i];
    thermistorValid[idx] = true;
    thermistorLastSeen[idx] = now;
  }
}

static void receive_can_messages() {
  twai_message_t rxMsg;
  while (twai_receive(&rxMsg, 0) == ESP_OK) {
    parse_subpack_temp_message(rxMsg);
  }
}

static void send_subpack_frames() {
  twai_message_t msg;
  memset(&msg, 0, sizeof(msg));
  msg.extd = 0;
  msg.data_length_code = 6;

  msg.identifier = (subpackID << 4) | 0x01;
  for (uint8_t i = 0; i < 6; i++) msg.data[i] = (uint8_t)localTemps[i];
  transmit_msg(msg, "subpack temps 0-5");

  msg.identifier = (subpackID << 4) | 0x02;
  for (uint8_t i = 0; i < 6; i++) msg.data[i] = (uint8_t)localTemps[i + 6];
  transmit_msg(msg, "subpack temps 6-11");
}

static TempStats compute_temperature_stats() {
  TempStats stats = {};
  int32_t sum = 0;

  for (uint8_t i = 0; i < TOTAL_THERMISTORS; i++) {
    if (!thermistorValid[i]) continue;

    int8_t temp = thermistorTemps[i];
    if (!stats.valid) {
      stats.valid = true;
      stats.lowTemp = temp;
      stats.highTemp = temp;
      stats.lowId = i;
      stats.highId = i;
    } else {
      if (temp < stats.lowTemp) {
        stats.lowTemp = temp;
        stats.lowId = i;
      }
      if (temp > stats.highTemp) {
        stats.highTemp = temp;
        stats.highId = i;
      }
    }

    sum += temp;
    stats.count++;
  }

  if (stats.valid && stats.count > 0) {
    stats.avgTemp = (int8_t)lroundf((float)sum / stats.count);
  }

  return stats;
}

static uint8_t checksum_orion_summary(const uint8_t data[8]) {
  uint16_t sum = 0;
  for (uint8_t i = 0; i < 7; i++) sum += data[i];
  sum += 0x39;
  sum += 8;
  return (uint8_t)(sum & 0xFF);
}

static void send_orion_address_claim() {
  twai_message_t msg;
  memset(&msg, 0, sizeof(msg));

  msg.identifier = ORION_ADDR_CLAIM_ID;
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

  transmit_msg(msg, "Orion address claim");
}

static void send_orion_summary_frame(const TempStats& stats) {
  if (!stats.valid) return;

  twai_message_t msg;
  memset(&msg, 0, sizeof(msg));

  msg.identifier = ORION_SUMMARY_ID;
  msg.extd = 1;
  msg.data_length_code = 8;
  msg.data[0] = ORION_MODULE_NUMBER;
  msg.data[1] = (uint8_t)stats.lowTemp;
  msg.data[2] = (uint8_t)stats.highTemp;
  msg.data[3] = (uint8_t)stats.avgTemp;
  msg.data[4] = stats.count;
  msg.data[5] = stats.highId;
  msg.data[6] = stats.lowId;
  msg.data[7] = checksum_orion_summary(msg.data);

  transmit_msg(msg, "Orion summary");
}

static bool find_next_valid_thermistor(uint8_t* indexOut) {
  for (uint8_t attempts = 0; attempts < TOTAL_THERMISTORS; attempts++) {
    uint8_t idx = nextGeneralThermistorIndex++;
    if (nextGeneralThermistorIndex >= TOTAL_THERMISTORS) nextGeneralThermistorIndex = 0;

    if (thermistorValid[idx]) {
      *indexOut = idx;
      return true;
    }
  }

  return false;
}

static void send_orion_general_broadcast_frame(const TempStats& stats) {
  if (!stats.valid) return;

  uint8_t thermistorIndex = 0;
  if (!find_next_valid_thermistor(&thermistorIndex)) return;

  twai_message_t msg;
  memset(&msg, 0, sizeof(msg));

  msg.identifier = ORION_GENERAL_ID;
  msg.extd = 1;
  msg.data_length_code = 8;
  msg.data[0] = thermistorIndex;
  msg.data[1] = (uint8_t)thermistorTemps[thermistorIndex];
  msg.data[2] = thermistorIndex;
  msg.data[3] = (uint8_t)stats.lowTemp;
  msg.data[4] = (uint8_t)stats.highTemp;
  msg.data[5] = stats.highId;
  msg.data[6] = stats.lowId;
  msg.data[7] = 0x80;

  transmit_msg(msg, "Orion general");
}

static uint8_t detect_subpack_id() {
  pinMode(GPIO_NUM_11, INPUT);  // D19
  pinMode(GPIO_NUM_7, INPUT);   // D8
  pinMode(GPIO_NUM_12, INPUT);  // D18
  pinMode(GPIO_NUM_8, INPUT);   // D9
  pinMode(GPIO_NUM_13, INPUT);  // D17
  pinMode(GPIO_NUM_9, INPUT);   // D10

  if (digitalRead(GPIO_NUM_11) == HIGH) return 1;
  if (digitalRead(GPIO_NUM_7) == HIGH) return 2;
  if (digitalRead(GPIO_NUM_12) == HIGH) return 3;
  if (digitalRead(GPIO_NUM_8) == HIGH) return 4;
  if (digitalRead(GPIO_NUM_13) == HIGH) return 5;
  if (digitalRead(GPIO_NUM_9) == HIGH) return 6;
  return 1;
}

static void print_debug(const TempStats& stats) {
  Serial.printf("Subpack %u mode=%s\n", subpackID, isMaster ? "master" : "sender");

  if (isMaster && stats.valid) {
    Serial.printf(
      "Orion stats: count=%u low=%d id=%u high=%d id=%u avg=%d next=%u\n",
      stats.count,
      stats.lowTemp,
      stats.lowId,
      stats.highTemp,
      stats.highId,
      stats.avgTemp,
      nextGeneralThermistorIndex
    );
  }
>>>>>>> theirs
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

<<<<<<< ours
  if (!init_can()) {
    Serial.println("CAN init failed!");
    while (1) {}
  }

  memset(&CAN_outMsg1, 0, sizeof(CAN_outMsg1));
  memset(&CAN_outMsg2, 0, sizeof(CAN_outMsg2));

  pinMode(GPIO_NUM_11, INPUT);  // D19
  pinMode(GPIO_NUM_7, INPUT);  // D8
  pinMode(GPIO_NUM_12, INPUT);  // D18
  pinMode(GPIO_NUM_8, INPUT);  // D9
  pinMode(GPIO_NUM_13, INPUT);  // D17
  pinMode(GPIO_NUM_9, INPUT);  // D10

  if (digitalRead(GPIO_NUM_11) == HIGH) subpackID = 1;
  else if (digitalRead(GPIO_NUM_7) == HIGH) subpackID = 2;
  else if (digitalRead(GPIO_NUM_12) == HIGH) subpackID = 3;
  else if (digitalRead(GPIO_NUM_8) == HIGH) subpackID = 4;
  else if (digitalRead(GPIO_NUM_13) == HIGH) subpackID = 5;
  else if (digitalRead(GPIO_NUM_9) == HIGH) subpackID = 6;
  else subpackID = 1;  // default to 1 if no pin is high

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
=======
  subpackID = detect_subpack_id();
  isMaster = subpackID == 1;
  clear_thermistor_data();

  if (!init_can()) {
    Serial.println("CAN init failed!");
    while (1) delay(500);
  }

  Serial.printf("Thermistor board started as subpack %u (%s)\n",
                subpackID,
                isMaster ? "master aggregator" : "subpack sender");
>>>>>>> theirs
}

void loop() {
  digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
<<<<<<< ours

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
=======
  check_bus_off();

  read_local_thermistors();

  uint32_t now = millis();
  TempStats stats = {};

  if (isMaster) {
    store_subpack_temps(1, localTemps);
    receive_can_messages();
    mark_stale_thermistors();
    stats = compute_temperature_stats();

    if (now - lastAddrClaimTx >= ORION_ADDR_CLAIM_PERIOD_MS) {
      lastAddrClaimTx = now;
      send_orion_address_claim();
    }

    if (now - lastSummaryTx >= ORION_SUMMARY_PERIOD_MS) {
      lastSummaryTx = now;
      send_orion_summary_frame(stats);
    }

    if (now - lastGeneralTx >= ORION_GENERAL_PERIOD_MS) {
      lastGeneralTx = now;
      send_orion_general_broadcast_frame(stats);
    }
  } else if (now - lastSubpackTx >= SUBPACK_SEND_PERIOD_MS) {
    lastSubpackTx = now;
    send_subpack_frames();
  }

  if (now - lastDebugPrint >= DEBUG_PRINT_PERIOD_MS) {
    lastDebugPrint = now;
    print_debug(stats);
  }
>>>>>>> theirs
}
