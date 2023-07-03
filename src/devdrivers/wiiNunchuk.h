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
 * WiiNunchuk driver is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * WiiNunchuk driver is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with WiiNunchuk driver. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#include "fabglconf.h"
#include "fabutils.h"
#include "fabui.h"

#define USE_EXPANDER

#ifdef USE_EXPANDER
	#include "CH32V003.h"
#else
	#include "comdrivers/tsi2c.h"
	#define WII_NUNCHUK_SDA       GPIO_NUM_4
	#define WII_NUNCHUK_SCL       GPIO_NUM_15


	#define LOG_DEBUG_ENABLED

	#ifdef LOG_DEBUG_ENABLED
		#define LOG_DEBUG(f_, ...) printf((f_), ##__VA_ARGS__)
	#else
		#define LOG_DEBUG(f_, ...)
	#endif
#endif

#define WII_NUNCHUK_I2C_ADDRESS   0x52
#define WII_NUNCHUK_CMD_SIZE      0x02
#define WII_NUNCHUK_BUFF_SIZE     0x06

#define WII_NUNCHUK_X_ZERO         128
#define WII_NUNCHUK_Y_ZERO         128

#define WII_NUNCHUK_XA_ZERO        512
#define WII_NUNCHUK_YA_ZERO        512
#define WII_NUNCHUK_ZA_ZERO        512

#define WII_NUNCHUK_STACK         1600
#define WII_NUNCHUK_PRIORITY         5
#define WII_NUNCHUK_TIMEOUT         20



namespace fabgl {

/**
 * @brief Contains raw data received from WiiNunchuk.
 */
typedef struct {
	uint8_t cmd[WII_NUNCHUK_CMD_SIZE];
	uint8_t data[WII_NUNCHUK_BUFF_SIZE];
} WiiNunchukPacket;

typedef struct {
	int16_t x, y;
} WiiNunchukJoystick;

typedef struct {
	int16_t x, y, z;
} WiiNunchukAccel;

typedef struct {
	uint8_t c, z;
} WiiNunchukButtons;

/**
 * @brief Contains WiiNunchuk status i.e. decoded WiiNunchukPacket
 */
typedef struct {
	WiiNunchukJoystick joystick;
	WiiNunchukAccel accel;
	WiiNunchukButtons buttons;
	float pitch;
	float roll;
} WiiNunchukStatus;

class WiiNunchuk {
public:
	
	WiiNunchuk();
	
	~WiiNunchuk();
	
	void begin(uint32_t i2c_clock, bool original = false, bool createTask = true);
	
	void end();
	
	bool isAvailable();
	
	bool packetAvailable();
	
	WiiNunchukStatus & getStatus();
	
	/**
	* @brief Empties the WiiNunchuk status queue
	*/
	void emptyQueue();
	
private:
	
	bool getNextPacket();
	void writeCommand(uint8_t address, uint8_t data);
	void readPacket(uint8_t address, WiiNunchukPacket * packet);
	bool checkIdent();
	void decodePacket(WiiNunchukPacket * packet);
	static void WiiNunchukUpdate(void * arg);
	
#ifdef USE_EXPANDER
	CH32V003 * controller;
#else
	I2C * controller;
	uint32_t i2c_frequency;
#endif
	
	bool available;
	
	bool decrypt;
	
	TaskHandle_t updateTask;
	
	// queue of one WiiNunchukPacket
	QueueHandle_t packetQueue;
	
	WiiNunchukStatus status;
};

} // namespace