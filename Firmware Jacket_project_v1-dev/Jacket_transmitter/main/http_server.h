/*
 * http_server.h
 *
 *  Created on: Nov 20, 2025
 *      Author: panka
 */

#ifndef MAIN_HTTP_SERVER_H_
#define MAIN_HTTP_SERVER_H_


#include "esp_http_server.h"

// Functions to update sensor data
void http_update_gps_sensor_data(float longitude, float latitude);
void http_update_sp02_sensor_data(float bpm, float sp02);
void http_update_finger_status(bool val);
void http_update_lora_status(bool val);
void http_update_tmp116_sensor_data(float temp);

// Start and stop the webserver
httpd_handle_t start_webserver(void);
void stop_webserver(httpd_handle_t server);

#endif /* MAIN_HTTP_SERVER_H_ */
