#ifndef VITALS_ALGO_H
#define VITALS_ALGO_H

#include <stdint.h>

float calculate_spo2(uint16_t red[], uint16_t ir[], int size);
float calculate_heart_rate(uint16_t ir[], int size);

#endif // VITALS_ALGO_H
