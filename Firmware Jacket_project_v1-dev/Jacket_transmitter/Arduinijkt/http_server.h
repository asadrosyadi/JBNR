#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

// Functions to update sensor data
void http_update_gps_sensor_data(float longitude, float latitude);
void http_update_sp02_sensor_data(float bpm, float sp02);
void http_update_finger_status(bool val);
void http_update_lora_status(bool val);
void http_update_tmp116_sensor_data(float temp);

// Start the webserver and pump its event loop
void start_webserver(void);
void http_server_loop(void);

#endif // HTTP_SERVER_H
