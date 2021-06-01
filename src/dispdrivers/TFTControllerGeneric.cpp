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



#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "fabutils.h"
#include "TFTControllerGeneric.h"


#pragma GCC optimize ("O2")


#define TFT_UPDATETASK_STACK             1024
#define TFT_UPDATETASK_PRIORITY          5

#define TFT_BACKGROUND_PRIMITIVE_TIMEOUT 10000  // uS

#define TFT_SPI_WRITE_FREQUENCY          40000000
#define TFT_SPI_MODE                     3
#define TFT_DMACHANNEL                   2




namespace fabgl {



// ESP32 is little-endian (in SPI, low byte of 16bit word is sent first), so the 16bit word must be converted from
//   RRRRRGGG GGGBBBBB
// to
//   GGGBBBBB RRRRRGGG
inline uint16_t preparePixel(RGB888 const & px)
{
  return ((uint16_t)(px.G & 0xe0) >> 5) |    //  0 ..  2: bits 5..7 of G
         ((uint16_t)(px.R & 0xf8)) |         //  3 ..  7: bits 3..7 of R
         ((uint16_t)(px.B & 0xf8) << 5) |    //  8 .. 12: bits 3..7 of B
         ((uint16_t)(px.G & 0x1c) << 11);    // 13 .. 15: bits 2..4 of G
}


inline RGB888 nativeToRGB888(uint16_t pattern)
{
  return RGB888(
    (pattern & 0xf8),
    ((pattern & 7) << 5) | ((pattern & 0xe000) >> 11),
    ((pattern & 0x1f00) >> 5)
  );
}


inline RGBA8888 nativeToRGBA8888(uint16_t pattern)
{
  return RGBA8888(
    (pattern & 0xf8),
    ((pattern & 7) << 5) | ((pattern & 0xe000) >> 11),
    ((pattern & 0x1f00) >> 5),
    0xff
  );
}


inline uint16_t RGBA2222toNative(uint8_t rgba2222)
{
  return preparePixel(RGB888((rgba2222 & 3) * 85, ((rgba2222 >> 2) & 3) * 85, ((rgba2222 >> 4) & 3) * 85));
}


inline uint16_t RGBA8888toNative(RGBA8888 const & rgba8888)
{
  return preparePixel(RGB888(rgba8888.R, rgba8888.G, rgba8888.B));
}


TFTController::TFTController()
  :
    #ifdef ARDUINO
    m_spi(nullptr),
    #endif
    m_SPIDevHandle(nullptr),
    m_viewPort(nullptr),
    m_controllerWidth(240),
    m_controllerHeight(320),
    m_rotOffsetX(0),
    m_rotOffsetY(0),
    m_updateTaskHandle(nullptr),
    m_updateTaskRunning(false),
    m_orientation(TFTOrientation::Rotate0),
    m_reverseHorizontal(false)
{
}


TFTController::~TFTController()
{
  end();
}


//// setup manually controlled pins
void TFTController::setupGPIO()
{
  // DC GPIO
  configureGPIO(m_DC, GPIO_MODE_OUTPUT);
  gpio_set_level(m_DC, 1);

  // reset GPIO
  if (m_RESX != GPIO_UNUSED) {
    configureGPIO(m_RESX, GPIO_MODE_OUTPUT);
    gpio_set_level(m_RESX, 1);
  }

  // CS GPIO
  if (m_CS != GPIO_UNUSED) {
    configureGPIO(m_CS, GPIO_MODE_OUTPUT);
    gpio_set_level(m_CS, 1);
  }
}


// use SPIClass
// without CS it is not possible to share SPI with other devices
#ifdef ARDUINO
void TFTController::begin(SPIClass * spi, gpio_num_t DC, gpio_num_t RESX, gpio_num_t CS)
{
  #ifdef ARDUINO
  m_spi   = spi;
  #endif

  m_DC    = DC;
  m_RESX  = RESX;
  m_CS    = CS;

  setupGPIO();
}
#endif


// use SPIClass
// without CS it is not possible to share SPI with other devices
#ifdef ARDUINO
void TFTController::begin(SPIClass * spi, int DC, int RESX, int CS)
{
  begin(spi, int2gpio(DC), int2gpio(RESX), int2gpio(CS));
}
#endif


// use SDK driver
// without CS it is not possible to share SPI with other devices
void TFTController::begin(int SCK, int MOSI, int DC, int RESX, int CS, int host)
{
  m_SPIHost = (spi_host_device_t)host;
  m_SCK     = int2gpio(SCK);
  m_MOSI    = int2gpio(MOSI);
  m_DC      = int2gpio(DC);
  m_RESX    = int2gpio(RESX);
  m_CS      = int2gpio(CS);

  setupGPIO();
  SPIBegin();
}


void TFTController::begin()
{
  begin(18, 23, 22, 21, 5, VSPI_HOST);
}


void TFTController::end()
{
  if (m_updateTaskHandle)
    vTaskDelete(m_updateTaskHandle);
  m_updateTaskHandle = nullptr;

  freeViewPort();

  SPIEnd();
}


void TFTController::setResolution(char const * modeline, int viewPortWidth, int viewPortHeight, bool doubleBuffered)
{
  char label[32];
  int pos = 0, swidth, sheight;
  int count = sscanf(modeline, "\"%[^\"]\" %d %d %n", label, &swidth, &sheight, &pos);
  if (count != 3 || pos == 0)
    return; // invalid modeline

  m_screenWidth  = swidth;
  m_screenHeight = sheight;
  m_screenCol    = 0;
  m_screenRow    = 0;

  // inform base class about screen size
  setScreenSize(m_screenWidth, m_screenHeight);

  setDoubleBuffered(doubleBuffered);

  m_viewPortWidth  = viewPortWidth < 0 ? m_screenWidth : viewPortWidth;
  m_viewPortHeight = viewPortHeight < 0 ? m_screenHeight : viewPortHeight;

  m_rot0ViewPortWidth  = m_viewPortWidth;
  m_rot0ViewPortHeight = m_viewPortHeight;

  resetPaintState();

  hardReset();
  softReset();

  // setup update task
  xTaskCreate(&updateTaskFunc, "", TFT_UPDATETASK_STACK, this, TFT_UPDATETASK_PRIORITY, &m_updateTaskHandle);

  // allows updateTaskFunc() to run
  m_updateTaskFuncSuspended = 0;
}


void TFTController::setScreenCol(int value)
{
  if (value != m_screenCol) {
    m_screenCol = iclamp(value, 0, m_viewPortWidth - m_screenWidth);
    Primitive p(PrimitiveCmd::Refresh, Rect(0, 0, m_viewPortWidth - 1, m_viewPortHeight - 1));
    addPrimitive(p);
  }
}


void TFTController::setScreenRow(int value)
{
  if (value != m_screenRow) {
    m_screenRow = iclamp(value, 0, m_viewPortHeight - m_screenHeight);
    Primitive p(PrimitiveCmd::Refresh, Rect(0, 0, m_viewPortWidth - 1, m_viewPortHeight - 1));
    addPrimitive(p);
  }
}


void TFTController::hardReset()
{
  if (m_RESX != GPIO_UNUSED) {
    SPIBeginWrite();

    configureGPIO(m_RESX, GPIO_MODE_OUTPUT);
    gpio_set_level(m_RESX, 1);
    vTaskDelay(5 / portTICK_PERIOD_MS);
    gpio_set_level(m_RESX, 0);
    vTaskDelay(20 / portTICK_PERIOD_MS);
    gpio_set_level(m_RESX, 1);

    SPIEndWrite();

    vTaskDelay(150 / portTICK_PERIOD_MS);
  }
}


void TFTController::setupOrientation()
{
  freeViewPort();
  m_viewPortWidth  = m_rot0ViewPortWidth;
  m_viewPortHeight = m_rot0ViewPortHeight;
  m_rotOffsetX = 0;
  m_rotOffsetY = 0;
  uint8_t MX = m_reverseHorizontal ? 0x40 : 0;
  uint8_t madclt = 0x08 | MX;    // BGR
  switch (m_orientation) {
    case TFTOrientation::Rotate90:
      tswap(m_viewPortWidth, m_viewPortHeight);
      madclt |= 0x20;            // MV = 1
      madclt ^= 0x40;            // inv MX
      break;
    case TFTOrientation::Rotate180:
      madclt |= 0x80;            // MY = 1
      madclt ^= 0x40;            // inv MX
      m_rotOffsetY = m_controllerHeight - m_viewPortHeight;
      m_rotOffsetX = m_controllerWidth - m_viewPortWidth;
      break;
    case TFTOrientation::Rotate270:
      tswap(m_viewPortWidth, m_viewPortHeight);
      madclt |= 0x20 | 0x80;     // MV = 1, MY = 1
      m_rotOffsetX = m_controllerHeight - m_viewPortWidth;
      break;
    default:
      break;
  }
  // Memory Access Control
  writeCommand(TFT_MADCTL);
  writeByte(madclt);

  // alloc viewport
  allocViewPort();

  // resets scrolling region, clipping rect, etc...
  Primitive p;
  p.cmd = PrimitiveCmd::Reset;
  addPrimitive(p);
}


void TFTController::setOrientation(TFTOrientation value, bool force)
{
  if (m_orientation != value || force) {
    suspendBackgroundPrimitiveExecution();
    m_orientation = value;
    SPIBeginWrite();
    setupOrientation();
    SPIEndWrite();
    resumeBackgroundPrimitiveExecution();
    sendRefresh();
  }
}


void TFTController::setReverseHorizontal(bool value)
{
  m_reverseHorizontal = value;
  setOrientation(m_orientation, true);
}


void TFTController::SPIBegin()
{
  #ifdef ARDUINO
  if (m_spi)
    return;
  #endif

  spi_bus_config_t busconf;
  memset(&busconf, 0, sizeof(busconf));
  busconf.mosi_io_num     = m_MOSI;
  busconf.miso_io_num     = -1;
  busconf.sclk_io_num     = m_SCK;
  busconf.quadwp_io_num   = -1;
  busconf.quadhd_io_num   = -1;
  busconf.flags           = SPICOMMON_BUSFLAG_MASTER;
  auto r = spi_bus_initialize(m_SPIHost, &busconf, TFT_DMACHANNEL);
  if (r == ESP_OK || r == ESP_ERR_INVALID_STATE) {  // ESP_ERR_INVALID_STATE, maybe spi_bus_initialize already called
    spi_device_interface_config_t devconf;
    memset(&devconf, 0, sizeof(devconf));
    devconf.mode           = TFT_SPI_MODE;
    devconf.clock_speed_hz = TFT_SPI_WRITE_FREQUENCY;
    devconf.spics_io_num   = -1;
    devconf.flags          = 0;
    devconf.queue_size     = 1;
    spi_bus_add_device(m_SPIHost, &devconf, &m_SPIDevHandle);
  }

  if (m_updateTaskFuncSuspended)
    resumeBackgroundPrimitiveExecution();
}


void TFTController::SPIEnd()
{
  #ifdef ARDUINO
  if (m_spi)
    return;
  #endif

  suspendBackgroundPrimitiveExecution();

  if (m_SPIDevHandle) {
    spi_bus_remove_device(m_SPIDevHandle);
    m_SPIDevHandle = nullptr;
    spi_bus_free(m_SPIHost);  // this will not free bus if there is a device still connected (ie sdcard)
  }
}


void TFTController::SPIBeginWrite()
{
  #ifdef ARDUINO
  if (m_spi) {
    m_spi->beginTransaction(SPISettings(TFT_SPI_WRITE_FREQUENCY, SPI_MSBFIRST, TFT_SPI_MODE));
  }
  #endif

  if (m_SPIDevHandle) {
    spi_device_acquire_bus(m_SPIDevHandle, portMAX_DELAY);
  }

  if (m_CS != GPIO_UNUSED) {
    gpio_set_level(m_CS, 0);
  }
}


void TFTController::SPIEndWrite()
{
  if (m_CS != GPIO_UNUSED) {
    gpio_set_level(m_CS, 1);
  }

  // leave in data mode
  gpio_set_level(m_DC, 1);  // 1 = DATA

  #ifdef ARDUINO
  if (m_spi) {
    m_spi->endTransaction();
  }
  #endif

  if (m_SPIDevHandle) {
    spi_device_release_bus(m_SPIDevHandle);
  }
}


void TFTController::SPIWriteByte(uint8_t data)
{
  #ifdef ARDUINO
  if (m_spi) {
    m_spi->write(data);
  }
  #endif

  if (m_SPIDevHandle) {
    spi_transaction_t ta;
    ta.flags = SPI_TRANS_USE_TXDATA;
    ta.length = 8;
    ta.rxlength = 0;
    ta.tx_data[0] = data;
    ta.rx_buffer = nullptr;
    spi_device_polling_transmit(m_SPIDevHandle, &ta);
  }
}


void TFTController::SPIWriteWord(uint16_t data)
{
  #ifdef ARDUINO
  if (m_spi) {
    m_spi->write(data >> 8);
    m_spi->write(data & 0xff);
  }
  #endif

  if (m_SPIDevHandle) {
    spi_transaction_t ta;
    ta.flags = SPI_TRANS_USE_TXDATA;
    ta.length = 16;
    ta.rxlength = 0;
    ta.tx_data[0] = data >> 8;
    ta.tx_data[1] = data & 0xff;
    ta.rx_buffer = nullptr;
    spi_device_polling_transmit(m_SPIDevHandle, &ta);
  }
}


void TFTController::SPIWriteBuffer(void * data, size_t size)
{
  #ifdef ARDUINO
  if (m_spi) {
    m_spi->writeBytes((uint8_t*)data, size);
  }
  #endif

  if (m_SPIDevHandle) {
    spi_transaction_t ta;
    ta.flags = 0;
    ta.length = 8 * size;
    ta.rxlength = 0;
    ta.tx_buffer = data;
    ta.rx_buffer = nullptr;
    spi_device_polling_transmit(m_SPIDevHandle, &ta);
  }
}


void TFTController::writeCommand(uint8_t cmd)
{
  gpio_set_level(m_DC, 0);  // 0 = CMD
  SPIWriteByte(cmd);
}


void TFTController::writeByte(uint8_t data)
{
  gpio_set_level(m_DC, 1);  // 1 = DATA
  SPIWriteByte(data);
}


void TFTController::writeData(void * data, size_t size)
{
  gpio_set_level(m_DC, 1);  // 1 = DATA
  SPIWriteBuffer(data, size);
}


// high byte first
void TFTController::writeWord(uint16_t data)
{
  gpio_set_level(m_DC, 1);  // 1 = DATA
  SPIWriteWord(data);
}


void TFTController::sendRefresh()
{
  Primitive p(PrimitiveCmd::Refresh, Rect(0, 0, m_viewPortWidth - 1, m_viewPortHeight - 1));
  addPrimitive(p);
}


void TFTController::sendScreenBuffer(Rect updateRect)
{
  SPIBeginWrite();

  updateRect = updateRect.intersection(Rect(0, 0, m_viewPortWidth - 1, m_viewPortHeight - 1));

  // Column Address Set
  writeCommand(TFT_CASET);
  writeWord(m_rotOffsetX + updateRect.X1);   // XS (X Start)
  writeWord(m_rotOffsetX + updateRect.X2);   // XE (X End)

  // Row Address Set
  writeCommand(TFT_RASET);
  writeWord(m_rotOffsetY + updateRect.Y1);  // YS (Y Start)
  writeWord(m_rotOffsetY + updateRect.Y2);  // YE (Y End)

  writeCommand(TFT_RAMWR);
  const int width = updateRect.width();
  for (int row = updateRect.Y1; row <= updateRect.Y2; ++row) {
    writeData(m_viewPort[row] + updateRect.X1, sizeof(uint16_t) * width);
  }

  SPIEndWrite();
}


void TFTController::allocViewPort()
{
  m_viewPort = (uint16_t**) heap_caps_malloc(m_viewPortHeight * sizeof(uint16_t*), MALLOC_CAP_32BIT | MALLOC_CAP_INTERNAL);
  for (int i = 0; i < m_viewPortHeight; ++i) {
    m_viewPort[i] = (uint16_t*) heap_caps_malloc(m_viewPortWidth * sizeof(uint16_t), MALLOC_CAP_DMA);
    memset(m_viewPort[i], 0, m_viewPortWidth * sizeof(uint16_t));
  }
}


void TFTController::freeViewPort()
{
  if (m_viewPort) {
    for (int i = 0; i < m_viewPortHeight; ++i)
      heap_caps_free(m_viewPort[i]);
    heap_caps_free(m_viewPort);
    m_viewPort = nullptr;
  }
}


void TFTController::updateTaskFunc(void * pvParameters)
{
  TFTController * ctrl = (TFTController*) pvParameters;

  while (true) {

    ctrl->waitForPrimitives();

    // primitive processing blocked?
    if (ctrl->m_updateTaskFuncSuspended > 0)
      ulTaskNotifyTake(true, portMAX_DELAY); // yes, wait for a notify

    ctrl->m_updateTaskRunning = true;

    Rect updateRect = Rect(SHRT_MAX, SHRT_MAX, SHRT_MIN, SHRT_MIN);

    int64_t startTime = ctrl->backgroundPrimitiveTimeoutEnabled() ? esp_timer_get_time() : 0;
    do {

      Primitive prim;
      if (ctrl->getPrimitive(&prim, TFT_BACKGROUND_PRIMITIVE_TIMEOUT / 1000) == false)
        break;

      ctrl->execPrimitive(prim, updateRect, false);

      if (ctrl->m_updateTaskFuncSuspended > 0)
        break;

    } while (!ctrl->backgroundPrimitiveTimeoutEnabled() || (startTime + TFT_BACKGROUND_PRIMITIVE_TIMEOUT > esp_timer_get_time()));

    ctrl->showSprites(updateRect);

    ctrl->m_updateTaskRunning = false;

    if (!ctrl->isDoubleBuffered())
      ctrl->sendScreenBuffer(updateRect);
  }
}



void TFTController::suspendBackgroundPrimitiveExecution()
{
  ++m_updateTaskFuncSuspended;
  while (m_updateTaskRunning)
    taskYIELD();
}


void TFTController::resumeBackgroundPrimitiveExecution()
{
  m_updateTaskFuncSuspended = tmax(0, m_updateTaskFuncSuspended - 1);
  if (m_updateTaskFuncSuspended == 0)
    xTaskNotifyGive(m_updateTaskHandle);  // resume updateTaskFunc()
}


void TFTController::setPixelAt(PixelDesc const & pixelDesc, Rect & updateRect)
{
  genericSetPixelAt(pixelDesc, updateRect,
                    [&] (RGB888 const & color)           { return preparePixel(color); },
                    [&] (int X, int Y, uint16_t pattern) { m_viewPort[Y][X] = pattern; }
                   );
}


// coordinates are absolute values (not relative to origin)
// line clipped on current absolute clipping rectangle
void TFTController::absDrawLine(int X1, int Y1, int X2, int Y2, RGB888 color)
{
  genericAbsDrawLine(X1, Y1, X2, Y2, color,
                     [&] (RGB888 const & color)                    { return preparePixel(color); },
                     [&] (int Y, int X1, int X2, uint16_t pattern) { rawFillRow(Y, X1, X2, pattern); },
                     [&] (int Y, int X1, int X2)                   { rawInvertRow(Y, X1, X2); },
                     [&] (int X, int Y, uint16_t pattern)          { m_viewPort[Y][X] = pattern; },
                     [&] (int X, int Y)                            { m_viewPort[Y][X] = ~m_viewPort[Y][X]; }
                     );
}


// parameters not checked
void TFTController::rawFillRow(int y, int x1, int x2, uint16_t pattern)
{
  auto px = m_viewPort[y] + x1;
  for (int x = x1; x <= x2; ++x, ++px)
    *px = pattern;
}


// parameters not checked
void TFTController::rawFillRow(int y, int x1, int x2, RGB888 color)
{
  rawFillRow(y, x1, x2, preparePixel(color));
}


// swaps all pixels inside the range x1...x2 of yA and yB
// parameters not checked
void TFTController::swapRows(int yA, int yB, int x1, int x2)
{
  auto pxA = m_viewPort[yA] + x1;
  auto pxB = m_viewPort[yB] + x1;
  for (int x = x1; x <= x2; ++x, ++pxA, ++pxB)
    tswap(*pxA, *pxB);
}


void TFTController::rawInvertRow(int y, int x1, int x2)
{
  auto px = m_viewPort[y] + x1;
  for (int x = x1; x <= x2; ++x, ++px)
    *px = ~*px;
}


void TFTController::drawEllipse(Size const & size, Rect & updateRect)
{
  genericDrawEllipse(size, updateRect,
                     [&] (RGB888 const & color)           { return preparePixel(color); },
                     [&] (int X, int Y, uint16_t pattern) { m_viewPort[Y][X] = pattern; }
                    );
}


void TFTController::clear(Rect & updateRect)
{
  hideSprites(updateRect);
  auto pattern = preparePixel(getActualBrushColor());
  for (int y = 0; y < m_viewPortHeight; ++y)
    rawFillRow(y, 0, m_viewPortWidth - 1, pattern);
}


void TFTController::VScroll(int scroll, Rect & updateRect)
{
  genericVScroll(scroll, updateRect,
                 [&] (int yA, int yB, int x1, int x2)        { swapRows(yA, yB, x1, x2); },              // swapRowsCopying
                 [&] (int yA, int yB)                        { tswap(m_viewPort[yA], m_viewPort[yB]); }, // swapRowsPointers
                 [&] (int y, int x1, int x2, RGB888 pattern) { rawFillRow(y, x1, x2, pattern); }         // rawFillRow
                );
}


void TFTController::HScroll(int scroll, Rect & updateRect)
{
  genericHScroll(scroll, updateRect,
                 [&] (RGB888 const & color)               { return preparePixel(color); }, // preparePixel
                 [&] (int y)                              { return m_viewPort[y]; },       // rawGetRow
                 [&] (uint16_t * row, int x)              { return row[x]; },              // rawGetPixelInRow
                 [&] (uint16_t * row, int x, int pattern) { row[x] = pattern; }            // rawSetPixelInRow
                );
}


void TFTController::drawGlyph(Glyph const & glyph, GlyphOptions glyphOptions, RGB888 penColor, RGB888 brushColor, Rect & updateRect)
{
  genericDrawGlyph(glyph, glyphOptions, penColor, brushColor, updateRect,
                   [&] (RGB888 const & color) { return preparePixel(color); },
                   [&] (int y)                { return m_viewPort[y]; },
                   [&] (uint16_t * row, int x, uint16_t pattern) { row[x] = pattern; }
                  );
}


void TFTController::invertRect(Rect const & rect, Rect & updateRect)
{
  genericInvertRect(rect, updateRect,
                    [&] (int Y, int X1, int X2) { rawInvertRow(Y, X1, X2); }
                   );
}


void TFTController::swapFGBG(Rect const & rect, Rect & updateRect)
{
  genericSwapFGBG(rect, updateRect,
                  [&] (RGB888 const & color)                    { return preparePixel(color); },
                  [&] (int y)                                   { return m_viewPort[y]; },
                  [&] (uint16_t * row, int x)                   { return row[x]; },
                  [&] (uint16_t * row, int x, uint16_t pattern) { row[x] = pattern; }
                 );
}


// supports overlapping of source and dest rectangles
void TFTController::copyRect(Rect const & source, Rect & updateRect)
{
  genericCopyRect(source, updateRect,
                  [&] (int y)                                   { return m_viewPort[y]; },
                  [&] (uint16_t * row, int x)                   { return row[x]; },
                  [&] (uint16_t * row, int x, uint16_t pattern) { row[x] = pattern; }
                 );
}


// no bounds check is done!
void TFTController::readScreen(Rect const & rect, RGB888 * destBuf)
{
  for (int y = rect.Y1; y <= rect.Y2; ++y) {
    auto row = m_viewPort[y] + rect.X1;
    for (int x = rect.X1; x <= rect.X2; ++x, ++destBuf, ++row)
      *destBuf = nativeToRGB888(*row);
  }
}


void TFTController::rawDrawBitmap_Native(int destX, int destY, Bitmap const * bitmap, int X1, int Y1, int XCount, int YCount)
{
  genericRawDrawBitmap_Native(destX, destY, (uint16_t*) bitmap->data, bitmap->width, X1, Y1, XCount, YCount,
                                 [&] (int y)                               { return m_viewPort[y]; },  // rawGetRow
                                 [&] (uint16_t * row, int x, uint16_t src) { row[x] = src; }           // rawSetPixelInRow
                                );
}


void TFTController::rawDrawBitmap_Mask(int destX, int destY, Bitmap const * bitmap, void * saveBackground, int X1, int Y1, int XCount, int YCount)
{
  auto foregroundPattern = preparePixel(bitmap->foregroundColor);
  genericRawDrawBitmap_Mask(destX, destY, bitmap, (uint16_t*)saveBackground, X1, Y1, XCount, YCount,
                            [&] (int y)                 { return m_viewPort[y]; },            // rawGetRow
                            [&] (uint16_t * row, int x) { return row[x]; },                   // rawGetPixelInRow
                            [&] (uint16_t * row, int x) { row[x] = foregroundPattern; }       // rawSetPixelInRow
                           );
}


void TFTController::rawDrawBitmap_RGBA2222(int destX, int destY, Bitmap const * bitmap, void * saveBackground, int X1, int Y1, int XCount, int YCount)
{
  genericRawDrawBitmap_RGBA2222(destX, destY, bitmap, (uint16_t*)saveBackground, X1, Y1, XCount, YCount,
                                [&] (int y)                              { return m_viewPort[y]; },            // rawGetRow
                                [&] (uint16_t * row, int x)              { return row[x]; },                   // rawGetPixelInRow
                                [&] (uint16_t * row, int x, uint8_t src) { row[x] = RGBA2222toNative(src); }   // rawSetPixelInRow
                               );
}


void TFTController::rawDrawBitmap_RGBA8888(int destX, int destY, Bitmap const * bitmap, void * saveBackground, int X1, int Y1, int XCount, int YCount)
{
  genericRawDrawBitmap_RGBA8888(destX, destY, bitmap, (uint16_t*)saveBackground, X1, Y1, XCount, YCount,
                                 [&] (int y)                                       { return m_viewPort[y]; },            // rawGetRow
                                 [&] (uint16_t * row, int x)                       { return row[x]; },                   // rawGetPixelInRow
                                 [&] (uint16_t * row, int x, RGBA8888 const & src) { row[x] = RGBA8888toNative(src); }   // rawSetPixelInRow
                                );
}


void TFTController::swapBuffers()
{
  // nothing to do, we just send current view port to the device
  sendScreenBuffer(Rect(0, 0, getViewPortWidth() - 1, getViewPortHeight() - 1));
}




} // end of namespace
