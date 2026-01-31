#pragma once
#include <Arduino.h>
#include <driver/twai.h>

typedef struct {
  uint32_t id;
  uint8_t len;
  uint8_t buf[8];
  struct {
    bool extended = false;
    bool remote = false;
  } flags;
} CAN_message_t;

class ESP32_CAN {
public:
  bool begin();
  bool write(const CAN_message_t &msg);
  bool read(CAN_message_t &msg);
};
