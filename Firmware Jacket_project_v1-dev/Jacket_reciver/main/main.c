#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include "lora.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ble_server.h"
#include <string.h>
#include "data_packet.h"
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
void decode_and_print_packet(const uint8_t *rx_buf, uint8_t rx_len)
{
    if (rx_buf == NULL)
    {
        printf("RX error: NULL buffer\n");
        return;
    }

    if (rx_len != sizeof(data_packet_t))
    {
        printf("RX error: invalid length %d (expected %d)\n",
               rx_len, sizeof(data_packet_t));
        return;
    }

    data_packet_t pkt;
    memcpy(&pkt, rx_buf, sizeof(data_packet_t));

    /* Convert back to real-world values */
    float latitude  = pkt.lat_x1e6  / 1e6f;
    float longitude = pkt.lon_x1e6  / 1e6f;
    float temperature = pkt.temp_x100 / 100.0f;
    float heart_rate  = pkt.hr_x10    / 10.0f;
    float spo2        = pkt.spo2_x10  / 10.0f;

    /* Print decoded data */
    printf("----- RX DATA PACKET -----\n");
    printf("Latitude   : %.6f\n", latitude);
    printf("Longitude  : %.6f\n", longitude);
    printf("Temperature: %.2f C\n", temperature);
    printf("Heart Rate : %.1f bpm\n", heart_rate);
    printf("SpO2       : %.1f %%\n", spo2);
    printf("--------------------------\n");
    
    update_sensor_data_and_notify(heart_rate, spo2, latitude, longitude, temperature);
}

void app_main(void)
{
    spi_init();

	radio_init();

	printf("LoRa radio init done\n");
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


	// receiver example
	uint8_t messageBuffer[MAX_PAYLOAD_LENGTH];
/*while (1) {
    setRxMode();  // Ensure the radio is in RX mode

    uint8_t irq_flags = register_read(RFM9X_12_REG_IRQ_FLAGS);

    // --- Check for CRC error and discard packet ---
    if (irq_flags & IRQ_PAYLOAD_CRC_ERROR_MASK) {
        printf("CRC ERROR - packet discarded\n");

        // Clear CRC error flag
        register_write(RFM9X_12_REG_IRQ_FLAGS, IRQ_PAYLOAD_CRC_ERROR_MASK);

        vTaskDelay(pdMS_TO_TICKS(10));  // short delay to yield CPU
        continue;  // skip to next loop iteration
    }

    // --- Check if RX done (packet received successfully) ---
    if (irq_flags & IRQ_RX_DONE_MASK) {

        // Number of bytes in payload
        uint8_t length = register_read(RFM9X_13_REG_RX_NB_BYTES);
        printf("Length: %d\n", length);

        memset(messageBuffer, 0, sizeof(messageBuffer));

        // Set FIFO pointer to start of received packet
        uint8_t fifoStart = register_read(RFM9X_10_REG_FIFO_RX_CURRENT_ADDR);
        register_write(RFM9X_0D_REG_FIFO_ADDR_PTR, fifoStart);

        // Read payload from FIFO
        for (int i = 0; i < length; i++) {
            messageBuffer[i] = register_read(RFM9X_00_REG_FIFO);
        }
        messageBuffer[length] = '\0';

        // --- Optional: Remove header bytes if needed ---
        int remove = 4;  // adjust if your protocol uses header
        if (length > remove) {
            memmove(messageBuffer, messageBuffer + remove, length - remove + 1);
        }

        // --- RSSI and SNR calculation ---
        int16_t rssi = -137 + register_read(0x1A);
        int8_t snr_raw = register_read(0x19);
        int16_t snr = (snr_raw < 0) ? -(snr_raw >> 2) : (snr_raw >> 2);

        printf("RSSI= %d\n", rssi);
        printf("SNR= %d\n", snr);
        printf("Message: %s\n", messageBuffer);

        // --- Decode payload ---
        decode_and_print_packet(messageBuffer, length - remove);

        // --- Clear RX_DONE flag ---
        register_write(RFM9X_12_REG_IRQ_FLAGS, IRQ_RX_DONE_MASK);

        // Reset FIFO pointer to RX base
        register_write(RFM9X_0D_REG_FIFO_ADDR_PTR,
                       register_read(RFM9X_0F_REG_FIFO_RX_BASE_ADDR));
    }

    // Small delay to yield CPU and avoid WDT triggers
    vTaskDelay(pdMS_TO_TICKS(10));
}*/

while (1) {
    setRxMode();
    set_rx_enable();
	//printf("-----------------------------RX MODE---------------------------\n");
    uint8_t irq_flags = register_read(RFM9X_12_REG_IRQ_FLAGS);
    if (irq_flags & IRQ_RX_DONE_MASK) {
		
		uint8_t length = register_read(RFM9X_13_REG_RX_NB_BYTES);

		printf("Length: %d\n", length);
        memset(messageBuffer, 0, sizeof(messageBuffer));
        
		int16_t rssi = -137 + register_read(0x1A);
        int16_t snr = -(register_read(0x19))/4;
        printf("RSSI= %d\n",rssi);
        printf("SNR= %d\n",snr);
        
        uint8_t fifoStart = register_read(RFM9X_10_REG_FIFO_RX_CURRENT_ADDR);
        register_write(RFM9X_0D_REG_FIFO_ADDR_PTR, fifoStart);
        
        
		if (irq_flags & 0X20) {
        printf("CRC ERROR - packet discarded\n");
		
        register_write(RFM9X_12_REG_IRQ_FLAGS, 0XFF);
        register_write(RFM9X_0D_REG_FIFO_ADDR_PTR, register_read(RFM9X_0F_REG_FIFO_RX_BASE_ADDR));

        vTaskDelay(pdMS_TO_TICKS(10)); 
        continue;
   		 }

		

        for (int i = 0; i < length; i++) {
            messageBuffer[i] = register_read(RFM9X_00_REG_FIFO);
        }
        messageBuffer[length] = '\0';
        

        
		int remove = 4; 

		if (length > remove) {
		    memmove(messageBuffer, messageBuffer + remove, length - remove + 1);  
		}

        printf("Message (%d bytes): ", length);
		for (uint8_t i = 0; i < length; i++) {
		    printf("%02X ", messageBuffer[i]);
		}
		printf("\r\n");
		
        decode_and_print_packet(messageBuffer, (length-4));

		disable_rx_tx();
		
        register_write(RFM9X_12_REG_IRQ_FLAGS, 0xFF);
        register_write(RFM9X_0D_REG_FIFO_ADDR_PTR, register_read(RFM9X_0F_REG_FIFO_RX_BASE_ADDR));
        
       // send("ACK");
    }
   // send("ACKKKK");
    vTaskDelay(pdMS_TO_TICKS(10));
}


}
