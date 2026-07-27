#include "max30102.h"
#include <Wire.h>
#include <string.h>
#include <stdio.h>
#include "i2c_lock.h"

esp_err_t max30102_init(max30102_t *self)
{
    self->_interrupt_flag = 0;
    memset(self->_ir_samples, 0, sizeof(self->_ir_samples));
    memset(self->_red_samples, 0, sizeof(self->_red_samples));

    esp_err_t ret = max30102_write_register(self, MAX30102_FIFO_WR_PTR, 0x00);
    if (ret != ESP_OK) return ret;
    ret = max30102_write_register(self, MAX30102_MULTI_LED_CTRL_1, 0x21);
    if (ret != ESP_OK) return ret;
    ret = max30102_write_register(self, MAX30102_OVF_COUNTER, 0x00);
    if (ret != ESP_OK) return ret;
    ret = max30102_write_register(self, MAX30102_FIFO_RD_PTR, 0x00);
    if (ret != ESP_OK) return ret;
    ret = max30102_write_register(self, MAX30102_MODE_CONFIG, 0x03);
    if (ret != ESP_OK) return ret;
    ret = max30102_write_register(self, MAX30102_SPO2_CONFIG, 0x4F);  // 400 Hz, 16-bit pulse width
    if (ret != ESP_OK) return ret;
    ret = max30102_write_register(self, MAX30102_FIFO_CONFIG, 0x70);  // 8-sample averaging, rollover, fifo full 32
    if (ret != ESP_OK) return ret;
    ret = max30102_write_register(self, MAX30102_INTERRUPT_ENABLE_1, 0xC0);
    if (ret != ESP_OK) return ret;
    ret = max30102_set_led_current(self, MAX30102_LED_CURRENT_11MA, MAX30102_LED_CURRENT_11MA);
    if (ret != ESP_OK) return ret;
    printf("Max 30102 Initialized successfully\n");
    return ret;
}

esp_err_t max30102_set_led_current(max30102_t *self, max30102_current_t red_current, max30102_current_t ir_current)
{
    esp_err_t ret = max30102_write_register(self, MAX30102_LED_IR_PA1, ir_current);
    if (ret != ESP_OK) return ret;
    return max30102_write_register(self, MAX30102_LED_RED_PA2, red_current);
}

esp_err_t max30102_write_register(max30102_t *self, uint8_t address, uint8_t val)
{
    (void)self;
    Wire.beginTransmission(MAX30102_I2C_ADDR);
    Wire.write(address);
    Wire.write(val);
    return (Wire.endTransmission() == 0) ? ESP_OK : ESP_FAIL;
}

esp_err_t max30102_read_register(max30102_t *self, uint8_t address, uint8_t *reg)
{
    (void)self;
    Wire.beginTransmission(MAX30102_I2C_ADDR);
    Wire.write(address);
    if (Wire.endTransmission(false) != 0) return ESP_FAIL;

    if (Wire.requestFrom((int)MAX30102_I2C_ADDR, 1) != 1) return ESP_FAIL;
    *reg = Wire.read();
    return ESP_OK;
}

esp_err_t max30102_read_fifo(uint32_t ir[], uint32_t red[], int samples)
{
    i2c_lock();

    Wire.beginTransmission(MAX30102_I2C_ADDR);
    Wire.write(MAX30102_FIFO_DATA);
    if (Wire.endTransmission() != 0) {
        i2c_unlock();
        return ESP_FAIL;
    }

    // Burst read of FIFO (6 bytes * samples)
    int total = 6 * samples;
    uint8_t fifo_data[6 * MAX30102_SAMPLE_LEN_MAX];
    if (Wire.requestFrom((int)MAX30102_I2C_ADDR, total) != total) {
        i2c_unlock();
        return ESP_FAIL;
    }
    for (int i = 0; i < total; i++) {
        fifo_data[i] = Wire.read();
    }

    i2c_unlock();

    // Parse each 18-bit IR/RED sample
    for (int i = 0; i < samples; i++) {
        int offset = i * 6;

        ir[i]  = ((fifo_data[offset + 0] & 0x03) << 16) |
                  (fifo_data[offset + 1] << 8) |
                   fifo_data[offset + 2];

        red[i] = ((fifo_data[offset + 3] & 0x03) << 16) |
                  (fifo_data[offset + 4] << 8) |
                   fifo_data[offset + 5];
    }

    return ESP_OK;
}

esp_err_t max30102_print_registers(max30102_t *self)
{
    uint8_t int_status, int_enable, fifo_write, fifo_ovf_cnt, fifo_read;
    uint8_t fifo_data, mode_conf, sp02_conf, led_conf, temp_int;
    uint8_t rev_id;
    esp_err_t ret;

    ret = max30102_read_register(self, MAX30102_INTERRUPT_STATUS_1, &int_status);
    if (ret != ESP_OK) return ret;
    ret = max30102_read_register(self, MAX30102_INTERRUPT_ENABLE_1, &int_enable);
    if (ret != ESP_OK) return ret;
    ret = max30102_read_register(self, MAX30102_FIFO_WR_PTR, &fifo_write);
    if (ret != ESP_OK) return ret;
    ret = max30102_read_register(self, MAX30102_OVF_COUNTER, &fifo_ovf_cnt);
    if (ret != ESP_OK) return ret;
    ret = max30102_read_register(self, MAX30102_FIFO_RD_PTR, &fifo_read);
    if (ret != ESP_OK) return ret;
    ret = max30102_read_register(self, MAX30102_FIFO_DATA, &fifo_data);
    if (ret != ESP_OK) return ret;
    ret = max30102_read_register(self, MAX30102_MODE_CONFIG, &mode_conf);
    if (ret != ESP_OK) return ret;
    ret = max30102_read_register(self, MAX30102_SPO2_CONFIG, &sp02_conf);
    if (ret != ESP_OK) return ret;
    ret = max30102_read_register(self, MAX30102_LED_IR_PA1, &led_conf);
    if (ret != ESP_OK) return ret;
    ret = max30102_read_register(self, MAX30102_LED_RED_PA2, &temp_int);
    if (ret != ESP_OK) return ret;
    // NOTE: original firmware reads MAX30102_I2C_ADDR (0x57) as if it were a register address here
    // (likely meant the Part ID register, 0xFF) - kept as-is, this is a cosmetic debug print only.
    ret = max30102_read_register(self, MAX30102_I2C_ADDR, &rev_id);
    if (ret != ESP_OK) return ret;

    printf("Status\t Enable\t FIFO Wrt\t FIFO Ovf Cnt\t FIFO Read\t FIFO Data\t Mode Conf\t Spo2 Conf\t LED Conf\t Temp Conf\t Rev Id \n");
    printf("%x\t\t", int_status);
    printf("%x\t\t", int_enable);
    printf("%x\t\t", fifo_write);
    printf("%x\t\t", fifo_ovf_cnt);
    printf("%x\t\t", fifo_read);
    printf("%x\t\t", fifo_data);
    printf("%x\t\t", mode_conf);
    printf("%x\t\t", sp02_conf);
    printf("%x\t\t", led_conf);
    printf("%x\t\t", temp_int);
    printf("%x\n", rev_id);

    return ESP_OK;
}
