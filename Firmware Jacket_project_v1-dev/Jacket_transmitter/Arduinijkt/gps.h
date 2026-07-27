#ifndef GPS_H
#define GPS_H

typedef struct {
    double latitude;
    double longitude;
} GPS_Coordinates;

void gps_init(void);
void gps_start(void);
void parse_gpgll(const char *nmea);
GPS_Coordinates get_gps_coordinates(void);

#endif // GPS_H
