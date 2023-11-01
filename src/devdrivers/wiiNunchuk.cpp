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

#include "wiiNunchuk.h"

namespace fabgl {

WiiNunchuk::WiiNunchuk() :
	decrypt(false),
	available(false),
	controller(nullptr),
	packetQueue(0),
	updateTask(nullptr)
{
}

WiiNunchuk::~WiiNunchuk() {
	end();
}

void WiiNunchuk::begin(uint32_t i2c_clock, bool original, bool createTask) {
	decrypt = original;
	status = {};
	
#ifdef USE_EXPANDER
	controller = new CH32V003();
	bool ready = controller->begin();
#else
	controller = new I2C();
	bool ready = controller->begin(WII_NUNCHUK_SDA, WII_NUNCHUK_SCL);
#endif
	
	bool init = false;
	bool ident = false;
	
	if (controller != nullptr && ready) {
#ifdef USE_EXPANDER
		controller->configureI2C(i2c_clock);
#else
		i2c_frequency = i2c_clock;
#endif
		
		// Init WiiNunchuk and read first WiiNunchukPacket
		LOG_DEBUG("Initializing ");
		if (decrypt) {
			LOG_DEBUG("ORIGINAL\r\n");
			writeCommand(0x40, 0x00);
		} else {
			LOG_DEBUG("CLONE\r\n");
			writeCommand(0xF0, 0x55);
			vTaskDelay(pdMS_TO_TICKS(100));
			writeCommand(0xFB, 0x00);
		}
		vTaskDelay(pdMS_TO_TICKS(100));
		init = true;
	}
	
	ident = checkIdent();
	getNextPacket();
	
	if (createTask) {
		packetQueue = xQueueCreate(1, sizeof(WiiNunchukPacket));
		xTaskCreate(&WiiNunchukUpdate, "WiiNunchuk", WII_NUNCHUK_STACK, this, WII_NUNCHUK_PRIORITY, &updateTask);
	}
	
	available = (
		controller != nullptr 
		&& 
		ready && init && ident
		&& 
		(!createTask || packetQueue != 0)
		&& 
		(!createTask || updateTask != nullptr)
	);
}

void WiiNunchuk::end() {
	if (updateTask) {
		vTaskDelete(updateTask);
		updateTask = nullptr;
	}
	
	if (packetQueue) {
		vQueueDelete(packetQueue);
		packetQueue = 0;
	}
	
	if (controller) {
		controller->end();
		delete controller;
		controller = nullptr;
	}
	
	available = false;
}

bool WiiNunchuk::isAvailable() {
	return available;
}

bool WiiNunchuk::packetAvailable() {
	if (packetQueue) {
		return uxQueueMessagesWaiting(packetQueue) > 0;
	}
	return true;
}

bool WiiNunchuk::getNextPacket() {
	WiiNunchukPacket packet = {};
	bool res;
	
	if (packetQueue) {
		res = xQueueReceive(packetQueue, &packet, msToTicks(WII_NUNCHUK_TIMEOUT));
	} else {
		readPacket(0x00, &packet);
		res = true;
	}
	
	if (res) {
		decodePacket(&packet);
	}
	
	return res;
}

WiiNunchukStatus & WiiNunchuk::getStatus() {
	if (isAvailable()) {
		getNextPacket();
	}
	return status; 
}

void WiiNunchuk::emptyQueue() {
	if (packetQueue) {
		xQueueReset(packetQueue);
	}
}

void WiiNunchuk::writeCommand(uint8_t address, uint8_t data) {
	if (controller) {
		WiiNunchukPacket packet = {};
		packet.cmd[0] = address;
		packet.cmd[1] = data;
#ifdef USE_EXPANDER
		controller->writeI2C(WII_NUNCHUK_I2C_ADDRESS, packet.cmd, 2);
#else
		controller->write(WII_NUNCHUK_I2C_ADDRESS, packet.cmd, 2, i2c_frequency, WII_NUNCHUK_TIMEOUT);
#endif
	}
}

void WiiNunchuk::readPacket(uint8_t address, WiiNunchukPacket * packet) {
	if (controller) {
		packet->cmd[0] = address;
#ifdef USE_EXPANDER
		controller->writeI2C(WII_NUNCHUK_I2C_ADDRESS, packet->cmd, 1);
		controller->readI2C(WII_NUNCHUK_I2C_ADDRESS, packet->data, WII_NUNCHUK_BUFF_SIZE);
#else
		controller->write(WII_NUNCHUK_I2C_ADDRESS, packet->cmd, 1, i2c_frequency, WII_NUNCHUK_TIMEOUT);
		controller->read(WII_NUNCHUK_I2C_ADDRESS, packet->data, WII_NUNCHUK_BUFF_SIZE, i2c_frequency, WII_NUNCHUK_TIMEOUT);
#endif
	}
}

bool WiiNunchuk::checkIdent() {
	WiiNunchukPacket packet = {};
	WiiNunchuk::readPacket(0xFA, &packet);
	LOG_DEBUG("IDENT 0x");
	for (int i=0; i<WII_NUNCHUK_BUFF_SIZE; i++) {
		LOG_DEBUG("%02X", packet.data[i]);
	}
	LOG_DEBUG("\r\n");
	return packet.data[2] == 0xA4 && packet.data[3] == 0x20;
}

void WiiNunchuk::decodePacket(WiiNunchukPacket * packet) {
	if (decrypt) {
		for (uint8_t i=0; i<WII_NUNCHUK_BUFF_SIZE; i++) {
			packet->data[i] = (packet->data[i] ^ 0x17) + 0x17;
		}
	}
	
	status.joystick.x = (int16_t) packet->data[0] - (int16_t) WII_NUNCHUK_X_ZERO;
	status.joystick.y = (int16_t) packet->data[1] - (int16_t) WII_NUNCHUK_Y_ZERO;
	
	status.buttons.c = ((packet->data[5] >> 1) & 0x01) == 0;
	status.buttons.z = ((packet->data[5] >> 0) & 0x01) == 0;
	
	status.accel.x = ((int16_t) (packet->data[2] << 2) | ((packet->data[5] >> 2) & 0x03)) - (int16_t) WII_NUNCHUK_XA_ZERO;
	status.accel.y = ((int16_t) (packet->data[3] << 2) | ((packet->data[5] >> 4) & 0x03)) - (int16_t) WII_NUNCHUK_YA_ZERO;
	status.accel.z = ((int16_t) (packet->data[4] << 2) | ((packet->data[5] >> 6) & 0x03)) - (int16_t) WII_NUNCHUK_ZA_ZERO;
	
	status.pitch = atan2((float) status.accel.y, (float) status.accel.z);
	status.roll  = atan2((float) status.accel.x, (float) status.accel.z);
}

void WiiNunchuk::WiiNunchukUpdate(void * arg) {
	WiiNunchuk * nunchuk = (WiiNunchuk*) arg;
	WiiNunchukPacket packet = {};
	
	while (true) {
		if (nunchuk->isAvailable()) {
			// Read  following WiiNunchukPacket
			nunchuk->readPacket(0x00, &packet);
			xQueueOverwrite(nunchuk->packetQueue, &packet);
		}
		vTaskDelay(pdMS_TO_TICKS(1));
	}
}

} // namespace