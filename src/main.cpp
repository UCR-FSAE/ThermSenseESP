#include <Arduino.h>
#include "driver/twai.h"

// ESP32 -> transceiver TXD/RXD pins
static const gpio_num_t CAN_TX = GPIO_NUM_4;
static const gpio_num_t CAN_RX = GPIO_NUM_5;

static const uint32_t SEND_PERIOD_MS = 5; // scope-friendly burst rate

static void print_status(const char* where) {
  twai_status_info_t st;
  if (twai_get_status_info(&st) == ESP_OK) {
    Serial.printf("[%s] state=%d tx_err=%d rx_err=%d bus_err=%lu tx_failed=%lu rx_missed=%lu\n",
                  where, (int)st.state, (int)st.tx_error_counter, (int)st.rx_error_counter,
                  (unsigned long)st.bus_error_count,
                  (unsigned long)st.tx_failed_count,
                  (unsigned long)st.rx_missed_count);
  }
}

static bool twai_up = false;

static bool twai_begin_no_ack_500k() {
  twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX, CAN_RX, TWAI_MODE_NO_ACK);
  g.tx_queue_len = 25;
  g.rx_queue_len = 25;

  twai_timing_config_t t = TWAI_TIMING_CONFIG_500KBITS();
  twai_filter_config_t f = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  esp_err_t err = twai_driver_install(&g, &t, &f);
  Serial.printf("twai_driver_install: %s\n", esp_err_to_name(err));
  if (err != ESP_OK) return false;

  err = twai_start();
  Serial.printf("twai_start: %s\n", esp_err_to_name(err));
  if (err != ESP_OK) return false;

  twai_up = true;
  Serial.println("TWAI started: 500 kbit, NO_ACK, burst TX for scope.");
  print_status("after_start");
  return true;
}

static void twai_restart() {
  Serial.println("[TWAI] BUS-OFF -> restarting driver...");
  twai_up = false;

  twai_stop();
  twai_driver_uninstall();
  delay(50);

  twai_begin_no_ack_500k();
}

void setup() {
  Serial.begin(115200);
  delay(300);

  twai_begin_no_ack_500k();
}

void loop() {
  static uint8_t ctr = 0;

  // Check state; if bus-off, restart
  twai_status_info_t st;
  if (twai_up && twai_get_status_info(&st) == ESP_OK) {
    if (st.state == TWAI_STATE_BUS_OFF) {
      print_status("bus_off_detected");
      twai_restart();
      delay(10);
      return;
    }
  }

  // Build message
  twai_message_t msg;
  memset(&msg, 0, sizeof(msg));
  msg.identifier = 0x123;
  msg.data_length_code = 8;
  msg.flags = 0;

  msg.data[0] = ctr++;
  msg.data[1] = 0xAA;
  msg.data[2] = 0x55;
  msg.data[3] = 0x00;
  msg.data[4] = 0x11;
  msg.data[5] = 0x22;
  msg.data[6] = 0x33;
  msg.data[7] = 0x44;

  // Try transmit
  esp_err_t err = twai_transmit(&msg, pdMS_TO_TICKS(10));
  if (err != ESP_OK) {
    Serial.printf("[TX] FAILED: %s\n", esp_err_to_name(err));
    print_status("tx_fail");

    // If driver says invalid state, we're likely bus-off already-restart
    if (err == ESP_ERR_INVALID_STATE) {
      twai_restart();
    }
    delay(20);
  }

  delay(SEND_PERIOD_MS);
}
