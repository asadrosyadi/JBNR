#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <inttypes.h>
#include <sys/_intsup.h>

#include "esp_log.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"
#include "driver/i2c.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_mac.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_log.h"
#include "portmacro.h"

#include "tmp116.h"
#include "http_server.h"
#include "i2c_lock.h"
#include "ble_server.h"
#include "data_packet.h"
#include "lora.h"
#include "gps.h"
#include "max30102.h"


data_packet_t pkt = {0};
max30102_t max30102 = {};

// i2c config esp32 c3 pins
#define I2C_SDA   1
#define I2C_SCL   6
//esp32 s3 pins
//#define I2C_SDA
//#define I2C_SCL 

#define I2C_FRQ   400000
#define I2C_PORT  I2C_NUM_0

//heart rate 
#define BUFFER_SIZE 256
#define SAMPLE_RATE 50.0f
#define MIN_BPM 40.0f
#define MAX_BPM 180.0f
#define PEAK_WINDOW_SIZE 25
#define GAUSSIAN_KERNEL_SIZE 9
#define MAX_SAMPLES 10000


//http server config
#define EXAMPLE_ESP_WIFI_SSID      "ESP"
#define EXAMPLE_ESP_WIFI_PASS      "1234567890"
#define EXAMPLE_ESP_WIFI_CHANNEL   6
#define EXAMPLE_MAX_STA_CONN       4
 
 // Peak detection structure
typedef struct {
    int index;
    float value;
} peak_t;
//send meggage through lora
typedef struct {
    int32_t lat;
    int32_t lon;
} gps_t;
gps_t gps = {0};
typedef struct {
    uint16_t hr;
    uint16_t spo2;
} ppg_t;
ppg_t ppg = {0};
static int16_t tmp116;

static SemaphoreHandle_t datasent_mutex_gps = NULL;
static SemaphoreHandle_t datasent_mutex_max30102 = NULL;
static SemaphoreHandle_t datasent_mutex_tmp116 = NULL;

//BLE CONFIG
void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    switch (event) {
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
            adv_config_done &= (~adv_config_flag);
            if (adv_config_done == 0) {
                esp_ble_gap_start_advertising(&adv_params);
            }
            break;
        case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
            adv_config_done &= (~scan_rsp_config_flag);
            if (adv_config_done == 0) {
                esp_ble_gap_start_advertising(&adv_params);
            }
            break;
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
                ESP_LOGE(GATTS_TAG, "Advertising start failed");
            }
            break;
        case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
            if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
                ESP_LOGE(GATTS_TAG, "Advertising stop failed");
            } else {
                ESP_LOGI(GATTS_TAG, "Stop adv successfully");
            }
            break;
        case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
            ESP_LOGI(GATTS_TAG, "update connection params status = %d, min_int = %d, max_int = %d,conn_int = %d,latency = %d, timeout = %d",
                     param->update_conn_params.status,
                     param->update_conn_params.min_int,
                     param->update_conn_params.max_int,
                     param->update_conn_params.conn_int,
                     param->update_conn_params.latency,
                     param->update_conn_params.timeout);
            break;
        case ESP_GAP_BLE_SET_PKT_LENGTH_COMPLETE_EVT:
            ESP_LOGI(GATTS_TAG, "packet length updated: rx = %d, tx = %d, status = %d",
                     param->pkt_data_length_cmpl.params.rx_len,
                     param->pkt_data_length_cmpl.params.tx_len,
                     param->pkt_data_length_cmpl.status);
            break;
        default:
            break;
    }
}

void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
    /* If event is register event, store the gatts_if for each profile */
    if (event == ESP_GATTS_REG_EVT) {
        if (param->reg.status == ESP_GATT_OK) {
            gl_profile_tab[param->reg.app_id].gatts_if = gatts_if;
        } else {
            ESP_LOGI(GATTS_TAG, "Reg app failed, app_id %04x, status %d",
                    param->reg.app_id,
                    param->reg.status);
            return;
        }
    }

    /* If the gatts_if equal to profile A, call profile A cb handler,
     * so here call each profile's callback */
    do {
        int idx;
        for (idx = 0; idx < PROFILE_NUM; idx++) {
            if (gatts_if == ESP_GATT_IF_NONE || /* ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb function */
                    gatts_if == gl_profile_tab[idx].gatts_if) {
                if (gl_profile_tab[idx].gatts_cb) {
                    gl_profile_tab[idx].gatts_cb(event, gatts_if, param);
                }
            }
        }
    } while (0);
}

//WIFI CONFIG
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d, reason=%d",
                 MAC2STR(event->mac), event->aid, event->reason);
    }
}

void wifi_init_softap(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
            .channel = EXAMPLE_ESP_WIFI_CHANNEL,
            .password = EXAMPLE_ESP_WIFI_PASS,
            .max_connection = EXAMPLE_MAX_STA_CONN,
#ifdef CONFIG_ESP_WIFI_SOFTAP_SAE_SUPPORT
            .authmode = WIFI_AUTH_WPA3_PSK,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
#else /* CONFIG_ESP_WIFI_SOFTAP_SAE_SUPPORT */
            .authmode = WIFI_AUTH_WPA2_PSK,
#endif
            .pmf_cfg = {
                    .required = true,
            },
        },
    };
    if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS, EXAMPLE_ESP_WIFI_CHANNEL);
}

// I2C init
esp_err_t i2c_master_init(i2c_port_t i2c_port){
    i2c_config_t conf = {};
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_SDA;
    conf.scl_io_num = I2C_SCL;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_FRQ;
    i2c_param_config(i2c_port, &conf);
    return i2c_driver_install(i2c_port, I2C_MODE_MASTER, 0, 0, 0);
}

/*----------------------------------------------------Blood Oxygen--------------------------------------------------------------------*/
float calculate_spo2(uint16_t red[], uint16_t ir[], int size)
{
	uint16_t max_val = 0;
    uint16_t min_val = 65535;
    float mean = 0.0f;
    if (size < 10) return -1;
    
    for (int i = 0; i < size; i++) {
        mean += red[i];
        if (red[i] > max_val) max_val = red[i];
        if (red[i] < min_val) min_val = red[i];
    }
    mean /= size;

    float threshold = mean;
    
    if (threshold<3000) {
		printf("threshold = %f",threshold);
		printf("No finger Present ");
		return -1.0f;
	}

    float redDC = 0.0f, irDC = 0.0f;

    static float prev_spo2 = 100.0f;

    // Compute DC component
    for (int i = 0; i < size; i++) {
        redDC += red[i];
        irDC  += ir[i];
    }
    redDC /= size;
    irDC  /= size;

    // Compute AC (RMS)
    float redAC = 0.0f, irAC = 0.0f;

    for (int i = 0; i < size; i++) {
        float redDiff = (float)red[i] - redDC;
        float irDiff  = (float)ir[i]  - irDC;

        redAC += redDiff * redDiff;
        irAC  += irDiff  * irDiff;
    }

    redAC = sqrtf(redAC / size);
    irAC  = sqrtf(irAC / size);

    if (irAC <= 0.0f || redAC <= 0.0f) return -1;

    float ratio = (redAC / redDC) / (irAC / irDC);

    // Empirical linear formula
    float spo2 = 110.0f - 25.0f * ratio;

    if (spo2 > 100) spo2 = 100;
    if (spo2 < 50)  spo2 = 50;

    // Low-pass filtering
    const float alpha = 0.1f;
    spo2 = alpha * spo2 + (1.0f - alpha) * prev_spo2;

    prev_spo2 = spo2;

    return spo2;
}

/*----------------------------Heart Rate------------------------------------------*/

// Gaussian kernel for smoothing (sigma=0.5, size=5)
/*static const float gaussian_kernel[GAUSSIAN_KERNEL_SIZE] = {
    0.06136f, 0.24477f, 0.38774f, 0.24477f, 0.06136f
};*/
static const float gaussian_kernel[GAUSSIAN_KERNEL_SIZE] = {
    0.02763f, 0.06628f, 0.12383f, 0.18017f,
    0.20416f,
    0.18017f, 0.12383f, 0.06628f, 0.02763f
};
// Function to apply Gaussian smoothing
static void smooth_signal(const uint16_t *input, float *output, int size) {
    int half_kernel = GAUSSIAN_KERNEL_SIZE / 2;
    
    for (int i = 0; i < size; i++) {
        output[i] = 0.0f;
        float weight_sum = 0.0f;
        
        for (int j = -half_kernel; j <= half_kernel; j++) {
            int idx = i + j;
            if (idx >= 0 && idx < size) {
                output[i] += input[idx] * gaussian_kernel[j + half_kernel];
                weight_sum += gaussian_kernel[j + half_kernel];
            }
        }
        
        // Normalize by actual weight sum to handle boundaries
        if (weight_sum > 0.0f) {
            output[i] /= weight_sum;
        }
    }
}

// Function to detect peaks in the smoothed signal
static int detect_peaks(const float *signal, int size, peak_t *peaks, int max_peaks) {
    int half_window = PEAK_WINDOW_SIZE / 2;
    int peak_count = 0;
    
    for (int i = half_window; i < size - half_window && peak_count < max_peaks; i++) {
        int is_peak = 1;
        float current_val = signal[i];
        
        // Check if current point is the maximum in the window
        for (int j = i - half_window; j <= i + half_window; j++) {
            if (j != i && signal[j] >= current_val) {
                is_peak = 0;
                break;
            }
        }
        
        if (is_peak) {
            peaks[peak_count].index = i;
            peaks[peak_count].value = current_val;
            peak_count++;
        }
    }
    
    return peak_count;
}

// Function to calculate heart rate from IR data
float calculate_heart_rate(uint16_t ir[], int size) {
    static float filtered_bpm = 75.0f;
    static float prev_filtered_bpm = 75.0f;
    //mean
    uint16_t max_val = 0;
    uint16_t min_val = 65535;
    float mean = 0.0f;

    for (int i = 0; i < size; i++) {
        mean += ir[i];
        if (ir[i] > max_val) max_val = ir[i];
        if (ir[i] < min_val) min_val = ir[i];
    }
    mean /= size;

    float range = max_val - min_val;
    if (range < 50) {
        printf("Signal too weak(No signal Detected): range=%.0f\n", range);
        return -1.0f;
    }

    float threshold = mean + range * 0.5f;
    
    if (threshold<2000) {
		printf("threshold = %f",threshold);
		printf("No finger Present ");
		return -1.0f;
	}
    
    if (size < 100) {
        ESP_LOGE(TAG, "Insufficient data points: %d", size);
        return 0.0f;
    }
    
    // Allocate memory for smoothed signal
    float *smoothed_ir = malloc(size * sizeof(float));
    if (!smoothed_ir) {
        ESP_LOGE(TAG, "Failed to allocate memory for smoothed signal");
        return 0.0f;
    }
    
    // Apply Gaussian smoothing
    smooth_signal(ir, smoothed_ir, size);
    
    // Allocate memory for peaks (maximum possible peaks)
    int max_possible_peaks = size / (PEAK_WINDOW_SIZE / 2);
    peak_t *peaks = malloc(max_possible_peaks * sizeof(peak_t));
    if (!peaks) {
        ESP_LOGE(TAG, "Failed to allocate memory for peaks");
        free(smoothed_ir);
        return 0.0f;
    }
    
    // Detect peaks
    int peak_count = detect_peaks(smoothed_ir, size, peaks, max_possible_peaks);
    
    ESP_LOGI(TAG, "Detected %d peaks", peak_count);
    
    // Calculate heart rate if we have enough peaks
    if (peak_count > 1) {
        // Calculate RR intervals in samples
        float *rr_intervals_sec = malloc((peak_count - 1) * sizeof(float));
        if (!rr_intervals_sec) {
            ESP_LOGE(TAG, "Failed to allocate memory for RR intervals");
            free(smoothed_ir);
            free(peaks);
            return 0.0f;
        }
        
        // Convert peak differences to RR intervals in seconds
        for (int i = 0; i < peak_count - 1; i++) {
            int rr_interval_samples = peaks[i + 1].index - peaks[i].index;
            rr_intervals_sec[i] = rr_interval_samples / SAMPLE_RATE;
        }
        
        // Filter RR intervals for plausible heart rates 
        float min_rr = 60.0f / MAX_BPM;  // Minimum 180 BPM
        float max_rr = 60.0f / MIN_BPM;  // Maximum 40 BPM
        
        float sum_valid_rr = 0.0f;
        int valid_rr_count = 0;
        
        for (int i = 0; i < peak_count - 1; i++) {
            if (rr_intervals_sec[i] >= min_rr && rr_intervals_sec[i] <= max_rr) {
                sum_valid_rr += rr_intervals_sec[i];
                valid_rr_count++;
            }
        }
        
        ESP_LOGI(TAG, "Valid RR intervals: %d out of %d", valid_rr_count, peak_count - 1);
        
        if (valid_rr_count > 0) {
            float avg_rr = sum_valid_rr / valid_rr_count;
            filtered_bpm = 60.0f / avg_rr;
            
            ESP_LOGI(TAG, "Calculated BPM: %.2f", filtered_bpm);
        } else {
            ESP_LOGW(TAG, "No valid RR intervals found for possible heart rates");
            filtered_bpm = 0.0f;
        }
        
        free(rr_intervals_sec);
    } else {
        ESP_LOGW(TAG, "Not enough peaks to calculate average BPM");
        filtered_bpm = 0.0f;
    }
    
    // Clean
    free(smoothed_ir);
    free(peaks);
    //low pass filter to make smooth change of the bpm 
    const float alpha = 0.1f; 

    filtered_bpm = alpha * filtered_bpm + (1.0f - alpha) * prev_filtered_bpm;
    prev_filtered_bpm = filtered_bpm;
    
    return filtered_bpm;
}

float calculate_heart_rate_static(uint16_t ir[], int size) {
    static float filtered_bpm = 0.0f;
    
    if (size < 100) {
        ESP_LOGE(TAG, "Insufficient data points: %d", size);
        return 0.0f;
    }
   

    if (size > MAX_SAMPLES) {
        ESP_LOGE(TAG, "Input size too large: %d, maximum is %d", size, MAX_SAMPLES);
        return 0.0f;
    }
    
    static float smoothed_ir[MAX_SAMPLES];
    static peak_t peaks[MAX_SAMPLES / (PEAK_WINDOW_SIZE / 2)];
    
    // Apply Gaussian smoothing
    smooth_signal(ir, smoothed_ir, size);
    
    // Detect peaks
    int peak_count = detect_peaks(smoothed_ir, size, peaks, 
                                 sizeof(peaks) / sizeof(peaks[0]));
    
    ESP_LOGI(TAG, "Detected %d peaks", peak_count);
    
    // Calculate heart rate if we have enough peaks
    if (peak_count > 1) {
        static float rr_intervals_sec[MAX_SAMPLES / (PEAK_WINDOW_SIZE / 2)];
        
        // Convert peak differences to RR intervals in seconds
        for (int i = 0; i < peak_count - 1; i++) {
            int rr_interval_samples = peaks[i + 1].index - peaks[i].index;
            rr_intervals_sec[i] = rr_interval_samples / SAMPLE_RATE;
        }
        
        // Filter RR intervals for plausible heart rates (40-180 BPM)
        float min_rr = 60.0f / MAX_BPM;
        float max_rr = 60.0f / MIN_BPM;
        
        float sum_valid_rr = 0.0f;
        int valid_rr_count = 0;
        
        for (int i = 0; i < peak_count - 1; i++) {
            if (rr_intervals_sec[i] >= min_rr && rr_intervals_sec[i] <= max_rr) {
                sum_valid_rr += rr_intervals_sec[i];
                valid_rr_count++;
            }
        }
        
        ESP_LOGI(TAG, "Valid RR intervals: %d out of %d", valid_rr_count, peak_count - 1);
        
        if (valid_rr_count > 0) {
            float avg_rr = sum_valid_rr / valid_rr_count;
            filtered_bpm = 60.0f / avg_rr;
            
            ESP_LOGI(TAG, "Calculated BPM: %.2f", filtered_bpm);
        } else {
            ESP_LOGW(TAG, "No valid RR intervals found for plausible heart rates");
            filtered_bpm = 0.0f;
        }
    } else {
        ESP_LOGW(TAG, "Not enough peaks to calculate average BPM");
        filtered_bpm = 0.0f;
    }
    
    return filtered_bpm;
}

// MAX30102 task
void max30102_task(void* arg) 
{
	uint16_t red_buffer[BUFFER_SIZE] = {0};
	uint16_t ir_buffer[BUFFER_SIZE]  = {0};
	int write_index = 0;
    uint32_t red[32], ir[32];
    float bpm = 0.0, spo2 = 0.0;
    uint8_t ppg_count = 0;

  while (1) {
	
    uint8_t intStatus = 0;
    // Read interrupt status register
    if (max30102_read_register(&max30102, MAX30102_INTERRUPT_STATUS_1, &intStatus) == ESP_OK) {
		
        bool fifo_full = (intStatus & 0x80) != 0;   

		bool ppg_rdy = (intStatus & 0x40) != 0;
		if (ppg_rdy) {
			ppg_count++;
		}
        if (fifo_full || ppg_count >=32) {
           ppg_count=0;
            // Read 32 samples from FIFO
            if (max30102_read_fifo(I2C_PORT, ir, red, 32) == ESP_OK) {

                    // Append new samples to circular buffer
				    for (int i = 0; i < 32; i++) {
				        red_buffer[write_index] = (uint16_t)red[i];
				        ir_buffer[write_index]  = (uint16_t)ir[i];
				     	printf("%" PRIu16 ",%" PRIu16 "\n", (uint16_t)red[i], (uint16_t)ir[i]); 
				        write_index = (write_index + 1) % BUFFER_SIZE;  // wrap around
				    }
				
				    //calculate HR and SpO2 from entire buffer
				    bpm  = calculate_heart_rate(ir_buffer, BUFFER_SIZE);
				    spo2 = calculate_spo2(red_buffer, ir_buffer, BUFFER_SIZE);
				    
				    if(bpm == -1.0f || spo2 == -1.0f) {
						
						http_update_finger_status(0);
						http_update_sp02_sensor_data(0,0);
						update_max30102_sensor_data(0, 0);
						bpm = 0.0;
						spo2 = 0.0;
						
					}else{
						
						http_update_finger_status(1);
						http_update_sp02_sensor_data(bpm,spo2);
						update_max30102_sensor_data(bpm,spo2);
						printf("HR: %.1f | SpO2: %.1f\n", bpm, spo2);
						

					}
					if (xSemaphoreTake(datasent_mutex_max30102, portMAX_DELAY) == pdTRUE) {
							
							
							ppg.hr = (uint16_t)(bpm*10);
							ppg.spo2 = (uint16_t)(spo2*10); 
							
							xSemaphoreGive(datasent_mutex_max30102);
						}
					

           		    } 
            else {
                printf("FIFO READ ERROR!\n");
            }
        }
        
    }
	vTaskDelay(pdMS_TO_TICKS(10));
}

}

//GPS task
void gps_task(void* arg) {

    while (1) {
        // Send GPS
        
        GPS_Coordinates coordinate = get_gps_coordinates();
       
        http_update_gps_sensor_data(coordinate.longitude, coordinate.latitude);
        update_gps_sensor_data(coordinate.longitude, coordinate.latitude);
        
                  
         //send task handled here
		if (xSemaphoreTake(datasent_mutex_gps, portMAX_DELAY) == pdTRUE) {
			
			gps.lat = (int32_t)(coordinate.latitude*1e6);
			gps.lon = (int32_t)(coordinate.longitude*1e6);
                      
					xSemaphoreGive(datasent_mutex_gps);
					
			}

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

//tmp116 task
void tmp116_task(void* arg) {
	float temp;
    while (1) {

        int16_t raw = tmp116_read_raw();
        if (!raw) {
			vTaskDelay(pdMS_TO_TICKS(500));
			continue;
		}
        temp = tmp116_to_celsius(raw);
        
        http_update_tmp116_sensor_data(temp);
        update_tpm116_sensor_data(temp);
            
		if (xSemaphoreTake(datasent_mutex_tmp116, portMAX_DELAY) == pdTRUE) {
			
				tmp116 = (int16_t)(temp*100);
                      
					xSemaphoreGive(datasent_mutex_tmp116);
					
			}

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

//lora message send task
void lora_task(void* arg){

	esp_task_wdt_add(NULL);
	while (1) {
		
		if (xSemaphoreTake(datasent_mutex_gps, pdMS_TO_TICKS(100)) == pdTRUE &&
						xSemaphoreTake(datasent_mutex_max30102, pdMS_TO_TICKS(100)) == pdTRUE &&
						xSemaphoreTake(datasent_mutex_tmp116, pdMS_TO_TICKS(100)) == pdTRUE) {
			
					pkt.hr_x10=ppg.hr;
					pkt.spo2_x10=ppg.spo2;
					pkt.lat_x1e6=gps.lat;
					pkt.lon_x1e6=gps.lon;
					pkt.temp_x100=tmp116;
				
				//send the combined buffer
				uint8_t tx_buf[sizeof(data_packet_t)];
				memcpy(tx_buf, &pkt, sizeof(data_packet_t));
				//send the packet 
				send(tx_buf, sizeof(data_packet_t));

				/*Receive ACK*/
				uint8_t messageBuffer[32];
				uint32_t start = xTaskGetTickCount();
				bool ack_received = false;
			//	set_rx_enable();
				while ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(1000)) 
				{	
				  setRxMode();
				  esp_task_wdt_reset();

				    uint8_t irq_flags = register_read(RFM9X_12_REG_IRQ_FLAGS);
				    if (irq_flags & IRQ_RX_DONE_MASK) {
				
				        uint8_t length = register_read(RFM9X_13_REG_RX_NB_BYTES);
				        memset(messageBuffer, 0, sizeof(messageBuffer));
				
				        uint8_t fifoStart = register_read(RFM9X_10_REG_FIFO_RX_CURRENT_ADDR);
				        register_write(RFM9X_0D_REG_FIFO_ADDR_PTR, fifoStart);
						
				
				        for (int i = 0; i < length; i++) {
				            messageBuffer[i] = register_read(RFM9X_00_REG_FIFO);
				        }
				        messageBuffer[length] = '\0';
				        
						int remove = 4; 
				
						if (length > remove) {
						    memmove(messageBuffer, messageBuffer + remove, length - remove + 1);  
						}
				       printf("Message recived : %s\n",messageBuffer);
				        register_write(RFM9X_12_REG_IRQ_FLAGS, 0xFF);
				        register_write(RFM9X_0D_REG_FIFO_ADDR_PTR, register_read(RFM9X_0F_REG_FIFO_RX_BASE_ADDR));
						}
					if (strcmp((char*)messageBuffer, "ACK") == 0) {
						ack_received = true;

						}
					else {
						ack_received = false;
						
						}
					vTaskDelay(pdMS_TO_TICKS(5));
				}
				//disable_rx_tx();
				
				if (ack_received) {
				    http_update_lora_status(1);
				    memset(messageBuffer, 0, sizeof(messageBuffer));
				    
				} else {
				    http_update_lora_status(0);
				}
				
					xSemaphoreGive(datasent_mutex_gps);
					xSemaphoreGive(datasent_mutex_max30102);
					xSemaphoreGive(datasent_mutex_tmp116);
			}
			
			vTaskDelay(pdMS_TO_TICKS(3000));
			
		
	}
}
void app_main(void)
{
    // Initialize watchdog with 5 seconds timeout
	esp_task_wdt_config_t wdt_config = {
	    .timeout_ms = 10000,
	   .idle_core_mask = BIT(0) | BIT(1)
	};
	
	esp_err_t err = esp_task_wdt_init(&wdt_config);
	if (err == ESP_OK) {
	    printf("TWDT initialized\n");
	} else if (err == ESP_ERR_INVALID_STATE) {
	    printf("TWDT already initialized\n");
	}


	//make semaphore
	datasent_mutex_gps = xSemaphoreCreateMutex();
    if (datasent_mutex_gps == NULL) {
        printf("Failed to create mutex for GPS data.");
        return;
    }
    datasent_mutex_max30102 = xSemaphoreCreateMutex();
    if (datasent_mutex_max30102 == NULL) {
        printf("Failed to create mutex for MAX30102 data.");
        return;
    }
    datasent_mutex_tmp116 = xSemaphoreCreateMutex();
    if (datasent_mutex_tmp116 == NULL) {
        printf("Failed to create mutex for TMP116 data.");
        return;
    }
    i2c_mutex = xSemaphoreCreateMutex();
    if (i2c_mutex == NULL) {
        ESP_LOGE("I2C", "Failed to create I2C mutex");
        return;
    }
   
   //i2c init 
    err  = i2c_master_init(I2C_PORT);
    if (err == ESP_OK) {
	    printf("I2C initialized\n");
	} else{
	    printf("I2c initialize Failed\n");
	}
    vTaskDelay(pdMS_TO_TICKS(10));
   
	//max30102 init
    err = max30102_init(&max30102, I2C_PORT);
    if (err == ESP_OK) {
	    printf("MAX30102 initialized\n");
	} else{
	    printf("MAX30102 initialize Failed\n");
	}
    max30102_print_registers(&max30102);


	//gps init
    gps_init();
    
    //ap + web server
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      nvs_flash_erase();
      ret = nvs_flash_init();
    }
    if (ret == ESP_OK) {
	    printf("NVS initialized\n");
	} else if (ret == ESP_ERR_INVALID_STATE) {
	    printf("NVS initialize Failed\n");
	}

/*    ESP_LOGI(TAG, "ESP_WIFI_MODE_AP");
    wifi_init_softap();
    vTaskDelay(4000 / portTICK_PERIOD_MS);
    start_webserver();*/
    
    //tmp116 init
    tmp116_init();
    
	//BLE
	 ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(GATTS_TAG, "%s initialize controller failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(GATTS_TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(GATTS_TAG, "%s init bluetooth failed: %s", __func__, esp_err_to_name(ret));
        return;
    }
    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(GATTS_TAG, "%s enable bluetooth failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_ble_gatts_register_callback(gatts_event_handler);
    if (ret) {
        ESP_LOGE(GATTS_TAG, "gatts register error, error code = %x", ret);
        return;
    }
    ret = esp_ble_gap_register_callback(gap_event_handler);
    if (ret) {
        ESP_LOGE(GATTS_TAG, "gap register error, error code = %x", ret);
        return;
    }

    // Register all 4 services
    ret = esp_ble_gatts_app_register(PROFILE_A_APP_ID);
    if (ret) {
        ESP_LOGE(GATTS_TAG, "gatts app register error, error code = %x", ret);
        return;
    }
    ret = esp_ble_gatts_app_register(PROFILE_B_APP_ID);
    if (ret) {
        ESP_LOGE(GATTS_TAG, "gatts app register error, error code = %x", ret);
        return;
    }
    ret = esp_ble_gatts_app_register(PROFILE_C_APP_ID);
    if (ret) {
        ESP_LOGE(GATTS_TAG, "gatts app register error, error code = %x", ret);
        return;
    }
    ret = esp_ble_gatts_app_register(PROFILE_D_APP_ID);
    if (ret) {
        ESP_LOGE(GATTS_TAG, "gatts app register error, error code = %x", ret);
        return;
    }

    esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(500);
    if (local_mtu_ret) {
        ESP_LOGE(GATTS_TAG, "set local MTU failed, error code = %x", local_mtu_ret);
    }
    vTaskDelay(3000/portTICK_PERIOD_MS);
    printf("Lora Init Begun\n");
   //lora init
   spi_init();
   vTaskDelay(pdMS_TO_TICKS(10));
   radio_init();
   printf("LoRa radio init done\n");
	//tasks
   xTaskCreatePinnedToCore(max30102_task, "max30102_task", 4096, NULL, 10, NULL,0);
   xTaskCreatePinnedToCore(gps_task, "gps_task", 4096, NULL, 9, NULL,0);
   xTaskCreatePinnedToCore(tmp116_task, "tmp116_task", 4096, NULL, 8, NULL,0);
   xTaskCreatePinnedToCore(lora_task, "lora_task", 4096, NULL, 7, NULL,1);
}
