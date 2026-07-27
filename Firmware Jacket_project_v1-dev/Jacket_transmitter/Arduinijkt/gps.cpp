#include "gps.h"
#include "config.h"
#include <Arduino.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "esp_log.h"

static char nmeaLine[GPS_BUF_SIZE] = {0};
static GPS_Coordinates lastCoords = {0.0, 0.0};

void gps_init(void)
{
    Serial1.begin(GPS_BAUD, SERIAL_8N1, GPS_RXD_PIN, GPS_TXD_PIN);
}

void gps_start(void)
{
    while (Serial1.available()) {
        char inChar = (char)Serial1.read();

        // Accumulate NMEA sentence
        if (inChar != '\n' && inChar != '\r') {
            strncat(nmeaLine, &inChar, 1);
        }

        // When newline is received, process the sentence
        if (inChar == '\n') {
            parse_gpgll(nmeaLine);
            memset(nmeaLine, 0, sizeof(nmeaLine));
        }
    }
}

void parse_gpgll(const char *nmea)
{
    char latitude[16] = {0};
    char longitude[16] = {0};
    char lat_dir = 0, lon_dir = 0;
    char utc_time[16] = {0};
    char status = 0, mode = 0;

    // Accept ANY talker ID: GPGLL, GNGLL, BDGLL, GAGLL
    int fields = sscanf(nmea,
               "$%*[^,],%15[^,],%c,%15[^,],%c,%15[^,],%c,%c",
               latitude, &lat_dir, longitude, &lon_dir,
               utc_time, &status, &mode);

    if (fields < 7) {
        return;
    }

    if (status != 'A') {
        return;
    }

    double lat_raw = atof(latitude);
    double lon_raw = atof(longitude);

    int lat_deg = (int)(lat_raw / 100);
    double lat_min = lat_raw - lat_deg * 100;
    double lat_dd = lat_deg + lat_min / 60.0;

    int lon_deg = (int)(lon_raw / 100);
    double lon_min = lon_raw - lon_deg * 100;
    double lon_dd = lon_deg + lon_min / 60.0;

    if (lat_dir == 'S') lat_dd = -lat_dd;
    if (lon_dir == 'W') lon_dd = -lon_dd;

    lastCoords.latitude = lat_dd;
    lastCoords.longitude = lon_dd;

    ESP_LOGI("GPS", "Latitude:  %.6f %c", lat_dd, lat_dir);
    ESP_LOGI("GPS", "Longitude: %.6f %c", lon_dd, lon_dir);
}

GPS_Coordinates get_gps_coordinates(void)
{
    gps_start();
    return lastCoords;
}
