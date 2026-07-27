#ifndef MAX30102_H
#define MAX30102_H

#include <stdint.h>
#include "esp_err.h"

#define MAX30102_I2C_ADDR 0x57

#define MAX30102_SAMPLE_LEN_MAX 32

#define MAX30102_INTERRUPT_STATUS_1 0x00
#define MAX30102_INTERRUPT_STATUS_2 0x01
#define MAX30102_INTERRUPT_ENABLE_1 0x02
#define MAX30102_INTERRUPT_ENABLE_2 0x03

#define MAX30102_FIFO_WR_PTR 0x04
#define MAX30102_OVF_COUNTER 0x05
#define MAX30102_FIFO_RD_PTR 0x06
#define MAX30102_FIFO_DATA   0x07

#define MAX30102_FIFO_CONFIG 0x08
#define MAX30102_MODE_CONFIG 0x09
#define MAX30102_SPO2_CONFIG 0x0a

#define MAX30102_LED_IR_PA1  0x0c
#define MAX30102_LED_RED_PA2 0x0d
#define MAX30102_MULTI_LED_CTRL_1 0x11
#define MAX30102_MULTI_LED_CTRL_2 0x12

typedef enum LEDCurrent {
    MAX30102_LED_CURRENT_11MA = 0x37,
} max30102_current_t;

typedef struct max30102_t
{
    uint32_t _ir_samples[MAX30102_SAMPLE_LEN_MAX];
    uint32_t _red_samples[MAX30102_SAMPLE_LEN_MAX];
    uint8_t _interrupt_flag;
} max30102_t;

esp_err_t max30102_init(max30102_t *self);
esp_err_t max30102_write_register(max30102_t *self, uint8_t address, uint8_t val);
esp_err_t max30102_read_register(max30102_t *self, uint8_t address, uint8_t *reg);
esp_err_t max30102_set_led_current(max30102_t *self, max30102_current_t red_current, max30102_current_t ir_current);
esp_err_t max30102_read_fifo(uint32_t ir[], uint32_t red[], int samples);
esp_err_t max30102_print_registers(max30102_t *self);

#endif // MAX30102_H
