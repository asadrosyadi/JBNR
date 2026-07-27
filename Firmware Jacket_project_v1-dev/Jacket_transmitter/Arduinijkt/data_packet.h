#ifndef DATA_PACKET_H
#define DATA_PACKET_H

#include <stdint.h>

#pragma pack(push, 1)
typedef struct
{
    int32_t  lat_x1e6;     // GPS latitude  * 1e6
    int32_t  lon_x1e6;     // GPS longitude * 1e6
    int16_t  temp_x100;    // Temperature (deg C * 100)
    uint16_t hr_x10;       // Heart rate (bpm * 10)
    uint16_t spo2_x10;     // SpO2 (% * 10)
} data_packet_t;
#pragma pack(pop)

static_assert(sizeof(data_packet_t) == 14, "data_packet_t size mismatch");

#endif // DATA_PACKET_H
