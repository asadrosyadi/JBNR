
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <driver/gpio.h>

#include "esp_log.h"
#include "esp_err.h"
#include "driver/spi_master.h"

#include "lora.h"

// define SPI2_HOST / VSPI pins
#define GPIO_NUM_MOSI 		5
#define GPIO_NUM_MISO 		4
#define GPIO_NUM_SCLK 		7
#define GPIO_NUM_CS    		11
#define GPIO_NUM_RST  		10 
#define GPIO_NUM_G0   		21
#define RX_EN 12
#define TX_EN 13

static spi_device_handle_t spi_handle;

/*
 * Initialize the SPI bus and device
 */
esp_err_t spi_init(void)
{
	esp_err_t ret;

	// reset pin configuration
	gpio_reset_pin(GPIO_NUM_RST);
   	gpio_set_direction(GPIO_NUM_RST, GPIO_MODE_OUTPUT);

	// chip select pin configuration
   	gpio_reset_pin(GPIO_NUM_CS);
   	gpio_set_direction(GPIO_NUM_CS, GPIO_MODE_OUTPUT);
	gpio_set_level(GPIO_NUM_CS, 1);
	
	gpio_reset_pin(RX_EN);
   	gpio_set_direction(RX_EN, GPIO_MODE_OUTPUT);
   	gpio_reset_pin(TX_EN);
   	gpio_set_direction(TX_EN, GPIO_MODE_OUTPUT);
   	gpio_set_level(RX_EN, 0);
	gpio_set_level(TX_EN, 0);

	spi_bus_config_t spi_bus_config = {
        .miso_io_num   = GPIO_NUM_MISO,
        .mosi_io_num   = GPIO_NUM_MOSI,
        .sclk_io_num   = GPIO_NUM_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
		.max_transfer_sz = 0
    };

	ret = spi_bus_initialize(SPI2_HOST, &spi_bus_config, SPI_DMA_CH_AUTO);
	ESP_LOGI(TAG, "spi_bus_initialize=%d",ret);
	assert(ret == ESP_OK);

	// set SPI device config and mode to CPOL=0, CPHA=0
	spi_device_interface_config_t spi_device_interface_config = {
		.clock_speed_hz = CLOCK_SPEED_MHZ * 1000 * 1000,
		.mode = 0,
		.queue_size = 1,
		.spics_io_num = -1,
		// .flags = SPI_DEVICE_NO_DUMMY,
		.pre_cb = NULL,
		.post_cb = NULL
	};

	ret = spi_bus_add_device(SPI2_HOST, &spi_device_interface_config, &spi_handle);
	ESP_LOGI(TAG, "spi_bus_add_device=%d", ret);
	assert(ret == ESP_OK);

	return ESP_OK;
}

/*
 * Do a radio reset on the RST pin
 */
void reset_radio(void)
{
	ESP_LOGI(TAG, "reseting lora radio. Starting...");
	gpio_set_level(GPIO_NUM_RST, 0);
	vTaskDelay(pdMS_TO_TICKS(10));
	gpio_set_level(GPIO_NUM_RST, 1);
	vTaskDelay(pdMS_TO_TICKS(10));
	ESP_LOGI(TAG, "reseting lora radio. Done.");
}

/**
 * Initialize default radio settings
 */
void radio_init(void)
{
	// reset radio
	reset_radio();

	// check device present
	check_radio_version();

    // set sleep mode, so we can also set LoRa mode:
	setRegOpMode(RFM9X_MODE_SLEEP);
	vTaskDelay(pdMS_TO_TICKS(10));

    // set up FIFO to use the 256 bytes
	register_write(RFM9X_0E_REG_FIFO_TX_BASE_ADDR, 0);
	register_write(RFM9X_0F_REG_FIFO_RX_BASE_ADDR, 0);

	// set stby mode
	setRegOpMode(RFM9X_MODE_STDBY);
	vTaskDelay(pdMS_TO_TICKS(10));

	/*
	 * set settings compatible with RadioHead
	 * mode Bw125Cr45Sf128 wich is:
	 * BW = 125kHz
	 * CR 4/5,
	 * SF = 128chips/symbol (SF7),
	 * CRC enabled
	 * explicit header mode
	 *
	 * at the end, the registers should be:
	 * RFM9X_1E_REG_MODEM_CONFIG1 = 0x72
	 * RFM9X_1E_REG_MODEM_CONFIG2 = 0x74
	 * RFM9X_1E_REG_MODEM_CONFIG3 = 0x04
	 */
	 
	setBandwidth(62.5e3);
	setCodingRate(4);
	setSpreadingFactor(10);
	enableCRC();
	setExplicitHeaderMode();

	// the default is 13 dBm
	setTxPower(20);
	printf("Configured TX Power :%d \n ",getTxPower());
	// default 8 bytes to match radiohead packet
	setPreambleLength(10);

	// set frequency = 915 MHz
	setFrequency(915);
	printf("Frequency: %" PRIu32 "\n", getFrecuency());

	// debbuging
	uint8_t current_reg = register_read(RFM9X_1D_REG_MODEM_CONFIG1);
	printf("Current register 1 value: 0x%x\n", current_reg);
	current_reg = register_read(RFM9X_1E_REG_MODEM_CONFIG2);
	printf("Current register 2 value: 0x%x\n", current_reg);
	current_reg = register_read(RFM9X_26_REG_MODEM_CONFIG3);
	printf("Current register 3 value: 0x%x\n", current_reg);
}

/**
 * Check the radio version
 */
void check_radio_version(void)
{
	uint8_t version_register;
	uint8_t result;

	version_register = RFM9X_42_REG_VERSION;
	printf("Reading register: %x\n", version_register);
	result = register_read(version_register);
	printf("fetched result is: 0x%x\n", result);
	if (result == RFM9X_42_REG_VERSION_VALUE) {
		printf("LoRa radio init success\n");
	} else {
		printf("LoRa radio init failed. Check wiring.\n");
	}
}

/*
 * Read a register value
 */
int register_read(int reg)
{
   uint8_t buffer_out[2] = { reg, 0xff };
   uint8_t buffer_in[2];

   spi_transaction_t t = {
      .flags = 0,	// transaction line mode
      .length = 8 * sizeof(buffer_out),
      .tx_buffer = buffer_out,
      .rx_buffer = buffer_in
   };

   gpio_set_level(GPIO_NUM_CS, 0);
   spi_device_transmit(spi_handle, &t);
   gpio_set_level(GPIO_NUM_CS, 1);

   return buffer_in[1];
}

/*
 * Write a value to a register
 */
esp_err_t register_write(uint8_t reg, uint8_t value)
{
	uint8_t buffer_out[2] = { reg | SPI_WRITE_BIT_MASK, value };

	spi_transaction_t t = {
		.flags = 0,
		.length = 8 * sizeof(buffer_out),
		.tx_buffer = buffer_out,
		.rx_buffer = NULL
	};

	gpio_set_level(GPIO_NUM_CS, 0);
	spi_device_transmit(spi_handle, &t);
	gpio_set_level(GPIO_NUM_CS, 1);

	return ESP_OK;
}

/*
 * Set the preamble length
 * param length, can be from 6 to 65535 symbols
*/
void setPreambleLength(uint16_t length)
{
	// write the MSB in case length > 255
	register_write(RFM9X_20_REG_PREAMBLE_MSB, (length >> 8) & 0xff);
	// write the LSB
	register_write(RFM9X_21_REG_PREAMBLE_LSB, length & 0xff);
}

/*
 * Get the preamble length
*/
uint8_t getPreambleLength()
{
	uint8_t msb = register_read(RFM9X_20_REG_PREAMBLE_MSB);
	uint8_t lsb = register_read(RFM9X_21_REG_PREAMBLE_LSB);

	uint16_t length = (msb << 8) | lsb;

	return length;
}

/*
 * Set the frequency in MHz
*/
void setFrequency(uint32_t frequency)
{
	// calculate frf register value
	frequency = frequency * 1000000.0;
	uint64_t frf = ((uint64_t) (frequency) << 19) / RFM9X_FXOSC;

	register_write(RFM9X_06_REG_FRF_MSB, (frf >> 16) & 0xff);
	register_write(RFM9X_07_REG_FRF_MID, (frf >> 8) & 0xff);
	register_write(RFM9X_08_REG_FRF_LSB, frf & 0xff);
}

/*
 * Get the frequency in Hz
*/
uint32_t getFrecuency()
{
	uint32_t frequency = 0;

	uint8_t msb = register_read(RFM9X_06_REG_FRF_MSB);
	uint8_t mid = register_read(RFM9X_07_REG_FRF_MID);
	uint8_t lsb = register_read(RFM9X_08_REG_FRF_LSB);

	frequency = ((msb << 16) | (mid << 8) | lsb) * RFM9X_FXOSC / (1 << 19);

	return frequency;
}

/*
 * Set output power in dBm
 */
/*void setTxPower(int8_t power)
{
	// set limits
	if (power > 17) {
		power = 17;
	} else if (power < 2) {
		power = 2;
	}

	register_write(RFM9X_09_REG_PA_CONFIG, PA_BOOST_PIN | (power - 2));
}*/
void setTxPower(int8_t power)
{
    uint8_t paConfig;
    uint8_t paDac;
    uint8_t ocpTrim;

    if (power < 2)  power = 2;
    if (power > 20) power = 20;

    setRegOpMode(RFM9X_MODE_STDBY);
    vTaskDelay(pdMS_TO_TICKS(2));

    if (power == 20) {
        paDac    = 0x87;     // +20 dBm enable
        paConfig = 0x80 | 0x0F;
        ocpTrim  = 18;       // ~130 mA SAFE
    } else {
        paDac    = 0x84;
        paConfig = 0x80 | ((power >= 17) ? 15 : (power - 2));
        ocpTrim  = 12;       // ~100 mA
    }

    register_write(RFM9X_4D_REG_PA_DAC, paDac);
    register_write(RFM9X_0B_REG_OCP, 0x20 | (ocpTrim & 0x1F));
    register_write(RFM9X_09_REG_PA_CONFIG, paConfig);
}

/*
 * Get the tx power in dBm
 */
/*uint8_t getTxPower()
{
	uint8_t regPaConfig = register_read(RFM9X_09_REG_PA_CONFIG);

	uint8_t power = regPaConfig & 0x0f;

	return power + 2;
}*/
uint8_t getTxPower(void)
{
    uint8_t paConfig = register_read(RFM9X_09_REG_PA_CONFIG);
    uint8_t paDac    = register_read(RFM9X_4D_REG_PA_DAC);

    printf("Debug: PA_Config=0x%02X, PA_DAC=0x%02X\n", paConfig, paDac);

    if ((paConfig & 0x80) && paDac == 0x87) {
        // PA_BOOST + +20 dBm mode
        return 20;
    } else if (paConfig & 0x80) {
        // PA_BOOST, normal mode +2..17
        return 2 + (paConfig & 0x0F);
    } else {
        // RFO pin, limited to -4..15
        return -4 + (paConfig & 0x0F);
    }
}

/*
 * Set the signal bandwidth
*/
void setBandwidth(double bandwidth)
{
	uint8_t signal_bw;
	uint8_t regModemConfig1 = register_read(RFM9X_1D_REG_MODEM_CONFIG1);

	if (bandwidth <= 7.8e3) {
		signal_bw = 0b00000000;
	} else if (bandwidth <= 10.4e3) {
		signal_bw = 1;
	} else if (bandwidth <= 15.6e3) {
		signal_bw = 2;
	} else if (bandwidth <= 20.8e3) {
		signal_bw = 3;
	} else if (bandwidth <= 31.25e3) {
		signal_bw = 4;
	} else if (bandwidth <= 41.7e3) {
		signal_bw = 5;
	} else if (bandwidth <= 62.5e3) {
		signal_bw = 6;
	} else if (bandwidth <= 125e3) {
		signal_bw = 7;
	} else if (bandwidth <= 250e3) {
		signal_bw = 8;
	} else if (bandwidth <= 500e3) {
		signal_bw = 9;
	} else {
		printf("Invalid bandwidth\n");
		return;
	}

	// clear the bits 7-4
	regModemConfig1 = regModemConfig1 & 0x0f;

	// set the new value
	regModemConfig1 |= (signal_bw << 4);

	register_write(RFM9X_1D_REG_MODEM_CONFIG1, regModemConfig1);
}

/*
 * set implicit header mode
 */
void setImplicitHeaderMode()
{
	uint8_t regModemConfig1 = register_read(RFM9X_1D_REG_MODEM_CONFIG1);

	// set bit 0 = 1
	regModemConfig1 |= 0x01;

	register_write(RFM9X_1D_REG_MODEM_CONFIG1, regModemConfig1);
}

/*
 * set explicit header mode
 */
void setExplicitHeaderMode()
{
	uint8_t regModemConfig1 = register_read(RFM9X_1D_REG_MODEM_CONFIG1);

	// clear bit 0
	regModemConfig1 &= 0xfe;

	register_write(RFM9X_1D_REG_MODEM_CONFIG1, regModemConfig1);
}

/*
 * Set the spreading factor
 * param sf, spreading factor from 6 to 12
 * sf 6 is a special case and uses implicit header mode
*/
void setSpreadingFactor(uint8_t sf)
{
	uint8_t regModemConfig2 = register_read(RFM9X_1E_REG_MODEM_CONFIG2);

	if (sf < 6 || sf > 12) {
		printf("Invalid spreading factor\n");
		return;
	}

	// SF are in bits 7-4 (SpreadingFactor of register RegModemConfig2)
	regModemConfig2 = (regModemConfig2 & 0x0f) | ((sf << 4) & 0xf0);

	register_write(RFM9X_1E_REG_MODEM_CONFIG2, regModemConfig2);
}

/*
 * Get the spreading factor
*/
uint8_t getSpreadingFactor()
{
	uint8_t regModemConfig2 = register_read(RFM9X_1E_REG_MODEM_CONFIG2);

	uint8_t sf = (regModemConfig2 >> 4) & 0x0f;

	return sf;
}

/*
* Get the signal bandwidth number from 0 to 9
*/
uint8_t getSignalBandwidth()
{
	uint8_t regModemConfig1 = register_read(RFM9X_1D_REG_MODEM_CONFIG1);

	uint8_t bw = (regModemConfig1 >> 4) & 0x0f;

	return bw;
}

/*
 * Set the coding rate denominator (4/x)
 * bits 3-1 in register RFM9X_1D_REG_MODEM_CONFIG1
*/
void setCodingRate(uint8_t denominator)
{
	uint8_t regModemConfig1 = register_read(RFM9X_1D_REG_MODEM_CONFIG1);

	// fallback to a valid value
	if (denominator < 5) {
		denominator = 5;
	} else if (denominator > 8) {
		denominator = 8;
	}

	// CR are in bits 3-1 with an offset of 4
	// 0xf1 is used to clear the bits 3-1 (11110001)
	// set the new value, multiply by 2 to get the denominator
	regModemConfig1 = (regModemConfig1 & 0xf1) | ((denominator - 4) << 1);

	register_write(RFM9X_1D_REG_MODEM_CONFIG1, regModemConfig1);
}

/*
 * Get the coding rate denominator (4/x)
 * bits 3-1 in register RFM9X_1D_REG_MODEM_CONFIG1
*/
uint8_t getCodingRate()
{
	uint8_t regModemConfig1 = register_read(RFM9X_1D_REG_MODEM_CONFIG1);

	// clear all bits except 1-3, then shift to the right and add 4
	uint8_t denominator = ((regModemConfig1 & 0x0e) >> 1) + 4;

	return denominator;
}

/*
 * Enable CRC
 * bit 2 in register RFM9X_1E_REG_MODEM_CONFIG2
*/
void enableCRC()
{
	uint8_t regModemConfig2 = register_read(RFM9X_1E_REG_MODEM_CONFIG2);

	// set bit 2 = 1
	regModemConfig2 |= 0x04;

	register_write(RFM9X_1E_REG_MODEM_CONFIG2, regModemConfig2);
}

/*
 * Disable CRC
*/
void disableCRC()
{
	uint8_t regModemConfig2 = register_read(RFM9X_1E_REG_MODEM_CONFIG2);

	// clear bit 2
	regModemConfig2 &= 0xfb;

	register_write(RFM9X_1E_REG_MODEM_CONFIG2, regModemConfig2);
}

/*
 * Get the current operation mode
 */
uint8_t getRegOpMode()
{
	uint8_t regOpMode = register_read(RFM9X_01_REG_OP_MODE);

	return regOpMode;
}

/*
 * Set the operation mode
 */
uint8_t setRegOpMode(uint8_t mode)
{
	switch (mode) {
		case RFM9X_MODE_SLEEP:
		case RFM9X_MODE_STDBY:
		case RFM9X_MODE_FSTX:
		case RFM9X_MODE_TX:
		case RFM9X_MODE_FSRX:
		case RFM9X_MODE_RXCONTINUOUS:
		case RFM9X_MODE_RXSINGLE:
			register_write(RFM9X_01_REG_OP_MODE, mode | RFM9X_LONG_RANGE_MODE);
			break;

		default:
			printf("Unknown mode\n");
	}

	return getRegOpMode();
}

/*
 * Get the LoRa operation mode
  */
uint8_t getRegOpLoraMode()
{
	uint8_t regOpMode = getRegOpMode();
	uint8_t mode = regOpMode & 0x07;

	return mode;
}

/**
 * Get the current operation mode of the RFM9x module
*/
void getCurrentOpMode()
{
	uint8_t regOpMode = getRegOpMode();
    uint8_t mode = regOpMode & 0x80;

	if (mode == RFM9X_LONG_RANGE_MODE) {
		printf("LoRa mode: ");
		mode = regOpMode & 0x07;

		switch (mode) {
			case RFM9X_MODE_SLEEP:
				printf("Sleep mode\n");
				break;
			case RFM9X_MODE_STDBY:
				printf("Stdby mode\n");
				break;
			case RFM9X_MODE_FSTX:
				printf("FS mode TX (FSTx)\n");
				break;
			case RFM9X_MODE_TX:
				printf("Transmitter mode (Tx)\n");
				break;
			case RFM9X_MODE_FSRX:
				printf("FS mode RX (FSRx)\n");
				break;
			case RFM9X_MODE_RXCONTINUOUS:
				printf("Continuous receiver mode (Rx)\n");
				break;
			case RFM9X_MODE_RXSINGLE:
				printf("Single receiver mode (Rx)\n");
				break;
			default:
				printf("Unknown mode\n");
		}

	} else {
		printf("FSK/OOK mode\n");
	}
}

/*
 * Send a packet to the radio module
 */
void send(uint8_t *data,uint8_t length)
{
	uint8_t identifier = 1;
	
	gpio_set_level(TX_EN, 1);
	gpio_set_level(RX_EN, 0);
	// debbuging
	
	// debbuging
	printf("The length of the data is: %d\n", length);
	
	// set standby mode
	setRegOpMode(RFM9X_MODE_STDBY);
	vTaskDelay(pdMS_TO_TICKS(10));
	// Reset FIFO for TX
	
	//additional line to fix the format error to clean the buffer
	//register_write(RF9X_REG_FIFO_ADDR_PTR, 0x00);

	// set the address pointer in FIFO data buffer
	register_write(RFM9X_0D_REG_FIFO_ADDR_PTR, 0);

	printf("writing header to FIFO\n");
	register_write(RFM9X_00_REG_FIFO, RADIOHEAD_HEADER_TO);
	register_write(RFM9X_00_REG_FIFO, RADIOHEAD_HEADER_FROM);
	register_write(RFM9X_00_REG_FIFO, RADIOHEAD_HEADER_ID);
	register_write(RFM9X_00_REG_FIFO, RADIOHEAD_HEADER_FLAGS + identifier++);

	// write the packet to the FIFO
	for (int i = 0; i < length; i++) {
		register_write(RFM9X_00_REG_FIFO, data[i]);
		//printf("%c\n", data[i]);

	}

	// set the length of the packet
	register_write(RFM9X_22_REG_PAYLOAD_LENGTH, length + RFM9X_HEADER_LEN);

	// debbuging
	printf("packet length: %d\n", length + RFM9X_HEADER_LEN);

	// set TX mode
	setRegOpMode(RFM9X_MODE_TX);
	TickType_t start = xTaskGetTickCount();
	TickType_t timeout = pdMS_TO_TICKS(3000);   // 3 seconds max
	
	while ((register_read(RFM9X_12_REG_IRQ_FLAGS) & IRQ_TX_DONE_MASK) == 0)
	{
	    if ((xTaskGetTickCount() - start) >= timeout) {
	        printf("LoRa TX timeout (3s)\n");
	        break;  
	    }
	    vTaskDelay(pdMS_TO_TICKS(100));
	}


	// clear the TX_DONE flag
	register_write(RFM9X_12_REG_IRQ_FLAGS, IRQ_TX_DONE_MASK);
	printf("\npacket sent\n");

}

/**
 * Wait for the packet to be received
*/
void setRxMode()
{
	// set continuous RX mode
	setRegOpMode(RFM9X_MODE_RXCONTINUOUS);
	// interrupt on RxDone
	register_write(RFM9X_40_REG_DIO_MAPPING1, 0x00);
}
void disable_rx_tx(){
	gpio_set_level(TX_EN, 0);
	gpio_set_level(RX_EN, 0);
}
void set_tx_enable(){
	gpio_set_level(TX_EN, 1);
	gpio_set_level(RX_EN, 0);
}
void set_rx_enable(){
	gpio_set_level(TX_EN, 0);
	gpio_set_level(RX_EN, 1);
}