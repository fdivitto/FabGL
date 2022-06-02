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


#pragma once



/**
 * @file
 *
 * @brief This file contains fabgl::CVBSBaseController definition.
 */


#include <stdint.h>
#include <stddef.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "fabglconf.h"
#include "fabutils.h"
#include "devdrivers/cvbsgenerator.h"
#include "displaycontroller.h"



namespace fabgl {


class CVBSBaseController : public GenericBitmappedDisplayController {

public:

  CVBSBaseController();

  // unwanted methods
  CVBSBaseController(CVBSBaseController const&) = delete;
  void operator=(CVBSBaseController const&)     = delete;

  void begin(gpio_num_t videoGPIO);
  
  void begin();

  virtual void end();

  // abstract method of BitmappedDisplayController
  virtual void suspendBackgroundPrimitiveExecution();

  // abstract method of BitmappedDisplayController
  virtual void resumeBackgroundPrimitiveExecution();

  void setResolution(char const * modeline, int viewPortWidth = -1, int viewPortHeight = -1, bool doubleBuffered = false);

  void setHorizontalRate(int value)               { m_horizontalRate = value; }
  int horizontalRate()                            { return m_horizontalRate; }
  
  virtual void setResolution(CVBSParams const * params, int viewPortWidth = -1, int viewPortHeight = -1, bool doubleBuffered = false);

  uint8_t * getScanline(int y)                     { return (uint8_t*) m_viewPort[y]; }
  
  CVBSParams const * params()                      { return m_CVBSGenerator.params(); }
  
  virtual bool suspendDoubleBuffering(bool value);


protected:

  void setDrawScanlineCallback(CVBSDrawScanlineCallback drawScanlineCallback)  { m_CVBSGenerator.setDrawScanlineCallback(drawScanlineCallback, this); }

  virtual void freeViewPort();

  virtual void init();

  void allocateViewPort(uint32_t allocCaps, int rowlen);
  virtual void allocateViewPort() = 0;
  virtual void checkViewPortSize() { };

  // abstract method of BitmappedDisplayController
  virtual void swapBuffers();
  
  void run();
  
  // when double buffer is enabled the "drawing" view port is always m_viewPort, while the "visible" view port is always m_viewPortVisible
  // when double buffer is not enabled then m_viewPort = m_viewPortVisible
  volatile uint8_t * *   m_viewPort;
  volatile uint8_t * *   m_viewPortVisible;
  
  // screen buffers (m_viewPort and m_viewPortVisible contain these values)
  volatile uint8_t * *   m_viewPorts[2];

  volatile int           m_primitiveProcessingSuspended;             // 0 = enabled, >0 suspended


private:

  CVBSGenerator          m_CVBSGenerator;
  
  int                    m_horizontalRate; // 1...
  
  uint8_t * *            m_viewPortMemoryPool;  // array ends with nullptr
};








} // end of namespace
