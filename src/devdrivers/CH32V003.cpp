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

#include "CH32V003.h"

// ------------------------------------------- public -------------------------------------------

CH32V003::CH32V003 () :
	CH_MISO(GPIO_NUM_NC),
	CH_MOSI(GPIO_NUM_NC),
	CH_CLK(GPIO_NUM_NC),
	CH_CS(GPIO_NUM_NC),
	CH_SPIHost(HSPI_HOST),
	CH_SPIDevHandle(nullptr)
{
}

CH32V003::~CH32V003 ()
{
	end();
}

bool CH32V003::available()
{
	return CH_SPIDevHandle != nullptr;
}

bool CH32V003::begin (gpio_num_t MISO, gpio_num_t MOSI, gpio_num_t CLK, gpio_num_t CS, int16_t CSActiveState, int16_t host)
{
	if (CSActiveState == -1)
		CSActiveState = 1;
	
	CH_MISO    = MISO;
	CH_MOSI    = MOSI;
	CH_CLK     = CLK;
	CH_CS      = CS;
	CH_SPIHost = (spi_host_device_t) host;
	
	spi_bus_config_t busconf = { }; // zero init
	busconf.mosi_io_num     = CH_MOSI;
	busconf.miso_io_num     = CH_MISO;
	busconf.sclk_io_num     = CH_CLK;
	busconf.quadwp_io_num   = -1;
	busconf.quadhd_io_num   = -1;
	busconf.flags           = SPICOMMON_BUSFLAG_MASTER;
	
	auto result = spi_bus_initialize(CH_SPIHost, &busconf, CH_DMACHANNEL);
	if (result == ESP_OK || result == ESP_ERR_INVALID_STATE) {
		// ESP_ERR_INVALID_STATE, maybe spi_bus_initialize already called
		spi_device_interface_config_t devconf = { };  // zero init
		devconf.mode           = 0;
		devconf.clock_speed_hz = CH_SPI_FREQ;
		devconf.spics_io_num   = CH_CS;
		devconf.flags          = (CSActiveState == 1 ? SPI_DEVICE_POSITIVE_CS : 0);
		devconf.queue_size     = 1;
		
		result = spi_bus_add_device(CH_SPIHost, &devconf, &CH_SPIDevHandle);
		if (result == ESP_OK) {
			sync();
			return true;
		}
	}
	
	end ();
	return false;
}

void CH32V003::end()
{
	if (CH_SPIDevHandle) {
		spi_bus_remove_device(CH_SPIDevHandle);
		CH_SPIDevHandle = nullptr;
	}
}

void CH32V003::uextPowerEnable()
{
	setGPIO(GPIO_PORTC, GPIO_3, 0);
}

void CH32V003::uextPowerDisable()
{
	setGPIO(GPIO_PORTC, GPIO_3, 1);
}

// ------------------------------------------- GPIO -------------------------------------------

/**
 * Sample protocol message configure GPIO port
 * 
 * |  mode  cmd    dir  |  size       |  port      |  mask           |  dir               |  pullup      |
 * |  GPIO  init   out  |  0x04       |  PortA     |  Pin2 and Pin0  |  Pin2 in Pin0 out  |  no pullups  |
 * |  00    00001  0    |  00000100   |  00000001  |  00000101       |  00000100          |  00000000    |
 */
void CH32V003::configurePort(uint8_t port, uint8_t mask, uint8_t in_out, uint8_t pullup)
{
	uint8_t spi_send[] = {protocolHeader(MODE_GPIO, CMD_PORT_INIT, DIRECTION_OUT), 0x04, port, mask, in_out, pullup};
	transferProtocol (spi_send, nullptr, sizeof(spi_send)/sizeof(uint8_t));
}

/**
 * Sample protocol message set GPIO port
 * 
 * |  mode  cmd    dir  |  size       |  port      |  mask      |  value     |
 * |  GPIO  set    out  |  0x03       |  PortC     |  Pin2      |  1         |
 * |  00    00010  0    |  00000011   |  00000011  |  00000100  |  00000100  |
 */
void CH32V003::setPort (uint8_t port, uint8_t mask, uint8_t value)
{
	uint8_t spi_send[] = {protocolHeader(MODE_GPIO, CMD_PORT_SET, DIRECTION_OUT), 0x03, port, mask, value};
	transferProtocol (spi_send, nullptr, sizeof(spi_send)/sizeof(uint8_t));
}

/**
 * Sample protocol message get GPIO port
 * 
 * |  mode  cmd    dir  |  size       |  port      |  mask      |  value     |
 * |  GPIO  get    in   |  0x03       |  PortD     |  Pin2      |  0         |
 * |  00    00011  1    |  00000011   |  00000100  |  00000100  |  00000000  |
 */
uint8_t CH32V003::getPort(uint8_t port, uint8_t mask)
{
	uint8_t spi_send[] = {protocolHeader(MODE_GPIO, CMD_PORT_GET, DIRECTION_IN), 0x03, port, mask, 0};
	const int size = sizeof(spi_send)/sizeof(uint8_t);
	uint8_t spi_receive[size];
	transferProtocol (spi_send, spi_receive, size);
	return spi_receive[size-1];
}

void CH32V003::configureGPIO (uint8_t port, uint8_t gpio, uint8_t dir, uint8_t pullup)
{
	configurePort (port, 1<<gpio, dir<<gpio, pullup<<gpio);
}

void CH32V003::setGPIO (uint8_t port, uint8_t gpio, uint8_t value)
{
	setPort (port, 1<<gpio, value<<gpio);
}

uint8_t CH32V003::getGPIO (uint8_t port, uint8_t gpio)
{
	uint8_t data = getPort (port, 1<<gpio);
	return (data & (1<<gpio)) != 0;
}

// ------------------------------------------- I2C -------------------------------------------

/**
 * Sample protocol message configure I2C
 * 
 * |  mode  cmd    dir  |  size       |  clock MSB |         1048576         |  clock LSB |
 * |  I2C   init   out  |  0x04       |            |            |            |            |
 * |  01    00001  0    |  00000100   |  00000000  |  00010000  |  00000000  |  00000000  |
 */
void CH32V003::configureI2C (uint32_t clock)
{
	uint8_t spi_send[6] = {protocolHeader(MODE_I2C, CMD_I2C_INIT, DIRECTION_OUT), 0x04};
	for (int i=0; i<4; i++)	// byte 0 most significant BYTE, byte 3 least significant BYTE
		spi_send[i+2] = (clock>>((3-i)*8)) & 0xFF;
	transferProtocol (spi_send, nullptr, sizeof(spi_send)/sizeof(uint8_t));
}

/**
 * Sample protocol message write I2C
 * 
 * |  mode  cmd    dir  |  size       |  address   |  data 0 tx |  data 1 tx |  data 2 tx |
 * |  I2C   write  out  |  0x04       |  0x10      |            |            |            |
 * |  01    00010  0    |  00000100   |  00010000  |  00000000  |  00000000  |  00000000  |
 */
void CH32V003::writeI2C (uint8_t address, uint8_t buffer[], uint8_t size)
{
	uint8_t spi_send[size+3] = {protocolHeader(MODE_I2C, CMD_I2C_WRITE, DIRECTION_OUT), size+1, address};
	for (int i=0; i<size; i++)
		spi_send [i+3] = buffer[i];
	transferProtocol (spi_send, nullptr, sizeof(spi_send)/sizeof(uint8_t));
}

/**
 * Sample protocol message read I2C
 * 
 * |  mode  cmd    dir  |  size       |  address   |  data 0 rx |  data 1 rx |
 * |  I2C   read   in   |  0x03       |  0x10      |            |            |
 * |  01    00011  1    |  00000011   |  00010000  |  00000000  |  00000000  |
 */
void CH32V003::readI2C (uint8_t address, uint8_t buffer[], uint8_t size)
{
	uint8_t spi_send[size+3] = {protocolHeader(MODE_I2C, CMD_I2C_READ, DIRECTION_IN), size+1, address};
	transferProtocol (spi_send, nullptr, 3);
	// Give 1 ms for each byte to allow I2C communication
	vTaskDelay(pdMS_TO_TICKS(size));
	for (int i=0; i<size; i++)
		spi_send[i] = 0;
	transferProtocol (spi_send, buffer, size);
}

/**
 * Sample protocol message writeReg I2C
 * 
 * |  mode  cmd    dir  |  size       |  address   |  register  |  data tx   |
 * |  I2C   write  out  |  0x03       |  0x10      |  0x01      |            |
 * |  01    00010  0    |  00000011   |  00010000  |  00000001  |  00000000  |
 */
void CH32V003::writeRegI2C (uint8_t address, uint8_t reg, uint8_t value)
{
	uint8_t spi_send[] = {protocolHeader(MODE_I2C, CMD_I2C_WRITE, DIRECTION_OUT), 3, address, reg, value};
	transferProtocol (spi_send, nullptr, sizeof(spi_send)/sizeof(uint8_t));
}

/**
 * Sample protocol message readReg I2C
 * 
 * |  mode  cmd    dir  |  size       |  address   |  register  |  data rx   |
 * |  I2C   read   in   |  0x03       |  0x10      |  0x01      |            |
 * |  01    00100  1    |  00000011   |  00010000  |  00000001  |  00000000  |
 */
uint8_t CH32V003::readRegI2C (uint8_t address, uint8_t reg)
{
	uint8_t spi_send[] = {protocolHeader(MODE_I2C, CMD_I2C_READREG, DIRECTION_IN), 3, address, reg, 0};
	uint8_t ReceivedData;
	transferProtocol (spi_send, nullptr, 4);
	// Give 1 ms for each byte to allow I2C communication
	vTaskDelay(pdMS_TO_TICKS(1));
	transferProtocol (&spi_send[4], &ReceivedData, 1);
	return ReceivedData;
}

// ------------------------------------------- SPI -------------------------------------------

/**
 * Sample protocol message configure SPI
 * 
 * |  mode  cmd    dir  |  size       |  spi mode   |  clock MSB |         1048576         |  clock LSB |
 * |  SPI   init   out  |  0x05       |  0x03       |            |            |            |            |
 * |  10    00001  0    |  00000101   |  00000011   |  00000000  |  00010000  |  00000000  |  00000000  |
 */
void CH32V003::configureSPI (uint8_t spi_mode, uint32_t clock)
{
	uint8_t spi_send[7] = {protocolHeader(MODE_SPI, CMD_SPI_INIT, DIRECTION_OUT), 0x05, spi_mode};
	for (int i=0; i<4; i++)	// byte 0 most significant BYTE, byte 3 least significant BYTE
		spi_send[i+3] = (clock>>((3-i)*8)) & 0xFF;
	transferProtocol (spi_send, nullptr, sizeof(spi_send)/sizeof(uint8_t));
}

/**
 * Sample protocol message transfer8 SPI
 * 
 * |  mode  cmd        dir  |  size       |  data 0 tx |  data 1 tx |  data 2 tx |            |
 * |  SPI   transfer8  out  |  0x04       |            |            |            |            |
 * |  10    00010      0    |  000000!0   |  00000000  |  00000000  |  00000000  |  00000000  |
 * |                        |             |            |  data 0 rx |  data 1 rx |  data 2 rx |
 */
void CH32V003::transferSPI8 (uint8_t tx_buffer[], uint8_t rx_buffer[], const uint8_t size)
{
	uint8_t spi_send[size+3] = {protocolHeader(MODE_SPI, CMD_SPI_TRANSFER8, DIRECTION_OUT), size+1};
	uint8_t spi_receive[size+3];
	for (int i=0; i<size; i++)
		spi_send[i+2] = tx_buffer[i];
	spi_send[size+2] = 0;
	transferProtocol (spi_send, spi_receive, sizeof(spi_send)/sizeof(uint8_t));
	for (int i=0; i<size; i++)
		rx_buffer[i] = spi_receive[i+3];
}

/**
 * Sample protocol message transfer16 SPI
 * 
 * |  mode  cmd        dir  |  size       |  data 0 tx |  data 0 tx |  data 1 tx |  data 1 tx |            |            |
 * |  SPI   transfer16 out  |  0x06       |  0x00      |            |            |            |            |            |
 * |  10    00011      0    |  00000110   |  00000000  |  00000000  |  00000000  |  00000000  |  00000000  |  00000000  |
 * |                        |             |            |            |  data 0 rx |  data 0 rx |  data 1 rx |  data 1 rx |
 */
void CH32V003::transferSPI16 (uint16_t tx_buffer[], uint16_t rx_buffer[], const uint8_t size)
{
	uint8_t spi_send[size*2+4] = {protocolHeader(MODE_SPI, CMD_SPI_TRANSFER16, DIRECTION_OUT), size*2+2};
	uint8_t spi_receive[size*2+4];
	for (int i=0; i<size; i++) {
		spi_send[i*2+2] = (tx_buffer[i] >> 8);
		spi_send[i*2+3] = (tx_buffer[i] & 0xFF);
	}
	spi_send[size*2+2] = 0;
	spi_send[size*2+3] = 0;
	transferProtocol (spi_send, spi_receive, sizeof(spi_send)/sizeof(uint8_t));
	for (int i=0; i<size; i++)
		rx_buffer[i] = ((uint16_t) spi_receive[i*2+4] << 8) | spi_receive[i*2+5];
}

// ------------------------------------------- private -------------------------------------------


uint8_t CH32V003::protocolHeader (uint8_t mode, uint8_t command, uint8_t direction)
{
	return ((mode&0x3)<<6) | ((command&0x1F)<<1) | (direction & 0x01);
}

uint8_t CH32V003::transferByte (uint8_t send)
{
	uint8_t txdata[1] = { send };
	uint8_t rxdata[1];
	spi_transaction_t ta;
	ta.flags      = 0;
	ta.length     = 8;
	ta.rxlength   = 8;
	ta.rx_buffer  = rxdata;
	ta.tx_buffer  = txdata;
	spi_device_transmit(CH_SPIDevHandle, &ta);

	return rxdata[0];
}

void CH32V003::transferProtocol(uint8_t txdata[], uint8_t rxdata[], uint16_t size)
{
	bool sync_detect;
	
	spi_device_acquire_bus(CH_SPIDevHandle, portMAX_DELAY);
	do {
		sync_detect = false;
		for (int i=0; i<size; i++) {
			uint8_t rxbyte;
			// LOG_DEBUG("Sending 0x%02X\n\r", txdata[i]);
			rxbyte = transferByte (txdata[i]);
			if (i == 0 && rxbyte == SYNC_RESPONSE) {
				LOG_DEBUG("CH32V003 is waiting for sync sequence\n\r");
				sync_detect = true;
				break;
			}
			if (rxdata != nullptr) {
				rxdata[i] = rxbyte;
				// LOG_DEBUG("Received 0x%02X\n\r", rxbyte);
			}
			vTaskDelay(pdMS_TO_TICKS(5));
		}
		vTaskDelay(pdMS_TO_TICKS(5));
		
		if (sync_detect) sync();
	} while (sync_detect);
	spi_device_release_bus(CH_SPIDevHandle);
}

void CH32V003::sync() {
	uint8_t response = 0x00;
	LOG_DEBUG("SYNC initiated...\r\n");
	do {
		response = transferByte(SYNC_MAGIC);
		vTaskDelay(pdMS_TO_TICKS(10));
		LOG_DEBUG("sync 0x%02X\r\n", response);
	} while (response != SYNC_RESPONSE);
	LOG_DEBUG("SYNC done.\r\n");
}
