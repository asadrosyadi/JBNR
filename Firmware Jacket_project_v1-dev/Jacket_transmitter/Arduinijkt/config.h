#ifndef CONFIG_H
#define CONFIG_H

// ---- I2C (MAX30102 @0x57, TMP116 @0x49) ----
#define I2C_SDA   1
#define I2C_SCL   6
#define I2C_FRQ   400000

// ---- GPS UART (NEO-6M, GLL sentences only) ----
#define GPS_BAUD      9600
#define GPS_RXD_PIN   2
#define GPS_TXD_PIN   3
#define GPS_BUF_SIZE  1024

// ---- LoRa SPI (SX127x/RFM9x on SPI2/VSPI) ----
#define LORA_PIN_MOSI  5
#define LORA_PIN_MISO  4
#define LORA_PIN_SCLK  7
#define LORA_PIN_CS    11
#define LORA_PIN_RST   10
#define LORA_PIN_G0    21   // DIO0, wired but unused (no RX interrupt in original firmware)
#define LORA_PIN_RX_EN 12
#define LORA_PIN_TX_EN 13

// ---- WiFi AP + HTTP dashboard ----
#define WIFI_AP_SSID       "ESP"
#define WIFI_AP_PASS       "1234567890"
#define WIFI_AP_CHANNEL    6
#define WIFI_AP_MAX_CONN   4

// ---- Heart-rate / SpO2 signal processing ----
#define VITALS_BUFFER_SIZE        256
#define VITALS_SAMPLE_RATE        50.0f
#define VITALS_MIN_BPM            40.0f
#define VITALS_MAX_BPM            180.0f
#define VITALS_PEAK_WINDOW_SIZE   25
#define VITALS_GAUSSIAN_KERNEL_SIZE 9
#define VITALS_MAX_SAMPLES        10000

#endif // CONFIG_H
