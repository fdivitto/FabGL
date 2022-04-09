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



#include <alloca.h>
#include <stdarg.h>
#include <math.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "fabutils.h"
#include "devdrivers/cvbsgenerator.h"
#include "dispdrivers/cvbsbasecontroller.h"


#pragma GCC optimize ("O2")


namespace fabgl {


CVBSBaseController::CVBSBaseController()
  : m_horizontalRate(1)
{
}


void CVBSBaseController::init()
{
  CurrentVideoMode::set(VideoMode::CVBS);
  m_primitiveProcessingSuspended = 1; // >0 suspended
  m_viewPort                     = nullptr;
  m_viewPortMemoryPool           = nullptr;
}


void CVBSBaseController::begin(gpio_num_t videoGPIO)
{
  init();
  m_CVBSGenerator.setVideoGPIO(videoGPIO);
}


void CVBSBaseController::begin()
{
  begin(GPIO_NUM_25);
}


void CVBSBaseController::end()
{
  m_CVBSGenerator.stop();
}


void CVBSBaseController::freeViewPort()
{
  if (m_viewPortMemoryPool) {
    for (auto poolPtr = m_viewPortMemoryPool; *poolPtr; ++poolPtr)
      heap_caps_free((void*) *poolPtr);
    heap_caps_free(m_viewPortMemoryPool);
    m_viewPortMemoryPool = nullptr;
  }
  if (m_viewPort) {
    heap_caps_free(m_viewPort);
    m_viewPort = nullptr;
  }
  if (isDoubleBuffered())
    heap_caps_free(m_viewPortVisible);
  m_viewPortVisible = nullptr;
}


// Warning: After call to suspendBackgroundPrimitiveExecution() adding primitives may cause a deadlock.
// To avoid this a call to "processPrimitives()" should be performed very often.
// Can be nested
void CVBSBaseController::suspendBackgroundPrimitiveExecution()
{
  ++m_primitiveProcessingSuspended;
}


// Resume after suspendBackgroundPrimitiveExecution()
// Can be nested
void CVBSBaseController::resumeBackgroundPrimitiveExecution()
{
  m_primitiveProcessingSuspended = tmax(0, m_primitiveProcessingSuspended - 1);
}


void CVBSBaseController::setResolution(char const * modeline, int viewPortWidth, int viewPortHeight, bool doubleBuffered)
{
  auto params = CVBSGenerator::getParamsFromDesc(modeline);
  if (params)
    setResolution(params, viewPortWidth, viewPortHeight);
}


void CVBSBaseController::setResolution(CVBSParams const * params, int viewPortWidth, int viewPortHeight, bool doubleBuffered)
{
  // just in case setResolution() was called before
  end();

  m_CVBSGenerator.setup(params);

  m_viewPortWidth  = viewPortWidth < 0 ? m_CVBSGenerator.visibleSamples() : viewPortWidth;
  m_viewPortHeight = viewPortHeight < 0 ? m_CVBSGenerator.visibleLines() * m_CVBSGenerator.params()->interlaceFactor : viewPortHeight;

  // reduce viewport if more than one sample per color is required
  m_viewPortWidth /= m_horizontalRate;

  // inform base class about screen size
  setScreenSize(m_viewPortWidth, m_viewPortHeight);

  setDoubleBuffered(doubleBuffered);

  // adjust view port size if necessary
  checkViewPortSize();

  // allocate the viewport
  allocateViewPort();

  // adjust again view port size if necessary
  checkViewPortSize();

  resetPaintState();
}


void CVBSBaseController::run()
{
  m_CVBSGenerator.run();
}


// this method may adjust m_viewPortHeight to the actual number of allocated rows.
// to reduce memory allocation overhead try to allocate the minimum number of blocks.
void CVBSBaseController::allocateViewPort(uint32_t allocCaps, int rowlen)
{
  int linesCount[FABGLIB_VIEWPORT_MEMORY_POOL_COUNT]; // where store number of lines for each pool
  int poolsCount = 0; // number of allocated pools
  int remainingLines = m_viewPortHeight;
  m_viewPortHeight = 0; // m_viewPortHeight needs to be recalculated

  if (isDoubleBuffered())
    remainingLines *= 2;

  // allocate pools
  m_viewPortMemoryPool = (uint8_t * *) heap_caps_malloc(sizeof(uint8_t*) * (FABGLIB_VIEWPORT_MEMORY_POOL_COUNT + 1), MALLOC_CAP_32BIT);
  while (remainingLines > 0 && poolsCount < FABGLIB_VIEWPORT_MEMORY_POOL_COUNT) {
    int largestBlock = heap_caps_get_largest_free_block(allocCaps);
    if (largestBlock < FABGLIB_MINFREELARGESTBLOCK)
      break;
    linesCount[poolsCount] = tmax(1, tmin(remainingLines, (largestBlock - FABGLIB_MINFREELARGESTBLOCK) / rowlen));
    m_viewPortMemoryPool[poolsCount] = (uint8_t*) heap_caps_malloc(linesCount[poolsCount] * rowlen, allocCaps);
    if (m_viewPortMemoryPool[poolsCount] == nullptr)
      break;
    remainingLines -= linesCount[poolsCount];
    m_viewPortHeight += linesCount[poolsCount];
    ++poolsCount;
  }
  m_viewPortMemoryPool[poolsCount] = nullptr;

  // fill m_viewPort[] with line pointers
  if (isDoubleBuffered()) {
    m_viewPortHeight /= 2;
    m_viewPortVisible = (volatile uint8_t * *) heap_caps_malloc(sizeof(uint8_t*) * m_viewPortHeight, MALLOC_CAP_32BIT | MALLOC_CAP_INTERNAL);
  }
  m_viewPort = (volatile uint8_t * *) heap_caps_malloc(sizeof(uint8_t*) * m_viewPortHeight, MALLOC_CAP_32BIT | MALLOC_CAP_INTERNAL);
  if (!isDoubleBuffered())
    m_viewPortVisible = m_viewPort;
  for (int p = 0, l = 0; p < poolsCount; ++p) {
    uint8_t * pool = m_viewPortMemoryPool[p];
    for (int i = 0; i < linesCount[p]; ++i) {
      if (l + i < m_viewPortHeight)
        m_viewPort[l + i] = pool;
      else
        m_viewPortVisible[l + i - m_viewPortHeight] = pool; // set only when double buffered is enabled
      pool += rowlen;
    }
    l += linesCount[p];
  }
}


void IRAM_ATTR CVBSBaseController::swapBuffers()
{
  tswap(m_viewPort, m_viewPortVisible);
}




} // end of namespace


