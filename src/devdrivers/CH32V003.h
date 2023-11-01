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
#include <rom/ets_sys.h>

#include "hal/gpio_types.h"
#include "hal/spi_types.h"
#include "driver/spi_master.h"

/**
 * @file
 *
 * @brief This file contains the CH32V003 driver class
 *
 * This driver implements simple protocol from ESP32 to CH32V003 over SPI
 * It allows to use pins located on UEXT connector as GPIO, I2C, SPI.or UART.
 * 
 * It is NOT RECOMMENDED to use UEXT pins 5 and 6 as GPIOs. If you disable
 * UEXT power by calling CH32V003::uextPowerDisable() they can not be used as
 * I2C as well.
 * 
 * Example usage:
 * -----------------------------------------------------------------------------
 * 
 * CH32V003 expander;
 * 
 * expander.begin();
 * 
 * expander.configureGPIO(UEXT_GPIO_10, DIRECTION_OUT);
 * expander.configureGPIO(UEXT_GPIO_9,  DIRECTION_IN, PULL_DOWN);
 * expander.configureGPIO(UEXT_GPIO_3,  DIRECTION_IN, PULL_UP);
 * // same as
 * expander.configureGPIO(GPIO_PORTD, GPIO_3, DIRECTION_OUT);
 * expander.configureGPIO(GPIO_PORTD, GPIO_4, DIRECTION_IN, PULL_DOWN);
 * expander.configureGPIO(GPIO_PORTD, GPIO_5, DIRECTION_IN, PULL_UP);
 * // same as
 * expander.configurePort(
 *    GPIO_PORTD, 
 *    GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_5, 
 *    (GPIO_Pin_3 & PIN_OUT) | ((GPIO_Pin_4 | GPIO_Pin_5) & PIN_IN), 
 *    (GPIO_Pin_4 & PIN_PULL_DOWN) | (GPIO_Pin_5 & PIN_PULL_UP)
 * );
 * 
 * expander.setGPIO(GPIO_PORTD, GPIO_3, 1);
 * uint8_t gpio4 = expander.getGPIO(UEXT_GPIO_9);
 * 
 * expander.configureI2C(100000);
 * uint8_t buff[5] = {1, 2, 3, 4, 5};
 * expander.writeI2C((0x15, buff, 5);
 * expander.readI2C((0x15, buff, 5);
 * 
 * expander.writeRegI2C(0x15, 0x10, 0x10);
 * uint8_t reg = expander.readRegI2C(0x15, 0x10);
 * 
 * Protocol description
 * -----------------------------------------------------------------------------
 * 1 byte      - Header 
 * 1 byte      - Payload size
 * 1-255 bytes - Payload
 * 
 * Protocol sync
 * -----------------------------------------------------------------------------
 * Master (ESP32) sends 0xAA and expects 0x55 as response from slave (CH32V003)
 * 
 * Header description (most significant bits first)
 * -----------------------------------------------------------------------------
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
 *     01001 = Power Sense
 *     01010 = Battery Sense
 * 
 *     10001 = Interrupt Set Active Level
 *     10010 = Interrupt Enable
 *     10011 = Interrupt Disable
 *     10100 = Interrupt Get Flags
 *     10101 = Interrupt Get Capture
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
 *     UART mode
 *     00001 = UART Init
 *     00010 = UART Write
 *     00011 = UART Read
 * 
 * 1 bit   - direction
 *     0 = out
 *     1 = in
 * 
 * Response to header byte must be 0xFA. If slave is out of sync then response
 * will be 0x55 and master must initiate sync.
 * 
 * For payload content description see comments in corresponding method impelemtation
 * 
 */

//#define CH32V003_DEBUG_ENABLED
//#define LOG_DEBUG_ENABLED

#ifdef LOG_DEBUG_ENABLED
    #define LOG_DEBUG(f_, ...) printf((f_), ##__VA_ARGS__)
#else
    #define LOG_DEBUG(f_, ...)
#endif

#define EOL "\r\n"

#define CH_SPI_FREQ		5000000
#define CH_DMACHANNEL	2

typedef enum {
	GPIO_PORTA = 1,
	GPIO_PORTB = 2,
	GPIO_PORTC = 3,
	GPIO_PORTD = 4
} GPIO_PORT_Index;

typedef enum {
	FRONT_RISING  = 0x01,
	FRONT_FALLING = 0x02,
	FRONT_CHANGE  = 0x03
} GPIO_INT_Trigger;

typedef enum {
	UART_StopBits_1    = 0x01,
	UART_StopBits_0_5  = 0x02,
	UART_StopBits_2    = 0x03,
	UART_StopBits_1_5  = 0x04
} UART_StopBits;

typedef enum {
	UART_Parity_No   = 0x00,
	UART_Parity_Odd  = 0x01,
	UART_Parity_Even = 0x02
} UART_Parity;

/* GPIO_pins_define */
#define GPIO_Pin_0          ((uint8_t)0x01) /* Pin 0 mask */
#define GPIO_Pin_1          ((uint8_t)0x02) /* Pin 1 mask */
#define GPIO_Pin_2          ((uint8_t)0x04) /* Pin 2 mask */
#define GPIO_Pin_3          ((uint8_t)0x08) /* Pin 3 mask */
#define GPIO_Pin_4          ((uint8_t)0x10) /* Pin 4 mask */
#define GPIO_Pin_5          ((uint8_t)0x20) /* Pin 5 mask */
#define GPIO_Pin_6          ((uint8_t)0x40) /* Pin 6 mask */
#define GPIO_Pin_7          ((uint8_t)0x80) /* Pin 7 mask */

#define PIN_OUT             0x00
#define PIN_IN              0xFF

#define PIN_PULL_DOWN       0x00
#define PIN_PULL_UP         0xFF

#define GPIO_0              ((uint8_t)0x00) /* Pin 0 */
#define GPIO_1              ((uint8_t)0x01) /* Pin 1 */
#define GPIO_2              ((uint8_t)0x02) /* Pin 2 */
#define GPIO_3              ((uint8_t)0x03) /* Pin 3 */
#define GPIO_4              ((uint8_t)0x04) /* Pin 4 */
#define GPIO_5              ((uint8_t)0x05) /* Pin 5 */
#define GPIO_6              ((uint8_t)0x06) /* Pin 6 */
#define GPIO_7              ((uint8_t)0x07) /* Pin 7 */

#define UEXT_GPIO_3         GPIO_PORTD, GPIO_5
#define UEXT_GPIO_4         GPIO_PORTD, GPIO_6
#define UEXT_GPIO_7         GPIO_PORTA, GPIO_2
#define UEXT_GPIO_8         GPIO_PORTA, GPIO_1
#define UEXT_GPIO_9         GPIO_PORTD, GPIO_4
#define UEXT_GPIO_10        GPIO_PORTD, GPIO_3

#define DIRECTION_OUT       0x00
#define DIRECTION_IN        0x01

#define PULL_DOWN           0x00
#define PULL_UP             0x01

#define SYNC_MAGIC          0xAA
#define SYNC_RESPONSE       0x55
#define SYNC_TIMEOUT        3000000 // 3 sec

#define MODE_GPIO           0x00
#define MODE_I2C            0x01
#define MODE_SPI            0x02
#define MODE_UART           0x03

#define CMD_PORT_INIT       0x01
#define CMD_PORT_SET        0x02
#define CMD_PORT_GET        0x03

#define CMD_PWR_SENSE       0x09
#define CMD_BAT_SENSE       0x0A

#define CMD_INT_ACTIVE      0x11
#define CMD_INT_ENABLE      0x12
#define CMD_INT_DISABLE     0x13
#define CMD_INT_FLAGS       0x14
#define CMD_INT_CAPTURE     0x15

#define CMD_I2C_INIT        0x01
#define CMD_I2C_WRITE       0x02
#define CMD_I2C_READ        0x03
#define CMD_I2C_READREG     0x04

#define CMD_SPI_INIT        0x01
#define CMD_SPI_TRANSFER8   0x02
#define CMD_SPI_TRANSFER16  0x03

#define CMD_UART_CONFIGURE  0x01
#define CMD_UART_WRITE      0x02
#define CMD_UART_READ       0x03

#define IO_EXP_IRQ          36

#define	BATTERY_MIN_MV      3500
#define	BATTERY_MAX_MV      4200

class CH32V003 {

public:
	CH32V003 ();
	~CH32V003 ();
	
	
	bool available();
	
	/**
	* @brief Initializes CH32V003 driver
	*
	* @param MISO MISO pin
	* @param MOSI MOSI pin
	* @param CLK CLK pin
	* @param CS CS pin
	* @param CSActiveState CS active state
	* @param host SPI host
	* 
	* @return bool success
	*/
	bool begin(gpio_num_t MISO = GPIO_NUM_35, gpio_num_t MOSI = GPIO_NUM_12, gpio_num_t CLK = GPIO_NUM_14, gpio_num_t CS = GPIO_NUM_13, int16_t CSActiveState = -1, int16_t host = HSPI_HOST);
	
	/**
	* @brief Deinitializes CH32V003 driver
	*/
	void end ();
	
	/**
	* @brief 
	* Gets CH32V003 firmware version
	* Most Significant Byte is major version. Least Significant Byte is minor version
	* Example: 
	*     0x0100 = ver 1.0
	*     0x0101 = ver 1.1
	*     0x020A = ver 2.10
	* @return uint16_t MSB - major; LSB - minor
	*/
	uint16_t version();
	
	/**
	* @brief 
	* Turns ON power at UEXT (pin 1) and enables external pull-ups on UEXT pins 5 and 6
	* The power is ON by default
	*/
	void uextPowerEnable();
	
	/**
	* @brief 
	* Turns OFF power at UEXT (pin 1)
	*/
	void uextPowerDisable();
	
	// GPIO
	/**
	* @brief 
	* Configures GPIO port at once.
	* 
	* @param port     GPIO_PORTx where x is one of A, C or D
	* @param mask     One or more of GPIO_Pin_x - pins to be configured as GPIO
	* @param in_out   Set corresponding bits as 0 - output;   1 - input
	* @param pullup   Set corresponding bits as 0 - pulldown; 1 - pullup
	*/
	void configurePort (uint8_t port, uint8_t mask, uint8_t in_out, uint8_t pullup);
	
	/**
	* @brief 
	* Set GPIO port at once.
	* 
	* @param port     GPIO_PORTx where x is one of A, C or D
	* @param mask     One or more of GPIO_Pin_x - pins to be set
	* @param value    Set corresponding bits as 0 - LOW; 1 - HIGH
	* 
	* value parameter has no effect on pins configured as input
	*/
	void setPort (uint8_t port, uint8_t mask, uint8_t value);
	
	/**
	* @brief 
	* Get GPIO port at once.
	* 
	* @param port     GPIO_PORTx where x is one of A, C or D
	* @param mask     One or more of GPIO_Pin_x - pins to be get
	* 
	* @return corresponding bits as 0 if input is LOW or 1 if input is HIGH
	*/
	uint8_t getPort (uint8_t port, uint8_t mask);
	
	/**
	* @brief 
	* Configure individual GPIO pin
	*
	* @param port     GPIO_PORTx where x is one of A, C or D
	* @param gpio     GPIO_x where x is one from 0 to 7
	* @param dir      DIRECTION_IN or DIRECTION_OUT
	* @param pullup   if pin is input 1 - pullup; 0 - pulldown
	* 
	* Macros UEXT_GPIO_x can be used as combination of port and gpio
	*/
	void configureGPIO (uint8_t port, uint8_t gpio, uint8_t dir, uint8_t pullup = 0x00);
	
	/**
	* @brief 
	* Sets individual GPIO pin
	* 
	* @param port     GPIO_PORTx where x is one of A, C or D
	* @param gpio     GPIO_x where x is one from 0 to 7
	* @param value    0 - LOW; 1 - HIGH
	* 
	* Macros UEXT_GPIO_x can be used as combination of port and gpio
	*/
	void setGPIO (uint8_t port, uint8_t gpio, uint8_t value);
	
	/**
	* @brief 
	* Gets individual GPIO pin level
	* 
	* @param port     GPIO_PORTx where x is one of A, C or D
	* @param gpio     GPIO_x where x is one from 0 to 7
	* 
	* Macros UEXT_GPIO_x can be used as combination of port and gpio
	* 
	* @return uint8_t 1 - pin level is HIGH; 0 - pin level is LOW
	*/
	uint8_t getGPIO (uint8_t port, uint8_t gpio);
	
	/**
	* @brief 
	* Get Power Sense 
	* 
	* When battery is attached and external power is plugged in or 
	* out - IO_EXP_IRQ pin and interrupt flag for PortD pin 0 are set.
	* 
	* The level of IO_EXP_IRQ pin by default is HIGH. This can be changed 
	* by calling CH32V003::setIntActive()
	* 
	* Using this function will clear interrupt flag for PortD pin 0. 
	* IO_EXP_IRQ pin may remain set if there are other interrupts pending
	* 
	* @return uint8_t 1 - external power is present; 0 - running on battery
	*/
	uint8_t powerSense();
	
	/**
	* @brief 
	* Get Battery Sense 
	* @return uint16_t battery level in mV
	*/
	uint16_t batterySense();
	
	uint8_t batteryPercent (uint16_t bat_sense = 0);
	
	/**
	* @brief 
	* Set level for IO_EXP_IRQ pin - default is HIGH on interrupt
	*/
	void setIntActive(uint8_t level);
	
	/**
	* @brief 
	* Enable interrupt for specified pin
	* Pin needs to be configured as input before calling this function.
	* When interrupt is triggered IO_EXP_IRQ pin and interrupt flag for 
	* specified pin are set.
	* 
	* The level of IO_EXP_IRQ pin by default is HIGH. This can be changed 
	* by calling CH32V003::setIntActive()
	* 
	* The interrupt flag is cleared by reading the pin or by calling 
	* CH32V003::getPortIntCaptured(). IO_EXP_IRQ pin may remain set if 
	* there are other interrupts pending
	*/
	void enableInterrupt(uint8_t port, uint8_t pin, GPIO_INT_Trigger trigger);
	
	/**
	* @brief 
	* Disable interrupt for specified pin
	*/
	void disableInterrupt(uint8_t port, uint8_t pin);
	
	/**
	* @brief 
	* Get which pin triggered interrupt. Interrupt flags and IO_EXP_IRQ will NOT be cleared.
	* @return uint8_t 
	*/
	uint8_t getPortIntFlags(uint8_t port);
	
	/**
	* @brief 
	* Get pin level at the time interrupt was triggered. Interrupt flags are cleared
	* IO_EXP_IRQ IS cleared only if there ane no other interrups pending.
	* @return uint8_t 
	*/
	uint8_t getPortIntCaptured(uint8_t port);
	
	// I2C
	/**
	* @brief 
	* Configures I2C master at UEXT (pins 5 and 6)
	*/
	void configureI2C (uint32_t clock);
	
	/**
	* @brief 
	* Sends data over I2C
	* 
	* @param address   slave address
	* @param buffer    data to be send
	* @param size      buffer size
	*/
	void writeI2C (uint8_t address, uint8_t buffer[], uint8_t size);
	
	/**
	* @brief 
	* Receive data over I2C
	* 
	* @param address   slave address
	* @param buffer    data to be read
	* @param size      buffer size
	*/
	void readI2C (uint8_t address, uint8_t buffer[], uint8_t size);
	
	/**
	* @brief 
	* Set register over I2C
	* 
	* @param address   slave address
	* @param reg       register num
	* @param value     value to be set
	*/
	void writeRegI2C (uint8_t address, uint8_t reg, uint8_t value);
	
	/**
	* @brief 
	* Get register over I2C
	* 
	* @param address   slave address
	* @param reg       register num
	* @return          register value
	*/
	uint8_t readRegI2C (uint8_t address, uint8_t reg);
	
	// SPI
	/**
	* @brief 
	* Configures software SPI at UEXT (pins 7, 8, 9 and 10)
	*/
	void configureSPI (uint8_t spi_mode, uint32_t clock);
	void transferSPI8 (uint8_t tx_buffer[], uint8_t rx_buffer[], const uint8_t size);
	void transferSPI16 (uint16_t tx_buffer[], uint16_t rx_buffer[], const uint8_t size);
	
	// UART
	/**
	* @brief 
	* Configures UART at UEXT (pins 3 and 4)
	* When the data is received IO_EXP_IRQ and interrupt flag for PortD pin 6 are set.
	* Using readUART() will clear the interrupt flag if all data is acquired.
	* IO_EXP_IRQ may remain set if there are other interrupts pending
	*/
	void configureUART (uint32_t baudrate, UART_StopBits stop_bits, UART_Parity parity);
	
	/**
	* @brief 
	* Send data to UART
	*/
	void writeUART (uint8_t buff[], uint8_t size);
	void strWriteUART(char message[]);
	
	/**
	* @brief 
	* Read data from UART. The data is buffered (up to 254 bytes) and returned when requested.
	* When there is unread data in the buffer - IO_EXP_IRQ and interrupt flag for PortD pin 6 are set.
	* If the requested data is less than the buffered - IO_EXP_IRQ and interrupt flag for PortD pin 6 will remain set.
	* @return uint8_t bytes actual bytes count readed - up to size
	*/
	uint8_t readUART (uint8_t buff[], uint8_t size);
	
	// UEXT as virtual GPIO port
	/**
	* @brief
	* Enables UEXT power and clear interrupt flags and IO_EXP_IRQ
	*/
	void initUEXT ();
	
	/**
	* @brief
	* Configures virtual UEXT GPIO
	* 
	* Pin mapping
	* -------------------------------------------------
	* UEXT GPIO 0 - UEXT pin 3  - CH32V003 Port D pin 5
	* UEXT GPIO 1 - UEXT pin 4  - CH32V003 Port D pin 6
	* UEXT GPIO 2 - UEXT pin 5  - CH32V003 Port C pin 2
	* UEXT GPIO 3 - UEXT pin 6  - CH32V003 Port C pin 1
	* UEXT GPIO 4 - UEXT pin 7  - CH32V003 Port A pin 2
	* UEXT GPIO 5 - UEXT pin 8  - CH32V003 Port A pin 1
	* UEXT GPIO 6 - UEXT pin 9  - CH32V003 Port D pin 4
	* UEXT GPIO 7 - UEXT pin 10 - CH32V003 Port D pin 3
	* 
	* @param uint8_t gpio   - GPIO_x where x is one from 0 to 7
	* @param uint8_t dir    - DIRECTION_IN or DIRECTION_OUT
	* @param uint8_t pullup - if pin is input 1 - pullup; 0 - pulldown
	*/
	void configureUEXT (uint8_t gpio, uint8_t dir, uint8_t pullup = 0x00);
	
	/**
	* @brief
	* Enable interrupt for specified GPIO on virtual UEXT port
	* Pin needs to be configured as input before calling this function.
	*/
	void enableUEXTInterrupt (uint8_t gpio, GPIO_INT_Trigger trigger);
	
	/**
	* @brief
	* Disable interrupt for specified GPIO on virtual UEXT port
	*/
	void disableUEXTInterrupt (uint8_t gpio);
	
	/**
	* @brief
	* Get interrupt flags on virtual UEXT port
	*/
	uint8_t getUEXTIntFlags();
	
	/**
	* @brief
	* Get GPIO level at time of interruptwas triggered on virtual UEXT port
	*/
	uint8_t getUEXTIntCaptured();
	
	/**
	* @brief
	* Get GPIO level on virtual UEXT port
	*/
	uint8_t readUEXT (uint8_t gpio);
	
	/**
	* @brief
	* Set GPIO level on virtual UEXT port
	*/
	void writeUEXT (uint8_t gpio, uint8_t value);
	
private:
	gpio_num_t          CH_MISO, CH_MOSI, CH_CLK, CH_CS;
	spi_host_device_t   CH_SPIHost;
	spi_device_handle_t CH_SPIDevHandle;
	uint8_t spi_acquired;
	bool synced;
	uint16_t firmware_ver = 0x0000;
	
	void uext2port(uint8_t uextGPIO, uint8_t *port, uint8_t *portGPIO);

	void acquireSPI();
	void releaseSPI();
	
	uint8_t protocolHeader (uint8_t mode, uint8_t command, uint8_t direction);
	
	uint8_t transferByte (uint8_t send);
	uint16_t transferWord (uint16_t send);
	void transferProtocol(uint8_t txdata[], uint8_t rxdata[], uint16_t size);
	void sync();
};

#endif
