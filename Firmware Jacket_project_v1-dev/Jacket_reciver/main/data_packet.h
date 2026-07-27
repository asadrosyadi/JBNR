/*
 * data_packet.h
 *
 *  Created on: Dec 18, 2025
 *      Author: panka
 */

#ifndef MAIN_DATA_PACKET_H_
#define MAIN_DATA_PACKET_H_

#include <stdint.h>

#pragma pack(push,1)
typedef struct
{
    int32_t  lat_x1e6;     // GPS latitude
    int32_t  lon_x1e6;     // GPS longitude
    int16_t  temp_x100;    // Temperature (°C * 100)
    uint16_t hr_x10;       // Heart rate (bpm * 10)
    uint16_t spo2_x10;     // SpO2 (% * 10)
} data_packet_t;
#pragma pack(pop)

_Static_assert(sizeof(data_packet_t) == 14,
               "sensor_payload_t size mismatch");


#endif /* MAIN_DATA_PACKET_H_ */
