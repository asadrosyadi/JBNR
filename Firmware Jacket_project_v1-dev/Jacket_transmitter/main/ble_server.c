#include "ble_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "string.h"
#include <stdlib.h>
#include <stdio.h>

#define GATTS_TAG "GATTS_SERVER"
#define ESP_GATT_UUID_CHAR_CLIENT_CONFIG 0x2902

// Static variables for sensor data
static float heart_rate = 0.0f;
static float spo2_level = 0.0f;
static float latitude = 0.0f;
static float longitude = 0.0f;
static float temperature = 0.0f;
static char user_text[100] = "Initial Text";

// Advertising data
uint8_t adv_service_uuid128[48] = {
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xEE, 0x00, 0x00, 0x00,
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00,
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xCC, 0x00, 0x00, 0x00,
};

esp_ble_adv_data_t adv_data = {
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = false,
    .min_interval = 0x0006,
    .max_interval = 0x0010,
    .appearance = 0x00,
    .manufacturer_len = 0,
    .p_manufacturer_data = NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = sizeof(adv_service_uuid128),
    .p_service_uuid = adv_service_uuid128,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

esp_ble_adv_data_t scan_rsp_data = {
    .set_scan_rsp = true,
    .include_name = true,
    .include_txpower = true,
    .appearance = 0x00,
    .manufacturer_len = 0,
    .p_manufacturer_data = NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = sizeof(adv_service_uuid128),
    .p_service_uuid = adv_service_uuid128,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

esp_ble_adv_params_t adv_params = {
    .adv_int_min = 0x20,
    .adv_int_max = 0x40,
    .adv_type = ADV_TYPE_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

// Characteristic values
static uint8_t vitals_char_str[] = {0x11, 0x22, 0x33};  // HR & SpO2
static uint8_t location_char_str[] = {0x44, 0x55, 0x66}; // Lat & Lon
static uint8_t temp_char_str[] = {0x77, 0x88, 0x99};
static uint8_t text_char_str[] = {0xAA, 0xBB, 0xCC};

static esp_attr_value_t gatts_char_vitals_val = {
    .attr_max_len = 0x40,
    .attr_len = sizeof(vitals_char_str),
    .attr_value = vitals_char_str,
};

static esp_attr_value_t gatts_char_location_val = {
    .attr_max_len = 0x40,
    .attr_len = sizeof(location_char_str),
    .attr_value = location_char_str,
};

static esp_attr_value_t gatts_char_temp_val = {
    .attr_max_len = 0x40,
    .attr_len = sizeof(temp_char_str),
    .attr_value = temp_char_str,
};

static esp_attr_value_t gatts_char_text_val = {
    .attr_max_len = 0x40,
    .attr_len = sizeof(text_char_str),
    .attr_value = text_char_str,
};

// Profile table initialization
struct gatts_profile_inst gl_profile_tab[PROFILE_NUM] = {
    [PROFILE_A_APP_ID] = {
        .gatts_cb = gatts_profile_a_event_handler,
        .gatts_if = ESP_GATT_IF_NONE,
        .app_id = PROFILE_A_APP_ID,
        .conn_id = 0,
        .service_handle = 0,
        .char_handle = 0,
        .cccd_value_vitals = 0,  // Combined CCCD for HR & SpO2
        .descr_handle_vitals = 0,
    },
    [PROFILE_B_APP_ID] = {
        .gatts_cb = gatts_profile_b_event_handler,
        .gatts_if = ESP_GATT_IF_NONE,
        .app_id = PROFILE_B_APP_ID,
        .conn_id = 0,
        .service_handle = 0,
        .char_handle = 0,
        .cccd_value_location = 0,  // Combined CCCD for Lat & Lon
        .descr_handle_location = 0,
    },
    [PROFILE_C_APP_ID] = {
        .gatts_cb = gatts_profile_c_event_handler,
        .gatts_if = ESP_GATT_IF_NONE,
        .app_id = PROFILE_C_APP_ID,
        .conn_id = 0,
        .service_handle = 0,
        .char_handle = 0,
        .cccd_value_temp = 0,
        .descr_handle_temp = 0,
    },
    [PROFILE_D_APP_ID] = {
        .gatts_cb = gatts_profile_d_event_handler,
        .gatts_if = ESP_GATT_IF_NONE,
        .app_id = PROFILE_D_APP_ID,
        .conn_id = 0,
        .service_handle = 0,
        .char_handle = 0,
        .cccd_value_text = 0,
        .descr_handle_text = 0,
    },
};

// Configuration flags
uint8_t adv_config_done = 0;
#define adv_config_flag      (1 << 0)
#define scan_rsp_config_flag (1 << 1)

// Prepare write environments
prepare_type_env_t a_prepare_write_env;
prepare_type_env_t b_prepare_write_env;
prepare_type_env_t c_prepare_write_env;
prepare_type_env_t d_prepare_write_env;

// Connection ID for notifications
static uint16_t conn_id = 0;

// Function to send notifications with CCCD checking
void send_notification_to_all_services(void) {
    char notification_data[100];
    
    // Send notification for Service A (MAX30102 - Vitals: HR & SpO2)
    if (gl_profile_tab[PROFILE_A_APP_ID].char_handle != 0 && 
        gl_profile_tab[PROFILE_A_APP_ID].cccd_value_vitals & 0x0001) {
        int len = snprintf(notification_data, sizeof(notification_data), "HR:%.2f,SpO2:%.2f", heart_rate, spo2_level);
        esp_ble_gatts_send_indicate(gl_profile_tab[PROFILE_A_APP_ID].gatts_if, 
                                    gl_profile_tab[PROFILE_A_APP_ID].conn_id,
                                    gl_profile_tab[PROFILE_A_APP_ID].char_handle,
                                    len, (uint8_t*)notification_data, false);
        ESP_LOGI(GATTS_TAG, "Sent Vitals notification: HR=%.2f, SpO2=%.2f", heart_rate, spo2_level);
    }
    
    // Send notification for Service B (GPS - Location: Lat & Lon)
    if (gl_profile_tab[PROFILE_B_APP_ID].char_handle != 0 && 
        gl_profile_tab[PROFILE_B_APP_ID].cccd_value_location & 0x0001) {
        int len = snprintf(notification_data, sizeof(notification_data), "%.6f,%.6f", latitude, longitude);
        esp_ble_gatts_send_indicate(gl_profile_tab[PROFILE_B_APP_ID].gatts_if, 
                                    gl_profile_tab[PROFILE_B_APP_ID].conn_id,
                                    gl_profile_tab[PROFILE_B_APP_ID].char_handle,
                                    len, (uint8_t*)notification_data, false);
        ESP_LOGI(GATTS_TAG, "Sent Location notification: Lat=%.6f, Lon=%.6f", latitude, longitude);
    }
    
    // Send notification for Service C (Temperature)
    if (gl_profile_tab[PROFILE_C_APP_ID].char_handle != 0 && 
        gl_profile_tab[PROFILE_C_APP_ID].cccd_value_temp & 0x0001) {
        
        int len = snprintf(notification_data, sizeof(notification_data), "Temp:%.2f", temperature);
        esp_ble_gatts_send_indicate(gl_profile_tab[PROFILE_C_APP_ID].gatts_if, 
                                    gl_profile_tab[PROFILE_C_APP_ID].conn_id,
                                    gl_profile_tab[PROFILE_C_APP_ID].char_handle,
                                    len, (uint8_t*)notification_data, false);
        ESP_LOGI(GATTS_TAG, "Sent Temperature notification: %.2f", temperature);
    }
    
    // Send notification for Service D (Text)
    if (gl_profile_tab[PROFILE_D_APP_ID].char_handle != 0 && 
        gl_profile_tab[PROFILE_D_APP_ID].cccd_value_text & 0x0001) {
        
        int len = snprintf(notification_data, sizeof(notification_data), "Text:%s", user_text);
        esp_ble_gatts_send_indicate(gl_profile_tab[PROFILE_D_APP_ID].gatts_if, 
                                    gl_profile_tab[PROFILE_D_APP_ID].conn_id,
                                    gl_profile_tab[PROFILE_D_APP_ID].char_handle,
                                    len, (uint8_t*)notification_data, false);
        ESP_LOGI(GATTS_TAG, "Sent Text notification: %s", user_text);
    }
}

// Function to update sensor data and send notifications
void update_sensor_data_and_notify(float bpm, float spo2, float lat, float lon, float temp) {
    // Update all sensor data
    heart_rate = bpm;
    spo2_level = spo2;
    latitude = lat;
    longitude = lon;
    temperature = temp;
    
    // Send notifications
    send_notification_to_all_services();
}

void update_max30102_sensor_data(float bpm, float spo2) {
    heart_rate = bpm;
    spo2_level = spo2;
    send_notification_to_all_services();
}

void update_gps_sensor_data(float lat, float lon) {
    latitude = lat;
    longitude = lon;
    //send_notification_to_all_services();
}

void update_tpm116_sensor_data(float temp) {
    temperature = temp;
   // send_notification_to_all_services();
}

void update_user_text(const char* text) {
    strncpy(user_text, text, sizeof(user_text) - 1);
    user_text[sizeof(user_text) - 1] = '\0';
    send_notification_to_all_services();
}

// Helper functions
void example_write_event_env(esp_gatt_if_t gatts_if, prepare_type_env_t *prepare_write_env, esp_ble_gatts_cb_param_t *param) {
    esp_gatt_status_t status = ESP_GATT_OK;
    if (param->write.need_rsp) {
        if (param->write.is_prep) {
            if (param->write.offset > PREPARE_BUF_MAX_SIZE) {
                status = ESP_GATT_INVALID_OFFSET;
            } else if ((param->write.offset + param->write.len) > PREPARE_BUF_MAX_SIZE) {
                status = ESP_GATT_INVALID_ATTR_LEN;
            }
            if (status == ESP_GATT_OK && prepare_write_env->prepare_buf == NULL) {
                prepare_write_env->prepare_buf = (uint8_t *)malloc(PREPARE_BUF_MAX_SIZE * sizeof(uint8_t));
                prepare_write_env->prepare_len = 0;
                if (prepare_write_env->prepare_buf == NULL) {
                    ESP_LOGE(GATTS_TAG, "Gatt_server prep no mem");
                    status = ESP_GATT_NO_RESOURCES;
                }
            }

            esp_gatt_rsp_t *gatt_rsp = (esp_gatt_rsp_t *)malloc(sizeof(esp_gatt_rsp_t));
            if (gatt_rsp) {
                gatt_rsp->attr_value.len = param->write.len;
                gatt_rsp->attr_value.handle = param->write.handle;
                gatt_rsp->attr_value.offset = param->write.offset;
                gatt_rsp->attr_value.auth_req = ESP_GATT_AUTH_REQ_NONE;
                memcpy(gatt_rsp->attr_value.value, param->write.value, param->write.len);
                esp_err_t response_err = esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, status, gatt_rsp);
                if (response_err != ESP_OK) {
                    ESP_LOGE(GATTS_TAG, "Send response error\n");
                }
                free(gatt_rsp);
            } else {
                ESP_LOGE(GATTS_TAG, "malloc failed, no resource to send response error\n");
                status = ESP_GATT_NO_RESOURCES;
            }
            if (status != ESP_GATT_OK) {
                return;
            }
            memcpy(prepare_write_env->prepare_buf + param->write.offset,
                   param->write.value,
                   param->write.len);
            prepare_write_env->prepare_len += param->write.len;
        } else {
            esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, status, NULL);
        }
    }
}

void example_exec_write_event_env(prepare_type_env_t *prepare_write_env, esp_ble_gatts_cb_param_t *param) {
    if (param->exec_write.exec_write_flag == ESP_GATT_PREP_WRITE_EXEC) {
        esp_log_buffer_hex(GATTS_TAG, prepare_write_env->prepare_buf, prepare_write_env->prepare_len);
    } else {
        ESP_LOGI(GATTS_TAG, "ESP_GATT_PREP_WRITE_CANCEL");
    }
    if (prepare_write_env->prepare_buf) {
        free(prepare_write_env->prepare_buf);
        prepare_write_env->prepare_buf = NULL;
    }
    prepare_write_env->prepare_len = 0;
}

void float_to_string(float value, char* buffer, int buffer_size) {
    snprintf(buffer, buffer_size, "%.4f", value);
}

// Handle CCCD (Client Characteristic Configuration Descriptor) writes
static void handle_cccd_write(esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param, 
                              uint16_t *cccd_value, uint16_t char_handle) {
    if (param->write.len == 2) {
        *cccd_value = param->write.value[0] | (param->write.value[1] << 8);
        ESP_LOGI(GATTS_TAG, "CCCD value: 0x%04x", *cccd_value);
        
        if (*cccd_value == 0x0001) {
            ESP_LOGI(GATTS_TAG, "Notifications enabled for handle %d", char_handle);
        } else if (*cccd_value == 0x0002) {
            ESP_LOGI(GATTS_TAG, "Indications enabled for handle %d", char_handle);
        } else if (*cccd_value == 0x0003) {
            ESP_LOGI(GATTS_TAG, "Notifications and Indications enabled for handle %d", char_handle);
        } else if (*cccd_value == 0x0000) {
            ESP_LOGI(GATTS_TAG, "Notifications/Indications disabled for handle %d", char_handle);
        }
    }
}

// Service A: MAX30102 Event Handler
void gatts_profile_a_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
    switch (event) {
        case ESP_GATTS_REG_EVT:
            ESP_LOGI(GATTS_TAG, "Vitals Service REGISTER_APP_EVT, status %d, app_id %d", param->reg.status, param->reg.app_id);
            gl_profile_tab[PROFILE_A_APP_ID].service_id.is_primary = true;
            gl_profile_tab[PROFILE_A_APP_ID].service_id.id.inst_id = 0x00;
            gl_profile_tab[PROFILE_A_APP_ID].service_id.id.uuid.len = ESP_UUID_LEN_16;
            gl_profile_tab[PROFILE_A_APP_ID].service_id.id.uuid.uuid.uuid16 = GATTS_SERVICE_UUID_TEST_A;

            esp_err_t set_dev_name_ret = esp_ble_gap_set_device_name(TEST_DEVICE_NAME);
            if (set_dev_name_ret) {
                ESP_LOGE(GATTS_TAG, "set device name failed, error code = %x", set_dev_name_ret);
            }

            // Config adv data
            esp_err_t ret = esp_ble_gap_config_adv_data(&adv_data);
            if (ret) {
                ESP_LOGE(GATTS_TAG, "config adv data failed, error code = %x", ret);
            }
            adv_config_done |= adv_config_flag;
            
            // Config scan response data
            ret = esp_ble_gap_config_adv_data(&scan_rsp_data);
            if (ret) {
                ESP_LOGE(GATTS_TAG, "config scan response data failed, error code = %x", ret);
            }
            adv_config_done |= scan_rsp_config_flag;

            esp_ble_gatts_create_service(gatts_if, &gl_profile_tab[PROFILE_A_APP_ID].service_id, GATTS_NUM_HANDLE_TEST_A);
            break;

		case ESP_GATTS_READ_EVT: {
		    ESP_LOGI(GATTS_TAG,
		             "Vitals READ_EVT, conn_id %d, trans_id %" PRIu32 ", handle %d",
		             param->read.conn_id, param->read.trans_id, param->read.handle);
		
		    // Mandatory offset check for BLE long reads
		    if (param->read.offset != 0) {
		        esp_ble_gatts_send_response(
		            gatts_if,
		            param->read.conn_id,
		            param->read.trans_id,
		            ESP_GATT_INVALID_OFFSET,
		            NULL
		        );
		        break;
		    }
		
		    esp_gatt_rsp_t rsp;
		    memset(&rsp, 0, sizeof(rsp));
		
		    rsp.attr_value.handle = param->read.handle;
		    rsp.attr_value.auth_req = ESP_GATT_AUTH_REQ_NONE;
		
		    // Prepare HR & SpO2 strings manually
		    char hr_str[16];
		    char spo2_str[16];
		
		    // Format the floats directly
		    snprintf(hr_str, sizeof(hr_str), "%.2f", heart_rate);
		    snprintf(spo2_str, sizeof(spo2_str), "%.2f", spo2_level);
		
		    // Build final BLE string
		    rsp.attr_value.len = snprintf(
		        (char *)rsp.attr_value.value,
		        sizeof(rsp.attr_value.value),
		        "HR:%s,SpO2:%s",
		        hr_str,
		        spo2_str
		    );
		
		    // Send final response
		    esp_ble_gatts_send_response(
		        gatts_if,
		        param->read.conn_id,
		        param->read.trans_id,
		        ESP_GATT_OK,
		        &rsp
		    );
		
		    break;
		}


        case ESP_GATTS_WRITE_EVT: {
            ESP_LOGI(GATTS_TAG, "Vitals WRITE_EVT, conn_id %d, trans_id %" PRIu32 ", handle %d",
                     param->write.conn_id, param->write.trans_id, param->write.handle);

            if (!param->write.is_prep) {
                ESP_LOGI(GATTS_TAG, "GATT_WRITE_EVT, value len %d, value:", param->write.len);
                esp_log_buffer_hex(GATTS_TAG, param->write.value, param->write.len);

                // Check if this is a CCCD write
                if (param->write.handle == gl_profile_tab[PROFILE_A_APP_ID].char_handle + 1) {
                    handle_cccd_write(gatts_if, param, &gl_profile_tab[PROFILE_A_APP_ID].cccd_value_vitals, 
                                     gl_profile_tab[PROFILE_A_APP_ID].char_handle);
                } else {
                    // If text is written to Vitals service, update static variables
                    if (param->write.len > 0) {
                        char received_text[100];
                        memcpy(received_text, param->write.value, param->write.len);
                        received_text[param->write.len] = '\0';

                        // Parse received data (format: "HR=72.5,SpO2=98.2")
                        char *token = strtok(received_text, ",");
                        while (token != NULL) {
                            if (strstr(token, "HR=")) {
                                heart_rate = atof(token + 3);
                                ESP_LOGI(GATTS_TAG, "Updated HR: %.2f", heart_rate);
                            } else if (strstr(token, "SpO2=")) {
                                spo2_level = atof(token + 5);
                                ESP_LOGI(GATTS_TAG, "Updated SpO2: %.2f", spo2_level);
                            }
                            token = strtok(NULL, ",");
                        }
                        // Send notification after update
                        send_notification_to_all_services();
                    }
                }
            }
            example_write_event_env(gatts_if, &a_prepare_write_env, param);
            break;
        }

        case ESP_GATTS_EXEC_WRITE_EVT:
            ESP_LOGI(GATTS_TAG, "Vitals EXEC_WRITE_EVT");
            esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
            example_exec_write_event_env(&a_prepare_write_env, param);
            break;

        case ESP_GATTS_MTU_EVT:
            ESP_LOGI(GATTS_TAG, "Vitals MTU_EVT, MTU %d", param->mtu.mtu);
            break;

        case ESP_GATTS_CREATE_EVT:
            ESP_LOGI(GATTS_TAG, "Vitals CREATE_SERVICE_EVT, status %d, service_handle %d",
                     param->create.status, param->create.service_handle);
            gl_profile_tab[PROFILE_A_APP_ID].service_handle = param->create.service_handle;

            // Single characteristic for Vitals (HR & SpO2)
            gl_profile_tab[PROFILE_A_APP_ID].char_uuid.len = ESP_UUID_LEN_16;
            gl_profile_tab[PROFILE_A_APP_ID].char_uuid.uuid.uuid16 = GATTS_CHAR_UUID_TEST_A_VITALS;
            esp_gatt_char_prop_t a_property = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_NOTIFY;

            esp_err_t add_char_ret = esp_ble_gatts_add_char(gl_profile_tab[PROFILE_A_APP_ID].service_handle,
                                                           &gl_profile_tab[PROFILE_A_APP_ID].char_uuid,
                                                           ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                                           a_property,
                                                           &gatts_char_vitals_val, NULL);
            if (add_char_ret) {
                ESP_LOGE(GATTS_TAG, "add Vitals char failed, error code =%x", add_char_ret);
            }
            break;

        case ESP_GATTS_ADD_CHAR_EVT:
            ESP_LOGI(GATTS_TAG, "Vitals ADD_CHAR_EVT, status %d, attr_handle %d, service_handle %d",
                    param->add_char.status, param->add_char.attr_handle, param->add_char.service_handle);
            gl_profile_tab[PROFILE_A_APP_ID].char_handle = param->add_char.attr_handle;

            // Add CCCD for Vitals
            esp_bt_uuid_t descr_uuid = { .len = ESP_UUID_LEN_16, .uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG };
            esp_err_t add_descr_ret = esp_ble_gatts_add_char_descr(gl_profile_tab[PROFILE_A_APP_ID].service_handle,
                                                                   &descr_uuid,
                                                                   ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                                                   NULL, NULL);
            if (add_descr_ret) {
                ESP_LOGE(GATTS_TAG, "add Vitals CCCD failed, error code = %x", add_descr_ret);
            }

            // Start the service
            esp_ble_gatts_start_service(gl_profile_tab[PROFILE_A_APP_ID].service_handle);
            break;

        case ESP_GATTS_ADD_CHAR_DESCR_EVT:
            ESP_LOGI(GATTS_TAG, "Vitals ADD_DESCR_EVT, status %d, attr_handle %d, service_handle %d",
                     param->add_char_descr.status, param->add_char_descr.attr_handle, param->add_char_descr.service_handle);
            break;

        case ESP_GATTS_START_EVT:
            ESP_LOGI(GATTS_TAG, "Vitals SERVICE_START_EVT, status %d, service_handle %d",
                     param->start.status, param->start.service_handle);
            break;

        case ESP_GATTS_CONNECT_EVT: {
            esp_ble_conn_update_params_t conn_params = {0};
            memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
            conn_params.latency = 0;
            conn_params.max_int = 0x20;
            conn_params.min_int = 0x10;
            conn_params.timeout = 400;

            ESP_LOGI(GATTS_TAG, "Vitals CONNECT_EVT, conn_id %d, remote %02x:%02x:%02x:%02x:%02x:%02x",
                     param->connect.conn_id,
                     param->connect.remote_bda[0], param->connect.remote_bda[1], param->connect.remote_bda[2],
                     param->connect.remote_bda[3], param->connect.remote_bda[4], param->connect.remote_bda[5]);
            gl_profile_tab[PROFILE_A_APP_ID].conn_id = param->connect.conn_id;
            conn_id = param->connect.conn_id;

            // Update all other profiles with the connection ID
            gl_profile_tab[PROFILE_B_APP_ID].conn_id = param->connect.conn_id;
            gl_profile_tab[PROFILE_C_APP_ID].conn_id = param->connect.conn_id;
            gl_profile_tab[PROFILE_D_APP_ID].conn_id = param->connect.conn_id;

            // Send initial notifications on connect
            send_notification_to_all_services();

            esp_ble_gap_update_conn_params(&conn_params);
            break;
        }

        case ESP_GATTS_DISCONNECT_EVT:
            ESP_LOGI(GATTS_TAG, "Vitals DISCONNECT_EVT, disconnect reason 0x%x", param->disconnect.reason);
            gl_profile_tab[PROFILE_A_APP_ID].conn_id = 0;
            gl_profile_tab[PROFILE_B_APP_ID].conn_id = 0;
            gl_profile_tab[PROFILE_C_APP_ID].conn_id = 0;
            gl_profile_tab[PROFILE_D_APP_ID].conn_id = 0;
            
            // Reset CCCD values on disconnect
            gl_profile_tab[PROFILE_A_APP_ID].cccd_value_vitals = 0;
            gl_profile_tab[PROFILE_B_APP_ID].cccd_value_location = 0;
            gl_profile_tab[PROFILE_C_APP_ID].cccd_value_temp = 0;
            gl_profile_tab[PROFILE_D_APP_ID].cccd_value_text = 0;
            
            esp_ble_gap_start_advertising(&adv_params);
            break;

        default:
            break;
    }
}

// Service B: GPS Event Handler
void gatts_profile_b_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
    switch (event) {
        case ESP_GATTS_REG_EVT:
            ESP_LOGI(GATTS_TAG, "GPS Service REGISTER_APP_EVT, status %d, app_id %d", param->reg.status, param->reg.app_id);
            gl_profile_tab[PROFILE_B_APP_ID].service_id.is_primary = true;
            gl_profile_tab[PROFILE_B_APP_ID].service_id.id.inst_id = 0x01;
            gl_profile_tab[PROFILE_B_APP_ID].service_id.id.uuid.len = ESP_UUID_LEN_16;
            gl_profile_tab[PROFILE_B_APP_ID].service_id.id.uuid.uuid.uuid16 = GATTS_SERVICE_UUID_TEST_B;

            esp_ble_gatts_create_service(gatts_if, &gl_profile_tab[PROFILE_B_APP_ID].service_id, GATTS_NUM_HANDLE_TEST_B);
            break;

		case ESP_GATTS_READ_EVT: {
		    ESP_LOGI(GATTS_TAG,
		             "GPS READ_EVT, conn_id %d, trans_id %" PRIu32 ", handle %d",
		             param->read.conn_id, param->read.trans_id, param->read.handle);
		
		    // Reject long-read offsets
		    if (param->read.offset != 0) {
		        esp_ble_gatts_send_response(
		            gatts_if,
		            param->read.conn_id,
		            param->read.trans_id,
		            ESP_GATT_INVALID_OFFSET,
		            NULL
		        );
		        break;
		    }
		
		    esp_gatt_rsp_t rsp;
		    memset(&rsp, 0, sizeof(rsp));
		
		    rsp.attr_value.handle   = param->read.handle;
		    rsp.attr_value.auth_req = ESP_GATT_AUTH_REQ_NONE;
		
		    char lat_str[20];
		    char lon_str[20];
		
		    snprintf(lat_str, sizeof(lat_str), "%.6f", latitude);
		    snprintf(lon_str, sizeof(lon_str), "%.6f", longitude);
		
		    // Build payload "lat,lon"
		    int written = snprintf(
		        (char *)rsp.attr_value.value,
		        sizeof(rsp.attr_value.value),
		        "%s,%s",
		        lat_str,
		        lon_str
		    );
		    int max_len = 22; 
		    rsp.attr_value.len = (written > max_len) ? max_len : written;
		
		    esp_err_t ret = esp_ble_gatts_send_response(
		        gatts_if,
		        param->read.conn_id,
		        param->read.trans_id,
		        ESP_GATT_OK,
		        &rsp
		    );
		
		    if (ret != ESP_OK) {
		        ESP_LOGE(GATTS_TAG, "Failed to send READ response: %s", esp_err_to_name(ret));
		    }
		
		    break;
		}


        case ESP_GATTS_WRITE_EVT: {
            ESP_LOGI(GATTS_TAG, "GPS WRITE_EVT, conn_id %d, trans_id %" PRIu32 ", handle %d",
                     param->write.conn_id, param->write.trans_id, param->write.handle);

            if (!param->write.is_prep) {
                ESP_LOGI(GATTS_TAG, "GATT_WRITE_EVT, value len %d, value:", param->write.len);
                esp_log_buffer_hex(GATTS_TAG, param->write.value, param->write.len);

                // Check if this is a CCCD write
                if (param->write.handle == gl_profile_tab[PROFILE_B_APP_ID].char_handle + 1) {
                    handle_cccd_write(gatts_if, param, &gl_profile_tab[PROFILE_B_APP_ID].cccd_value_location, 
                                     gl_profile_tab[PROFILE_B_APP_ID].char_handle);
                } else {
                    // If text is written to GPS service, update static variables
                    if (param->write.len > 0) {
                        char received_text[100];
                        memcpy(received_text, param->write.value, param->write.len);
                        received_text[param->write.len] = '\0';

                        // Parse received data (format: "Lat=7.7749,Lon=2.4194")
                        char *token = strtok(received_text, ",");
                        while (token != NULL) {
                            if (strstr(token, "Lat=")) {
                                latitude = atof(token + 4);
                                ESP_LOGI(GATTS_TAG, "Updated Latitude: %.4f", latitude);
                            } else if (strstr(token, "Lon=")) {
                                longitude = atof(token + 4);
                                ESP_LOGI(GATTS_TAG, "Updated Longitude: %.4f", longitude);
                            }
                            token = strtok(NULL, ",");
                        }
                        // Send notification after update
                        send_notification_to_all_services();
                    }
                }
            }
            example_write_event_env(gatts_if, &b_prepare_write_env, param);
            break;
        }

        case ESP_GATTS_EXEC_WRITE_EVT:
            ESP_LOGI(GATTS_TAG, "GPS EXEC_WRITE_EVT");
            esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
            example_exec_write_event_env(&b_prepare_write_env, param);
            break;

        case ESP_GATTS_CREATE_EVT:
            ESP_LOGI(GATTS_TAG, "GPS CREATE_SERVICE_EVT, status %d, service_handle %d",
                     param->create.status, param->create.service_handle);
            gl_profile_tab[PROFILE_B_APP_ID].service_handle = param->create.service_handle;

            // Single characteristic for Location (Lat & Lon)
            gl_profile_tab[PROFILE_B_APP_ID].char_uuid.len = ESP_UUID_LEN_16;
            gl_profile_tab[PROFILE_B_APP_ID].char_uuid.uuid.uuid16 = GATTS_CHAR_UUID_TEST_B_LOCATION;
            esp_gatt_char_prop_t b_property = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_NOTIFY;

            esp_err_t add_char_ret = esp_ble_gatts_add_char(gl_profile_tab[PROFILE_B_APP_ID].service_handle,
                                                           &gl_profile_tab[PROFILE_B_APP_ID].char_uuid,
                                                           ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                                           b_property,
                                                           &gatts_char_location_val, NULL);
            if (add_char_ret) {
                ESP_LOGE(GATTS_TAG, "add Location char failed, error code =%x", add_char_ret);
            }
            break;

        case ESP_GATTS_ADD_CHAR_EVT:
            ESP_LOGI(GATTS_TAG, "GPS ADD_CHAR_EVT, status %d, attr_handle %d, service_handle %d",
                    param->add_char.status, param->add_char.attr_handle, param->add_char.service_handle);
            gl_profile_tab[PROFILE_B_APP_ID].char_handle = param->add_char.attr_handle;

            // Add CCCD for Location
            esp_bt_uuid_t descr_uuid = { .len = ESP_UUID_LEN_16, .uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG };
            esp_err_t add_descr_ret = esp_ble_gatts_add_char_descr(gl_profile_tab[PROFILE_B_APP_ID].service_handle,
                                                                   &descr_uuid,
                                                                   ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                                                   NULL, NULL);
            if (add_descr_ret) {
                ESP_LOGE(GATTS_TAG, "add Location CCCD failed, error code = %x", add_descr_ret);
            }

            // Start the service
            esp_ble_gatts_start_service(gl_profile_tab[PROFILE_B_APP_ID].service_handle);
            break;

        case ESP_GATTS_START_EVT:
            ESP_LOGI(GATTS_TAG, "GPS SERVICE_START_EVT, status %d, service_handle %d",
                     param->start.status, param->start.service_handle);
            break;

        case ESP_GATTS_CONNECT_EVT:
            ESP_LOGI(GATTS_TAG, "GPS CONNECT_EVT, conn_id %d", param->connect.conn_id);
            gl_profile_tab[PROFILE_B_APP_ID].conn_id = param->connect.conn_id;
            break;

        case ESP_GATTS_DISCONNECT_EVT:
            ESP_LOGI(GATTS_TAG, "GPS DISCONNECT_EVT, disconnect reason 0x%x", param->disconnect.reason);
            gl_profile_tab[PROFILE_B_APP_ID].conn_id = 0;
            break;

        default:
            break;
    }
}

// Service C: Temperature Event Handler
void gatts_profile_c_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
    switch (event) {
        case ESP_GATTS_REG_EVT:
            ESP_LOGI(GATTS_TAG, "Temperature Service REGISTER_APP_EVT, status %d, app_id %d", param->reg.status, param->reg.app_id);
            gl_profile_tab[PROFILE_C_APP_ID].service_id.is_primary = true;
            gl_profile_tab[PROFILE_C_APP_ID].service_id.id.inst_id = 0x02;
            gl_profile_tab[PROFILE_C_APP_ID].service_id.id.uuid.len = ESP_UUID_LEN_16;
            gl_profile_tab[PROFILE_C_APP_ID].service_id.id.uuid.uuid.uuid16 = GATTS_SERVICE_UUID_TEST_C;

            esp_ble_gatts_create_service(gatts_if, &gl_profile_tab[PROFILE_C_APP_ID].service_id, GATTS_NUM_HANDLE_TEST_C);
            break;

        case ESP_GATTS_READ_EVT: {
            ESP_LOGI(GATTS_TAG, "Temperature READ_EVT, conn_id %d, trans_id %" PRIu32 ", handle %d",
                     param->read.conn_id, param->read.trans_id, param->read.handle);

            esp_gatt_rsp_t rsp;
            memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
            rsp.attr_value.handle = param->read.handle;

            char temp_str[20];
            float_to_string(temperature, temp_str, sizeof(temp_str));
            rsp.attr_value.len = snprintf((char*)rsp.attr_value.value,
                                         sizeof(rsp.attr_value.value),
                                         "Temp:%s", temp_str);

            esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id,
                                        ESP_GATT_OK, &rsp);
            break;
        }

        case ESP_GATTS_WRITE_EVT: {
            ESP_LOGI(GATTS_TAG, "Temperature WRITE_EVT, conn_id %d, trans_id %" PRIu32 ", handle %d",
                     param->write.conn_id, param->write.trans_id, param->write.handle);

            if (!param->write.is_prep) {
                ESP_LOGI(GATTS_TAG, "GATT_WRITE_EVT, value len %d, value:", param->write.len);
                esp_log_buffer_hex(GATTS_TAG, param->write.value, param->write.len);

                // Check if this is a CCCD write
                if (param->write.handle == gl_profile_tab[PROFILE_C_APP_ID].char_handle + 1) {
                    handle_cccd_write(gatts_if, param, &gl_profile_tab[PROFILE_C_APP_ID].cccd_value_temp, 
                                     gl_profile_tab[PROFILE_C_APP_ID].char_handle);
                } else {
                    // Update temperature from received value
                    if (param->write.len > 0) {
                        char temp_text[20];
                        memcpy(temp_text, param->write.value, param->write.len);
                        temp_text[param->write.len] = '\0';

                        if (strstr(temp_text, "Temp=")) {
                            temperature = atof(temp_text + 5);
                            ESP_LOGI(GATTS_TAG, "Updated Temperature: %.2f°C", temperature);
                        } else {
                            temperature = atof(temp_text);
                            ESP_LOGI(GATTS_TAG, "Updated Temperature: %.2f°C", temperature);
                        }
                        // Send notification after update
                        send_notification_to_all_services();
                    }
                }
            }
            example_write_event_env(gatts_if, &c_prepare_write_env, param);
            break;
        }

        case ESP_GATTS_EXEC_WRITE_EVT:
            ESP_LOGI(GATTS_TAG, "Temperature EXEC_WRITE_EVT");
            esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
            example_exec_write_event_env(&c_prepare_write_env, param);
            break;

        case ESP_GATTS_CREATE_EVT:
            ESP_LOGI(GATTS_TAG, "Temperature CREATE_SERVICE_EVT, status %d, service_handle %d",
                     param->create.status, param->create.service_handle);
            gl_profile_tab[PROFILE_C_APP_ID].service_handle = param->create.service_handle;

            gl_profile_tab[PROFILE_C_APP_ID].char_uuid.len = ESP_UUID_LEN_16;
            gl_profile_tab[PROFILE_C_APP_ID].char_uuid.uuid.uuid16 = GATTS_CHAR_UUID_TEST_C_TEMP;
            esp_gatt_char_prop_t c_property = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_NOTIFY;

            esp_err_t add_char_ret = esp_ble_gatts_add_char(gl_profile_tab[PROFILE_C_APP_ID].service_handle,
                                                           &gl_profile_tab[PROFILE_C_APP_ID].char_uuid,
                                                           ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                                           c_property,
                                                           &gatts_char_temp_val, NULL);
            if (add_char_ret) {
                ESP_LOGE(GATTS_TAG, "add Temp char failed, error code =%x", add_char_ret);
            }
            break;

        case ESP_GATTS_ADD_CHAR_EVT:
            ESP_LOGI(GATTS_TAG, "Temperature ADD_CHAR_EVT, status %d, attr_handle %d, service_handle %d",
                    param->add_char.status, param->add_char.attr_handle, param->add_char.service_handle);
            gl_profile_tab[PROFILE_C_APP_ID].char_handle = param->add_char.attr_handle;

            // Add CCCD for Temperature
            esp_bt_uuid_t descr_uuid = { .len = ESP_UUID_LEN_16, .uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG };
            esp_err_t add_descr_ret = esp_ble_gatts_add_char_descr(gl_profile_tab[PROFILE_C_APP_ID].service_handle,
                                                                   &descr_uuid,
                                                                   ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                                                   NULL, NULL);
            if (add_descr_ret) {
                ESP_LOGE(GATTS_TAG, "add Temp CCCD failed, error code = %x", add_descr_ret);
            }

            esp_ble_gatts_start_service(gl_profile_tab[PROFILE_C_APP_ID].service_handle);
            break;

        case ESP_GATTS_START_EVT:
            ESP_LOGI(GATTS_TAG, "Temperature SERVICE_START_EVT, status %d, service_handle %d",
                     param->start.status, param->start.service_handle);
            break;

        case ESP_GATTS_CONNECT_EVT:
            ESP_LOGI(GATTS_TAG, "Temperature CONNECT_EVT, conn_id %d", param->connect.conn_id);
            gl_profile_tab[PROFILE_C_APP_ID].conn_id = param->connect.conn_id;
            break;

        case ESP_GATTS_DISCONNECT_EVT:
            ESP_LOGI(GATTS_TAG, "Temperature DISCONNECT_EVT, disconnect reason 0x%x", param->disconnect.reason);
            gl_profile_tab[PROFILE_C_APP_ID].conn_id = 0;
            break;

        default:
            break;
    }
}

// Service D: Text Event Handler
void gatts_profile_d_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
    switch (event) {
        case ESP_GATTS_REG_EVT:
            ESP_LOGI(GATTS_TAG, "Text Service REGISTER_APP_EVT, status %d, app_id %d", param->reg.status, param->reg.app_id);
            gl_profile_tab[PROFILE_D_APP_ID].service_id.is_primary = true;
            gl_profile_tab[PROFILE_D_APP_ID].service_id.id.inst_id = 0x03;
            gl_profile_tab[PROFILE_D_APP_ID].service_id.id.uuid.len = ESP_UUID_LEN_16;
            gl_profile_tab[PROFILE_D_APP_ID].service_id.id.uuid.uuid.uuid16 = GATTS_SERVICE_UUID_TEST_D;

            esp_ble_gatts_create_service(gatts_if, &gl_profile_tab[PROFILE_D_APP_ID].service_id, GATTS_NUM_HANDLE_TEST_D);
            break;

        case ESP_GATTS_READ_EVT: {
            ESP_LOGI(GATTS_TAG, "Text READ_EVT, conn_id %d, trans_id %" PRIu32 ", handle %d",
                     param->read.conn_id, param->read.trans_id, param->read.handle);

            esp_gatt_rsp_t rsp;
            memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
            rsp.attr_value.handle = param->read.handle;

            rsp.attr_value.len = snprintf((char*)rsp.attr_value.value,
                                         sizeof(rsp.attr_value.value),
                                         "Text:%s", user_text);

            esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id,
                                        ESP_GATT_OK, &rsp);
            break;
        }

        case ESP_GATTS_WRITE_EVT: {
            ESP_LOGI(GATTS_TAG, "Text WRITE_EVT, conn_id %d, trans_id %" PRIu32 ", handle %d",
                     param->write.conn_id, param->write.trans_id, param->write.handle);

            if (!param->write.is_prep) {
                ESP_LOGI(GATTS_TAG, "GATT_WRITE_EVT, value len %d, value:", param->write.len);
                esp_log_buffer_hex(GATTS_TAG, param->write.value, param->write.len);

                // Check if this is a CCCD write
                if (param->write.handle == gl_profile_tab[PROFILE_D_APP_ID].char_handle + 1) {
                    handle_cccd_write(gatts_if, param, &gl_profile_tab[PROFILE_D_APP_ID].cccd_value_text, 
                                     gl_profile_tab[PROFILE_D_APP_ID].char_handle);
                } else {
                    // Update text from received value
                    if (param->write.len > 0) {
                        int len = param->write.len;
                        if (len > sizeof(user_text) - 1) {
                            len = sizeof(user_text) - 1;
                        }
                        memcpy(user_text, param->write.value, len);
                        user_text[len] = '\0';
                        ESP_LOGI(GATTS_TAG, "Updated Text: %s", user_text);

                        // Print the received text
                        printf("Received text from user: %s\n", user_text);
                        
                        // Send notification after update
                        send_notification_to_all_services();
                    }
                }
            }
            example_write_event_env(gatts_if, &d_prepare_write_env, param);
            break;
        }

        case ESP_GATTS_EXEC_WRITE_EVT:
            ESP_LOGI(GATTS_TAG, "Text EXEC_WRITE_EVT");
            esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
            example_exec_write_event_env(&d_prepare_write_env, param);
            break;

        case ESP_GATTS_CREATE_EVT:
            ESP_LOGI(GATTS_TAG, "Text CREATE_SERVICE_EVT, status %d, service_handle %d",
                     param->create.status, param->create.service_handle);
            gl_profile_tab[PROFILE_D_APP_ID].service_handle = param->create.service_handle;

            gl_profile_tab[PROFILE_D_APP_ID].char_uuid.len = ESP_UUID_LEN_16;
            gl_profile_tab[PROFILE_D_APP_ID].char_uuid.uuid.uuid16 = GATTS_CHAR_UUID_TEST_D_TEXT;
            esp_gatt_char_prop_t d_property = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_NOTIFY;

            esp_err_t add_char_ret = esp_ble_gatts_add_char(gl_profile_tab[PROFILE_D_APP_ID].service_handle,
                                                           &gl_profile_tab[PROFILE_D_APP_ID].char_uuid,
                                                           ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                                           d_property,
                                                           &gatts_char_text_val, NULL);
            if (add_char_ret) {
                ESP_LOGE(GATTS_TAG, "add Text char failed, error code =%x", add_char_ret);
            }
            break;

        case ESP_GATTS_ADD_CHAR_EVT:
            ESP_LOGI(GATTS_TAG, "Text ADD_CHAR_EVT, status %d, attr_handle %d, service_handle %d",
                    param->add_char.status, param->add_char.attr_handle, param->add_char.service_handle);
            gl_profile_tab[PROFILE_D_APP_ID].char_handle = param->add_char.attr_handle;

            // Add CCCD for Text
            esp_bt_uuid_t descr_uuid = { .len = ESP_UUID_LEN_16, .uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG };
            esp_err_t add_descr_ret = esp_ble_gatts_add_char_descr(gl_profile_tab[PROFILE_D_APP_ID].service_handle,
                                                                   &descr_uuid,
                                                                   ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                                                   NULL, NULL);
            if (add_descr_ret) {
                ESP_LOGE(GATTS_TAG, "add Text CCCD failed, error code = %x", add_descr_ret);
            }

            esp_ble_gatts_start_service(gl_profile_tab[PROFILE_D_APP_ID].service_handle);
            break;

        case ESP_GATTS_START_EVT:
            ESP_LOGI(GATTS_TAG, "Text SERVICE_START_EVT, status %d, service_handle %d",
                     param->start.status, param->start.service_handle);
            break;

        case ESP_GATTS_CONNECT_EVT:
            ESP_LOGI(GATTS_TAG, "Text CONNECT_EVT, conn_id %d", param->connect.conn_id);
            gl_profile_tab[PROFILE_D_APP_ID].conn_id = param->connect.conn_id;
            break;

        case ESP_GATTS_DISCONNECT_EVT:
            ESP_LOGI(GATTS_TAG, "Text DISCONNECT_EVT, disconnect reason 0x%x", param->disconnect.reason);
            gl_profile_tab[PROFILE_D_APP_ID].conn_id = 0;
            break;

        default:
            break;
    }
}