#include "tmp116.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2c_lock.h"
#include <stdint.h>

static const char *TAG = "TMP116";

void tmp116_init(void)
{
    uint8_t config_data[3] = {
        TMP116_CONFIG_REG,
        0x02,   // MSB config
        0x20    // LSB config
    };

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (TMP116_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, config_data, sizeof(config_data), true);
    i2c_master_stop(cmd);
    esp_err_t err = i2c_master_cmd_begin(I2C_PORT, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "TMP116 init failed: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "TMP116 initialized");
    }
}

int16_t tmp116_read_raw(void)
{	//i2c_lock();
    esp_err_t err;
    uint8_t reg = TMP116_TEMP_REG;
    uint8_t data[2];
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (TMP116_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_stop(cmd);

    err = i2c_master_cmd_begin(I2C_PORT, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "TMP116 reg write failed: %s", esp_err_to_name(err));
       // return err;
       return 0;
    }
    vTaskDelay(pdMS_TO_TICKS(2));  

    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (TMP116_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read(cmd, data, 2, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);

    err = i2c_master_cmd_begin(I2C_PORT, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
	
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "TMP116 read failed: %s", esp_err_to_name(err));
        return 0;
    }
	return (int16_t)((data[0] << 8) | data[1]);
	//i2c_unlock();
}


float tmp116_to_celsius(int16_t raw)
{
    return raw * 0.0078125f;
}