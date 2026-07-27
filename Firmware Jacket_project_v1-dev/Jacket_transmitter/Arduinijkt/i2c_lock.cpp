#include "i2c_lock.h"

SemaphoreHandle_t i2c_mutex = NULL;

void i2c_lock()
{
    if (i2c_mutex == NULL) {
        i2c_mutex = xSemaphoreCreateMutex();
    }
    xSemaphoreTake(i2c_mutex, portMAX_DELAY);
}

void i2c_unlock()
{
    xSemaphoreGive(i2c_mutex);
}
