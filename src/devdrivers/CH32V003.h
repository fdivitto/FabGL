/**
 * Created by Olimex - <http://www.olimex.com>
 * Copyright (c) 2023 Olimex Ltd
 * All rights reserved.
 *
 *
 * Please contact info@olimex.com if you need a commercial license.
 *
 *
 * This library and related software is available under GPL v3.
 *
 * CH32V003 driver is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * CH32V003 driver is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with CH32V003 driver. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef	_CH32V003_H_
#define	_CH32V003_H_

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include <esp_system.h>

#include "hal/gpio_types.h"
#include "hal/spi_types.h"
#include "driver/spi_master.h"

/**
 * @file
 *
 * @brief This file contains the CH32V003 driver class
 *
 */

#define LOG_DEBUG_ENABLED

#ifdef LOG_DEBUG_ENABLED
    #define LOG_DEBUG(f_, ...) printf((f_), ##__VA_ARGS__)
#else
    #define LOG_DEBUG(f_, ...)
#endif

#define CH_SPI_FREQ		5000000
#define CH_DMACHANNEL	2

typedef enum {
	GPIO_PORTA = 1,
	GPIO_PORTB = 2,
	GPIO_PORTC = 3,
	GPIO_PORTD = 4
} GPIO_PORT_Index;

/* GPIO_pins_define */
#define GPIO_Pin_0      ((uint8_t)0x01) /* Pin 0 mask */
#define GPIO_Pin_1      ((uint8_t)0x02) /* Pin 1 mask */
#define GPIO_Pin_2      ((uint8_t)0x04) /* Pin 2 mask */
#define GPIO_Pin_3      ((uint8_t)0x08) /* Pin 3 mask */
#define GPIO_Pin_4      ((uint8_t)0x10) /* Pin 4 mask */
#define GPIO_Pin_5      ((uint8_t)0x20) /* Pin 5 mask */
#define GPIO_Pin_6      ((uint8_t)0x40) /* Pin 6 mask */
#define GPIO_Pin_7      ((uint8_t)0x80) /* Pin 7 mask */

#define PIN_OUT         0x00
#define PIN_IN          0xFF

#define PIN_PULL_DOWN   0x00
#define PIN_PULL_UP     0xFF

#define GPIO_0          ((uint8_t)0x00) /* Pin 0 */
#define GPIO_1          ((uint8_t)0x01) /* Pin 1 */
#define GPIO_2          ((uint8_t)0x02) /* Pin 2 */
#define GPIO_3          ((uint8_t)0x03) /* Pin 3 */
#define GPIO_4          ((uint8_t)0x04) /* Pin 4 */
#define GPIO_5          ((uint8_t)0x05) /* Pin 5 */
#define GPIO_6          ((uint8_t)0x06) /* Pin 6 */
#define GPIO_7          ((uint8_t)0x07) /* Pin 7 */

#define DIRECTION_OUT   0x00
#define DIRECTION_IN    0x01

#define PULL_DOWN       0x00
#define PULL_UP         0x01

#define SYNC_MAGIC      0xAA
#define SYNC_RESPONSE   0x55

#define MODE_GPIO       0x00
#define MODE_I2C        0x01
#define MODE_SPI        0x02
#define MODE_UART       0x03

#define CMD_PORT_INIT   0x01
#define CMD_PORT_SET    0x02
#define CMD_PORT_GET    0x03

#define CMD_I2C_INIT    0x01
#define CMD_I2C_WRITE   0x02
#define CMD_I2C_READ    0x03
#define CMD_I2C_READREG 0x04

#define CMD_SPI_INIT        0x01
#define CMD_SPI_TRANSFER8   0x02
#define CMD_SPI_TRANSFER16  0x03

/**
 * @brief CH32V003 driver
 *
 * This driver implements simple protocol from ESP32 to CH32V003 over SPI
 * It allows to use pins located on UEXT connector as GPIO, I2C, SPI.or UART.
 * 
 * Example usage:
 * ------------------------------------------------
 * 
 * CH32V003 extender;
 * 
 * extender.begin();
 * extender.configureGPIO(GPIO_PORTD, GPIO_3, DIRECTION_OUT);
 * extender.configureGPIO(GPIO_PORTD, GPIO_4, DIRECTION_IN, PULL_DOWN);
 * extender.configureGPIO(GPIO_PORTD, GPIO_5, DIRECTION_IN, PULL_UP);
 * // same as
 * extender.configurePort(
 *    GPIO_PORTD, 
 *    GPIO_Pin_3 | GPIO_Pin_4, 
 *    (GPIO_Pin_3 & PIN_OUT) | ((GPIO_Pin_4 | GPIO_Pin_5) & PIN_IN), 
 *    (GPIO_Pin_4 & PIN_PULL_DOWN) | (GPIO_Pin_5 & PIN_PULL_UP)
 * );
 * 
 * extender.setGPIO(GPIO_PORTD, GPIO_3, 1);
 * uint8_t gpio4 = extender.getGPIO(GPIO_PORTD, GPIO_4);
 * 
 * extender.configureI2C(100000);
 * uint8_t buff[5] = {1, 2, 3, 4, 5};
 * extender.writeI2C((0x15, buff, 5);
 * extender.readI2C((0x15, buff, 5);
 * 
 * extender.writeRegI2C(0x15, 0x10, 0x10);
 * uint8_t reg = extender.readRegI2C(0x15, 0x10);
 * 
 * Protocol description
 * ------------------------------------------------
 * 1 byte      - Header 
 * 1 byte      - Payload size
 * 1-255 bytes - Payload
 * 
 * Protocol sync
 * ------------------------------------------------
 * Master (ESP32) sends 0xAA and expects 0x55 as response from slave (CH32V003)
 * 
 * Header description (most significant bits first)
 * ------------------------------------------------
 * 2 bits - mode
 *     GPIO   = 00
 *     I2C    = 01
 *     SPI    = 10
 *     UART   = 11
 * 
 * 5 bits  - command
 *     GPIO mode
 *     00001 = Port Init
 *     00010 = Port Get
 *     00011 = Port Set
 * 
 *     I2C mode
 *     00001 = I2C Init
 *     00010 = I2C Write
 *     00011 = I2C Read
 *     00100 = I2C Read Register
 * 
 *     SPI mode
 *     00001 = SPI Init
 *     00010 = SPI Transfer8
 *     00011 = SPI Transfer16
 * 
 * 1 bit   - direction
 *     0 = out
 *     1 = in
 * 
 * Response to header byte must be 0xFA. If slave is out of sync then response
 * will be 0x55 and master must initiate sync.
 * 
 * For payload see comments in corresponding method
 * 
 */
 
class CH32V003 {

public:
	CH32V003 ();
	~CH32V003 ();
	
	bool available();
	
	bool begin(gpio_num_t MISO = GPIO_NUM_35, gpio_num_t MOSI = GPIO_NUM_12, gpio_num_t CLK = GPIO_NUM_14, gpio_num_t CS = GPIO_NUM_13, int16_t CSActiveState = -1, int16_t host = HSPI_HOST);
	void end ();
	
	void uextPowerEnable();
	void uextPowerDisable();
	
	// GPIO
	void configurePort (uint8_t port, uint8_t mask, uint8_t in_out, uint8_t pullup);
	void setPort (uint8_t port, uint8_t mask, uint8_t value);
	uint8_t getPort (uint8_t port, uint8_t mask);
	
	void configureGPIO (uint8_t port, uint8_t gpio, uint8_t dir, uint8_t pullup = 0x00);
	void setGPIO (uint8_t port, uint8_t gpio, uint8_t value);
	uint8_t getGPIO (uint8_t port, uint8_t gpio);
	
	// I2C
	void configureI2C (uint32_t clock);
	void writeI2C (uint8_t address, uint8_t buffer[], uint8_t size);
	void readI2C (uint8_t address, uint8_t buffer[], uint8_t size);
	void writeRegI2C (uint8_t address, uint8_t reg, uint8_t value);
	uint8_t readRegI2C (uint8_t address, uint8_t reg);
	
	// SPI
	void configureSPI (uint8_t spi_mode, uint32_t clock);
	void transferSPI8 (uint8_t tx_buffer[], uint8_t rx_buffer[], const uint8_t size);
	void transferSPI16 (uint16_t tx_buffer[], uint16_t rx_buffer[], const uint8_t size);
	
	// UART
	// TODO
	
private:
	gpio_num_t          CH_MISO, CH_MOSI, CH_CLK, CH_CS;
	spi_host_device_t   CH_SPIHost;
	spi_device_handle_t CH_SPIDevHandle;

	uint8_t protocolHeader (uint8_t mode, uint8_t command, uint8_t direction);
	
	uint8_t transferByte (uint8_t send);
	void transferProtocol(uint8_t txdata[], uint8_t rxdata[], uint16_t size);
	void sync();
};

#endif
