# HR26E Thermistor Sense Module (ESP32-S3)

## Hardware
- Seeed Studio XIAO ESP32-S3  
- Waveshare CAN transceiver 


## Wiring

### ESP32-S3 → CAN Transceiver
| XIAO Pin | Function |
|--------|---------|
| 3V3 | VCC |
| GND | GND |
| D5 (GPIO 5) | CAN TX |
| D4 (GPIO 4) | CAN RX |

## Testing (No CAN Transceiver Connected)
- Open the repo on Platform IO
- Upload firmware
- Open Serial Monitor
- Expected output:
  - `ADC Voltage: ...`
  - `Temperature (C): ...`
