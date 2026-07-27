#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include "esp_task_wdt.h"
#include "esp_log.h"

#include "config.h"
#include "data_packet.h"
#include "vitals_algo.h"
#include "i2c_lock.h"
#include "max30102.h"
#include "tmp116.h"
#include "gps.h"
#include "lora.h"
#include "ble_server.h"
#include "http_server.h"

static const char *TAG = "MAIN";

static data_packet_t pkt = {0};
static max30102_t max30102 = {};

typedef struct {
    int32_t lat;
    int32_t lon;
} gps_t;
static gps_t gps_pkt = {0};

typedef struct {
    uint16_t hr;
    uint16_t spo2;
} ppg_t;
static ppg_t ppg = {0};
static int16_t tmp116_x100 = 0;

static SemaphoreHandle_t datasent_mutex_gps = NULL;
static SemaphoreHandle_t datasent_mutex_max30102 = NULL;
static SemaphoreHandle_t datasent_mutex_tmp116 = NULL;

// MAX30102 task
static void max30102_task(void *arg)
{
    static uint16_t red_buffer[VITALS_BUFFER_SIZE] = {0};
    static uint16_t ir_buffer[VITALS_BUFFER_SIZE] = {0};
    int write_index = 0;
    uint32_t red[32], ir[32];
    float bpm = 0.0, spo2 = 0.0;
    uint8_t ppg_count = 0;

    while (1) {
        uint8_t intStatus = 0;
        if (max30102_read_register(&max30102, MAX30102_INTERRUPT_STATUS_1, &intStatus) == ESP_OK) {

            bool fifo_full = (intStatus & 0x80) != 0;

            bool ppg_rdy = (intStatus & 0x40) != 0;
            if (ppg_rdy) {
                ppg_count++;
            }
            if (fifo_full || ppg_count >= 32) {
                ppg_count = 0;
                if (max30102_read_fifo(ir, red, 32) == ESP_OK) {

                    for (int i = 0; i < 32; i++) {
                        red_buffer[write_index] = (uint16_t)red[i];
                        ir_buffer[write_index]  = (uint16_t)ir[i];
                        write_index = (write_index + 1) % VITALS_BUFFER_SIZE;
                    }

                    bpm  = calculate_heart_rate(ir_buffer, VITALS_BUFFER_SIZE);
                    spo2 = calculate_spo2(red_buffer, ir_buffer, VITALS_BUFFER_SIZE);

                    if (bpm == -1.0f || spo2 == -1.0f) {
                        http_update_finger_status(0);
                        http_update_sp02_sensor_data(0, 0);
                        update_max30102_sensor_data(0, 0);
                        bpm = 0.0;
                        spo2 = 0.0;
                    } else {
                        http_update_finger_status(1);
                        http_update_sp02_sensor_data(bpm, spo2);
                        update_max30102_sensor_data(bpm, spo2);
                        printf("HR: %.1f | SpO2: %.1f\n", bpm, spo2);
                    }
                    if (xSemaphoreTake(datasent_mutex_max30102, portMAX_DELAY) == pdTRUE) {
                        ppg.hr = (uint16_t)(bpm * 10);
                        ppg.spo2 = (uint16_t)(spo2 * 10);
                        xSemaphoreGive(datasent_mutex_max30102);
                    }
                } else {
                    printf("FIFO READ ERROR!\n");
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// GPS task
static void gps_task(void *arg)
{
    while (1) {
        GPS_Coordinates coordinate = get_gps_coordinates();

        http_update_gps_sensor_data(coordinate.longitude, coordinate.latitude);
        update_gps_sensor_data(coordinate.longitude, coordinate.latitude);

        if (xSemaphoreTake(datasent_mutex_gps, portMAX_DELAY) == pdTRUE) {
            gps_pkt.lat = (int32_t)(coordinate.latitude * 1e6);
            gps_pkt.lon = (int32_t)(coordinate.longitude * 1e6);
            xSemaphoreGive(datasent_mutex_gps);
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

// TMP116 task
static void tmp116_task(void *arg)
{
    float temp;
    while (1) {
        int16_t raw = tmp116_read_raw();
        if (!raw) {
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }
        temp = tmp116_to_celsius(raw);

        http_update_tmp116_sensor_data(temp);
        update_tpm116_sensor_data(temp);

        if (xSemaphoreTake(datasent_mutex_tmp116, portMAX_DELAY) == pdTRUE) {
            tmp116_x100 = (int16_t)(temp * 100);
            xSemaphoreGive(datasent_mutex_tmp116);
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// LoRa message send task
static void lora_task(void *arg)
{
    esp_task_wdt_add(NULL);
    while (1) {
        if (xSemaphoreTake(datasent_mutex_gps, pdMS_TO_TICKS(100)) == pdTRUE &&
            xSemaphoreTake(datasent_mutex_max30102, pdMS_TO_TICKS(100)) == pdTRUE &&
            xSemaphoreTake(datasent_mutex_tmp116, pdMS_TO_TICKS(100)) == pdTRUE) {

            pkt.hr_x10 = ppg.hr;
            pkt.spo2_x10 = ppg.spo2;
            pkt.lat_x1e6 = gps_pkt.lat;
            pkt.lon_x1e6 = gps_pkt.lon;
            pkt.temp_x100 = tmp116_x100;

            uint8_t tx_buf[sizeof(data_packet_t)];
            memcpy(tx_buf, &pkt, sizeof(data_packet_t));
            lora_send(tx_buf, sizeof(data_packet_t));

            // Receive ACK
            uint8_t messageBuffer[32];
            uint32_t start = xTaskGetTickCount();
            bool ack_received = false;
            while ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(1000)) {
                setRxMode();
                esp_task_wdt_reset();

                uint8_t irq_flags = register_read(RFM9X_12_REG_IRQ_FLAGS);
                if (irq_flags & IRQ_RX_DONE_MASK) {

                    uint8_t length = register_read(RFM9X_13_REG_RX_NB_BYTES);
                    memset(messageBuffer, 0, sizeof(messageBuffer));

                    uint8_t fifoStart = register_read(RFM9X_10_REG_FIFO_RX_CURRENT_ADDR);
                    register_write(RFM9X_0D_REG_FIFO_ADDR_PTR, fifoStart);

                    for (int i = 0; i < length; i++) {
                        messageBuffer[i] = register_read(RFM9X_00_REG_FIFO);
                    }
                    messageBuffer[length] = '\0';

                    int remove = 4;
                    if (length > remove) {
                        memmove(messageBuffer, messageBuffer + remove, length - remove + 1);
                    }
                    printf("Message recived : %s\n", messageBuffer);
                    register_write(RFM9X_12_REG_IRQ_FLAGS, 0xFF);
                    register_write(RFM9X_0D_REG_FIFO_ADDR_PTR, register_read(RFM9X_0F_REG_FIFO_RX_BASE_ADDR));
                }
                if (strcmp((char *)messageBuffer, "ACK") == 0) {
                    ack_received = true;
                } else {
                    ack_received = false;
                }
                vTaskDelay(pdMS_TO_TICKS(5));
            }

            if (ack_received) {
                http_update_lora_status(1);
                memset(messageBuffer, 0, sizeof(messageBuffer));
            } else {
                http_update_lora_status(0);
            }

            xSemaphoreGive(datasent_mutex_gps);
            xSemaphoreGive(datasent_mutex_max30102);
            xSemaphoreGive(datasent_mutex_tmp116);
        }

        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

void setup()
{
    Serial.begin(115200);

    // Initialize watchdog with 10 second timeout
    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = 10000,
        .idle_core_mask = (1 << 0) | (1 << 1),
        .trigger_panic = false,
    };
    esp_err_t err = esp_task_wdt_init(&wdt_config);
    if (err == ESP_OK) {
        printf("TWDT initialized\n");
    } else if (err == ESP_ERR_INVALID_STATE) {
        printf("TWDT already initialized\n");
    }

    // Create mutexes
    datasent_mutex_gps = xSemaphoreCreateMutex();
    if (datasent_mutex_gps == NULL) {
        printf("Failed to create mutex for GPS data.");
        return;
    }
    datasent_mutex_max30102 = xSemaphoreCreateMutex();
    if (datasent_mutex_max30102 == NULL) {
        printf("Failed to create mutex for MAX30102 data.");
        return;
    }
    datasent_mutex_tmp116 = xSemaphoreCreateMutex();
    if (datasent_mutex_tmp116 == NULL) {
        printf("Failed to create mutex for TMP116 data.");
        return;
    }
    i2c_mutex = xSemaphoreCreateMutex();
    if (i2c_mutex == NULL) {
        ESP_LOGE("I2C", "Failed to create I2C mutex");
        return;
    }

    // I2C init
    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.setClock(I2C_FRQ);
    printf("I2C initialized\n");
    vTaskDelay(pdMS_TO_TICKS(10));

    // MAX30102 init
    err = max30102_init(&max30102);
    if (err == ESP_OK) {
        printf("MAX30102 initialized\n");
    } else {
        printf("MAX30102 initialize Failed\n");
    }
    max30102_print_registers(&max30102);

    // GPS init
    gps_init();

    // WiFi AP + HTTP dashboard
    ESP_LOGI(TAG, "ESP_WIFI_MODE_AP");
    WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS, WIFI_AP_CHANNEL, 0, WIFI_AP_MAX_CONN);
    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             WIFI_AP_SSID, WIFI_AP_PASS, WIFI_AP_CHANNEL);
    vTaskDelay(pdMS_TO_TICKS(4000));
    start_webserver();

    // TMP116 init
    tmp116_init();

    // BLE
    ble_setup();

    vTaskDelay(pdMS_TO_TICKS(3000));
    printf("Lora Init Begun\n");
    // LoRa init
    spi_init();
    vTaskDelay(pdMS_TO_TICKS(10));
    radio_init();
    printf("LoRa radio init done\n");

    // Tasks
    xTaskCreatePinnedToCore(max30102_task, "max30102_task", 4096, NULL, 10, NULL, 0);
    xTaskCreatePinnedToCore(gps_task, "gps_task", 4096, NULL, 9, NULL, 0);
    xTaskCreatePinnedToCore(tmp116_task, "tmp116_task", 4096, NULL, 8, NULL, 0);
    xTaskCreatePinnedToCore(lora_task, "lora_task", 4096, NULL, 7, NULL, 1);
}

void loop()
{
    http_server_loop();
    vTaskDelay(pdMS_TO_TICKS(2));
}
