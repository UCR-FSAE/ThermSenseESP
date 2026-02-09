#include "ESP32_CAN.h"

#define CAN_TX_PIN GPIO_NUM_43
#define CAN_RX_PIN GPIO_NUM_44

bool ESP32_CAN::begin() {
  twai_general_config_t g_config =
      TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_NORMAL);

  twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  if (twai_driver_install(&g_config, &t_config, &f_config) != ESP_OK) {
    return false;
  }

  if (twai_start() != ESP_OK) {
    return false;
  }

  return true;
}

bool ESP32_CAN::write(const CAN_message_t &msg) {
  twai_message_t tmsg = {};
  tmsg.identifier = msg.id;
  tmsg.data_length_code = msg.len;
  tmsg.extd = msg.flags.extended;
  tmsg.rtr = msg.flags.remote;

  memcpy(tmsg.data, msg.buf, msg.len);

  return twai_transmit(&tmsg, pdMS_TO_TICKS(10)) == ESP_OK;
}

bool ESP32_CAN::read(CAN_message_t &msg) {
  twai_message_t tmsg;

  if (twai_receive(&tmsg, 0) != ESP_OK) {
    return false;
  }

  msg.id = tmsg.identifier;
  msg.len = tmsg.data_length_code;
  msg.flags.extended = tmsg.extd;
  msg.flags.remote = tmsg.rtr;

  memcpy(msg.buf, tmsg.data, msg.len);
  return true;
}
