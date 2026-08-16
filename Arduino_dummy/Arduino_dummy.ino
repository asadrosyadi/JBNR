#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <inttypes.h>

#include "esp_log.h"
#include "esp_err.h"
#include "esp_system.h"
#include "esp_random.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"

#include "esp_task_wdt.h"
#include "nvs_flash.h"

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define TAG "JACKET"

#define NODE_NAME "ESP32-dummy" // nama bluetooth
#define NODE_NAME_MAX_LEN 16

static_assert(sizeof(NODE_NAME) <= NODE_NAME_MAX_LEN, "NODE_NAME too long, raise NODE_NAME_MAX_LEN");

#pragma pack(push, 1)
typedef struct
{
    int32_t  lat_x1e6;     // GPS latitude  * 1e6
    int32_t  lon_x1e6;     // GPS longitude * 1e6
    int16_t  temp_x100;    // Temperature (C * 100)
    uint16_t hr_x10;       // Heart rate (bpm * 10)
    uint16_t spo2_x10;     // SpO2 (% * 10)
} data_packet_t;
#pragma pack(pop)

static_assert(sizeof(data_packet_t) == 14, "data_packet_t size mismatch");

typedef struct {
    double latitude;
    double longitude;
} GPS_Coordinates;

#define SPI_WRITE_BIT_MASK      0x80
#define CLOCK_SPEED_MHZ         5
#define PA_BOOST_PIN            0x80

#define RFM9X_00_REG_FIFO                  0x00
#define RFM9X_01_REG_OP_MODE                0x01
#define RFM9X_06_REG_FRF_MSB                0x06
#define RFM9X_07_REG_FRF_MID                0x07
#define RFM9X_08_REG_FRF_LSB                0x08
#define RFM9X_09_REG_PA_CONFIG               0x09
#define RFM9X_0B_REG_OCP                     0x0b
#define RFM9X_0D_REG_FIFO_ADDR_PTR           0x0d
#define RFM9X_0E_REG_FIFO_TX_BASE_ADDR       0x0e
#define RFM9X_0F_REG_FIFO_RX_BASE_ADDR       0x0f
#define RFM9X_10_REG_FIFO_RX_CURRENT_ADDR    0x10
#define RFM9X_12_REG_IRQ_FLAGS               0x12
#define RFM9X_13_REG_RX_NB_BYTES             0x13
#define RFM9X_19_REG_PKT_SNR_VALUE           0x19
#define RFM9X_1A_REG_PKT_RSSI_VALUE          0x1a
#define RFM9X_1D_REG_MODEM_CONFIG1           0x1d
#define RFM9X_1E_REG_MODEM_CONFIG2           0x1e
#define RFM9X_20_REG_PREAMBLE_MSB            0x20
#define RFM9X_21_REG_PREAMBLE_LSB            0x21
#define RFM9X_22_REG_PAYLOAD_LENGTH          0x22
#define RFM9X_26_REG_MODEM_CONFIG3           0x26
#define RFM9X_40_REG_DIO_MAPPING1            0x40
#define RFM9X_42_REG_VERSION                 0x42
#define RFM9X_4D_REG_PA_DAC                  0x4D

#define RFM9X_LONG_RANGE_MODE      0x80
#define RFM9X_MODE_SLEEP           0x00
#define RFM9X_MODE_STDBY           0x01
#define RFM9X_MODE_FSTX            0x02
#define RFM9X_MODE_TX              0x03
#define RFM9X_MODE_FSRX            0x04
#define RFM9X_MODE_RXCONTINUOUS    0x05
#define RFM9X_MODE_RXSINGLE        0x06
#define RFM9X_MODE_CAD             0x07

#define RFM9X_42_REG_VERSION_VALUE 0x12
#define RFM9X_FXOSC                32000000.0

#define RFM9X_HEADER_LEN        4
#define RADIOHEAD_HEADER_TO     0xff
#define RADIOHEAD_HEADER_FROM   0xff
#define RADIOHEAD_HEADER_ID     0xff
#define RADIOHEAD_HEADER_FLAGS  0x00
#define IRQ_TX_DONE_MASK               0x08
#define IRQ_RX_DONE_MASK               0x40
#define IRQ_PAYLOAD_CRC_ERROR_MASK     0x20
#define MAX_PAYLOAD_LENGTH      (255 - RFM9X_HEADER_LEN)

// ===== LoRa application packet framing =====
// Every jacket broadcasts on the same channel (RadioHead header is always
// TO=FROM=0xff, i.e. no radio-level addressing), so the app-layer payload
// carries its own type tag + sender name. This lets two (or more) jackets
// tell each other's sensor packets and chat messages apart, and lets the
// receiving jacket relay each one to the phone tagged with the right node
// name (see BleUuids/BleService on the Flutter side for the matching
// "<node>:..." wire format).
#define LORA_PKT_SENSOR 0xA1
#define LORA_PKT_CHAT    0xA2

#define LORA_CHAT_MAX_LEN 128

#pragma pack(push, 1)
typedef struct {
    uint8_t type;                     // LORA_PKT_*
    char    node[NODE_NAME_MAX_LEN];  // sender's NODE_NAME, NUL-padded
} lora_pkt_header_t;

typedef struct {
    lora_pkt_header_t header;
    data_packet_t     data;
} lora_sensor_packet_t;

typedef struct {
    lora_pkt_header_t header;
    char target[NODE_NAME_MAX_LEN];   // recipient NODE_NAME, or "ALL"
    char text[LORA_CHAT_MAX_LEN];     // NUL-terminated chat message
} lora_chat_packet_t;
#pragma pack(pop)

static_assert(sizeof(lora_sensor_packet_t) <= MAX_PAYLOAD_LENGTH, "sensor packet too large for LoRa payload");
static_assert(sizeof(lora_chat_packet_t) <= MAX_PAYLOAD_LENGTH, "chat packet too large for LoRa payload");

// Outgoing chat messages queued from the BLE text-characteristic write
// callback (runs on the BLE stack's task) and drained by lora_task, which
// owns the SPI bus / RF switch and must not be touched from another task.
typedef struct {
    char target[NODE_NAME_MAX_LEN];
    char text[LORA_CHAT_MAX_LEN];
} chat_tx_item_t;

#define CHAT_TX_QUEUE_LEN 8
static QueueHandle_t chat_tx_queue = NULL;

// ESP32 Dev board + plain RFM95/SX1276/Ra-02 module - standard
// "sandeepmistry/LoRa"-style pinout. This module has no separate
// RX_EN/TX_EN antenna-switch pins (unlike the custom "Jaket" board),
// so TX/RX path switching is left entirely to the SX127x chip itself
// via its PA_CONFIG/PA_DAC registers (see setTxPower()).
#define GPIO_NUM_MOSI   GPIO_NUM_23  // MOSI
#define GPIO_NUM_MISO   GPIO_NUM_19  // MISO
#define GPIO_NUM_SCLK   GPIO_NUM_18  // SCK
#define GPIO_NUM_CS     GPIO_NUM_5   // SS / NSS
#define GPIO_NUM_RST    GPIO_NUM_14  // RST
#define GPIO_NUM_DIO0   GPIO_NUM_2   // DIO0 (not read by this polling-based driver, kept for reference)

static spi_device_handle_t spi_handle;

esp_err_t spi_init(void)
{
    esp_err_t ret;

    gpio_reset_pin(GPIO_NUM_RST);
    gpio_set_direction(GPIO_NUM_RST, GPIO_MODE_OUTPUT);

    gpio_reset_pin(GPIO_NUM_CS);
    gpio_set_direction(GPIO_NUM_CS, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_NUM_CS, 1);

    spi_bus_config_t spi_bus_config = {};
    spi_bus_config.miso_io_num   = GPIO_NUM_MISO;
    spi_bus_config.mosi_io_num   = GPIO_NUM_MOSI;
    spi_bus_config.sclk_io_num   = GPIO_NUM_SCLK;
    spi_bus_config.quadwp_io_num = -1;
    spi_bus_config.quadhd_io_num = -1;
    spi_bus_config.max_transfer_sz = 0;

    ret = spi_bus_initialize(SPI2_HOST, &spi_bus_config, SPI_DMA_CH_AUTO);
    ESP_LOGI(TAG, "spi_bus_initialize=%d", ret);
    assert(ret == ESP_OK);

    spi_device_interface_config_t spi_device_interface_config = {};
    spi_device_interface_config.clock_speed_hz = CLOCK_SPEED_MHZ * 1000 * 1000;
    spi_device_interface_config.mode = 0;
    spi_device_interface_config.queue_size = 1;
    spi_device_interface_config.spics_io_num = -1;
    spi_device_interface_config.pre_cb = NULL;
    spi_device_interface_config.post_cb = NULL;

    ret = spi_bus_add_device(SPI2_HOST, &spi_device_interface_config, &spi_handle);
    ESP_LOGI(TAG, "spi_bus_add_device=%d", ret);
    assert(ret == ESP_OK);

    return ESP_OK;
}

void reset_radio(void)
{
    ESP_LOGI(TAG, "reseting lora radio. Starting...");
    gpio_set_level(GPIO_NUM_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(GPIO_NUM_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_LOGI(TAG, "reseting lora radio. Done.");
}

int register_read(int reg)
{
    uint8_t buffer_out[2] = { (uint8_t)reg, 0xff };
    uint8_t buffer_in[2];

    spi_transaction_t t = {
        .flags = 0,
        .length = 8 * sizeof(buffer_out),
        .tx_buffer = buffer_out,
        .rx_buffer = buffer_in
    };

    gpio_set_level(GPIO_NUM_CS, 0);
    spi_device_transmit(spi_handle, &t);
    gpio_set_level(GPIO_NUM_CS, 1);

    return buffer_in[1];
}

esp_err_t register_write(uint8_t reg, uint8_t value)
{
    uint8_t buffer_out[2] = { (uint8_t)(reg | SPI_WRITE_BIT_MASK), value };

    spi_transaction_t t = {
        .flags = 0,
        .length = 8 * sizeof(buffer_out),
        .tx_buffer = buffer_out,
        .rx_buffer = NULL
    };

    gpio_set_level(GPIO_NUM_CS, 0);
    spi_device_transmit(spi_handle, &t);
    gpio_set_level(GPIO_NUM_CS, 1);

    return ESP_OK;
}

void setPreambleLength(uint16_t length)
{
    register_write(RFM9X_20_REG_PREAMBLE_MSB, (length >> 8) & 0xff);
    register_write(RFM9X_21_REG_PREAMBLE_LSB, length & 0xff);
}

void setFrequency(uint32_t frequency)
{
    uint64_t freq_hz = (uint64_t)frequency * 1000000ULL;
    uint64_t frf = (freq_hz << 19) / (uint64_t)RFM9X_FXOSC;

    register_write(RFM9X_06_REG_FRF_MSB, (frf >> 16) & 0xff);
    register_write(RFM9X_07_REG_FRF_MID, (frf >> 8) & 0xff);
    register_write(RFM9X_08_REG_FRF_LSB, frf & 0xff);
}

uint32_t getFrecuency()
{
    uint8_t msb = register_read(RFM9X_06_REG_FRF_MSB);
    uint8_t mid = register_read(RFM9X_07_REG_FRF_MID);
    uint8_t lsb = register_read(RFM9X_08_REG_FRF_LSB);

    return (uint32_t)(((uint32_t)msb << 16 | (uint32_t)mid << 8 | lsb) * RFM9X_FXOSC / (1 << 19));
}

uint8_t setRegOpMode(uint8_t mode);

void setTxPower(int8_t power)
{
    uint8_t paConfig;
    uint8_t paDac;
    uint8_t ocpTrim;

    if (power < 2)  power = 2;
    if (power > 20) power = 20;

    setRegOpMode(RFM9X_MODE_STDBY);
    vTaskDelay(pdMS_TO_TICKS(2));

    if (power == 20) {
        paDac    = 0x87;     // +20 dBm enable
        paConfig = 0x80 | 0x0F;
        ocpTrim  = 18;       // ~130 mA SAFE
    } else {
        paDac    = 0x84;
        paConfig = 0x80 | ((power >= 17) ? 15 : (power - 2));
        ocpTrim  = 12;       // ~100 mA
    }

    register_write(RFM9X_4D_REG_PA_DAC, paDac);
    register_write(RFM9X_0B_REG_OCP, 0x20 | (ocpTrim & 0x1F));
    register_write(RFM9X_09_REG_PA_CONFIG, paConfig);
}

uint8_t getTxPower(void)
{
    uint8_t paConfig = register_read(RFM9X_09_REG_PA_CONFIG);
    uint8_t paDac    = register_read(RFM9X_4D_REG_PA_DAC);

    printf("Debug: PA_Config=0x%02X, PA_DAC=0x%02X\n", paConfig, paDac);

    if ((paConfig & 0x80) && paDac == 0x87) {
        return 20;
    } else if (paConfig & 0x80) {
        return 2 + (paConfig & 0x0F);
    } else {
        return -4 + (paConfig & 0x0F);
    }
}

void setBandwidth(double bandwidth)
{
    uint8_t signal_bw;
    uint8_t regModemConfig1 = register_read(RFM9X_1D_REG_MODEM_CONFIG1);

    if (bandwidth <= 7.8e3) signal_bw = 0;
    else if (bandwidth <= 10.4e3) signal_bw = 1;
    else if (bandwidth <= 15.6e3) signal_bw = 2;
    else if (bandwidth <= 20.8e3) signal_bw = 3;
    else if (bandwidth <= 31.25e3) signal_bw = 4;
    else if (bandwidth <= 41.7e3) signal_bw = 5;
    else if (bandwidth <= 62.5e3) signal_bw = 6;
    else if (bandwidth <= 125e3) signal_bw = 7;
    else if (bandwidth <= 250e3) signal_bw = 8;
    else if (bandwidth <= 500e3) signal_bw = 9;
    else { printf("Invalid bandwidth\n"); return; }

    regModemConfig1 = (regModemConfig1 & 0x0f) | (signal_bw << 4);
    register_write(RFM9X_1D_REG_MODEM_CONFIG1, regModemConfig1);
}

void setExplicitHeaderMode()
{
    uint8_t regModemConfig1 = register_read(RFM9X_1D_REG_MODEM_CONFIG1);
    regModemConfig1 &= 0xfe;
    register_write(RFM9X_1D_REG_MODEM_CONFIG1, regModemConfig1);
}

void setSpreadingFactor(uint8_t sf)
{
    uint8_t regModemConfig2 = register_read(RFM9X_1E_REG_MODEM_CONFIG2);

    if (sf < 6 || sf > 12) { printf("Invalid spreading factor\n"); return; }

    regModemConfig2 = (regModemConfig2 & 0x0f) | ((sf << 4) & 0xf0);
    register_write(RFM9X_1E_REG_MODEM_CONFIG2, regModemConfig2);
}

void setCodingRate(uint8_t denominator)
{
    uint8_t regModemConfig1 = register_read(RFM9X_1D_REG_MODEM_CONFIG1);

    if (denominator < 5) denominator = 5;
    else if (denominator > 8) denominator = 8;

    regModemConfig1 = (regModemConfig1 & 0xf1) | ((denominator - 4) << 1);
    register_write(RFM9X_1D_REG_MODEM_CONFIG1, regModemConfig1);
}

void enableCRC()
{
    uint8_t regModemConfig2 = register_read(RFM9X_1E_REG_MODEM_CONFIG2);
    regModemConfig2 |= 0x04;
    register_write(RFM9X_1E_REG_MODEM_CONFIG2, regModemConfig2);
}

uint8_t getRegOpMode()
{
    return register_read(RFM9X_01_REG_OP_MODE);
}

uint8_t setRegOpMode(uint8_t mode)
{
    switch (mode) {
        case RFM9X_MODE_SLEEP:
        case RFM9X_MODE_STDBY:
        case RFM9X_MODE_FSTX:
        case RFM9X_MODE_TX:
        case RFM9X_MODE_FSRX:
        case RFM9X_MODE_RXCONTINUOUS:
        case RFM9X_MODE_RXSINGLE:
            register_write(RFM9X_01_REG_OP_MODE, mode | RFM9X_LONG_RANGE_MODE);
            break;
        default:
            printf("Unknown mode\n");
    }
    return getRegOpMode();
}

void check_radio_version(void)
{
    uint8_t result = register_read(RFM9X_42_REG_VERSION);
    printf("fetched result is: 0x%x\n", result);
    if (result == RFM9X_42_REG_VERSION_VALUE) {
        printf("LoRa radio init success\n");
    } else {
        printf("LoRa radio init failed. Check wiring.\n");
    }
}

void radio_init(void)
{
    reset_radio();
    check_radio_version();

    setRegOpMode(RFM9X_MODE_SLEEP);
    vTaskDelay(pdMS_TO_TICKS(10));

    register_write(RFM9X_0E_REG_FIFO_TX_BASE_ADDR, 0);
    register_write(RFM9X_0F_REG_FIFO_RX_BASE_ADDR, 0);

    setRegOpMode(RFM9X_MODE_STDBY);
    vTaskDelay(pdMS_TO_TICKS(10));

    setBandwidth(62.5e3);
    setCodingRate(4);
    setSpreadingFactor(10);
    enableCRC();
    setExplicitHeaderMode();

    setTxPower(20);
    printf("Configured TX Power :%d \n ", getTxPower());
    setPreambleLength(10);

    setFrequency(915);
    printf("Frequency: %" PRIu32 "\n", getFrecuency());

    uint8_t current_reg = register_read(RFM9X_1D_REG_MODEM_CONFIG1);
    printf("Current register 1 value: 0x%x\n", current_reg);
    current_reg = register_read(RFM9X_1E_REG_MODEM_CONFIG2);
    printf("Current register 2 value: 0x%x\n", current_reg);
    current_reg = register_read(RFM9X_26_REG_MODEM_CONFIG3);
    printf("Current register 3 value: 0x%x\n", current_reg);
}

// No-ops on this board: the plain RFM95/SX1276/Ra-02 module has no
// external RX_EN/TX_EN antenna switch to control (see GPIO pin comment
// above) - kept as stubs so the RX/TX-mode call sites throughout the
// LoRa code don't need to change.
void set_tx_enable() {}
void set_rx_enable() {}
void disable_rx_tx() {}

void setRxMode()
{
    setRegOpMode(RFM9X_MODE_RXCONTINUOUS);
    register_write(RFM9X_40_REG_DIO_MAPPING1, 0x00);
}

void lora_send(uint8_t *data, uint8_t length)
{
    uint8_t identifier = 1;

    set_tx_enable();

    setRegOpMode(RFM9X_MODE_STDBY);
    vTaskDelay(pdMS_TO_TICKS(10));

    register_write(RFM9X_0D_REG_FIFO_ADDR_PTR, 0);

    register_write(RFM9X_00_REG_FIFO, RADIOHEAD_HEADER_TO);
    register_write(RFM9X_00_REG_FIFO, RADIOHEAD_HEADER_FROM);
    register_write(RFM9X_00_REG_FIFO, RADIOHEAD_HEADER_ID);
    register_write(RFM9X_00_REG_FIFO, RADIOHEAD_HEADER_FLAGS + identifier++);

    for (int i = 0; i < length; i++) {
        register_write(RFM9X_00_REG_FIFO, data[i]);
    }

    register_write(RFM9X_22_REG_PAYLOAD_LENGTH, length + RFM9X_HEADER_LEN);

    setRegOpMode(RFM9X_MODE_TX);
    TickType_t start = xTaskGetTickCount();
    // Time-on-air at the current BW=62.5kHz/SF10 settings runs ~16.4ms/symbol,
    // so a near-MAX_PAYLOAD_LENGTH packet (the chat packet, ~165 bytes with
    // header, vs. the sensor packet's ~35) can take upwards of 3.1s to
    // actually finish transmitting - longer than the old fixed 3000ms
    // timeout. That caused every chat send to "time out" here while the
    // radio was still physically mid-TX, and the immediately-following
    // opmode/RX register writes (issued while the chip was still busy)
    // wedged the radio/SPI state, hanging lora_task forever and taking
    // the whole board down via the task watchdog. Give it enough margin
    // for the largest payload this app ever sends.
    TickType_t timeout = pdMS_TO_TICKS(8000);

    bool timed_out = false;
    while ((register_read(RFM9X_12_REG_IRQ_FLAGS) & IRQ_TX_DONE_MASK) == 0) {
        esp_task_wdt_reset();
        if ((xTaskGetTickCount() - start) >= timeout) {
            timed_out = true;
            printf("[LoRa TX] Timeout (%lu ms) - packet may not have sent\n", (unsigned long)pdTICKS_TO_MS(timeout));
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    register_write(RFM9X_12_REG_IRQ_FLAGS, IRQ_TX_DONE_MASK);
    if (!timed_out) {
        printf("[LoRa TX] Packet sent (%d bytes)\n", length + RFM9X_HEADER_LEN);
    }
}

void lora_send_sensor_packet(const data_packet_t *data)
{
    lora_sensor_packet_t out = {};
    out.header.type = LORA_PKT_SENSOR;
    strncpy(out.header.node, NODE_NAME, NODE_NAME_MAX_LEN - 1);
    out.data = *data;
    lora_send((uint8_t *)&out, sizeof(out));
}

void lora_send_chat_packet(const char *target, const char *text)
{
    lora_chat_packet_t out = {};
    out.header.type = LORA_PKT_CHAT;
    strncpy(out.header.node, NODE_NAME, NODE_NAME_MAX_LEN - 1);
    strncpy(out.target, target, NODE_NAME_MAX_LEN - 1);
    strncpy(out.text, text, LORA_CHAT_MAX_LEN - 1);
    printf("[CHAT] TX -> target=%s text=\"%s\"\n", out.target, out.text);
    lora_send((uint8_t *)&out, sizeof(out));
}

void lora_send_ack(void)
{
    lora_send((uint8_t *)"ACK", 3);
}

// ===== GPS (DUMMY) =====
// No real GPS wired up: this returns a fixed base coordinate with a
// small random jitter each call, just so the location field carries
// plausible, slightly-changing values.
GPS_Coordinates get_dummy_gps_coordinates(void)
{
    static const double base_lat = -6.973537;   // dummy base coordinate
    static const double base_lon = 107.630096;  // (Telkom University, Bandung)

    GPS_Coordinates coord;
    coord.latitude  = base_lat + ((int32_t)(esp_random() % 201) - 100) / 1000000.0;
    coord.longitude = base_lon + ((int32_t)(esp_random() % 201) - 100) / 1000000.0;
    return coord;
}

// ===== MAX30102 (DUMMY) =====
// No real pulse-oximeter wired up: heart rate and SpO2 are randomly
// generated within a plausible resting-adult range.
float get_dummy_heart_rate_bpm(void)
{
    return 70.0f + (esp_random() % 201) / 10.0f;   // 70.0 - 90.0 bpm
}

float get_dummy_spo2_percent(void)
{
    return 95.0f + (esp_random() % 51) / 10.0f;    // 95.0 - 100.0 %
}

// ===== TMP117 (DUMMY) =====
// No real temperature sensor wired up: body temperature is randomly
// generated within a plausible range.
float get_dummy_temperature_celsius(void)
{
    return 36.0f + (esp_random() % 101) / 100.0f;  // 36.00 - 37.00 C
}

// ===== BLE GATT server =====
#define GATTS_TAG "GATTS_SERVER"

#define GATTS_SERVICE_UUID_TEST_A     0x00FF  // HR & SpO2
#define GATTS_CHAR_UUID_TEST_A_VITALS 0xFF01

#define GATTS_SERVICE_UUID_TEST_B       0x00EE  // Lat & Lon
#define GATTS_CHAR_UUID_TEST_B_LOCATION 0xEE01

#define GATTS_SERVICE_UUID_TEST_C     0x00DD  // Temperature
#define GATTS_CHAR_UUID_TEST_C_TEMP   0xDD01

#define GATTS_SERVICE_UUID_TEST_D     0x00CC  // Text / status (dipakai juga utk laporan paket peer)
#define GATTS_CHAR_UUID_TEST_D_TEXT   0xCC01

// Kept as a prefix (not the full name) so every jacket still matches the
// Flutter app's scan filter (BleUuids.deviceNameFilter = "ESP-BLE",
// substring match) while advertising a distinct, per-node name.
#define TEST_DEVICE_NAME_PREFIX       "JKT-BLE-" // inisial kode ble

static float heart_rate = 0.0f;
static float spo2_level = 0.0f;
static float latitude = 0.0f;
static float longitude = 0.0f;
static float temperature = 0.0f;
static char user_text[NODE_NAME_MAX_LEN + 1 + LORA_CHAT_MAX_LEN] = "";

static BLEServer *bleServer = nullptr;
static BLECharacteristic *charVitals = nullptr;
static BLECharacteristic *charLocation = nullptr;
static BLECharacteristic *charTemp = nullptr;
static BLECharacteristic *charText = nullptr;
static volatile bool ble_device_connected = false;

void send_notification_to_all_services(void);

class JacketServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer *server) override {
        ble_device_connected = true;
        ESP_LOGI(GATTS_TAG, "BLE client connected");
        send_notification_to_all_services();
    }
    void onDisconnect(BLEServer *server) override {
        ble_device_connected = false;
        ESP_LOGI(GATTS_TAG, "BLE client disconnected, restart advertising");
        BLEDevice::startAdvertising();
    }
};

class VitalsCharCallbacks : public BLECharacteristicCallbacks {
    void onRead(BLECharacteristic *chr) override {
        char buf[32];
        int len = snprintf(buf, sizeof(buf), "HR:%.2f,SpO2:%.2f", heart_rate, spo2_level);
        chr->setValue((uint8_t *)buf, len);
    }
    void onWrite(BLECharacteristic *chr) override {
        String v = chr->getValue();
        if (v.length() == 0) return;
        char buf[100];
        size_t n = v.length();
        if (n > sizeof(buf) - 1) n = sizeof(buf) - 1;
        memcpy(buf, v.c_str(), n);
        buf[n] = '\0';

        char *token = strtok(buf, ",");
        while (token != NULL) {
            if (strstr(token, "HR=")) {
                heart_rate = atof(token + 3);
            } else if (strstr(token, "SpO2=")) {
                spo2_level = atof(token + 5);
            }
            token = strtok(NULL, ",");
        }
        send_notification_to_all_services();
    }
};

class LocationCharCallbacks : public BLECharacteristicCallbacks {
    void onRead(BLECharacteristic *chr) override {
        char buf[48];
        int len = snprintf(buf, sizeof(buf), "%.6f,%.6f", latitude, longitude);
        chr->setValue((uint8_t *)buf, len);
    }
    void onWrite(BLECharacteristic *chr) override {
        String v = chr->getValue();
        if (v.length() == 0) return;
        char buf[100];
        size_t n = v.length();
        if (n > sizeof(buf) - 1) n = sizeof(buf) - 1;
        memcpy(buf, v.c_str(), n);
        buf[n] = '\0';

        char *token = strtok(buf, ",");
        while (token != NULL) {
            if (strstr(token, "Lat=")) {
                latitude = atof(token + 4);
            } else if (strstr(token, "Lon=")) {
                longitude = atof(token + 4);
            }
            token = strtok(NULL, ",");
        }
        send_notification_to_all_services();
    }
};

class TempCharCallbacks : public BLECharacteristicCallbacks {
    void onRead(BLECharacteristic *chr) override {
        char buf[32];
        int len = snprintf(buf, sizeof(buf), "Temp:%.2f", temperature);
        chr->setValue((uint8_t *)buf, len);
    }
    void onWrite(BLECharacteristic *chr) override {
        String v = chr->getValue();
        if (v.length() == 0) return;
        char buf[20];
        size_t n = v.length();
        if (n > sizeof(buf) - 1) n = sizeof(buf) - 1;
        memcpy(buf, v.c_str(), n);
        buf[n] = '\0';

        if (strstr(buf, "Temp=")) {
            temperature = atof(buf + 5);
        } else {
            temperature = atof(buf);
        }
        send_notification_to_all_services();
    }
};

// Shared by BLE chat writes and Serial Monitor input: updates user_text and
// queues the message for LoRa transmit as "<target>:<message>" (bare text
// is treated as a broadcast) - see BleService.sendMessage() on the Flutter
// side. The actual LoRa transmit is handed off to lora_task via
// chat_tx_queue rather than touching the shared SPI/radio state here.
void submit_chat_text(const char *raw)
{
    if (raw[0] == '\0') return;

    char buf[sizeof(user_text)];
    strncpy(buf, raw, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    strncpy(user_text, buf, sizeof(user_text) - 1);
    user_text[sizeof(user_text) - 1] = '\0';

    chat_tx_item_t item = {};
    char *colon = strchr(buf, ':');
    if (colon != NULL) {
        size_t target_len = colon - buf;
        if (target_len > NODE_NAME_MAX_LEN - 1) target_len = NODE_NAME_MAX_LEN - 1;
        memcpy(item.target, buf, target_len);
        item.target[target_len] = '\0';
        strncpy(item.text, colon + 1, LORA_CHAT_MAX_LEN - 1);
    } else {
        strncpy(item.target, "ALL", NODE_NAME_MAX_LEN - 1);
        strncpy(item.text, buf, LORA_CHAT_MAX_LEN - 1);
    }

    if (chat_tx_queue == NULL || xQueueSend(chat_tx_queue, &item, 0) != pdTRUE) {
        printf("[CHAT] TX queue full or not ready, dropping outgoing message\n");
    }
}

class TextCharCallbacks : public BLECharacteristicCallbacks {
    void onRead(BLECharacteristic *chr) override {
        char buf[NODE_NAME_MAX_LEN + 8 + sizeof(user_text)];
        int len = snprintf(buf, sizeof(buf), "%s:Text:%s", NODE_NAME, user_text);
        chr->setValue((uint8_t *)buf, len);
    }
    void onWrite(BLECharacteristic *chr) override {
        String v = chr->getValue();
        if (v.length() == 0) return;
        submit_chat_text(v.c_str());
    }
};

void send_notification_to_all_services(void)
{
    if (!ble_device_connected) return;
    char buf[NODE_NAME_MAX_LEN + 8 + sizeof(user_text)];
    int len;

    len = snprintf(buf, sizeof(buf), "%s:HR:%.2f,SpO2:%.2f", NODE_NAME, heart_rate, spo2_level);
    charVitals->setValue((uint8_t *)buf, len);
    charVitals->notify();

    len = snprintf(buf, sizeof(buf), "%s:%.6f,%.6f", NODE_NAME, latitude, longitude);
    charLocation->setValue((uint8_t *)buf, len);
    charLocation->notify();

    len = snprintf(buf, sizeof(buf), "%s:Temp:%.2f", NODE_NAME, temperature);
    charTemp->setValue((uint8_t *)buf, len);
    charTemp->notify();
}

/// Relays one peer jacket's chat message onto the text characteristic,
/// tagged with the peer's own node name so the phone files it under that
/// node's chat room instead of ours (see BleService._onTextData).
void ble_notify_incoming_chat(const char *sender_node, const char *text)
{
    if (!ble_device_connected) return;
    char buf[NODE_NAME_MAX_LEN + 8 + LORA_CHAT_MAX_LEN];
    int len = snprintf(buf, sizeof(buf), "%s:Text:%s", sender_node, text);
    charText->setValue((uint8_t *)buf, len);
    charText->notify();
}

/// Relays one peer jacket's sensor packet onto the vitals/location/temp
/// characteristics, tagged with the peer's node name so the phone tracks
/// it as its own LoraNode entry (see BleService._onVitalsData etc.)
/// instead of merging it into our own telemetry.
void ble_notify_incoming_sensor(const char *sender_node, const data_packet_t *remote_pkt)
{
    if (!ble_device_connected) return;
    char buf[100];
    int len;

    float r_lat  = remote_pkt->lat_x1e6 / 1e6f;
    float r_lon  = remote_pkt->lon_x1e6 / 1e6f;
    float r_temp = remote_pkt->temp_x100 / 100.0f;
    float r_hr   = remote_pkt->hr_x10 / 10.0f;
    float r_spo2 = remote_pkt->spo2_x10 / 10.0f;

    len = snprintf(buf, sizeof(buf), "%s:HR:%.2f,SpO2:%.2f", sender_node, r_hr, r_spo2);
    charVitals->setValue((uint8_t *)buf, len);
    charVitals->notify();

    len = snprintf(buf, sizeof(buf), "%s:%.6f,%.6f", sender_node, r_lat, r_lon);
    charLocation->setValue((uint8_t *)buf, len);
    charLocation->notify();

    len = snprintf(buf, sizeof(buf), "%s:Temp:%.2f", sender_node, r_temp);
    charTemp->setValue((uint8_t *)buf, len);
    charTemp->notify();
}

void update_sensor_data_and_notify(float bpm, float spo2, float lat, float lon, float temp)
{
    heart_rate = bpm;
    spo2_level = spo2;
    latitude = lat;
    longitude = lon;
    temperature = temp;
    send_notification_to_all_services();
}

void update_max30102_sensor_data(float bpm, float spo2)
{
    heart_rate = bpm;
    spo2_level = spo2;
    send_notification_to_all_services();
}

void update_gps_sensor_data(float lat, float lon)
{
    latitude = lat;
    longitude = lon;
}

void update_tmp117_sensor_data(float temp)
{
    temperature = temp;
}

void ble_server_init(void)
{
    char device_name[NODE_NAME_MAX_LEN + sizeof(TEST_DEVICE_NAME_PREFIX)];
    snprintf(device_name, sizeof(device_name), "%s%s", TEST_DEVICE_NAME_PREFIX, NODE_NAME);

    BLEDevice::init(device_name);
    BLEDevice::setMTU(500);

    bleServer = BLEDevice::createServer();
    bleServer->setCallbacks(new JacketServerCallbacks());

    BLEService *svcVitals = bleServer->createService(BLEUUID((uint16_t)GATTS_SERVICE_UUID_TEST_A));
    charVitals = svcVitals->createCharacteristic(
        BLEUUID((uint16_t)GATTS_CHAR_UUID_TEST_A_VITALS),
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
    charVitals->addDescriptor(new BLE2902());
    charVitals->setCallbacks(new VitalsCharCallbacks());
    charVitals->setValue("HR:0.00,SpO2:0.00");
    svcVitals->start();

    BLEService *svcLocation = bleServer->createService(BLEUUID((uint16_t)GATTS_SERVICE_UUID_TEST_B));
    charLocation = svcLocation->createCharacteristic(
        BLEUUID((uint16_t)GATTS_CHAR_UUID_TEST_B_LOCATION),
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
    charLocation->addDescriptor(new BLE2902());
    charLocation->setCallbacks(new LocationCharCallbacks());
    charLocation->setValue("0.000000,0.000000");
    svcLocation->start();

    BLEService *svcTemp = bleServer->createService(BLEUUID((uint16_t)GATTS_SERVICE_UUID_TEST_C));
    charTemp = svcTemp->createCharacteristic(
        BLEUUID((uint16_t)GATTS_CHAR_UUID_TEST_C_TEMP),
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
    charTemp->addDescriptor(new BLE2902());
    charTemp->setCallbacks(new TempCharCallbacks());
    charTemp->setValue("Temp:0.00");
    svcTemp->start();

    BLEService *svcText = bleServer->createService(BLEUUID((uint16_t)GATTS_SERVICE_UUID_TEST_D));
    charText = svcText->createCharacteristic(
        BLEUUID((uint16_t)GATTS_CHAR_UUID_TEST_D_TEXT),
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
    charText->addDescriptor(new BLE2902());
    charText->setCallbacks(new TextCharCallbacks());
    charText->setValue(user_text);
    svcText->start();

    BLEAdvertising *advertising = BLEDevice::getAdvertising();
    advertising->addServiceUUID(BLEUUID((uint16_t)GATTS_SERVICE_UUID_TEST_A));
    advertising->addServiceUUID(BLEUUID((uint16_t)GATTS_SERVICE_UUID_TEST_B));
    advertising->addServiceUUID(BLEUUID((uint16_t)GATTS_SERVICE_UUID_TEST_C));
    advertising->addServiceUUID(BLEUUID((uint16_t)GATTS_SERVICE_UUID_TEST_D));
    advertising->setScanResponse(true);
    advertising->setMinPreferred(0x06);
    advertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();
    ESP_LOGI(GATTS_TAG, "BLE advertising started as \"%s\"", device_name);
}

/// A peer jacket's own sensor packet, received over LoRa. Relayed to the
/// phone tagged with the peer's node name so it shows up as its own
/// LoraNode entry, distinct from our own telemetry.
void handle_remote_sensor_packet(const char *sender_node, const data_packet_t *rpkt)
{
    float r_lat  = rpkt->lat_x1e6 / 1e6f;
    float r_lon  = rpkt->lon_x1e6 / 1e6f;
    float r_temp = rpkt->temp_x100 / 100.0f;
    float r_hr   = rpkt->hr_x10 / 10.0f;
    float r_spo2 = rpkt->spo2_x10 / 10.0f;

    printf("[LoRa RX] ----- Data sensor diterima dari %s -----\n", sender_node);
    printf("[LoRa RX] Latitude   : %.6f\n", r_lat);
    printf("[LoRa RX] Longitude  : %.6f\n", r_lon);
    printf("[LoRa RX] Temperature: %.2f C\n", r_temp);
    printf("[LoRa RX] Heart Rate : %.1f bpm\n", r_hr);
    printf("[LoRa RX] SpO2       : %.1f %%\n", r_spo2);
    printf("[LoRa RX] --------------------------\n");

    ble_notify_incoming_sensor(sender_node, rpkt);
}

/// A chat message from a peer jacket, received over LoRa. Relayed to the
/// phone only if it's addressed to us or broadcast to everyone - see
/// BleService._onTextData for how the phone files it by sender node.
void handle_remote_chat_packet(const char *sender_node, const char *target, const char *text)
{
    printf("[LoRa RX] ----- Chat diterima dari %s (target=%s) -----\n", sender_node, target);
    printf("[LoRa RX] %s\n", text);
    printf("[LoRa RX] --------------------------\n");

    if (strcmp(target, "ALL") == 0 || strcmp(target, NODE_NAME) == 0) {
        ble_notify_incoming_chat(sender_node, text);
    }
}

data_packet_t pkt = {0};

typedef struct { int32_t lat; int32_t lon; } gps_data_t;
gps_data_t gps_data = {0};

typedef struct { uint16_t hr; uint16_t spo2; } ppg_t;
ppg_t ppg = {0};

static int16_t TMP117_x100 = 0;

static SemaphoreHandle_t datasent_mutex_gps = NULL;
static SemaphoreHandle_t datasent_mutex_max30102 = NULL;
static SemaphoreHandle_t datasent_mutex_TMP117 = NULL;

void max30102_task(void *arg)
{
    while (1) {
        float bpm  = get_dummy_heart_rate_bpm();
        float spo2 = get_dummy_spo2_percent();

        update_max30102_sensor_data(bpm, spo2);
        //printf("[MAX30102-DUMMY] HR: %.1f bpm | SpO2: %.1f %%\n", bpm, spo2);

        if (xSemaphoreTake(datasent_mutex_max30102, portMAX_DELAY) == pdTRUE) {
            ppg.hr = (uint16_t)(bpm * 10);
            ppg.spo2 = (uint16_t)(spo2 * 10);
            xSemaphoreGive(datasent_mutex_max30102);
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void gps_task(void *arg)
{
    while (1) {
        GPS_Coordinates coordinate = get_dummy_gps_coordinates();

        update_gps_sensor_data(coordinate.latitude, coordinate.longitude);
        //printf("[GPS-DUMMY] Lat: %.6f  Lon: %.6f\n", coordinate.latitude, coordinate.longitude);

        if (xSemaphoreTake(datasent_mutex_gps, portMAX_DELAY) == pdTRUE) {
            gps_data.lat = (int32_t)(coordinate.latitude * 1e6);
            gps_data.lon = (int32_t)(coordinate.longitude * 1e6);
            xSemaphoreGive(datasent_mutex_gps);
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void TMP117_task(void *arg)
{
    while (1) {
        float temp = get_dummy_temperature_celsius();
        //printf("[TMP117-DUMMY] Suhu: %.2f C\n", temp);

        update_tmp117_sensor_data(temp);

        if (xSemaphoreTake(datasent_mutex_TMP117, portMAX_DELAY) == pdTRUE) {
            TMP117_x100 = (int16_t)(temp * 100);
            xSemaphoreGive(datasent_mutex_TMP117);
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/// Listens for up to timeout_ms, dispatching whatever comes in: an ACK
/// (sets *ack_received), a peer sensor packet, or a peer chat message.
/// Both types of inbound app packets get ACKed back immediately, using
/// the same RF-switch/mode dance lora_send() expects.
void lora_listen_and_dispatch(uint32_t timeout_ms, bool *ack_received)
{
    static uint8_t rxBuffer[MAX_PAYLOAD_LENGTH + 1];
    uint32_t start = xTaskGetTickCount();

    while ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(timeout_ms)) {
        setRxMode();
        esp_task_wdt_reset();

        uint8_t irq_flags = register_read(RFM9X_12_REG_IRQ_FLAGS);
        if (irq_flags & IRQ_RX_DONE_MASK) {

            if (irq_flags & IRQ_PAYLOAD_CRC_ERROR_MASK) {
                printf("[LoRa RX] CRC ERROR - packet discarded\n");
                register_write(RFM9X_12_REG_IRQ_FLAGS, 0xFF);
                register_write(RFM9X_0D_REG_FIFO_ADDR_PTR, register_read(RFM9X_0F_REG_FIFO_RX_BASE_ADDR));
                vTaskDelay(pdMS_TO_TICKS(5));
                continue;
            }

            uint8_t length = register_read(RFM9X_13_REG_RX_NB_BYTES);

            int16_t rssi = -137 + register_read(RFM9X_1A_REG_PKT_RSSI_VALUE);
            int16_t snr = -(register_read(RFM9X_19_REG_PKT_SNR_VALUE)) / 4;
            printf("[LoRa RX] Length: %d bytes | RSSI: %d dBm | SNR: %d dB\n", length, rssi, snr);

            if (length > sizeof(rxBuffer) - 1) length = sizeof(rxBuffer) - 1;
            memset(rxBuffer, 0, sizeof(rxBuffer));

            uint8_t fifoStart = register_read(RFM9X_10_REG_FIFO_RX_CURRENT_ADDR);
            register_write(RFM9X_0D_REG_FIFO_ADDR_PTR, fifoStart);

            for (int i = 0; i < length; i++) {
                rxBuffer[i] = register_read(RFM9X_00_REG_FIFO);
            }
            rxBuffer[length] = '\0';

            register_write(RFM9X_12_REG_IRQ_FLAGS, 0xFF);
            register_write(RFM9X_0D_REG_FIFO_ADDR_PTR, register_read(RFM9X_0F_REG_FIFO_RX_BASE_ADDR));

            printf("[LoRa RX] Raw data (%d bytes): ", length);
            for (uint8_t i = 0; i < length; i++) {
                printf("%02X ", rxBuffer[i]);
            }
            printf("\r\n");

            uint8_t payload_len = 0;
            if (length > RFM9X_HEADER_LEN) {
                memmove(rxBuffer, rxBuffer + RFM9X_HEADER_LEN, length - RFM9X_HEADER_LEN + 1);
                payload_len = length - RFM9X_HEADER_LEN;
            }

            if (payload_len == 3 && memcmp(rxBuffer, "ACK", 3) == 0) {
                printf("[LoRa RX] ACK received\n");
                if (ack_received != NULL) *ack_received = true;
            } else if (payload_len == sizeof(lora_sensor_packet_t) &&
                       rxBuffer[0] == LORA_PKT_SENSOR) {
                lora_sensor_packet_t in;
                memcpy(&in, rxBuffer, sizeof(in));
                in.header.node[NODE_NAME_MAX_LEN - 1] = '\0';
                handle_remote_sensor_packet(in.header.node, &in.data);
                set_tx_enable();
                lora_send_ack();
                set_rx_enable();
            } else if (payload_len == sizeof(lora_chat_packet_t) &&
                       rxBuffer[0] == LORA_PKT_CHAT) {
                lora_chat_packet_t in;
                memcpy(&in, rxBuffer, sizeof(in));
                in.header.node[NODE_NAME_MAX_LEN - 1] = '\0';
                in.target[NODE_NAME_MAX_LEN - 1] = '\0';
                in.text[LORA_CHAT_MAX_LEN - 1] = '\0';
                handle_remote_chat_packet(in.header.node, in.target, in.text);
                set_tx_enable();
                lora_send_ack();
                set_rx_enable();
            } else if (payload_len > 0) {
                printf("[LoRa RX] Unknown packet (%d bytes)\n", payload_len);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

// How often this node broadcasts its own sensor packet. Gates the beacon
// by elapsed time (instead of a flat vTaskDelay after each send) so the
// radio is never taken off-air waiting for the next beacon - see
// lora_task() below.
#define SENSOR_BEACON_INTERVAL_MS 4000

// Size of each listening slice inside lora_task's main loop. Small enough
// that the outgoing-chat queue and the sensor-beacon timer both stay
// responsive (worst case ~this many ms of added latency), but each slice
// still re-arms RXCONTINUOUS itself (see lora_listen_and_dispatch), so
// nothing is lost switching between slices.
#define LORA_LISTEN_SLICE_MS 200

// Kirim pesan chat yang di-queue dari BLE sesegera mungkin, kirim paket
// sensor sendiri secara berkala, dan SISANYA dihabiskan mendengarkan -
// TX dan RX dalam satu task, tapi radio tetap standby RX kapan pun tidak
// sedang mengirim, bukan dimatikan.
//
// Versi sebelumnya mengirim lalu mendengar singkat (<=1 detik) lalu
// mematikan switch RX/TX selama 3 detik penuh setiap siklus - karena
// setiap jaket menjalankan siklus dengan periode yang nyaris sama persis,
// itu bisa membuat jendela "dengar" satu jaket terus-menerus jatuh pas
// jaket lain sedang di fase "mati", sehingga paket dari LoRa lain (data
// sensor maupun chat) nyaris tidak pernah tertangkap walau pengiriman
// sendiri terlihat normal. Sekarang radio dibiarkan menyala mendengar
// hampir sepanjang waktu dan hanya berpindah ke TX sesaat saat benar-benar
// mengirim, lalu langsung kembali ke RX.
void lora_task(void *arg)
{
    esp_task_wdt_add(NULL);
    chat_tx_item_t chat_item;
    TickType_t last_beacon = xTaskGetTickCount() - pdMS_TO_TICKS(SENSOR_BEACON_INTERVAL_MS);

    set_rx_enable();
    setRxMode();

    while (1) {
        // Drain outgoing chat messages queued by the BLE text write
        // callback first, so chat feels responsive even if it lands
        // mid-cycle.
        while (xQueueReceive(chat_tx_queue, &chat_item, 0) == pdTRUE) {
            lora_send_chat_packet(chat_item.target, chat_item.text);
            // lora_send() meninggalkan switch RF di posisi TX, pindah ke RX dulu
            set_rx_enable();
            setRxMode();
        }

        if ((xTaskGetTickCount() - last_beacon) >= pdMS_TO_TICKS(SENSOR_BEACON_INTERVAL_MS)) {
            bool got_gps = xSemaphoreTake(datasent_mutex_gps, pdMS_TO_TICKS(100)) == pdTRUE;
            bool got_max30102 = got_gps && xSemaphoreTake(datasent_mutex_max30102, pdMS_TO_TICKS(100)) == pdTRUE;
            bool got_TMP117 = got_max30102 && xSemaphoreTake(datasent_mutex_TMP117, pdMS_TO_TICKS(100)) == pdTRUE;

            if (got_TMP117) {
                pkt.hr_x10 = ppg.hr;
                pkt.spo2_x10 = ppg.spo2;
                pkt.lat_x1e6 = gps_data.lat;
                pkt.lon_x1e6 = gps_data.lon;
                pkt.temp_x100 = TMP117_x100;

                printf("[LoRa TX] Mengirim data sensor -> Lat: %.6f  Lon: %.6f  Suhu: %.2f C  HR: %.1f bpm  SpO2: %.1f %%\n",
                       pkt.lat_x1e6 / 1e6f, pkt.lon_x1e6 / 1e6f, pkt.temp_x100 / 100.0f,
                       pkt.hr_x10 / 10.0f, pkt.spo2_x10 / 10.0f);

                lora_send_sensor_packet(&pkt);

                // lora_send() meninggalkan switch RF di posisi TX, pindah ke RX dulu
                set_rx_enable();
                setRxMode();

                xSemaphoreGive(datasent_mutex_gps);
                xSemaphoreGive(datasent_mutex_max30102);
                xSemaphoreGive(datasent_mutex_TMP117);
            } else {
                // Release whichever of the three were actually acquired above -
                // otherwise a partial acquire (e.g. gps taken, max30102 timed
                // out) would leak that mutex forever and deadlock its task.
                if (got_max30102) xSemaphoreGive(datasent_mutex_max30102);
                if (got_gps) xSemaphoreGive(datasent_mutex_gps);
            }

            last_beacon = xTaskGetTickCount();
        }

        // Spend the rest of the cycle actually listening for peers (sensor
        // packets, chat, ACKs) instead of sleeping with the radio off-air.
        lora_listen_and_dispatch(LORA_LISTEN_SLICE_MS, NULL);
    }
}

// Prints WHY the chip last restarted. This survives across the reboot
// itself (unlike a Guru Meditation dump, which scrolls by right before the
// reset and is easy to miss), so it's the first thing to check when
// "it just reboots and I can't read the error in time" - the reason
// shows up clearly on the *next* boot instead.
static const char *reset_reason_to_str(esp_reset_reason_t reason)
{
    switch (reason) {
        case ESP_RST_POWERON:   return "POWERON (cold boot / power applied)";
        case ESP_RST_EXT:       return "EXT (external reset pin / reset button)";
        case ESP_RST_SW:        return "SW (esp_restart() called from code)";
        case ESP_RST_PANIC:     return "PANIC (software crash - exception/abort, see backtrace above)";
        case ESP_RST_INT_WDT:   return "INT_WDT (interrupt watchdog - an ISR or critical section ran too long)";
        case ESP_RST_TASK_WDT:  return "TASK_WDT (task watchdog - a task didn't yield/reset in time)";
        case ESP_RST_WDT:       return "WDT (other watchdog, e.g. RTC WDT)";
        case ESP_RST_DEEPSLEEP: return "DEEPSLEEP (woke from deep sleep)";
        case ESP_RST_BROWNOUT:  return "BROWNOUT (power dip - check power supply/cable/current draw, not a code bug)";
        case ESP_RST_SDIO:      return "SDIO";
        default:                return "UNKNOWN";
    }
}

void setup()
{
    Serial.begin(115200);
    vTaskDelay(pdMS_TO_TICKS(200));
    printf("\n===== BOOT: last reset reason = %s =====\n\n", reset_reason_to_str(esp_reset_reason()));

    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = 10000,
        .idle_core_mask = BIT(0) | BIT(1),
        .trigger_panic = true
    };

    esp_err_t err = esp_task_wdt_init(&wdt_config);
    if (err == ESP_OK) {
        printf("TWDT initialized\n");
    } else if (err == ESP_ERR_INVALID_STATE) {
        printf("TWDT already initialized\n");
    }

    datasent_mutex_gps = xSemaphoreCreateMutex();
    if (datasent_mutex_gps == NULL) { printf("Failed to create mutex for GPS data.\n"); return; }

    datasent_mutex_max30102 = xSemaphoreCreateMutex();
    if (datasent_mutex_max30102 == NULL) { printf("Failed to create mutex for MAX30102 data.\n"); return; }

    datasent_mutex_TMP117 = xSemaphoreCreateMutex();
    if (datasent_mutex_TMP117 == NULL) { printf("Failed to create mutex for TMP117 data.\n"); return; }

    chat_tx_queue = xQueueCreate(CHAT_TX_QUEUE_LEN, sizeof(chat_tx_item_t));
    if (chat_tx_queue == NULL) { printf("Failed to create chat TX queue.\n"); return; }

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        ret = nvs_flash_init();
    }
    if (ret == ESP_OK) { printf("NVS initialized\n"); } else if (ret == ESP_ERR_INVALID_STATE) { printf("NVS initialize Failed\n"); }

    ble_server_init();

    vTaskDelay(3000 / portTICK_PERIOD_MS);
    printf("Lora Init Begun\n");

    spi_init();
    vTaskDelay(pdMS_TO_TICKS(10));
    radio_init();
    printf("LoRa radio init done\n");

    xTaskCreatePinnedToCore(max30102_task, "max30102_task", 4096, NULL, 10, NULL, 0);
    xTaskCreatePinnedToCore(gps_task, "gps_task", 4096, NULL, 9, NULL, 0);
    xTaskCreatePinnedToCore(TMP117_task, "TMP117_task", 4096, NULL, 8, NULL, 0);
    xTaskCreatePinnedToCore(lora_task, "lora_task", 4096, NULL, 7, NULL, 1);
}

// Typing a line into the Serial Monitor and pressing enter sends it as a
// chat message, same as a phone app write to charText (see submit_chat_text).
void loop()
{
    static char serial_buf[LORA_CHAT_MAX_LEN];
    static size_t serial_len = 0;

    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\n' || c == '\r') {
            if (serial_len > 0) {
                serial_buf[serial_len] = '\0';
                submit_chat_text(serial_buf);
                serial_len = 0;
            }
        } else if (serial_len < sizeof(serial_buf) - 1) {
            serial_buf[serial_len++] = c;
        }
    }

    vTaskDelay(pdMS_TO_TICKS(50));
}
