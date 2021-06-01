/*
  Created by Fabrizio Di Vittorio (fdivitto2013@gmail.com) - <http://www.fabgl.com>
  Copyright (c) 2019-2021 Fabrizio Di Vittorio.
  All rights reserved.

  This library and related software is available under GPL v3 or commercial license. It is always free for students, hobbyists, professors and researchers.
  It is not-free if embedded as firmware in commercial boards.


* Contact for commercial license: fdivitto2013@gmail.com


* GPL license version 3, for non-commercial use:

  FabGL is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  FabGL is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with FabGL.  If not, see <http://www.gnu.org/licenses/>.
 */


#ifdef ARDUINO


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "tsi2c.h"


#pragma GCC optimize ("O2")


namespace fabgl {


#define I2C_COMMTASK_STACK    1000
#define I2C_COMMTASK_PRIORITY 5
#define I2C_DEFAULT_FREQUENCY 100000


#define EVTGROUP_READY (1 << 0)
#define EVTGROUP_WRITE (1 << 1)
#define EVTGROUP_READ  (1 << 2)
#define EVTGROUP_DONE  (1 << 3)


I2C::I2C(int bus)
  : m_i2c(nullptr),
    m_bus(bus),
    m_commTaskHandle(nullptr),
    m_eventGroup(nullptr)
{
}


I2C::~I2C()
{
  end();
}


bool I2C::begin(gpio_num_t SDAGPIO, gpio_num_t SCLGPIO)
{
  m_SDAGPIO   = SDAGPIO;
  m_SCLGPIO   = SCLGPIO;

  m_eventGroup = xEventGroupCreate();

  // why a task? Because esp32 i2c communications must be done on the same core
  // must be pinned to one core (0 in this case)
  xTaskCreatePinnedToCore(&commTaskFunc, "", I2C_COMMTASK_STACK, this, I2C_COMMTASK_PRIORITY, &m_commTaskHandle, 0);

  // wait for commTaskFunc() ends initialization
  xEventGroupWaitBits(m_eventGroup, EVTGROUP_DONE, true, false, portMAX_DELAY);

  // ready to accept jobs
  xEventGroupSetBits(m_eventGroup, EVTGROUP_READY);

  return m_i2c != nullptr;
}


void I2C::end()
{
  if (m_commTaskHandle)
    vTaskDelete(m_commTaskHandle);
  m_commTaskHandle = nullptr;

  if (m_i2c)
    i2cRelease(m_i2c);
  m_i2c = nullptr;

  if (m_eventGroup)
    vEventGroupDelete(m_eventGroup);
  m_eventGroup = nullptr;
}


bool I2C::write(int address, uint8_t * buffer, int size, int frequency, int timeOutMS)
{
  // wait for I2C to be ready
  xEventGroupWaitBits(m_eventGroup, EVTGROUP_READY, true, false, portMAX_DELAY);

  m_jobInfo.frequency = frequency;
  m_jobInfo.address   = address;
  m_jobInfo.buffer    = buffer;
  m_jobInfo.size      = size;
  m_jobInfo.timeout   = timeOutMS;

  // unlock comm task for writing
  // wait for comm task to finish job
  xEventGroupSync(m_eventGroup, EVTGROUP_WRITE, EVTGROUP_DONE, portMAX_DELAY);


  bool ret = (m_jobInfo.lastError == I2C_ERROR_OK);

  // makes I2C ready for new requests
  xEventGroupSetBits(m_eventGroup, EVTGROUP_READY);

  return ret;
}


int I2C::read(int address, uint8_t * buffer, int size, int frequency, int timeOutMS)
{
  // wait for I2C to be ready
  xEventGroupWaitBits(m_eventGroup, EVTGROUP_READY, true, false, portMAX_DELAY);

  m_jobInfo.frequency = frequency;
  m_jobInfo.address   = address;
  m_jobInfo.buffer    = buffer;
  m_jobInfo.size      = size;
  m_jobInfo.timeout   = timeOutMS;

  // unlock comm task for reading
  // wait for comm task to finish job
  xEventGroupSync(m_eventGroup, EVTGROUP_READ, EVTGROUP_DONE, portMAX_DELAY);

  int ret = m_jobInfo.readCount;

  // makes I2C ready for new requests
  xEventGroupSetBits(m_eventGroup, EVTGROUP_READY);

  return ret;
}



void I2C::commTaskFunc(void * pvParameters)
{
  I2C * ths = (I2C*) pvParameters;

  i2c_t * i2c = i2cInit(ths->m_bus, ths->m_SDAGPIO, ths->m_SCLGPIO, I2C_DEFAULT_FREQUENCY);
  if (!i2c) {
    ESP_LOGE("unable to init I2C");
    abort();
  }

  i2cFlush(i2c);

  ths->m_i2c = i2c;

  // get initial default frequency
  int freq = i2cGetFrequency(i2c);

  I2CJobInfo * job = &ths->m_jobInfo;

  // main send/receive loop
  while (true) {

    // unlock waiting task
    xEventGroupSetBits(ths->m_eventGroup, EVTGROUP_DONE);

    // wait for another job
    auto bits = xEventGroupWaitBits(ths->m_eventGroup, EVTGROUP_WRITE | EVTGROUP_READ, true, false, portMAX_DELAY);

    // setup frequency if necessary
    if (freq != job->frequency) {
      freq = job->frequency;
      i2cSetFrequency(i2c, freq);
    }

    if (bits & EVTGROUP_WRITE)
      job->lastError = i2cWrite(i2c, job->address, job->buffer, job->size, true, job->timeout);
    else if (bits & EVTGROUP_READ)
      job->lastError = i2cRead(i2c, job->address, job->buffer, job->size, true, job->timeout, &job->readCount);

  }

}



} // end of namespace


#endif  // #ifdef ARDUINO
