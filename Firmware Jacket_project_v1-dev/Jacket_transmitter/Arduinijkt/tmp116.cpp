#include "tmp116.h"
#include <Wire.h>
#include "esp_log.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "i2c_lock.h"

static const char *TAG = "TMP116";

void tmp116_init(void)
{
    Wire.beginTransmission(TMP116_ADDR);
    Wire.write(TMP116_CONFIG_REG);
    Wire.write(0x02);   // MSB config
    Wire.write(0x20);   // LSB config
    uint8_t err = Wire.endTransmission();

    if (err != 0) {
        ESP_LOGE(TAG, "TMP116 init failed: %d", err);
    } else {
        ESP_LOGI(TAG, "TMP116 initialized");
    }
}

int16_t tmp116_read_raw(void)
{
    i2c_lock();

    Wire.beginTransmission(TMP116_ADDR);
    Wire.write(TMP116_TEMP_REG);
    uint8_t err = Wire.endTransmission();

    if (err != 0) {
        ESP_LOGE(TAG, "TMP116 reg write failed: %d", err);
        i2c_unlock();
        return 0;
    }
    vTaskDelay(pdMS_TO_TICKS(2));

    if (Wire.requestFrom((int)TMP116_ADDR, 2) != 2) {
        ESP_LOGE(TAG, "TMP116 read failed");
        i2c_unlock();
        return 0;
    }
    uint8_t data[2];
    data[0] = Wire.read();
    data[1] = Wire.read();

    i2c_unlock();
    return (int16_t)((data[0] << 8) | data[1]);
}

float tmp116_to_celsius(int16_t raw)
{
    return raw * 0.0078125f;
}
