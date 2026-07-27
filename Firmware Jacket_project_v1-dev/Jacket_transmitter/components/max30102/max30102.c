#include <stdio.h>
#include "max30102.h"
#include <string.h>
#include "freertos/task.h"
#include "esp_err.h"
#include "i2c_lock.h"

#define I2C_PORT I2C_NUM_0

esp_err_t max30102_init(max30102_t *this, i2c_port_t i2c_num)
{
    this->i2c_num = i2c_num;
    this->_interrupt_flag = 0;
    memset(this->_ir_samples, 0, MAX30102_SAMPLE_LEN_MAX * sizeof(uint32_t));
    memset(this->_red_samples, 0, MAX30102_SAMPLE_LEN_MAX * sizeof(uint32_t));

    esp_err_t ret = max30102_write_register(this,MAX30102_FIFO_WR_PTR,0x00);
    if(ret != ESP_OK) return ret;
    ret = max30102_write_register(this,MAX30102_MULTI_LED_CTRL_1,0x21);
    if(ret != ESP_OK) return ret;
    ret = max30102_write_register(this,MAX30102_OVF_COUNTER,0x00);
    if(ret != ESP_OK) return ret;
    ret = max30102_write_register(this,MAX30102_FIFO_RD_PTR,0x00);
    if(ret != ESP_OK) return ret;
    ret = max30102_write_register(this,MAX30102_MODE_CONFIG, 0x03);
    if(ret != ESP_OK) return ret;
    ret = max30102_write_register(this, MAX30102_SPO2_CONFIG, 0x4F);  // 400 Hz, 16-bit pulse width
    if(ret != ESP_OK) return ret;
	ret = max30102_write_register(this, MAX30102_FIFO_CONFIG, 0x70); // 8-sample averaging, rollove,fifo full 32
	if(ret != ESP_OK) return ret;
	ret = max30102_write_register(this, MAX30102_INTERRUPT_ENABLE_1, 0xC0);
	if(ret != ESP_OK) return ret;
	ret = max30102_set_led_current(this,  MAX30102_LED_CURRENT_11MA,  MAX30102_LED_CURRENT_11MA);
    if(ret != ESP_OK) return ret;
	printf("Max 30102 Initialized successfully\n");
    return ret;
}
esp_err_t max30102_set_led_current( max30102_t* this,max30102_current_t red_current,max30102_current_t ir_current )
{
   esp_err_t ret = max30102_write_register(this, MAX30102_LED_IR_PA1, ir_current);
   if(ret != ESP_OK) return ret;
   return max30102_write_register(this, MAX30102_LED_RED_PA2, red_current);
}

esp_err_t max30102_write_register( max30102_t* this, uint8_t address, uint8_t val)
{
    // start transmission to device
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MAX30102_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);

    i2c_master_write_byte(cmd, address, true); // send register address
    i2c_master_write_byte(cmd, val, true); // send value to write

    // end transmission
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin( this->i2c_num, cmd, pdMS_TO_TICKS(1000) );
    i2c_cmd_link_delete(cmd);
    return ret;
}

esp_err_t max30102_read_register( max30102_t* this, uint8_t address,uint8_t* reg)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MAX30102_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, address, true);

    //i2c_master_stop(cmd);
    i2c_master_start(cmd);

    i2c_master_write_byte(cmd, (MAX30102_I2C_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, reg, 1); //1 is NACK

    // end transmission
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin( this->i2c_num,
                                          cmd,
                                          pdMS_TO_TICKS(1000) );

    i2c_cmd_link_delete(cmd);
    return ret;
}
// Reads num bytes starting from address register on device in to _buff array
esp_err_t max30102_read_from( max30102_t* this, uint8_t address, uint8_t* reg, uint8_t size )
{
    if(!size)
        return ESP_OK;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MAX30102_I2C_ADDR << 1) | I2C_MASTER_WRITE, 1);
    i2c_master_write_byte(cmd, address, true);

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MAX30102_I2C_ADDR << 1) | I2C_MASTER_READ, true);

    if(size > 1)
        i2c_master_read(cmd, reg, size-1, 0); //0 is ACK

    i2c_master_read_byte(cmd, reg+size-1, 1); //1 is NACK

    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin( this->i2c_num,
                                          cmd,
                                          pdMS_TO_TICKS(1000) );
    i2c_cmd_link_delete(cmd);
    return ret;
}

void check_ret(esp_err_t ret,uint8_t sensor_data_h){
	if(ret == ESP_ERR_TIMEOUT) {
		printf("I2C timeout\n");
	} else if(ret == ESP_OK) {
		printf("******************* \n");
		printf("TASK[%d]  MASTER READ SENSOR( EN_MAX30102_READING_TASK )\n", 0);
		printf("*******************\n");
		printf("data: %02x\n", sensor_data_h);
	} else {
		printf("%s: No ack, sensor not connected...skip...\n", esp_err_to_name(ret));
	}
}

esp_err_t max30102_read_fifo(i2c_port_t i2c_num, uint32_t ir[], uint32_t red[], int samples)
{	//i2c_lock();
    uint8_t fifo_data[6 * samples];
    esp_err_t ret;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MAX30102_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, MAX30102_FIFO_DATA, true);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(i2c_num, cmd, pdMS_TO_TICKS(200));
    i2c_cmd_link_delete(cmd);
    if (ret != ESP_OK) return ret;

    //Burst read of FIFO (6 bytes * samples)
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MAX30102_I2C_ADDR << 1) | I2C_MASTER_READ, true);

    if (samples > 1) {
        i2c_master_read(cmd, fifo_data, (6 * samples) - 1, I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, fifo_data + (6 * samples) - 1, I2C_MASTER_NACK);

    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(i2c_num, cmd, pdMS_TO_TICKS(200));
    i2c_cmd_link_delete(cmd);
    if (ret != ESP_OK) return ret;
    
   // i2c_unlock();

    //Parse each 18-bit IR/RED sample
    for (int i = 0; i < samples; i++) {
        int offset = i * 6;

        ir[i]  = ((fifo_data[offset + 0] & 0x03) << 16) |
                  (fifo_data[offset + 1] << 8) |
                   fifo_data[offset + 2];

        red[i] = ((fifo_data[offset + 3] & 0x03) << 16) |
                 (fifo_data[offset + 4] << 8) |
                  fifo_data[offset + 5];
    }

    return ESP_OK;
}

esp_err_t max30102_print_registers(max30102_t* this)
{
    uint8_t int_status, int_enable, fifo_write, fifo_ovf_cnt, fifo_read;
    uint8_t fifo_data, mode_conf, sp02_conf, led_conf, temp_int;
    uint8_t rev_id;
    esp_err_t ret;

    ret = max30102_read_register(this, MAX30102_INTERRUPT_STATUS_1, &int_status);
    if(ret != ESP_OK) return ret;
    ret = max30102_read_register(this, MAX30102_INTERRUPT_ENABLE_1, &int_enable);
    if(ret != ESP_OK) return ret;
    ret = max30102_read_register(this, MAX30102_FIFO_WR_PTR, &fifo_write);
    if(ret != ESP_OK) return ret;
    ret = max30102_read_register( this, MAX30102_OVF_COUNTER, &fifo_ovf_cnt );
    if(ret != ESP_OK) return ret;
    ret = max30102_read_register(this, MAX30102_FIFO_RD_PTR, &fifo_read);
    if(ret != ESP_OK) return ret;
    ret = max30102_read_register(this, MAX30102_FIFO_DATA, &fifo_data);
    if(ret != ESP_OK) return ret;
    ret = max30102_read_register(this, MAX30102_MODE_CONFIG, &mode_conf);
    if(ret != ESP_OK) return ret;
    ret = max30102_read_register(this, MAX30102_SPO2_CONFIG, &sp02_conf);
    if(ret != ESP_OK) return ret;
    ret = max30102_read_register(this, MAX30102_LED_IR_PA1, &led_conf);
    if(ret != ESP_OK) return ret;
    ret = max30102_read_register(this, MAX30102_LED_RED_PA2, &temp_int);
    if(ret != ESP_OK) return ret;
    ret = max30102_read_register(this, MAX30102_I2C_ADDR, &rev_id);
    if(ret != ESP_OK) return ret;
	printf("Status\t Enable\t FIFO Wrt\t FIFO Ovf Cnt\t FIFO Read\t FIFO Data\t Mode Conf\t Spo2 Conf\t LED Conf\t Temp Conf\t Rev Id \n");
    printf("%x\t\t", int_status);
    printf("%x\t\t", int_enable);
    printf("%x\t\t", fifo_write);
    printf("%x\t\t", fifo_ovf_cnt);
    printf("%x\t\t", fifo_read);
    printf("%x\t\t", fifo_data);
    printf("%x\t\t", mode_conf);
    printf("%x\t\t", sp02_conf);
    printf("%x\t\t", led_conf);
    printf("%x\t\t", temp_int);
    printf("%x\n", rev_id);

    return ESP_OK;
}
