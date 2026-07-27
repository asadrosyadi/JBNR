#include "esp_err.h"
#include <stdint.h>
//#include "driver/i2c.h"

#define TAG 		  							"RFM9x"
#define SPI_WRITE_BIT_MASK 						0x80
// clock speed in MHz
#define CLOCK_SPEED_MHZ							5
// +20dBm power
#define PA_BOOST_PIN 							0x80

// Table 41. Registers Summary
#define RFM9X_00_REG_FIFO       				0x00
#define RFM9X_01_REG_OP_MODE     				0x01
#define RFM9X_02_REG_BITRATE_MSB 				0x02
#define RFM9X_03_REG_BITRATE_LSB 				0x03
#define RFM9X_04_REG_FDEV_MSB   				0x04
#define RFM9X_05_REG_FDEV_LSB   				0x05
#define RFM9X_06_REG_FRF_MSB    				0x06
#define RFM9X_07_REG_FRF_MID    				0x07
#define RFM9X_08_REG_FRF_LSB    				0x08
#define RFM9X_09_REG_PA_CONFIG   				0x09
#define RFM9X_0A_REG_PA_RAMP     				0x0a
#define RFM9X_0B_REG_OCP       					0x0b
#define RFM9X_0C_REG_LNA       					0x0c
#define RFM9X_0D_REG_FIFO_ADDR_PTR 				0x0d
#define RFM9X_0E_REG_FIFO_TX_BASE_ADDR 			0x0e
#define RFM9X_0F_REG_FIFO_RX_BASE_ADDR 			0x0f
#define RFM9X_10_REG_FIFO_RX_CURRENT_ADDR 		0x10
#define RFM9X_11_REG_IRQ_FLAGS_MASK 			0x11
#define RFM9X_12_REG_IRQ_FLAGS     				0x12
#define RFM9X_13_REG_RX_NB_BYTES   				0x13
#define RFM9X_14_REG_RX_HEADER_CNT_VALUE_MSB 	0x14
#define RFM9X_15_REG_RX_HEADER_CNT_VALUE_LSB 	0x15
#define RFM9X_16_REG_RX_PACKET_CNT_VALUE_MSB 	0x16
#define RFM9X_17_REG_RX_PACKET_CNT_VALUE_LSB 	0x17
#define RFM9X_18_REG_MODEM_STAT    				0x18
#define RFM9X_19_REG_PKT_SNR_VALUE 				0x19
#define RFM9X_1A_REG_PKT_RSSI_VALUE 			0x1a
#define RFM9X_1B_REG_RSSI_VALUE    				0x1b
#define RFM9X_1C_REG_HOP_CHANNEL   				0x1c
#define RFM9X_1D_REG_MODEM_CONFIG1 				0x1d
#define RFM9X_1E_REG_MODEM_CONFIG2 				0x1e
#define RFM9X_1F_REG_SYMB_TIMEOUT_LSB 			0x1f
#define RFM9X_20_REG_PREAMBLE_MSB  				0x20
#define RFM9X_21_REG_PREAMBLE_LSB  				0x21
#define RFM9X_22_REG_PAYLOAD_LENGTH 			0x22
#define RFM9X_23_REG_MAX_PAYLOAD_LENGTH 		0x23
#define RFM9X_24_REG_HOP_PERIOD    				0x24
#define RFM9X_25_REG_FIFO_RX_BYTE_ADDR 			0x25
#define RFM9X_26_REG_MODEM_CONFIG3 				0x26
#define RFM9X_28_REG_FEI_MSB       				0x28
#define RFM9X_29_REG_FEI_MID       				0x29
#define RFM9X_2A_REG_FEI_LSB       				0x2A
#define RFM9X_2C_REG_RSSI_WIDEBAND 				0x2C
#define RFM9X_31_REG_DETECTION_OPTIMIZE 		0x31
#define RFM9X_33_REG_INVERT_IQ     				0x33
#define RFM9X_37_REG_DETECTION_THRESHOLD 		0x37
#define RFM9X_39_REG_SYNC_WORD     				0x39
#define RFM9X_40_REG_DIO_MAPPING1  				0x40
#define RFM9X_41_REG_DIO_MAPPING2  				0x41
#define RFM9X_42_REG_VERSION       				0x42
#define RFM9X_4B_REG_TCXO         				0x4B
#define RFM9X_4D_REG_PA_DAC        				0x4D
#define RFM9X_5B_REG_FORMER_TEMP  				0x5B
#define RFM9X_61_REG_AGC_REF      				0x61
#define RFM9X_62_REG_AGC_THRESH1  				0x62
#define RFM9X_63_REG_AGC_THRESH2  				0x63
#define RFM9X_64_REG_AGC_THRESH3  				0x64
#define RFM9X_70_REG_PLL  						0x70

// Table 16. LoRa Operating Mode Functionality
#define RFM9X_LONG_RANGE_MODE					0x80
#define RFM9X_ACCESS_SHARED_REG					0x40
#define RFM9X_LOW_FREQUENCY_MODE                0x08
#define RFM9X_MODE                              0x07
#define RFM9X_MODE_SLEEP                        0x00
#define RFM9X_MODE_STDBY                        0x01
#define RFM9X_MODE_FSTX                         0x02
#define RFM9X_MODE_TX                           0x03
#define RFM9X_MODE_FSRX                         0x04
#define RFM9X_MODE_RXCONTINUOUS                 0x05
#define RFM9X_MODE_RXSINGLE                     0x06
#define RFM9X_MODE_CAD                          0x07

//additional 
//#define RF9X_REG_FIFO_ADDR_PTR					0x0D

// default value for register version
#define RFM9X_42_REG_VERSION_VALUE 				0x12

// FXOSC = 32MHz
#define RFM9X_FXOSC 							32000000.0

// RadioHead packet format
#define RFM9X_HEADER_LEN 						4
#define RADIOHEAD_HEADER_TO 					0xff
#define RADIOHEAD_HEADER_FROM 					0xff
#define RADIOHEAD_HEADER_ID 					0xff
#define RADIOHEAD_HEADER_FLAGS 					0x00
#define IRQ_TX_DONE_MASK 						0x08
#define IRQ_RX_DONE_MASK                        0x40
#define MAX_PAYLOAD_LENGTH 						255 - RFM9X_HEADER_LEN

// initialize SPI and radio functions
esp_err_t spi_init(void);
void reset_radio(void);
void radio_init(void);
void check_radio_version(void);

// register read/write functions
int register_read(int reg);
esp_err_t register_write(uint8_t reg, uint8_t value);

// LoRa radio functions
void setPreambleLength(uint16_t length);
uint8_t getPreambleLength();
void setFrequency(uint32_t frequency);
uint32_t getFrecuency();
void setTxPower(int8_t power);
uint8_t getTxPower();
void setBandwidth(double bandwidth);
uint8_t getSignalBandwidth();
void setCodingRate(uint8_t denominator);
uint8_t getCodingRate();
void setImplicitHeaderMode();
void setExplicitHeaderMode();
void setSpreadingFactor(uint8_t sf);
uint8_t getSpreadingFactor();
void enableCRC();
void disableCRC();
void send(uint8_t *data,uint8_t length);
void waitPacketSent();
void setRxMode();

// operation mode functions
uint8_t getRegOpMode();
uint8_t getRegOpLoraMode();
uint8_t setRegOpMode(uint8_t mode);
void getCurrentOpMode();
void set_rx_enable();
void set_tx_enable();
void disable_rx_tx();