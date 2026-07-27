#include <stdio.h>

#define TMP116_ADDR     0x49     
#define TMP116_TEMP_REG 0x00
#define TMP116_CONFIG_REG 0x01
#define I2C_PORT           I2C_NUM_0

void tmp116_init(void);
int16_t tmp116_read_raw(void);
float tmp116_to_celsius(int16_t raw);
