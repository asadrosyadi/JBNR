#ifndef BLE_SERVER_H
#define BLE_SERVER_H

void ble_setup(void);

// Sensor data update functions
void update_max30102_sensor_data(float bpm, float spo2);
void update_gps_sensor_data(float lat, float lon);
void update_tpm116_sensor_data(float temp);
void update_user_text(const char *text);
void update_sensor_data_and_notify(float bpm, float spo2, float lat, float lon, float temp);
void send_notification_to_all_services(void);

#endif // BLE_SERVER_H
