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
#include <string.h>

#ifdef CH32V003_DEBUG_ENABLED
	#define PROTOCOL_BYTE_DELAY()       (ets_delay_us(5000))
	#define PROTOCOL_MSG_DELAY()        (ets_delay_us(5000))
	
	#define PROTOCOL_SYNC_DELAY()       (ets_delay_us(10000))
#else
	#define PROTOCOL_BYTE_DELAY()
	#define PROTOCOL_MSG_DELAY()
	
	#define PROTOCOL_SYNC_DELAY()       (ets_delay_us(500))
#endif

// Delay for ADC measurement
static uint32_t ADC_Delay = 150;

// Delay for each byte when using I2C
// It is calculated at run time when I2C clock is set
static uint32_t I2C_Delay = 0;

// Delay for each byte when using SPI
// It is calculated at run time when SPI clock is set
static uint32_t SPI_Delay = 0;

// Delay for each byte when using UART
// It is calculated at run time when UART baudrate is set
static uint32_t UART_Delay = 0;
// ------------------------------------------- public -------------------------------------------

CH32V003::CH32V003 () :
	CH_MISO(GPIO_NUM_NC),
	CH_MOSI(GPIO_NUM_NC),
	CH_CLK(GPIO_NUM_NC),
	CH_CS(GPIO_NUM_NC),
	CH_SPIHost(HSPI_HOST),
	CH_SPIDevHandle(nullptr),
	spi_acquired(0),
	synced(false)
{
}

CH32V003::~CH32V003 ()
{
	end();
}

bool CH32V003::available()
{
	return CH_SPIDevHandle != nullptr && synced;
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
			if (synced) version();
			return synced;
		}
	}
	
	end ();
	return false;
}

void CH32V003::end()
{
	synced = false;
	if (CH_SPIDevHandle) {
		spi_bus_remove_device(CH_SPIDevHandle);
		spi_bus_free(CH_SPIHost);
		CH_SPIDevHandle = nullptr;
	}
}

/**
 * Sample protocol message version
 * 
 * |  cmd      |  size       |  major     |  minor     |
 * |  version  |  0x02       |            |            |
 * |  10111101 |  00000010   |  00000001  |  00000000  |
 */
uint16_t CH32V003::version()
{
	if (firmware_ver != 0x0000) {
		return firmware_ver;
	}
	uint8_t spi_send[] = {0xBD, 0x02, 0, 0};
	const int msg_size = sizeof(spi_send)/sizeof(uint8_t);
	uint8_t spi_receive[msg_size];
	transferProtocol (spi_send, spi_receive, msg_size);
	uint16_t ver = (spi_receive[2] << 8) |  spi_receive[3];
	if (ver == ((SYNC_RESPONSE << 8) | SYNC_RESPONSE)) {
		ver = 0x0009;
	}
	firmware_ver = ver;
	return firmware_ver;
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
	const int msg_size = sizeof(spi_send)/sizeof(uint8_t);
	transferProtocol (spi_send, nullptr, msg_size);
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
	const int msg_size = sizeof(spi_send)/sizeof(uint8_t);
	transferProtocol (spi_send, nullptr, msg_size);
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
	uint8_t spi_send[] = {protocolHeader(MODE_GPIO, CMD_PORT_GET, DIRECTION_IN), 0x03, port, mask};
	const int msg_size = sizeof(spi_send)/sizeof(uint8_t);
	
	acquireSPI();
	transferProtocol (spi_send, nullptr, msg_size);
	uint8_t data = transferByte(0x00);
	releaseSPI();
	
	return data;
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

/**
 * Sample protocol message Power Sense
 * 
 * |  mode  cmd    dir  |  size       |  data      |
 * |  GPIO  pwr    in   |  0x01       |            |
 * |  00    01001  1    |  00000001   |  00000000  |
 */
uint8_t CH32V003::powerSense ()
{
	uint8_t spi_send[] = {protocolHeader(MODE_GPIO, CMD_PWR_SENSE, DIRECTION_IN), 0x01};
	const int msg_size = sizeof(spi_send)/sizeof(uint8_t);
	
	acquireSPI();
	transferProtocol (spi_send, nullptr, msg_size);
	uint8_t data = transferByte(0x00);
	releaseSPI();
	
	return data;
}

/**
 * Sample protocol message Power Sense
 * 
 * |  mode  cmd    dir  |  size       |  data      |  data      |
 * |  GPIO  bat    in   |  0x02       |            |            |
 * |  00    01010  1    |  00000010   |  00000000  |  00000000  |
 */
uint16_t CH32V003::batterySense ()
{
	uint8_t spi_send[] = {protocolHeader(MODE_GPIO, CMD_BAT_SENSE, DIRECTION_IN), 0x02};
	const int msg_size = sizeof(spi_send)/sizeof(uint8_t);
	
	acquireSPI();
	transferProtocol (spi_send, nullptr, msg_size);
	ets_delay_us(ADC_Delay);
	uint16_t data = 0;
	for (uint8_t i=0; i<2; i++) {
		data = (data << 8) | transferByte(0x00);
	}
	releaseSPI();
	
	return data;
}

uint8_t CH32V003::batteryPercent (uint16_t bat_sense)
{
	uint16_t mV = (bat_sense != 0) ? bat_sense : batterySense();
	if (mV > BATTERY_MAX_MV)
		mV = BATTERY_MAX_MV;
	if (mV < BATTERY_MIN_MV)
		mV = BATTERY_MIN_MV;
	return (mV - BATTERY_MIN_MV) * 100 / (BATTERY_MAX_MV - BATTERY_MIN_MV);
}

/**
 * Sample protocol message set interrupt active
 * 
 * |  mode  cmd        dir  |  size       |  level     |
 * |  GPIO  int active out  |  0x01       |  high      |
 * |  00    10001      0    |  00000001   |  00000001  |
 */
void CH32V003::setIntActive(uint8_t level)
{
	uint8_t spi_send[] = {protocolHeader(MODE_GPIO, CMD_INT_ACTIVE, DIRECTION_OUT), 0x01, level};
	const int msg_size = sizeof(spi_send)/sizeof(uint8_t);
	transferProtocol (spi_send, nullptr, msg_size);
}

/**
 * Sample protocol message enable interrupt
 * 
 * |  mode  cmd        dir  |  size       |  port      |  pin      |  trigger   |
 * |  GPIO  int enable out  |  0x03       |  A         |  1         |  on change |
 * |  00    10010      0    |  00000011   |  00000001  |  00000001  |  00000011  |
 */
void CH32V003::enableInterrupt(uint8_t port, uint8_t pin, GPIO_INT_Trigger trigger)
{
	uint8_t spi_send[] = {protocolHeader(MODE_GPIO, CMD_INT_ENABLE, DIRECTION_OUT), 0x03, port, pin, (uint8_t)trigger};
	const int msg_size = sizeof(spi_send)/sizeof(uint8_t);
	transferProtocol (spi_send, nullptr, msg_size);
}

/**
 * Sample protocol message disable interrupt
 * 
 * |  mode  cmd         dir  |  size       |  port      |  pin       |
 * |  GPIO  int disable out  |  0x02       |  C         |  2         |
 * |  00    10011       0    |  00000010   |  00000011  |  00000010  |
 */
void CH32V003::disableInterrupt(uint8_t port, uint8_t pin)
{
	uint8_t spi_send[] = {protocolHeader(MODE_GPIO, CMD_INT_DISABLE, DIRECTION_OUT), 0x02, port, pin};
	const int msg_size = sizeof(spi_send)/sizeof(uint8_t);
	transferProtocol (spi_send, nullptr, msg_size);
}

/**
 * Sample protocol get interrupt flags (interrupt IS NOT cleared)
 * 
 * |  mode  cmd       dir |  size       |  port      |  data      |
 * |  GPIO  int flags in  |  0x02       |  D         |            |
 * |  00    10100     1   |  00000010   |  00000100  |  00000000  |
 */
uint8_t CH32V003::getPortIntFlags(uint8_t port)
{
	uint8_t spi_send[] = {protocolHeader(MODE_GPIO, CMD_INT_FLAGS, DIRECTION_IN), 0x02, port, 0};
	const int msg_size = sizeof(spi_send)/sizeof(uint8_t);
	uint8_t spi_receive[msg_size];
	transferProtocol (spi_send, spi_receive, msg_size);
	return spi_receive[msg_size-1];
}

/**
 * Sample protocol get interrupt capture (interrupt IS cleared)
 * 
 * |  mode  cmd       dir |  size       |  port      |  data      |
 * |  GPIO  int flags in  |  0x02       |  D         |            |
 * |  00    10101     1   |  00000010   |  00000100  |  00000000  |
 */
uint8_t CH32V003::getPortIntCaptured(uint8_t port)
{
	uint8_t spi_send[] = {protocolHeader(MODE_GPIO, CMD_INT_CAPTURE, DIRECTION_IN), 0x02, port, 0};
	const int msg_size = sizeof(spi_send)/sizeof(uint8_t);
	uint8_t spi_receive[msg_size];
	transferProtocol (spi_send, spi_receive, msg_size);
	return spi_receive[msg_size-1];
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
	if (clock == 0) return;
	
	uint8_t spi_send[6] = {protocolHeader(MODE_I2C, CMD_I2C_INIT, DIRECTION_OUT), 0x04};
	const int msg_size = sizeof(spi_send)/sizeof(uint8_t);
	
	for (int i=0; i<4; i++)	// byte 0 most significant BYTE, byte 3 least significant BYTE
		spi_send[i+2] = (clock>>((3-i)*8)) & 0xFF;
	transferProtocol (spi_send, nullptr, msg_size);
	
	// Microseconds needed for 1 byte to be transferred via I2C
	// approximate time (in microseconds) for sending 1 byte of data through I2C
	// Tbit = 1 000 000 / clock is the time for 1 bit of data
	// Tbyte = 8 * Tbit = 8 * 1 000 000 / clock
	// added 1 extra time for acknoledge bit + time for start and stop + some extra
	I2C_Delay = 12000000 / clock;
	if (version() < 0x0100)
		I2C_Delay += 150;	// 0.9v requires a little bit more delay between I2C bytes
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
	const int msg_size = sizeof(spi_send)/sizeof(uint8_t);
	
	for (int i=0; i<size; i++)
		spi_send [i+3] = buffer[i];
	transferProtocol (spi_send, nullptr, msg_size);
	
	// Wait CH32V003 to transfer the data
	// total bytes sent = address byte + I2C_DELAY on CH32 (~1 byte time) + data bytes (size) + some extra
	ets_delay_us(I2C_Delay * (size + 4));
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
	uint8_t spi_send[] = {protocolHeader(MODE_I2C, CMD_I2C_READ, DIRECTION_IN), size+1, address};
	const int msg_size = sizeof(spi_send)/sizeof(uint8_t);
	
	acquireSPI();
	transferProtocol (spi_send, nullptr, msg_size);
	
	// Delay for each byte to allow I2C communication
	// total bytes = address + I2C_DELAY on CH32 + register + address + I2C_DELAY on CH32 + reading data (size) + some extra
	ets_delay_us(I2C_Delay * (size + 6));
	
	for (int i=0; i<size; i++) {
		buffer[i] = transferByte(0x00);
		PROTOCOL_BYTE_DELAY();
	}
	releaseSPI();
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
	const int msg_size = sizeof(spi_send)/sizeof(uint8_t);
	transferProtocol (spi_send, nullptr, msg_size);
	
	// Wait CH32V003 to transfer the data
	// total bytes = address + I2C_DELAY on CH32 + register + value + some extra
	ets_delay_us(I2C_Delay * 6);
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
	uint8_t spi_send[] = {protocolHeader(MODE_I2C, CMD_I2C_READREG, DIRECTION_IN), 3, address, reg};
	const int msg_size = sizeof(spi_send)/sizeof(uint8_t);
	
	acquireSPI();
	transferProtocol (spi_send, nullptr, msg_size);
	
	// Delay to allow I2C communication
	// total bytes = writeI2C(1 byte) 1+4 + readI2C(1 byte) 1+6
	ets_delay_us(I2C_Delay * 15);
	
	uint8_t data = transferByte(0x00);
	ets_delay_us(I2C_Delay);
	
	releaseSPI();
	
	return data;
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
	if (clock == 0) return;
	
	uint8_t spi_send[] = {protocolHeader(MODE_SPI, CMD_SPI_INIT, DIRECTION_OUT), 0x05, spi_mode, 0, 0, 0, 0};
	const int msg_size = sizeof(spi_send)/sizeof(uint8_t);
	
	for (int i=0; i<4; i++)	// byte 0 most significant BYTE, byte 3 least significant BYTE
		spi_send[i+3] = (clock>>((3-i)*8)) & 0xFF;
	
	transferProtocol (spi_send, nullptr, msg_size);
	
	uint32_t t = 1000000 / clock;	// time for bit [us]
	if (t < 20) {
		t = 20;
	}
	// 8 bits * (time for bit + time for edges + time for calculating the data)
	SPI_Delay = 8 * (t + 30 + 20);
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
	uint8_t spi_send[] = {protocolHeader(MODE_SPI, CMD_SPI_TRANSFER8, DIRECTION_OUT), size+1};
	const int msg_size = sizeof(spi_send)/sizeof(uint8_t);
	
	acquireSPI();
	transferProtocol (spi_send, nullptr, msg_size);
	
	uint8_t response;
	for (int i=0; i<=size; i++) {
		response = (i < size) ? 
			transferByte(tx_buffer[i]) 
			: 
			transferByte(0)
		;
		
		if (i > 0) {
			rx_buffer[i-1] = response;
		}
		ets_delay_us(SPI_Delay);
	}
	releaseSPI();
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
	uint8_t spi_send[] = {protocolHeader(MODE_SPI, CMD_SPI_TRANSFER16, DIRECTION_OUT), size*2+2};
	const int msg_size = sizeof(spi_send)/sizeof(uint8_t);
	
	acquireSPI();
	transferProtocol (spi_send, nullptr, msg_size);
	
	uint16_t response;
	for (int i=0; i<=size; i++) {
		response = (i < size) ? 
			transferWord(tx_buffer[i]) 
			: 
			transferWord(0)
		;
		
		if (i > 0) {
			rx_buffer[i-1] = response;
		}
		ets_delay_us(SPI_Delay * 2);
	}
	releaseSPI();
}

// ------------------------------------------- UART -------------------------------------------

/**
 * Sample protocol message configure UART
 * 
 * |  mode  cmd    dir  |  size       |  baud MSB  |         115200          |  baud LSB  |  stop_bits |  parity    |
 * |  UART  init   out  |  0x06       |            |            |            |            |            |            |
 * |  11    00001  0    |  00000110   |  00000000  |  00000001  |  11000010  |  00000000  |  00000001  |  00000000  |
 */
void CH32V003::configureUART (uint32_t baudrate, UART_StopBits stop_bits, UART_Parity parity)
{
	uint8_t spi_send[] = {protocolHeader(MODE_UART, CMD_UART_CONFIGURE, DIRECTION_OUT), 6, 0, 0, 0, 0, (uint8_t)stop_bits, (uint8_t)parity};
	const int msg_size = sizeof(spi_send)/sizeof(uint8_t);
	
	for (uint8_t i=0; i<4; i++)
	{
		spi_send[i+2] = (baudrate >> ((3-i)*8)) & 0xFF;
	}
	
	transferProtocol (spi_send, nullptr, msg_size);
	
	// Time for 1 bit of data (in microseconds) = 1000000 / baudrate [us/bits]
	// Delay (in us) for the whole byte = (start bit + 8 data bits + stop bit(s) + parity) * time for 1 bit [us]
	UART_Delay = (1000000 / baudrate) * 10;
}

/**
 * Sample protocol message write UART
 * 
 * |  mode  cmd    dir  |  size       |  data 0 tx |  data 1 tx |  data 2 tx |  data 3 tx |
 * |  UART  write  out  |  0x04       |            |            |            |            |
 * |  11    00010  0    |  00000100   |  00000000  |  00000000  |  00000000  |  00000000  |
 */
void CH32V003::writeUART (uint8_t buff[], uint8_t size)
{
	uint8_t spi_send[size+2] = {protocolHeader(MODE_UART, CMD_UART_WRITE, DIRECTION_OUT), size};
	const int msg_size = sizeof(spi_send)/sizeof(uint8_t);
	
	for (int i=0; i<size; i++)
		spi_send [i+2] = buff[i];
	transferProtocol (spi_send, nullptr, msg_size);
	
	// Wait CH32V003 to transfer the data
	ets_delay_us(UART_Delay * size);
}

void CH32V003::strWriteUART(char message[])
{
	uint8_t len = strlen(message);
	if (len) {
		writeUART((uint8_t *)message, len);
	}
}


/**
 * Sample protocol message reaad UART
 * 
 * |  mode  cmd    dir  |  size       |  received  |  data 0 rx |  data 1 rx |  data 2 rx |  no data   |  no data   |  no data   |
 * |  UART  raad   in   |  0x06       |            |            |            |            |            |            |            |
 * |  11    00011  1    |  00000110   |  00000011  |  00000000  |  00000000  |  00000000  |  --------  |  --------  |  --------  |
 */
uint8_t CH32V003::readUART (uint8_t buff[], uint8_t size)
{
	uint8_t spi_send[] = {protocolHeader(MODE_UART, CMD_UART_READ, DIRECTION_IN), size+1};
	const int msg_size = sizeof(spi_send)/sizeof(uint8_t);
	
	acquireSPI();
	transferProtocol (spi_send, nullptr, msg_size);
	
	uint8_t received = transferByte(0x00);
	PROTOCOL_BYTE_DELAY ();
	
	for (int i=0; i<received; i++) {
		buff[i] = transferByte(0x00);
		PROTOCOL_BYTE_DELAY ();
	}
	releaseSPI();
	
	return received;
}

// UEXT
void CH32V003::initUEXT ()
{
	uextPowerEnable();
	// Clear IO_EXP_IRQ
	getPortIntCaptured(GPIO_PORTA);
	getPortIntCaptured(GPIO_PORTC);
	getPortIntCaptured(GPIO_PORTD);
}

void CH32V003::configureUEXT (uint8_t gpio, uint8_t dir, uint8_t pullup)
{
	uint8_t port, portGPIO;
	uext2port(gpio, &port, &portGPIO);
	configureGPIO(port, portGPIO, dir, pullup);
}

void CH32V003::enableUEXTInterrupt (uint8_t gpio, GPIO_INT_Trigger trigger)
{
	uint8_t port, portGPIO;
	uext2port(gpio, &port, &portGPIO);
	enableInterrupt(port, portGPIO, trigger);
}

void CH32V003::disableUEXTInterrupt (uint8_t gpio)
{
	uint8_t port, portGPIO;
	uext2port(gpio, &port, &portGPIO);
	disableInterrupt(port, portGPIO);
}

uint8_t CH32V003::getUEXTIntFlags() 
{
	uint8_t flags = 0;
	
	uint8_t flagsA = getPortIntFlags(GPIO_PORTA);
	uint8_t flagsC = getPortIntFlags(GPIO_PORTC);
	uint8_t flagsD = getPortIntFlags(GPIO_PORTD);
	
	flags |= ((flagsD & (1 << 5)) != 0) << 0; // D5 - gpio 0 - pin 3
	flags |= ((flagsD & (1 << 6)) != 0) << 1; // D6 - gpio 1 - pin 4
	flags |= ((flagsC & (1 << 2)) != 0) << 2; // C2 - gpio 2 - pin 5
	flags |= ((flagsC & (1 << 1)) != 0) << 3; // C1 - gpio 3 - pin 6
	flags |= ((flagsA & (1 << 2)) != 0) << 4; // A2 - gpio 4 - pin 7
	flags |= ((flagsA & (1 << 1)) != 0) << 5; // A1 - gpio 5 - pin 8
	flags |= ((flagsD & (1 << 4)) != 0) << 6; // D4 - gpio 6 - pin 9
	flags |= ((flagsD & (1 << 3)) != 0) << 7; // D3 - gpio 7 - pin 10
	
	return flags;
}

uint8_t CH32V003::getUEXTIntCaptured()
{
	uint8_t capture = 0;
	
	uint8_t captureA = getPortIntCaptured(GPIO_PORTA);
	uint8_t captureC = getPortIntCaptured(GPIO_PORTC);
	uint8_t captureD = getPortIntCaptured(GPIO_PORTD);
	
	capture |= ((captureD & (1 << 5)) != 0) << 0; // D5 - gpio 0 - pin 3
	capture |= ((captureD & (1 << 6)) != 0) << 1; // D6 - gpio 1 - pin 4
	capture |= ((captureC & (1 << 2)) != 0) << 2; // C2 - gpio 2 - pin 5
	capture |= ((captureC & (1 << 1)) != 0) << 3; // C1 - gpio 3 - pin 6
	capture |= ((captureA & (1 << 2)) != 0) << 4; // A2 - gpio 4 - pin 7
	capture |= ((captureA & (1 << 1)) != 0) << 5; // A1 - gpio 5 - pin 8
	capture |= ((captureD & (1 << 4)) != 0) << 6; // D4 - gpio 6 - pin 9
	capture |= ((captureD & (1 << 3)) != 0) << 7; // D3 - gpio 7 - pin 10
	
	return capture;
}

uint8_t CH32V003::readUEXT (uint8_t gpio)
{
	uint8_t port, portGPIO;
	uext2port(gpio, &port, &portGPIO);
	return getGPIO (port, portGPIO);
}

void CH32V003::writeUEXT (uint8_t gpio, uint8_t value)
{
	uint8_t port, portGPIO;
	uext2port(gpio, &port, &portGPIO);
	return setGPIO (port, portGPIO, value);
}

// ------------------------------------------- private -------------------------------------------

void CH32V003::uext2port(uint8_t uextGPIO, uint8_t *port, uint8_t *portGPIO)
{
	switch (uextGPIO) {
		case 0: // pin 3
			*port     = GPIO_PORTD;
			*portGPIO = GPIO_5;
		break;
		
		case 1: // pin 4
			*port     = GPIO_PORTD;
			*portGPIO = GPIO_6;
		break;
		
		case 2: // pin 5
			*port     = GPIO_PORTC;
			*portGPIO = GPIO_2;
		break;
		
		case 3: // pin 6
			*port     = GPIO_PORTC;
			*portGPIO = GPIO_1;
		break;
		
		case 4: // pin 7
			*port     = GPIO_PORTA;
			*portGPIO = GPIO_2;
		break;
		
		case 5: // pin 8
			*port     = GPIO_PORTA;
			*portGPIO = GPIO_1;
		break;
		
		case 6: // pin 9
			*port     = GPIO_PORTD;
			*portGPIO = GPIO_4;
		break;
		
		case 7: // pin 10
			*port     = GPIO_PORTD;
			*portGPIO = GPIO_3;
		break;
	}
}

void CH32V003::acquireSPI()
{
	if (CH_SPIDevHandle == nullptr) {
		LOG_DEBUG("CH32V003 not available or not started" EOL);
		return;
	}
	
	if (spi_acquired == 0) {
		spi_device_acquire_bus(CH_SPIDevHandle, portMAX_DELAY);
	}
	
	spi_acquired++;
}

void CH32V003::releaseSPI()
{
	if (CH_SPIDevHandle == nullptr) {
		LOG_DEBUG("CH32V003 not available or not started" EOL);
		return;
	}
	
	if (spi_acquired > 0) {
		spi_acquired--;
	}
	
	if (spi_acquired == 0) {
		spi_device_release_bus(CH_SPIDevHandle);
	}
}

uint8_t CH32V003::protocolHeader (uint8_t mode, uint8_t command, uint8_t direction)
{
	return ((mode&0x3)<<6) | ((command&0x1F)<<1) | (direction & 0x01);
}

uint8_t CH32V003::transferByte (uint8_t send)
{
	if (CH_SPIDevHandle == nullptr) {
		LOG_DEBUG("CH32V003 not available or not started" EOL);
		return 0x00;
	}
	
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

uint16_t CH32V003::transferWord (uint16_t send)
{
	uint16_t rx = transferByte(send >> 8) << 8;
	PROTOCOL_BYTE_DELAY();
	
	rx |= transferByte(send & 0xFF);
	PROTOCOL_BYTE_DELAY();
	
	return rx;
}


void CH32V003::transferProtocol(uint8_t txdata[], uint8_t rxdata[], uint16_t size)
{
	bool sync_detect;
	
	if (CH_SPIDevHandle == nullptr) {
		if (rxdata != nullptr) {
			for (int i=0; i<size; i++) {
				rxdata[i] = 0;
			}
		}
		LOG_DEBUG("CH32V003 not available or not started" EOL);
		return;
	}
	
	acquireSPI();
	do {
		sync_detect = false;
		for (int i=0; i<size; i++) {
			uint8_t rxbyte;
			// LOG_DEBUG("Sending 0x%02X" EOL, txdata[i]);
			rxbyte = transferByte (txdata[i]);
			if (i <= 1 && rxbyte == SYNC_RESPONSE) {
				LOG_DEBUG("CH32V003 is waiting for sync sequence" EOL);
				sync_detect = true;
				break;
			}
			if (rxdata != nullptr) {
				rxdata[i] = rxbyte;
				// LOG_DEBUG("Received 0x%02X" EOL, rxbyte);
			}
			PROTOCOL_BYTE_DELAY();
		}
		PROTOCOL_MSG_DELAY();
		
		if (sync_detect) sync();
	} while (sync_detect);
	releaseSPI();
}

void CH32V003::sync() {
	if (CH_SPIDevHandle == nullptr) {
		LOG_DEBUG("CH32V003 not available or not started" EOL);
		return;
	}
	
	bool timeout = false;
	uint8_t response = 0x00;
	int64_t t1 = esp_timer_get_time();
	
	LOG_DEBUG("SYNC initiated..." EOL);
	acquireSPI();
	do {
		response = transferByte(SYNC_MAGIC);
		PROTOCOL_SYNC_DELAY();
		LOG_DEBUG("sync 0x%02X" EOL, response);
		if (esp_timer_get_time() - t1 > SYNC_TIMEOUT) {
			timeout = true;
			break;
		}
	} while (response != SYNC_RESPONSE);
	releaseSPI();
	
	if (timeout) {
		LOG_DEBUG("SYNC timeout." EOL);
	} else {
		LOG_DEBUG("SYNC done." EOL);
		synced = true;
	}
}
