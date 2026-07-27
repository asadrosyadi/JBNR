#ifndef LORA_H
#define LORA_H

#include <stdint.h>
#include "esp_err.h"

#define SPI_WRITE_BIT_MASK 0x80
// clock speed in MHz
#define LORA_CLOCK_SPEED_MHZ 5

// Table 41. Registers Summary
#define RFM9X_00_REG_FIFO                    0x00
#define RFM9X_01_REG_OP_MODE                 0x01
#define RFM9X_06_REG_FRF_MSB                 0x06
#define RFM9X_07_REG_FRF_MID                 0x07
#define RFM9X_08_REG_FRF_LSB                 0x08
#define RFM9X_09_REG_PA_CONFIG                0x09
#define RFM9X_0B_REG_OCP                      0x0b
#define RFM9X_0D_REG_FIFO_ADDR_PTR            0x0d
#define RFM9X_0E_REG_FIFO_TX_BASE_ADDR        0x0e
#define RFM9X_0F_REG_FIFO_RX_BASE_ADDR        0x0f
#define RFM9X_10_REG_FIFO_RX_CURRENT_ADDR     0x10
#define RFM9X_12_REG_IRQ_FLAGS                0x12
#define RFM9X_13_REG_RX_NB_BYTES              0x13
#define RFM9X_1D_REG_MODEM_CONFIG1            0x1d
#define RFM9X_1E_REG_MODEM_CONFIG2            0x1e
#define RFM9X_20_REG_PREAMBLE_MSB             0x20
#define RFM9X_21_REG_PREAMBLE_LSB             0x21
#define RFM9X_22_REG_PAYLOAD_LENGTH           0x22
#define RFM9X_26_REG_MODEM_CONFIG3            0x26
#define RFM9X_40_REG_DIO_MAPPING1             0x40
#define RFM9X_42_REG_VERSION                  0x42
#define RFM9X_4D_REG_PA_DAC                   0x4D

// Table 16. LoRa Operating Mode Functionality
#define RFM9X_LONG_RANGE_MODE      0x80
#define RFM9X_MODE_SLEEP           0x00
#define RFM9X_MODE_STDBY           0x01
#define RFM9X_MODE_FSTX            0x02
#define RFM9X_MODE_TX              0x03
#define RFM9X_MODE_FSRX            0x04
#define RFM9X_MODE_RXCONTINUOUS    0x05
#define RFM9X_MODE_RXSINGLE        0x06

// default value for register version
#define RFM9X_42_REG_VERSION_VALUE 0x12

// FXOSC = 32MHz
#define RFM9X_FXOSC 32000000.0

// RadioHead packet format
#define RFM9X_HEADER_LEN       4
#define RADIOHEAD_HEADER_TO    0xff
#define RADIOHEAD_HEADER_FROM  0xff
#define RADIOHEAD_HEADER_ID    0xff
#define RADIOHEAD_HEADER_FLAGS 0x00
#define IRQ_TX_DONE_MASK       0x08
#define IRQ_RX_DONE_MASK       0x40

esp_err_t spi_init(void);
void reset_radio(void);
void radio_init(void);
void check_radio_version(void);

int register_read(int reg);
esp_err_t register_write(uint8_t reg, uint8_t value);

void setPreambleLength(uint16_t length);
uint16_t getPreambleLength();
void setFrequency(uint32_t frequency);
uint32_t getFrecuency();
void setTxPower(int8_t power);
uint8_t getTxPower(void);
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
// Named lora_send (not send) to avoid colliding with lwIP's send()/socket
// macros once <WiFi.h> is included in the same translation unit.
void lora_send(uint8_t *data, uint8_t length);
void setRxMode();

uint8_t getRegOpMode();
uint8_t getRegOpLoraMode();
uint8_t setRegOpMode(uint8_t mode);
void getCurrentOpMode();
void set_rx_enable();
void set_tx_enable();
void disable_rx_tx();

#endif // LORA_H
