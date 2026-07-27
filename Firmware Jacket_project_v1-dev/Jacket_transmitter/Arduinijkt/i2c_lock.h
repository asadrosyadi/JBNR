#ifndef I2C_LOCK_H
#define I2C_LOCK_H

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

extern SemaphoreHandle_t i2c_mutex;

void i2c_lock();
void i2c_unlock();

#endif // I2C_LOCK_H
