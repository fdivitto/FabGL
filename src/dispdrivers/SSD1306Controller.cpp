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


#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "fabutils.h"
#include "SSD1306Controller.h"


#pragma GCC optimize ("O2")


#define SSD1306_I2C_TIMEOUT         100  // ms
#define SSD1306_I2C_FREQUENCY       400000

#define SSD1306_UPDATETASK_STACK    1024
#define SSD1306_UPDATETASK_PRIORITY 5

#define SSD1306_BACKGROUND_PRIMITIVE_TIMEOUT 10000  // uS


#define SSD1306_SETLOWCOLUMN                         0x00
#define SSD1306_SETHIGHCOLUMN                        0x10
#define SSD1306_MEMORYMODE                           0x20
#define SSD1306_COLUMNADDR                           0x21
#define SSD1306_PAGEADDR                             0x22
#define SSD1306_RIGHT_HORIZONTAL_SCROLL              0x26
#define SSD1306_LEFT_HORIZONTAL_SCROLL               0x27
#define SSD1306_VERTICAL_AND_RIGHT_HORIZONTAL_SCROLL 0x29
#define SSD1306_VERTICAL_AND_LEFT_HORIZONTAL_SCROLL  0x2A
#define SSD1306_DEACTIVATE_SCROLL                    0x2E
#define SSD1306_ACTIVATE_SCROLL                      0x2F
#define SSD1306_SETSTARTLINE                         0x40
#define SSD1306_SETCONTRAST                          0x81
#define SSD1306_CHARGEPUMP                           0x8D
#define SSD1306_SEGREMAP                             0xA0
#define SSD1306_SET_VERTICAL_SCROLL_AREA             0xA3
#define SSD1306_DISPLAYALLON_RESUME                  0xA4
#define SSD1306_DISPLAYALLON                         0xA5
#define SSD1306_NORMALDISPLAY                        0xA6
#define SSD1306_INVERTDISPLAY                        0xA7
#define SSD1306_SETMULTIPLEX                         0xA8
#define SSD1306_DISPLAYOFF                           0xAE
#define SSD1306_DISPLAYON                            0xAF
#define SSD1306_COMSCANINC                           0xC0
#define SSD1306_COMSCANDEC                           0xC8
#define SSD1306_SETDISPLAYOFFSET                     0xD3
#define SSD1306_SETDISPLAYCLOCKDIV                   0xD5
#define SSD1306_SETPRECHARGE                         0xD9
#define SSD1306_SETCOMPINS                           0xDA
#define SSD1306_SETVCOMDETECT                        0xDB


#define SSD1306_SETPIXEL(x, y)              (m_screenBuffer[(x) + ((y) >> 3) * m_viewPortWidth] |=  (1 << ((y) & 7)))
#define SSD1306_CLEARPIXEL(x, y)            (m_screenBuffer[(x) + ((y) >> 3) * m_viewPortWidth] &= ~(1 << ((y) & 7)))
#define SSD1306_INVERTPIXEL(x, y)           (m_screenBuffer[(x) + ((y) >> 3) * m_viewPortWidth] ^=  (1 << ((y) & 7)))

#define SSD1306_SETPIXELCOLOR(x, y, color)  { if ((color)) SSD1306_SETPIXEL((x), (y)); else SSD1306_CLEARPIXEL((x), (y)); }

#define SSD1306_GETPIXEL(x, y)              ((m_screenBuffer[(x) + ((y) >> 3) * m_viewPortWidth] >> ((y) & 7)) & 1)


namespace fabgl {


inline uint8_t RGB888toMono(RGB888 const & rgb)
{
  return rgb.R > 0 || rgb.G > 0 || rgb.B > 0 ? 1 : 0;
}


inline uint8_t RGBA2222toMono(uint8_t rgba2222)
{
  return (rgba2222 & 0x3f) ? 1 : 0;
}


inline uint8_t RGBA8888toMono(RGBA8888 const & rgba)
{
  return rgba.R > 0 || rgba.G > 0 || rgba.B > 0 ? 1 : 0;
}


inline uint8_t preparePixel(RGB888 const & rgb)
{
  return RGB888toMono(rgb);
}



SSD1306Controller::SSD1306Controller()
  : m_i2c(nullptr),
    m_screenBuffer(nullptr),
    m_updateTaskHandle(nullptr),
    m_updateTaskRunning(false),
    m_orientation(SSD1306Orientation::Normal)
{
}


SSD1306Controller::~SSD1306Controller()
{
  end();
}


void SSD1306Controller::begin(I2C * i2c, int address, gpio_num_t resetGPIO)
{
  m_i2c        = i2c;
  m_i2cAddress = address;
  m_resetGPIO  = resetGPIO;
}


void SSD1306Controller::begin()
{
  auto i2c = new I2C;
  i2c->begin(GPIO_NUM_4, GPIO_NUM_15);
  begin(i2c);
}


void SSD1306Controller::end()
{
  if (m_updateTaskHandle)
    vTaskDelete(m_updateTaskHandle);
  m_updateTaskHandle = nullptr;

  free(m_screenBuffer);
  m_screenBuffer = nullptr;
}


void SSD1306Controller::setResolution(char const * modeline, int viewPortWidth, int viewPortHeight, bool doubleBuffered)
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

  resetPaintState();

  SSD1306_hardReset();

  if (!SSD1306_softReset())
    return;

  allocScreenBuffer();

  // setup update task
  xTaskCreate(&updateTaskFunc, "", SSD1306_UPDATETASK_STACK, this, SSD1306_UPDATETASK_PRIORITY, &m_updateTaskHandle);

  // allows updateTaskFunc() to run
  m_updateTaskFuncSuspended = 0;
}


void SSD1306Controller::setScreenCol(int value)
{
  if (value != m_screenCol) {
    m_screenCol = iclamp(value, 0, m_viewPortWidth - m_screenWidth);
    sendRefresh();
  }
}


void SSD1306Controller::setScreenRow(int value)
{
  if (value != m_screenRow) {
    m_screenRow = iclamp(value, 0, m_viewPortHeight - m_screenHeight);
    sendRefresh();
  }
}


void SSD1306Controller::sendRefresh()
{
  Primitive p(PrimitiveCmd::Refresh, Rect(0, 0, m_viewPortWidth - 1, m_viewPortHeight - 1));
  addPrimitive(p);
}


bool SSD1306Controller::SSD1306_sendData(uint8_t * buf, int count, uint8_t ctrl)
{
  int bufSize = m_i2c->getMaxBufferLength();
  uint8_t sbuf[bufSize];
  sbuf[0] = ctrl;
  while (count > 0) {
    int bToSend = imin(bufSize - 1, count);
    memcpy(&sbuf[1], buf, bToSend);
    if (!m_i2c->write(m_i2cAddress, sbuf, bToSend + 1, SSD1306_I2C_FREQUENCY, SSD1306_I2C_TIMEOUT))
      return false;
    count -= bToSend;
    buf   += bToSend;
    ets_delay_us(2);  // delay 2uS (standing to SSD1306 doc should be 1.3uS, before new transmission can start)
  }
  return true;
}


bool SSD1306Controller::SSD1306_sendCmd(uint8_t c)
{
  return SSD1306_sendData(&c, 1, 0x00);
}


bool SSD1306Controller::SSD1306_sendCmd(uint8_t c1, uint8_t c2)
{
  uint8_t buf[2] = { c1, c2 };
  return SSD1306_sendData(buf, 2, 0x00);
}


bool SSD1306Controller::SSD1306_sendCmd(uint8_t c1, uint8_t c2, uint8_t c3)
{
  uint8_t buf[3] = { c1, c2, c3 };
  return SSD1306_sendData(buf, 3, 0x00);
}


// hard reset SSD1306
void SSD1306Controller::SSD1306_hardReset()
{
  if (m_resetGPIO != GPIO_UNUSED) {
    configureGPIO(m_resetGPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(m_resetGPIO, 1);
    vTaskDelay(1 / portTICK_PERIOD_MS);
    gpio_set_level(m_resetGPIO, 0);
    vTaskDelay(10 / portTICK_PERIOD_MS);
    gpio_set_level(m_resetGPIO, 1);
  }
}


// soft reset SSD1306
bool SSD1306Controller::SSD1306_softReset()
{
  SSD1306_sendCmd(SSD1306_DISPLAYOFF);
  SSD1306_sendCmd(SSD1306_SETDISPLAYCLOCKDIV, 0x80);
  SSD1306_sendCmd(SSD1306_SETMULTIPLEX, m_screenHeight - 1);
  SSD1306_sendCmd(SSD1306_SETDISPLAYOFFSET, 0);
  SSD1306_sendCmd(SSD1306_SETSTARTLINE);
  SSD1306_sendCmd(SSD1306_CHARGEPUMP, 0x14);      // 0x14 = SWITCHCAPVCC,  0x10 = EXTERNALVCC
  SSD1306_sendCmd(SSD1306_MEMORYMODE, 0b100);     // 0b100 = page addressing mode
  setupOrientation();
  if (m_screenHeight == 64) {
    SSD1306_sendCmd(SSD1306_SETCOMPINS, 0x12);
    SSD1306_sendCmd(SSD1306_SETCONTRAST, 0xCF);   // max: 0xCF = SWITCHCAPVCC,  0x9F = EXTERNALVCC
  } else if (m_screenHeight == 32) {
    SSD1306_sendCmd(SSD1306_SETCOMPINS, 0x02);
    SSD1306_sendCmd(SSD1306_SETCONTRAST, 0x8F);
  }
  SSD1306_sendCmd(SSD1306_SETPRECHARGE, 0xF1);    // 0xF1 = SWITCHCAPVCC,  0x22 = EXTERNALVCC
  SSD1306_sendCmd(SSD1306_SETVCOMDETECT, 0x40);
  SSD1306_sendCmd(SSD1306_DISPLAYALLON_RESUME);
  SSD1306_sendCmd(SSD1306_NORMALDISPLAY);
  SSD1306_sendCmd(SSD1306_DEACTIVATE_SCROLL);
  return SSD1306_sendCmd(SSD1306_DISPLAYON);
}


void SSD1306Controller::setupOrientation()
{
  switch (m_orientation) {
    case SSD1306Orientation::Normal:
      SSD1306_sendCmd(SSD1306_SEGREMAP | 0x1);
      SSD1306_sendCmd(SSD1306_COMSCANDEC);
      break;
    case SSD1306Orientation::ReverseHorizontal:
      SSD1306_sendCmd(SSD1306_SEGREMAP | 0x0);
      SSD1306_sendCmd(SSD1306_COMSCANDEC);
      break;
    case SSD1306Orientation::ReverseVertical:
      SSD1306_sendCmd(SSD1306_SEGREMAP | 0x1);
      SSD1306_sendCmd(SSD1306_COMSCANINC);
      break;
    case SSD1306Orientation::Rotate180:
      SSD1306_sendCmd(SSD1306_SEGREMAP | 0x0);
      SSD1306_sendCmd(SSD1306_COMSCANINC);
      break;
  }
}


void SSD1306Controller::setOrientation(SSD1306Orientation value)
{
  m_orientation = value;
  setupOrientation();
  sendRefresh();
}


void SSD1306Controller::SSD1306_sendScreenBuffer(Rect updateRect)
{
  // align visible screen row to page (steps of 8 rows)
  const int screenRow = m_screenRow & ~7;

  // visible area
  const Rect scrRect = Rect(m_screenCol, screenRow, m_screenCol + m_screenWidth - 1, screenRow + m_screenHeight - 1);

  // align rectangle to update to pages (0, 8, 16...)
  updateRect.Y1 &= ~7;
  updateRect.Y2  = (updateRect.Y2 + 7) & ~7;

  // does the visible area intersect with area to update?
  if (scrRect.intersects(updateRect)) {

    // intersection between visible area and rectangle to update
    Rect r = updateRect.intersection(scrRect);

    // horizontal screen update limits
    const int screenX1 = r.X1 - m_screenCol;
    const int screenX2 = r.X2 - m_screenCol;

    // send one page (8 rows) at the time
    for (int y = r.Y1; y <= r.Y2; y += 8) {
      int screenY = y - screenRow;
      if (screenY >= 0) {
        int page = screenY >> 3;
        if (!(SSD1306_sendCmd(SSD1306_PAGEADDR, page, page) && SSD1306_sendCmd(SSD1306_COLUMNADDR, screenX1, screenX2)))
          break;  // address selection failed, try with next page
        SSD1306_sendData(m_screenBuffer + r.X1 + (y >> 3) * m_viewPortWidth, r.width(), 0x40);
      }
    }

  }
}


void SSD1306Controller::invert(bool value)
{
  SSD1306_sendCmd(value ? SSD1306_INVERTDISPLAY : SSD1306_NORMALDISPLAY);
}


void SSD1306Controller::allocScreenBuffer()
{
  m_screenBuffer = (uint8_t*) malloc(m_viewPortWidth * m_viewPortHeight / 8);
  memset(m_screenBuffer, 0, m_viewPortWidth * m_viewPortHeight / 8);
}


void SSD1306Controller::updateTaskFunc(void * pvParameters)
{
  SSD1306Controller * ctrl = (SSD1306Controller*) pvParameters;

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
      if (ctrl->getPrimitive(&prim) == false)
        break;

      ctrl->execPrimitive(prim, updateRect, false);

      if (ctrl->m_updateTaskFuncSuspended > 0)
        break;

    } while (!ctrl->backgroundPrimitiveTimeoutEnabled() || (startTime + SSD1306_BACKGROUND_PRIMITIVE_TIMEOUT > esp_timer_get_time()));

    ctrl->showSprites(updateRect);

    ctrl->m_updateTaskRunning = false;

    if (!ctrl->isDoubleBuffered())
      ctrl->SSD1306_sendScreenBuffer(updateRect);
  }
}



void SSD1306Controller::suspendBackgroundPrimitiveExecution()
{
  ++m_updateTaskFuncSuspended;
  while (m_updateTaskRunning)
    taskYIELD();
}


void SSD1306Controller::resumeBackgroundPrimitiveExecution()
{
  m_updateTaskFuncSuspended = tmax(0, m_updateTaskFuncSuspended - 1);
  if (m_updateTaskFuncSuspended == 0)
    xTaskNotifyGive(m_updateTaskHandle);  // resume updateTaskFunc()
}


void SSD1306Controller::setPixelAt(PixelDesc const & pixelDesc, Rect & updateRect)
{
  genericSetPixelAt(pixelDesc, updateRect,
                    [&] (RGB888 const & color)          { return preparePixel(color); },
                    [&] (int X, int Y, uint8_t pattern) { SSD1306_SETPIXELCOLOR(X, Y, pattern); }
                   );
}


// coordinates are absolute values (not relative to origin)
// line clipped on current absolute clipping rectangle
void SSD1306Controller::absDrawLine(int X1, int Y1, int X2, int Y2, RGB888 color)
{
  genericAbsDrawLine(X1, Y1, X2, Y2, color,
                     [&] (RGB888 const & color)                   { return preparePixel(color); },
                     [&] (int Y, int X1, int X2, uint8_t pattern) { rawFillRow(Y, X1, X2, pattern); },
                     [&] (int Y, int X1, int X2)                  { rawInvertRow(Y, X1, X2); },
                     [&] (int X, int Y, uint8_t pattern)          { SSD1306_SETPIXELCOLOR(X, Y, pattern); },
                     [&] (int X, int Y)                           { SSD1306_INVERTPIXEL(X, Y); }
                     );
}


// parameters not checked
void SSD1306Controller::rawFillRow(int y, int x1, int x2, uint8_t pattern)
{
  if (pattern) {
    for (; x1 <= x2; ++x1)
      SSD1306_SETPIXEL(x1, y);
  } else {
    for (; x1 <= x2; ++x1)
      SSD1306_CLEARPIXEL(x1, y);
  }
}


// parameters not checked
void SSD1306Controller::rawFillRow(int y, int x1, int x2, RGB888 color)
{
  rawFillRow(y, x1, x2, preparePixel(color));
}


// parameters not checked
void SSD1306Controller::rawInvertRow(int y, int x1, int x2)
{
  for (; x1 <= x2; ++x1)
    SSD1306_INVERTPIXEL(x1, y);
}


void SSD1306Controller::drawEllipse(Size const & size, Rect & updateRect)
{
  genericDrawEllipse(size, updateRect,
                     [&] (RGB888 const & color)          { return preparePixel(color); },
                     [&] (int X, int Y, uint8_t pattern) { SSD1306_SETPIXELCOLOR(X, Y, pattern); }
                    );
}


void SSD1306Controller::clear(Rect & updateRect)
{
  hideSprites(updateRect);
  uint8_t pattern = preparePixel(getActualBrushColor());
  memset(m_screenBuffer, (pattern ? 255 : 0), m_viewPortWidth * m_viewPortHeight / 8);
}


void SSD1306Controller::VScroll(int scroll, Rect & updateRect)
{
  genericVScroll(scroll, updateRect,
                 [&] (int x1, int x2, int srcY, int dstY)  { rawCopyRow(x1, x2, srcY, dstY); }, // rawCopyRow
                 [&] (int y, int x1, int x2, RGB888 color) { rawFillRow(y, x1, x2, color);   }  // rawFillRow
                );
}


void SSD1306Controller::HScroll(int scroll, Rect & updateRect)
{
  genericHScroll(scroll, updateRect,
                 [&] (RGB888 const & color)      { return preparePixel(color); },           // preparePixel
                 [&] (int y)                     { return y; },                             // rawGetRow
                 [&] (int y, int x)              { return SSD1306_GETPIXEL(x, y); },        // rawGetPixelInRow
                 [&] (int y, int x, int pattern) { SSD1306_SETPIXELCOLOR(x, y, pattern); }  // rawSetPixelInRow
                );
}


void SSD1306Controller::drawGlyph(Glyph const & glyph, GlyphOptions glyphOptions, RGB888 penColor, RGB888 brushColor, Rect & updateRect)
{
  genericDrawGlyph(glyph, glyphOptions, penColor, brushColor, updateRect,
                   [&] (RGB888 const & color)          { return preparePixel(color); },
                   [&] (int y)                         { return y; },
                   [&] (int y, int x, uint8_t pattern) { SSD1306_SETPIXELCOLOR(x, y, pattern); }
                  );
}


void SSD1306Controller::rawCopyRow(int x1, int x2, int srcY, int dstY)
{
  for (; x1 <= x2; ++x1) {
    uint8_t c = SSD1306_GETPIXEL(x1, srcY);
    SSD1306_SETPIXELCOLOR(x1, dstY, c);
  }
}


void SSD1306Controller::invertRect(Rect const & rect, Rect & updateRect)
{
  genericInvertRect(rect, updateRect,
                    [&] (int Y, int X1, int X2) { rawInvertRow(Y, X1, X2); }
                   );
}


void SSD1306Controller::swapFGBG(Rect const & rect, Rect & updateRect)
{
  invertRect(rect, updateRect);
}


// supports overlapping of source and dest rectangles
void SSD1306Controller::copyRect(Rect const & source, Rect & updateRect)
{
  genericCopyRect(source, updateRect,
                  [&] (int y)                         { return y; },
                  [&] (int y, int x)                  { return SSD1306_GETPIXEL(x, y); },
                  [&] (int y, int x, uint8_t pattern) { SSD1306_SETPIXELCOLOR(x, y, pattern); }
                 );
}


// no bounds check is done!
void SSD1306Controller::readScreen(Rect const & rect, RGB888 * destBuf)
{
  for (int y = rect.Y1; y <= rect.Y2; ++y)
    for (int x = rect.X1; x <= rect.X2; ++x, ++destBuf)
      *destBuf = SSD1306_GETPIXEL(x, y) ? RGB888(255, 255, 255) : RGB888(0, 0, 0);
}


void SSD1306Controller::rawDrawBitmap_Native(int destX, int destY, Bitmap const * bitmap, int X1, int Y1, int XCount, int YCount)
{
  genericRawDrawBitmap_Native(destX, destY, (uint8_t*) bitmap->data, bitmap->width, X1, Y1, XCount, YCount,
                                [&] (int y)                     { return y; },                         // rawGetRow
                                [&] (int y, int x, uint8_t src) { SSD1306_SETPIXELCOLOR(x, y, src); }  // rawSetPixelInRow
                               );
}


void SSD1306Controller::rawDrawBitmap_Mask(int destX, int destY, Bitmap const * bitmap, void * saveBackground, int X1, int Y1, int XCount, int YCount)
{
  uint8_t foregroundColor = RGB888toMono(bitmap->foregroundColor);
  genericRawDrawBitmap_Mask(destX, destY, bitmap, (uint8_t*)saveBackground, X1, Y1, XCount, YCount,
                            [&] (int y)        { return y; },                                     // rawGetRow
                            [&] (int y, int x) { return SSD1306_GETPIXEL(x, y); },                // rawGetPixelInRow
                            [&] (int y, int x) { SSD1306_SETPIXELCOLOR(x, y, foregroundColor); }  // rawSetPixelInRow
                           );
}


void SSD1306Controller::rawDrawBitmap_RGBA2222(int destX, int destY, Bitmap const * bitmap, void * saveBackground, int X1, int Y1, int XCount, int YCount)
{
  genericRawDrawBitmap_RGBA2222(destX, destY, bitmap, (uint8_t*)saveBackground, X1, Y1, XCount, YCount,
                                [&] (int y)                     { return y; },                                         // rawGetRow
                                [&] (int y, int x)              { return SSD1306_GETPIXEL(x, y); },                    // rawGetPixelInRow
                                [&] (int y, int x, uint8_t src) { SSD1306_SETPIXELCOLOR(x, y, RGBA2222toMono(src)); }  // rawSetPixelInRow
                               );
}


void SSD1306Controller::rawDrawBitmap_RGBA8888(int destX, int destY, Bitmap const * bitmap, void * saveBackground, int X1, int Y1, int XCount, int YCount)
{
  genericRawDrawBitmap_RGBA8888(destX, destY, bitmap, (uint8_t*)saveBackground, X1, Y1, XCount, YCount,
                                [&] (int y)                              { return y; },                                         // rawGetRow
                                [&] (int y, int x)                       { return SSD1306_GETPIXEL(x, y); },                    // rawGetPixelInRow
                                [&] (int y, int x, RGBA8888 const & src) { SSD1306_SETPIXELCOLOR(x, y, RGBA8888toMono(src)); }  // rawSetPixelInRow
                               );
}


void SSD1306Controller::swapBuffers()
{
  // nothing to do, we just send current view port to the device
  SSD1306_sendScreenBuffer(Rect(0, 0, getViewPortWidth() - 1, getViewPortHeight() - 1));
}




} // end of namespace



#endif // #ifdef ARDUINO
