/*
  Created by Fabrizio Di Vittorio (fdivitto2013@gmail.com) - <http://www.fabgl.com>
  Copyright (c) 2019-2020 Fabrizio Di Vittorio.
  All rights reserved.

  This file is part of FabGL Library.

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
#include "ST7789Controller.h"




#define ST7789_UPDATETASK_STACK             1024
#define ST7789_UPDATETASK_PRIORITY          5

#define ST7789_BACKGROUND_PRIMITIVE_TIMEOUT 10000  // uS

#define ST7789_SPI_WRITE_FREQUENCY          40000000
#define ST7789_SPI_MODE                     SPI_MODE3
#define ST7789_DMACHANNEL                   2



#define ST7789_SWRST      0x01
#define ST7789_SLPOUT     0x11
#define ST7789_NORON      0x13
#define ST7789_MADCTL     0x36
#define ST7789_COLMOD     0x3A
#define ST7789_RDDCOLMOD  0x0C
#define ST7789_PORCTRL    0xB2
#define ST7789_GCTRL      0xB7
#define ST7789_VCOMS      0xBB
#define ST7789_LCMCTRL    0xC0
#define ST7789_VDVVRHEN   0xC2
#define ST7789_VRHS       0xC3
#define ST7789_VDVS       0xC4
#define ST7789_FRCTRL2    0xC6
#define ST7789_PWCTRL1    0xD0
#define ST7789_PVGAMCTRL  0xE0
#define ST7789_NVGAMCTRL  0xE1
#define ST7789_INVON      0x21
#define ST7789_CASET      0x2A
#define ST7789_RASET      0x2B
#define ST7789_INVOFF     0x20
#define ST7789_SLPOUT     0x11
#define ST7789_DISPON     0x29
#define ST7789_RAMWR      0x2C
#define ST7789_RAMCTRL    0xB0
#define ST7789_PTLAR      0x30
#define ST7789_PTLON      0x12
#define ST7789_WRDISBV    0x51
#define ST7789_WRCTRLD    0x53
#define ST7789_WRCACE     0x55
#define ST7789_WRCABCMB   0x5E



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


ST7789Controller::ST7789Controller(int controllerWidth, int controllerHeight)
  : m_spi(nullptr),
    m_SPIDevHandle(nullptr),
    m_viewPort(nullptr),
    m_viewPortVisible(nullptr),
    m_controllerWidth(controllerWidth),
    m_controllerHeight(controllerHeight),
    m_rotOffsetX(0),
    m_rotOffsetY(0),
    m_updateTaskHandle(nullptr),
    m_updateTaskRunning(false),
    m_orientation(ST7789Orientation::Normal)
{
}


ST7789Controller::~ST7789Controller()
{
  end();
}


//// setup manually controlled pins
void ST7789Controller::setupGPIO()
{
  // DC GPIO
  PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[m_DC], PIN_FUNC_GPIO);
  gpio_set_direction(m_DC, GPIO_MODE_OUTPUT);
  gpio_set_level(m_DC, 1);

  // reset GPIO
  if (m_RESX != GPIO_UNUSED) {
    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[m_RESX], PIN_FUNC_GPIO);
    gpio_set_direction(m_RESX, GPIO_MODE_OUTPUT);
    gpio_set_level(m_RESX, 1);
  }

  // CS GPIO
  if (m_CS != GPIO_UNUSED) {
    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[m_CS], PIN_FUNC_GPIO);
    gpio_set_direction(m_CS, GPIO_MODE_OUTPUT);
    gpio_set_level(m_CS, 1);
  }
}


// use SPIClass
// without CS it is not possible to share SPI with other devices
void ST7789Controller::begin(SPIClass * spi, gpio_num_t DC, gpio_num_t RESX, gpio_num_t CS)
{
  m_spi   = spi;
  m_DC    = DC;
  m_RESX  = RESX;
  m_CS    = CS;

  setupGPIO();
}


// use SPIClass
// without CS it is not possible to share SPI with other devices
void ST7789Controller::begin(SPIClass * spi, int DC, int RESX, int CS)
{
  begin(spi, int2gpio(DC), int2gpio(RESX), int2gpio(CS));
}


// use SDK driver
// without CS it is not possible to share SPI with other devices
void ST7789Controller::begin(int SCK, int MOSI, int DC, int RESX, int CS, int host)
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


void ST7789Controller::end()
{
  if (m_updateTaskHandle)
    vTaskDelete(m_updateTaskHandle);
  m_updateTaskHandle = nullptr;

  freeViewPort();

  SPIEnd();
}


void ST7789Controller::setResolution(char const * modeline, int viewPortWidth, int viewPortHeight, bool doubleBuffered)
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

  setDoubleBuffered(doubleBuffered);

  m_viewPortWidth  = viewPortWidth < 0 ? m_screenWidth : viewPortWidth;
  m_viewPortHeight = viewPortHeight < 0 ? m_screenHeight : viewPortHeight;

  resetPaintState();

  hardReset();
  softReset();

  allocViewPort();

  // setup update task
  xTaskCreate(&updateTaskFunc, "", ST7789_UPDATETASK_STACK, this, ST7789_UPDATETASK_PRIORITY, &m_updateTaskHandle);

  // allows updateTaskFunc() to run
  m_updateTaskFuncSuspended = 0;
}


void ST7789Controller::setScreenCol(int value)
{
  if (value != m_screenCol) {
    m_screenCol = iclamp(value, 0, m_viewPortWidth - m_screenWidth);
    Primitive p(PrimitiveCmd::Refresh, Rect(0, 0, m_viewPortWidth - 1, m_viewPortHeight - 1));
    addPrimitive(p);
  }
}


void ST7789Controller::setScreenRow(int value)
{
  if (value != m_screenRow) {
    m_screenRow = iclamp(value, 0, m_viewPortHeight - m_screenHeight);
    Primitive p(PrimitiveCmd::Refresh, Rect(0, 0, m_viewPortWidth - 1, m_viewPortHeight - 1));
    addPrimitive(p);
  }
}


// hard reset ST7789
void ST7789Controller::hardReset()
{
  if (m_RESX != GPIO_UNUSED) {
    SPIBeginWrite();

    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[m_RESX], PIN_FUNC_GPIO);
    gpio_set_direction(m_RESX, GPIO_MODE_OUTPUT);
    gpio_set_level(m_RESX, 1);
    vTaskDelay(5 / portTICK_PERIOD_MS);
    gpio_set_level(m_RESX, 0);
    vTaskDelay(20 / portTICK_PERIOD_MS);
    gpio_set_level(m_RESX, 1);

    SPIEndWrite();

    vTaskDelay(150 / portTICK_PERIOD_MS);
  }
}


void ST7789Controller::SPIBegin()
{
  if (m_spi)
    return;

  spi_bus_config_t busconf;
  memset(&busconf, 0, sizeof(busconf));
  busconf.mosi_io_num     = m_MOSI;
  busconf.miso_io_num     = -1;
  busconf.sclk_io_num     = m_SCK;
  busconf.quadwp_io_num   = -1;
  busconf.quadhd_io_num   = -1;
  busconf.flags           = SPICOMMON_BUSFLAG_MASTER;
  auto r = spi_bus_initialize(m_SPIHost, &busconf, ST7789_DMACHANNEL);
  if (r == ESP_OK || r == ESP_ERR_INVALID_STATE) {  // ESP_ERR_INVALID_STATE, maybe spi_bus_initialize already called
    spi_device_interface_config_t devconf;
    memset(&devconf, 0, sizeof(devconf));
    devconf.mode           = ST7789_SPI_MODE;
    devconf.clock_speed_hz = ST7789_SPI_WRITE_FREQUENCY;
    devconf.spics_io_num   = -1;
    devconf.flags          = 0;
    devconf.queue_size     = 1;
    spi_bus_add_device(m_SPIHost, &devconf, &m_SPIDevHandle);
  }

  if (m_updateTaskFuncSuspended)
    resumeBackgroundPrimitiveExecution();
}


void ST7789Controller::SPIEnd()
{
  if (m_spi)
    return;

  suspendBackgroundPrimitiveExecution();

  if (m_SPIDevHandle) {
    spi_bus_remove_device(m_SPIDevHandle);
    m_SPIDevHandle = nullptr;
    spi_bus_free(m_SPIHost);  // this will not free bus if there is a device still connected (ie sdcard)
  }
}


void ST7789Controller::SPIBeginWrite()
{
  if (m_spi) {
    m_spi->beginTransaction(SPISettings(ST7789_SPI_WRITE_FREQUENCY, SPI_MSBFIRST, ST7789_SPI_MODE));
  }

  if (m_SPIDevHandle) {
    spi_device_acquire_bus(m_SPIDevHandle, portMAX_DELAY);
  }

  if (m_CS != GPIO_UNUSED) {
    gpio_set_level(m_CS, 0);
  }
}


void ST7789Controller::SPIEndWrite()
{
  if (m_CS != GPIO_UNUSED) {
    gpio_set_level(m_CS, 1);
  }

  // leave in data mode
  gpio_set_level(m_DC, 1);  // 1 = DATA

  if (m_spi) {
    m_spi->endTransaction();
  }

  if (m_SPIDevHandle) {
    spi_device_release_bus(m_SPIDevHandle);
  }
}


void ST7789Controller::SPIWriteByte(uint8_t data)
{
  if (m_spi) {
    m_spi->write(data);
  }

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


void ST7789Controller::SPIWriteWord(uint16_t data)
{
  if (m_spi) {
    m_spi->write(data >> 8);
    m_spi->write(data & 0xff);
  }

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


void ST7789Controller::SPIWriteBuffer(void * data, size_t size)
{
  if (m_spi) {
    m_spi->writeBytes((uint8_t*)data, size);
  }

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


void ST7789Controller::writeCommand(uint8_t cmd)
{
  gpio_set_level(m_DC, 0);  // 0 = CMD
  SPIWriteByte(cmd);
}


void ST7789Controller::writeByte(uint8_t data)
{
  gpio_set_level(m_DC, 1);  // 1 = DATA
  SPIWriteByte(data);
}


void ST7789Controller::writeData(void * data, size_t size)
{
  gpio_set_level(m_DC, 1);  // 1 = DATA
  SPIWriteBuffer(data, size);
}


// high byte first
void ST7789Controller::writeWord(uint16_t data)
{
  gpio_set_level(m_DC, 1);  // 1 = DATA
  SPIWriteWord(data);
}


// soft reset ST7789
void ST7789Controller::softReset()
{
  // ST7789 software reset
  SPIBeginWrite();
  writeCommand(ST7789_SWRST);
  SPIEndWrite();
  vTaskDelay(150 / portTICK_PERIOD_MS);

  // ST7789 setup

  SPIBeginWrite();

  // Sleep Out
  writeCommand(ST7789_SLPOUT);
  vTaskDelay(120 / portTICK_PERIOD_MS);

  // Normal Display Mode On
  writeCommand(ST7789_NORON);

  setupOrientation();

  // 0x55 = 0 (101) 0 (101) => 65K of RGB interface, 16 bit/pixel
  writeCommand(ST7789_COLMOD);
  writeByte(0x55);
  vTaskDelay(10 / portTICK_PERIOD_MS);

  // Porch Setting
  writeCommand(ST7789_PORCTRL);
  writeByte(0x0c);
  writeByte(0x0c);
  writeByte(0x00);
  writeByte(0x33);
  writeByte(0x33);

  // Gate Control
  // VGL = -10.43V
  // VGH = 13.26V
  writeCommand(ST7789_GCTRL);
  writeByte(0x35);

  // VCOM Setting
  // 1.1V
  writeCommand(ST7789_VCOMS);
  writeByte(0x28);

  // LCM Control
  // XMH, XMX
  writeCommand(ST7789_LCMCTRL);
  writeByte(0x0C);

  // VDV and VRH Command Enable
  // CMDEN = 1, VDV and VRH register value comes from command write.
  writeCommand(ST7789_VDVVRHEN);
  writeByte(0x01);
  writeByte(0xFF);

  // VRH Set
  // VAP(GVDD) = 4.35+( vcom+vcom offset+vdv) V
  // VAN(GVCL) = -4.35+( vcom+vcom offset-vdv) V
  writeCommand(ST7789_VRHS);
  writeByte(0x10);

  // VDV Set
  // VDV  = 0V
  writeCommand(ST7789_VDVS);
  writeByte(0x20);

  // Frame Rate Control in Normal Mode
  // RTNA = 0xf (60Hz)
  // NLA  = 0 (dot inversion)
  writeCommand(ST7789_FRCTRL2);
  writeByte(0x0f);

  // Power Control 1
  // VDS  = 2.3V
  // AVCL = -4.8V
  // AVDD = 6.8v
  writeCommand(ST7789_PWCTRL1);
  writeByte(0xa4);
  writeByte(0xa1);

  // Positive Voltage Gamma Control
  writeCommand(ST7789_PVGAMCTRL);
  writeByte(0xd0);
  writeByte(0x00);
  writeByte(0x02);
  writeByte(0x07);
  writeByte(0x0a);
  writeByte(0x28);
  writeByte(0x32);
  writeByte(0x44);
  writeByte(0x42);
  writeByte(0x06);
  writeByte(0x0e);
  writeByte(0x12);
  writeByte(0x14);
  writeByte(0x17);

  // Negative Voltage Gamma Control
  writeCommand(ST7789_NVGAMCTRL);
  writeByte(0xd0);
  writeByte(0x00);
  writeByte(0x02);
  writeByte(0x07);
  writeByte(0x0a);
  writeByte(0x28);
  writeByte(0x31);
  writeByte(0x54);
  writeByte(0x47);
  writeByte(0x0e);
  writeByte(0x1c);
  writeByte(0x17);
  writeByte(0x1b);
  writeByte(0x1e);

  // Display Inversion On
  writeCommand(ST7789_INVON);

  // Display On
  writeCommand(ST7789_DISPON);

  SPIEndWrite();
}


void ST7789Controller::setupOrientation()
{
  m_rotOffsetX = 0;
  m_rotOffsetY = 0;
  uint8_t madclt = 0x08;  // BGR
  switch (m_orientation) {
    case ST7789Orientation::ReverseHorizontal:
      madclt |= 0x40;         // MX = 1
      m_rotOffsetX = m_controllerWidth - m_viewPortWidth;
      break;
    case ST7789Orientation::ReverseVertical:
      madclt |= 0x80;         // MY = 1
      m_rotOffsetY = m_controllerHeight - m_viewPortHeight;
      break;
    case ST7789Orientation::Rotate90:
      madclt |= 0x20 | 0x40;  // MV = 1, MX = 1
      break;
    case ST7789Orientation::Rotate180:
      madclt |= 0x40 | 0x80;  // MX = 1, MY = 1
      m_rotOffsetY = m_controllerHeight - m_viewPortHeight;
      m_rotOffsetX = m_controllerWidth - m_viewPortWidth;
      break;
    case ST7789Orientation::Rotate270:
      madclt |= 0x20 | 0x80;  // MV = 1, MY = 1
      m_rotOffsetX = m_controllerHeight - m_viewPortWidth;
      break;
    default:
      break;
  }
  writeCommand(ST7789_MADCTL);
  writeByte(madclt);
}


void ST7789Controller::setOrientation(ST7789Orientation value)
{
  m_orientation = value;
  setupOrientation();
  sendRefresh();
}


void ST7789Controller::sendRefresh()
{
  Primitive p(PrimitiveCmd::Refresh, Rect(0, 0, m_viewPortWidth - 1, m_viewPortHeight - 1));
  addPrimitive(p);
}


void ST7789Controller::sendScreenBuffer(Rect updateRect)
{
  SPIBeginWrite();

  updateRect = updateRect.intersection(Rect(0, 0, m_viewPortWidth - 1, m_viewPortHeight - 1));

  // select the buffer to send
  auto viewPort = isDoubleBuffered() ? m_viewPortVisible : m_viewPort;

  // Column Address Set
  writeCommand(ST7789_CASET);
  writeWord(m_rotOffsetX + updateRect.X1);   // XS (X Start)
  writeWord(m_rotOffsetX + updateRect.X2);   // XE (X End)

  // Row Address Set
  writeCommand(ST7789_RASET);
  writeWord(m_rotOffsetY + updateRect.Y1);  // YS (Y Start)
  writeWord(m_rotOffsetY + updateRect.Y2);  // YE (Y End)

  writeCommand(ST7789_RAMWR);
  const int width = updateRect.width();
  for (int row = updateRect.Y1; row <= updateRect.Y2; ++row) {
    writeData(viewPort[row] + updateRect.X1, sizeof(uint16_t) * width);
  }

  SPIEndWrite();
}


void ST7789Controller::allocViewPort()
{
  m_viewPort = (uint16_t**) heap_caps_malloc(m_viewPortHeight * sizeof(uint16_t*), MALLOC_CAP_32BIT);
  for (int i = 0; i < m_viewPortHeight; ++i) {
    m_viewPort[i] = (uint16_t*) heap_caps_malloc(m_viewPortWidth * sizeof(uint16_t), MALLOC_CAP_DMA);
    memset(m_viewPort[i], 0, m_viewPortWidth * sizeof(uint16_t));
  }

  if (isDoubleBuffered()) {
    m_viewPortVisible = (uint16_t**) heap_caps_malloc(m_viewPortHeight * sizeof(uint16_t*), MALLOC_CAP_32BIT);
    for (int i = 0; i < m_viewPortHeight; ++i) {
      m_viewPortVisible[i] = (uint16_t*) heap_caps_malloc(m_viewPortWidth * sizeof(uint16_t), MALLOC_CAP_DMA);
      memset(m_viewPortVisible[i], 0, m_viewPortWidth * sizeof(uint16_t));
    }
  }
}


void ST7789Controller::freeViewPort()
{
  if (m_viewPort) {
    for (int i = 0; i < m_viewPortHeight; ++i)
      heap_caps_free(m_viewPort[i]);
    heap_caps_free(m_viewPort);
    m_viewPort = nullptr;
  }
  if (m_viewPortVisible) {
    for (int i = 0; i < m_viewPortHeight; ++i)
      heap_caps_free(m_viewPortVisible[i]);
    heap_caps_free(m_viewPortVisible);
    m_viewPortVisible = nullptr;
  }
}


void ST7789Controller::updateTaskFunc(void * pvParameters)
{
  ST7789Controller * ctrl = (ST7789Controller*) pvParameters;

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
      if (ctrl->getPrimitive(&prim, ST7789_BACKGROUND_PRIMITIVE_TIMEOUT / 1000) == false)
        break;

      ctrl->execPrimitive(prim, updateRect);

      if (ctrl->m_updateTaskFuncSuspended > 0)
        break;

    } while (!ctrl->backgroundPrimitiveTimeoutEnabled() || (startTime + ST7789_BACKGROUND_PRIMITIVE_TIMEOUT > esp_timer_get_time()));

    ctrl->showSprites(updateRect);

    ctrl->m_updateTaskRunning = false;

    ctrl->sendScreenBuffer(updateRect);
  }
}



void ST7789Controller::suspendBackgroundPrimitiveExecution()
{
  ++m_updateTaskFuncSuspended;
  while (m_updateTaskRunning)
    taskYIELD();
}


void ST7789Controller::resumeBackgroundPrimitiveExecution()
{
  m_updateTaskFuncSuspended = tmax(0, m_updateTaskFuncSuspended - 1);
  if (m_updateTaskFuncSuspended == 0)
    xTaskNotifyGive(m_updateTaskHandle);  // resume updateTaskFunc()
}


void ST7789Controller::setPixelAt(PixelDesc const & pixelDesc, Rect & updateRect)
{
  genericSetPixelAt(pixelDesc, updateRect,
                    [&] (RGB888 const & color)           { return preparePixel(color); },
                    [&] (int X, int Y, uint16_t pattern) { m_viewPort[Y][X] = pattern; }
                   );
}


// coordinates are absolute values (not relative to origin)
// line clipped on current absolute clipping rectangle
void ST7789Controller::absDrawLine(int X1, int Y1, int X2, int Y2, RGB888 color)
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
void ST7789Controller::rawFillRow(int y, int x1, int x2, uint16_t pattern)
{
  auto px = m_viewPort[y] + x1;
  for (int x = x1; x <= x2; ++x, ++px)
    *px = pattern;
}


// parameters not checked
void ST7789Controller::rawFillRow(int y, int x1, int x2, RGB888 color)
{
  rawFillRow(y, x1, x2, preparePixel(color));
}


// swaps all pixels inside the range x1...x2 of yA and yB
// parameters not checked
void ST7789Controller::swapRows(int yA, int yB, int x1, int x2)
{
  auto pxA = m_viewPort[yA] + x1;
  auto pxB = m_viewPort[yB] + x1;
  for (int x = x1; x <= x2; ++x, ++pxA, ++pxB)
    tswap(*pxA, *pxB);
}


void ST7789Controller::rawInvertRow(int y, int x1, int x2)
{
  auto px = m_viewPort[y] + x1;
  for (int x = x1; x <= x2; ++x, ++px)
    *px = ~*px;
}


void ST7789Controller::drawEllipse(Size const & size, Rect & updateRect)
{
  genericDrawEllipse(size, updateRect,
                     [&] (RGB888 const & color)           { return preparePixel(color); },
                     [&] (int X, int Y, uint16_t pattern) { m_viewPort[Y][X] = pattern; }
                    );
}


void ST7789Controller::clear(Rect & updateRect)
{
  hideSprites(updateRect);
  auto pattern = preparePixel(getActualBrushColor());
  for (int y = 0; y < m_viewPortHeight; ++y)
    rawFillRow(y, 0, m_viewPortWidth - 1, pattern);
}


void ST7789Controller::VScroll(int scroll, Rect & updateRect)
{
  genericVScroll(scroll, updateRect,
                 [&] (int yA, int yB, int x1, int x2)        { swapRows(yA, yB, x1, x2); },              // swapRowsCopying
                 [&] (int yA, int yB)                        { tswap(m_viewPort[yA], m_viewPort[yB]); }, // swapRowsPointers
                 [&] (int y, int x1, int x2, RGB888 pattern) { rawFillRow(y, x1, x2, pattern); }         // rawFillRow
                );
}


void ST7789Controller::HScroll(int scroll, Rect & updateRect)
{
  genericHScroll(scroll, updateRect,
                 [&] (RGB888 const & color)               { return preparePixel(color); }, // preparePixel
                 [&] (int y)                              { return m_viewPort[y]; },       // rawGetRow
                 [&] (uint16_t * row, int x)              { return row[x]; },              // rawGetPixelInRow
                 [&] (uint16_t * row, int x, int pattern) { row[x] = pattern; }            // rawSetPixelInRow
                );
}


void ST7789Controller::drawGlyph(Glyph const & glyph, GlyphOptions glyphOptions, RGB888 penColor, RGB888 brushColor, Rect & updateRect)
{
  genericDrawGlyph(glyph, glyphOptions, penColor, brushColor, updateRect,
                   [&] (RGB888 const & color) { return preparePixel(color); },
                   [&] (int y)                { return m_viewPort[y]; },
                   [&] (uint16_t * row, int x, uint16_t pattern) { row[x] = pattern; }
                  );
}


void ST7789Controller::invertRect(Rect const & rect, Rect & updateRect)
{
  genericInvertRect(rect, updateRect,
                    [&] (int Y, int X1, int X2) { rawInvertRow(Y, X1, X2); }
                   );
}


void ST7789Controller::swapFGBG(Rect const & rect, Rect & updateRect)
{
  genericSwapFGBG(rect, updateRect,
                  [&] (RGB888 const & color)                    { return preparePixel(color); },
                  [&] (int y)                                   { return m_viewPort[y]; },
                  [&] (uint16_t * row, int x)                   { return row[x]; },
                  [&] (uint16_t * row, int x, uint16_t pattern) { row[x] = pattern; }
                 );
}


// supports overlapping of source and dest rectangles
void ST7789Controller::copyRect(Rect const & source, Rect & updateRect)
{
  genericCopyRect(source, updateRect,
                  [&] (int y)                                   { return m_viewPort[y]; },
                  [&] (uint16_t * row, int x)                   { return row[x]; },
                  [&] (uint16_t * row, int x, uint16_t pattern) { row[x] = pattern; }
                 );
}


// no bounds check is done!
void ST7789Controller::readScreen(Rect const & rect, RGB888 * destBuf)
{
  for (int y = rect.Y1; y <= rect.Y2; ++y) {
    auto row = m_viewPort[y] + rect.X1;
    for (int x = rect.X1; x <= rect.X2; ++x, ++destBuf, ++row)
      *destBuf = nativeToRGB888(*row);
  }
}


void ST7789Controller::rawDrawBitmap_Native(int destX, int destY, Bitmap const * bitmap, int X1, int Y1, int XCount, int YCount)
{
  genericRawDrawBitmap_Native(destX, destY, (uint16_t*) bitmap->data, bitmap->width, X1, Y1, XCount, YCount,
                                 [&] (int y)                               { return m_viewPort[y]; },  // rawGetRow
                                 [&] (uint16_t * row, int x, uint16_t src) { row[x] = src; }           // rawSetPixelInRow
                                );
}


void ST7789Controller::rawDrawBitmap_Mask(int destX, int destY, Bitmap const * bitmap, void * saveBackground, int X1, int Y1, int XCount, int YCount)
{
  auto foregroundPattern = preparePixel(bitmap->foregroundColor);
  genericRawDrawBitmap_Mask(destX, destY, bitmap, (uint16_t*)saveBackground, X1, Y1, XCount, YCount,
                            [&] (int y)                 { return m_viewPort[y]; },            // rawGetRow
                            [&] (uint16_t * row, int x) { return row[x]; },                   // rawGetPixelInRow
                            [&] (uint16_t * row, int x) { row[x] = foregroundPattern; }       // rawSetPixelInRow
                           );
}


void ST7789Controller::rawDrawBitmap_RGBA2222(int destX, int destY, Bitmap const * bitmap, void * saveBackground, int X1, int Y1, int XCount, int YCount)
{
  genericRawDrawBitmap_RGBA2222(destX, destY, bitmap, (uint16_t*)saveBackground, X1, Y1, XCount, YCount,
                                [&] (int y)                              { return m_viewPort[y]; },            // rawGetRow
                                [&] (uint16_t * row, int x)              { return row[x]; },                   // rawGetPixelInRow
                                [&] (uint16_t * row, int x, uint8_t src) { row[x] = RGBA2222toNative(src); }   // rawSetPixelInRow
                               );
}


void ST7789Controller::rawDrawBitmap_RGBA8888(int destX, int destY, Bitmap const * bitmap, void * saveBackground, int X1, int Y1, int XCount, int YCount)
{
  genericRawDrawBitmap_RGBA8888(destX, destY, bitmap, (uint16_t*)saveBackground, X1, Y1, XCount, YCount,
                                 [&] (int y)                                       { return m_viewPort[y]; },            // rawGetRow
                                 [&] (uint16_t * row, int x)                       { return row[x]; },                   // rawGetPixelInRow
                                 [&] (uint16_t * row, int x, RGBA8888 const & src) { row[x] = RGBA8888toNative(src); }   // rawSetPixelInRow
                                );
}


void ST7789Controller::swapBuffers()
{
  tswap(m_viewPort, m_viewPortVisible);
}




} // end of namespace
