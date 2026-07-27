#ifndef GPS_H_
#define GPS_H_

#include <stdint.h>

#define GPS_BAUD 9600 
//esp32 c3 pins 
//#define RXD 3         
//#define TXD 4  
//esp32 s3 pins 
#define RXD  2        
#define TXD  3
      
#define BUF_SIZE 1024   
#define GPS_UART_PORT UART_NUM_1

typedef struct {
    double latitude;
    double longitude;
} GPS_Coordinates;



void gps_init(void);
char* getData(void);
void gps_start(void);
void parse_gpgll(const char *nmea);
GPS_Coordinates get_gps_coordinates(void);

#endif /* GPS_H_ */

