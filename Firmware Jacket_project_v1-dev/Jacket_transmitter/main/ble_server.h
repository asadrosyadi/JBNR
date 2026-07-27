#ifndef BLE_SERVER_H
#define BLE_SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gatt_common_api.h"

#define GATTS_TAG "GATTS_SERVER"

// Service UUIDs - Updated for combined characteristics
#define GATTS_SERVICE_UUID_TEST_A     0x00FF  // HR & SpO2
#define GATTS_CHAR_UUID_TEST_A_VITALS 0xFF01  //  HR & SpO2 characteristic
#define GATTS_NUM_HANDLE_TEST_A       4       

#define GATTS_SERVICE_UUID_TEST_B     0x00EE  //Lat & Lon
#define GATTS_CHAR_UUID_TEST_B_LOCATION 0xEE01 //Lat & Lon characteristic
#define GATTS_NUM_HANDLE_TEST_B       4       

#define GATTS_SERVICE_UUID_TEST_C     0x00DD  // Temperature Service
#define GATTS_CHAR_UUID_TEST_C_TEMP   0xDD01  // Temperature characteristic
#define GATTS_NUM_HANDLE_TEST_C       4     

#define GATTS_SERVICE_UUID_TEST_D     0x00CC  // Text Service
#define GATTS_CHAR_UUID_TEST_D_TEXT   0xCC01  // Text characteristic
#define GATTS_NUM_HANDLE_TEST_D       4       

#define TEST_DEVICE_NAME              "ESP-BLE"
#define PREPARE_BUF_MAX_SIZE          1024

#define PROFILE_NUM 4
#define PROFILE_A_APP_ID 0
#define PROFILE_B_APP_ID 1
#define PROFILE_C_APP_ID 2
#define PROFILE_D_APP_ID 3

#define adv_config_flag      (1 << 0)
#define scan_rsp_config_flag (1 << 1)

// Structure definitions
typedef struct {
    uint8_t *prepare_buf;
    int prepare_len;
} prepare_type_env_t;

// Updated gatts_profile_inst structure for combined characteristics
struct gatts_profile_inst {
    esp_gatts_cb_t gatts_cb;
    uint16_t gatts_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_handle;
    esp_gatt_srvc_id_t service_id;
    
    // Characteristic handle (only one per service now)
    uint16_t char_handle;
    esp_bt_uuid_t char_uuid;
    esp_gatt_char_prop_t property;
    
    // Combined CCCD values and descriptor handles
    // Service A (Vitals - HR & SpO2)
    uint16_t cccd_value_vitals;
    uint16_t descr_handle_vitals;
    
    // Service B (Location - Lat & Lon)
    uint16_t cccd_value_location;
    uint16_t descr_handle_location;
    
    // Service C (Temperature)
    uint16_t cccd_value_temp;
    uint16_t descr_handle_temp;
    
    // Service D (Text)
    uint16_t cccd_value_text;
    uint16_t descr_handle_text;
};

// Extern declarations for global variables
extern struct gatts_profile_inst gl_profile_tab[PROFILE_NUM];
extern uint8_t adv_config_done;
extern esp_ble_adv_data_t adv_data;
extern esp_ble_adv_data_t scan_rsp_data;
extern esp_ble_adv_params_t adv_params;

// Prepare write environments
extern prepare_type_env_t a_prepare_write_env;
extern prepare_type_env_t b_prepare_write_env;
extern prepare_type_env_t c_prepare_write_env;
extern prepare_type_env_t d_prepare_write_env;

// Event handlers
void gatts_profile_a_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
void gatts_profile_b_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
void gatts_profile_c_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
void gatts_profile_d_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

// Helper functions
void example_write_event_env(esp_gatt_if_t gatts_if, prepare_type_env_t *prepare_write_env, esp_ble_gatts_cb_param_t *param);
void example_exec_write_event_env(prepare_type_env_t *prepare_write_env, esp_ble_gatts_cb_param_t *param);
void float_to_string(float value, char* buffer, int buffer_size);

// Event handlers from main
void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
void sensor_update_task(void *pvParameters);

// Sensor data update functions
void update_max30102_sensor_data(float bpm, float spo2);
void update_gps_sensor_data(float lat, float lon);
void update_tpm116_sensor_data(float temp);
void update_user_text(const char* text);
void update_sensor_data_and_notify(float bpm, float spo2, float lat, float lon, float temp);
void send_notification_to_all_services(void);

#endif // BLE_SERVER_H