# HR26E Thermistor Sense Module (ESP32-S3)

## Hardware
- Seeed Studio XIAO ESP32-S3  
- Waveshare CAN transceiver SN65HVD230


## Wiring

### ESP32-S3 → CAN Transceiver
| XIAO Pin | Function |
|--------|---------|
| 3V3 | VCC |
| GND | GND |
| (GPIO 43) | CAN TX |
| (GPIO 44) | CAN RX |

## Testing (No CAN Transceiver Connected)
- Open the repo on Platform IO
- Upload firmware
- Open Serial Monitor
- Expected output:
  - `ADC Voltage: ...`
  - `Temperature (C): ...`
