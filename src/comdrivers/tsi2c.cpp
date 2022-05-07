/*
  Created by Fabrizio Di Vittorio (fdivitto2013@gmail.com) - <http://www.fabgl.com>
  Copyright (c) 2019-2022 Fabrizio Di Vittorio.
  All rights reserved.


* Please contact fdivitto2013@gmail.com if you need a commercial license.


* This library and related software is available under GPL v3.

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
  :
    #if FABGL_ESP_IDF_VERSION < FABGL_ESP_IDF_VERSION_VAL(4, 4, 0)
    m_i2c(nullptr),
    #endif
    m_i2cAvailable(false),
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

  return m_i2cAvailable;
}


void I2C::end()
{
  if (m_commTaskHandle)
    vTaskDelete(m_commTaskHandle);
  m_commTaskHandle = nullptr;

  #if FABGL_ESP_IDF_VERSION < FABGL_ESP_IDF_VERSION_VAL(4, 4, 0)
  if (m_i2c)
    i2cRelease(m_i2c);
  m_i2c = nullptr;
  #endif

  if (m_eventGroup)
    vEventGroupDelete(m_eventGroup);
  m_eventGroup = nullptr;
  
  m_i2cAvailable = false;
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

  #if FABGL_ESP_IDF_VERSION < FABGL_ESP_IDF_VERSION_VAL(4, 4, 0)
  bool ret = (m_jobInfo.lastError == I2C_ERROR_OK);
  #else
  bool ret = (m_jobInfo.lastError == ESP_OK);
  #endif

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


#if FABGL_ESP_IDF_VERSION >= FABGL_ESP_IDF_VERSION_VAL(4, 4, 0)
static int i2cGetFrequency(uint8_t i2c_num)
{
  uint32_t r;
  i2cGetClock(i2c_num, &r);
  return r;
}
static void i2cSetFrequency(uint8_t i2c_num, int freq)
{
  i2cSetClock(i2c_num, freq);
}
#endif


void I2C::commTaskFunc(void * pvParameters)
{
  I2C * ths = (I2C*) pvParameters;

  auto initRes = i2cInit(ths->m_bus, ths->m_SDAGPIO, ths->m_SCLGPIO, I2C_DEFAULT_FREQUENCY);
  
  #if FABGL_ESP_IDF_VERSION < FABGL_ESP_IDF_VERSION_VAL(4, 4, 0)
  if (!initRes) {
    ESP_LOGE("FabGL", "unable to init I2C");
    abort();
  }
  auto i2c = initRes;
  i2cFlush(i2c);
  ths->m_i2c = i2c;
  #else
  if (initRes != ESP_OK) {
    ESP_LOGE("FabGL", "unable to init I2C");
    abort();
  }
  auto i2c = ths->m_bus;
  #endif
  
  ths->m_i2cAvailable = true;

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

    #if FABGL_ESP_IDF_VERSION < FABGL_ESP_IDF_VERSION_VAL(4, 4, 0)
    if (bits & EVTGROUP_WRITE)
      job->lastError = i2cWrite(i2c, job->address, job->buffer, job->size, true, job->timeout);
    else if (bits & EVTGROUP_READ)
      job->lastError = i2cRead(i2c, job->address, job->buffer, job->size, true, job->timeout, &job->readCount);
    #else
    if (bits & EVTGROUP_WRITE)
      job->lastError = i2cWrite(i2c, job->address, job->buffer, job->size, job->timeout);
    else if (bits & EVTGROUP_READ)
      job->lastError = i2cRead(i2c, job->address, job->buffer, job->size, job->timeout, &job->readCount);
    #endif

  }

}



} // end of namespace


#endif  // #ifdef ARDUINO
