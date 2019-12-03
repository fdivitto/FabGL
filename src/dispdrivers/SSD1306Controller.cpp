/*
  Created by Fabrizio Di Vittorio (fdivitto2013@gmail.com) - <http://www.fabgl.com>
  Copyright (c) 2019 Fabrizio Di Vittorio.
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
#include "SSD1306Controller.h"




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


SSD1306Controller::SSD1306Controller()
  : m_i2c(nullptr),
    m_screenBuffer(nullptr),
    m_altScreenBuffer(nullptr),
    m_updateTaskHandle(nullptr)
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


void SSD1306Controller::end()
{
  if (m_updateTaskHandle)
    vTaskDelete(m_updateTaskHandle);
  m_updateTaskHandle = nullptr;

  free(m_screenBuffer);
  m_screenBuffer = nullptr;

  free(m_altScreenBuffer);
  m_altScreenBuffer = nullptr;
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
  m_screenCol = iclamp(value, 0, m_viewPortWidth - m_screenWidth);
  addPrimitive(Primitive(PrimitiveCmd::Refresh));
}


void SSD1306Controller::setScreenRow(int value)
{
  m_screenRow = iclamp(value, 0, m_viewPortHeight - m_screenHeight);
  addPrimitive(Primitive(PrimitiveCmd::Refresh));
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
  if (m_resetGPIO != GPIO_NUM_39) {
    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[m_resetGPIO], PIN_FUNC_GPIO);
    gpio_set_direction(m_resetGPIO, GPIO_MODE_OUTPUT);
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
  SSD1306_sendCmd(SSD1306_MEMORYMODE, 0);
  SSD1306_sendCmd(SSD1306_SEGREMAP | 0x1);
  SSD1306_sendCmd(SSD1306_COMSCANDEC);
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


void SSD1306Controller::SSD1306_sendScreenBuffer()
{
  // sending a row at the time reduces sending errors on wifi interrupt (when wifi is enabled)
  for (int y = 0; y < m_screenHeight; y += 8) {
    bool r = SSD1306_sendCmd(SSD1306_PAGEADDR, y >> 3, y >> 3) && SSD1306_sendCmd(SSD1306_COLUMNADDR, 0, m_screenWidth - 1);
    if (!r)
      break;
    SSD1306_sendData(m_screenBuffer + m_screenCol + ((y + m_screenRow) >> 3) * m_viewPortWidth, m_screenWidth, 0x40);
  }
}


void SSD1306Controller::allocScreenBuffer()
{
  m_screenBuffer = (uint8_t*) malloc(m_viewPortWidth * m_viewPortHeight / 8);
  memset(m_screenBuffer, 0, m_viewPortWidth * m_viewPortHeight / 8);

  if (isDoubleBuffered()) {
    m_altScreenBuffer = (uint8_t*) malloc(m_viewPortWidth * m_viewPortHeight / 8);
    memset(m_altScreenBuffer, 0, m_viewPortWidth * m_viewPortHeight / 8);
  }
}


void SSD1306Controller::updateTaskFunc(void * pvParameters)
{
  SSD1306Controller * ctrl = (SSD1306Controller*) pvParameters;

  while (true) {

    // primitive processing blocked?
    if (ctrl->m_updateTaskFuncSuspended > 0)
      ulTaskNotifyTake(true, portMAX_DELAY); // yes, wait for a notify

    ctrl->waitForPrimitives();

    int64_t startTime = ctrl->backgroundPrimitiveTimeoutEnabled() ? esp_timer_get_time() : 0;
    do {

      Primitive prim;
      if (ctrl->getPrimitive(&prim) == false)
        break;

      ctrl->execPrimitive(prim);

    } while (!ctrl->backgroundPrimitiveTimeoutEnabled() || (startTime + SSD1306_BACKGROUND_PRIMITIVE_TIMEOUT > esp_timer_get_time()));

    ctrl->showSprites();

    ctrl->SSD1306_sendScreenBuffer();
  }
}



void SSD1306Controller::suspendBackgroundPrimitiveExecution()
{
  ++m_updateTaskFuncSuspended;
}


void SSD1306Controller::resumeBackgroundPrimitiveExecution()
{
  m_updateTaskFuncSuspended = tmax(0, m_updateTaskFuncSuspended - 1);
  if (m_updateTaskFuncSuspended == 0)
    xTaskNotifyGive(m_updateTaskHandle);  // resume updateTaskFunc()
}


uint8_t SSD1306Controller::preparePixel(RGB888 const & rgb)
{
  return RGB888toMono(rgb);
}


void SSD1306Controller::setPixelAt(PixelDesc const & pixelDesc)
{
  hideSprites();
  uint8_t pattern = preparePixel(pixelDesc.color);

  const int x = pixelDesc.pos.X + paintState().origin.X;
  const int y = pixelDesc.pos.Y + paintState().origin.Y;

  const int clipX1 = paintState().absClippingRect.X1;
  const int clipY1 = paintState().absClippingRect.Y1;
  const int clipX2 = paintState().absClippingRect.X2;
  const int clipY2 = paintState().absClippingRect.Y2;

  if (x >= clipX1 && x <= clipX2 && y >= clipY1 && y <= clipY2)
    SSD1306_SETPIXELCOLOR(x, y, pattern);
}


// coordinates are absolute values (not relative to origin)
// line clipped on current absolute clipping rectangle
void SSD1306Controller::drawLine(int X1, int Y1, int X2, int Y2, RGB888 color)
{
  uint8_t pattern = preparePixel(color);

  if (Y1 == Y2) {
    // horizontal line
    if (Y1 < paintState().absClippingRect.Y1 || Y1 > paintState().absClippingRect.Y2)
      return;
    if (X1 > X2)
      tswap(X1, X2);
    if (X1 > paintState().absClippingRect.X2 || X2 < paintState().absClippingRect.X1)
      return;
    X1 = iclamp(X1, paintState().absClippingRect.X1, paintState().absClippingRect.X2);
    X2 = iclamp(X2, paintState().absClippingRect.X1, paintState().absClippingRect.X2);
    if (paintState().paintOptions.NOT) {
      for (int x = X1; x <= X2; ++x)
        SSD1306_INVERTPIXEL(x, Y1);
    } else if (pattern) {
      for (int x = X1; x <= X2; ++x)
        SSD1306_SETPIXEL(x, Y1);
    } else {
      for (int x = X1; x <= X2; ++x)
        SSD1306_CLEARPIXEL(x, Y1);
    }
  } else if (X1 == X2) {
    // vertical line
    if (X1 < paintState().absClippingRect.X1 || X1 > paintState().absClippingRect.X2)
      return;
    if (Y1 > Y2)
      tswap(Y1, Y2);
    if (Y1 > paintState().absClippingRect.Y2 || Y2 < paintState().absClippingRect.Y1)
      return;
    Y1 = iclamp(Y1, paintState().absClippingRect.Y1, paintState().absClippingRect.Y2);
    Y2 = iclamp(Y2, paintState().absClippingRect.Y1, paintState().absClippingRect.Y2);
    if (paintState().paintOptions.NOT) {
      for (int y = Y1; y <= Y2; ++y) {
        SSD1306_INVERTPIXEL(X1, y);
      }
    } else if (pattern) {
      for (int y = Y1; y <= Y2; ++y)
        SSD1306_SETPIXEL(X1, y);
    } else {
      for (int y = Y1; y <= Y2; ++y)
        SSD1306_CLEARPIXEL(X1, y);
    }
  } else {
    // other cases (Bresenham's algorithm)
    // TODO: to optimize
    //   Unfortunately here we cannot clip exactly using Sutherland-Cohen algorithm (as done before)
    //   because the starting line (got from clipping algorithm) may not be the same of Bresenham's
    //   line (think to continuing an existing line).
    //   Possible solutions:
    //      - "Yevgeny P. Kuzmin" algorithm:
    //               https://stackoverflow.com/questions/40884680/how-to-use-bresenhams-line-drawing-algorithm-with-clipping
    //               https://github.com/ktfh/ClippedLine/blob/master/clip.hpp
    // For now Sutherland-Cohen algorithm is only used to check the line is actually visible,
    // then test for every point inside the main Bresenham's loop.
    if (!clipLine(X1, Y1, X2, Y2, paintState().absClippingRect, true))  // true = do not change line coordinates!
      return;
    const int dx = abs(X2 - X1);
    const int dy = abs(Y2 - Y1);
    const int sx = X1 < X2 ? 1 : -1;
    const int sy = Y1 < Y2 ? 1 : -1;
    int err = (dx > dy ? dx : -dy) / 2;
    while (true) {
      if (paintState().absClippingRect.contains(X1, Y1)) {
        if (paintState().paintOptions.NOT)
          SSD1306_INVERTPIXEL(X1, Y1);
        else if (pattern)
          SSD1306_SETPIXEL(X1, Y1);
        else
          SSD1306_CLEARPIXEL(X1, Y1);
      }
      if (X1 == X2 && Y1 == Y2)
        break;
      int e2 = err;
      if (e2 > -dx) {
        err -= dy;
        X1 += sx;
      }
      if (e2 < dy) {
        err += dx;
        Y1 += sy;
      }
    }
  }
}


// parameters not checked
void SSD1306Controller::fillRow(int y, int x1, int x2, RGB888 color)
{
  uint8_t pattern = preparePixel(color);
  if (pattern) {
    for (; x1 <= x2; ++x1)
      SSD1306_SETPIXEL(x1, y);
  } else {
    for (; x1 <= x2; ++x1)
      SSD1306_CLEARPIXEL(x1, y);
  }
}


void SSD1306Controller::drawEllipse(Size const & size)
{
  hideSprites();
  uint8_t pattern = paintState().paintOptions.swapFGBG ? preparePixel(paintState().brushColor) : preparePixel(paintState().penColor);

  const int clipX1 = paintState().absClippingRect.X1;
  const int clipY1 = paintState().absClippingRect.Y1;
  const int clipX2 = paintState().absClippingRect.X2;
  const int clipY2 = paintState().absClippingRect.Y2;

  int x0 = paintState().position.X - size.width / 2;
  int y0 = paintState().position.Y - size.height / 2;
  int x1 = paintState().position.X + size.width / 2;
  int y1 = paintState().position.Y + size.height / 2;

  int a = abs (x1 - x0), b = abs (y1 - y0), b1 = b & 1;
  int dx = 4 * (1 - a) * b * b, dy = 4 * (b1 + 1) * a * a;
  int err = dx + dy + b1 * a * a, e2;

  if (x0 > x1) {
    x0 = x1;
    x1 += a;
  }
  if (y0 > y1)
    y0 = y1;
  y0 += (b + 1) / 2;
  y1 = y0-b1;
  a *= 8 * a;
  b1 = 8 * b * b;
  do {
    if (y0 >= clipY1 && y0 <= clipY2) {
      if (x1 >= clipX1 && x1 <= clipX2) {
        // bottom-right semicircle
        SSD1306_SETPIXELCOLOR(x1, y0, pattern);
      }
      if (x0 >= clipX1 && x0 <= clipX2) {
        // bottom-left semicircle
        SSD1306_SETPIXELCOLOR(x0, y0, pattern);
      }
    }
    if (y1 >= clipY1 && y1 <= clipY2) {
      if (x0 >= clipX1 && x0 <= clipX2) {
        // top-left semicircle
        SSD1306_SETPIXELCOLOR(x0, y1, pattern);
      }
      if (x1 >= clipX1 && x1 <= clipX2) {
        // top-right semicircle
        SSD1306_SETPIXELCOLOR(x1, y1, pattern);
      }
    }
    e2 = 2 * err;
    if (e2 >= dx) {
      ++x0;
      --x1;
      err += dx += b1;
    }
    if (e2 <= dy) {
      ++y0;
      --y1;
      err += dy += a;
    }
  } while (x0 <= x1);

  while (y0 - y1 < b) {
    int x = x0 - 1;
    int y = y0;
    if (x >= clipX1 && x <= clipX2 && y >= clipY1 && y <= clipY2)
      SSD1306_SETPIXELCOLOR(x, y, pattern);

    x = x1 + 1;
    y = y0++;
    if (x >= clipX1 && x <= clipX2 && y >= clipY1 && y <= clipY2)
      SSD1306_SETPIXELCOLOR(x, y, pattern);

    x = x0 - 1;
    y = y1;
    if (x >= clipX1 && x <= clipX2 && y >= clipY1 && y <= clipY2)
      SSD1306_SETPIXELCOLOR(x, y, pattern);

    x = x1 + 1;
    y = y1--;
    if (x >= clipX1 && x <= clipX2 && y >= clipY1 && y <= clipY2)
      SSD1306_SETPIXELCOLOR(x, y, pattern);
  }
}


void SSD1306Controller::clear()
{
  hideSprites();
  uint8_t pattern = paintState().paintOptions.swapFGBG ? preparePixel(paintState().penColor) : preparePixel(paintState().brushColor);
  memset(m_screenBuffer, (pattern ? 255 : 0), m_viewPortWidth * m_viewPortHeight / 8);
}


// scroll < 0 -> scroll UP
// scroll > 0 -> scroll DOWN
void SSD1306Controller::VScroll(int scroll)
{
  hideSprites();
  RGB888 color = paintState().paintOptions.swapFGBG ? paintState().penColor : paintState().brushColor;
  int Y1 = paintState().scrollingRegion.Y1;
  int Y2 = paintState().scrollingRegion.Y2;
  int X1 = paintState().scrollingRegion.X1;
  int X2 = paintState().scrollingRegion.X2;
  int height = Y2 - Y1 + 1;

  if (scroll < 0) {

    // scroll UP

    for (int i = 0; i < height + scroll; ++i) {
      // copy X1..X2 of (Y1 + i - scroll) to (Y1 + i)
      copyRow(X1, X2, (Y1 + i - scroll), (Y1 + i));
    }
    // fill lower area with brush color
    for (int i = height + scroll; i < height; ++i)
      fillRow(Y1 + i, X1, X2, color);

  } else if (scroll > 0) {

    // scroll DOWN
    for (int i = height - scroll - 1; i >= 0; --i) {
      // copy X1..X2 of (Y1 + i) to (Y1 + i + scroll)
      copyRow(X1, X2, (Y1 + i), (Y1 + i + scroll));
    }

    // fill upper area with brush color
    for (int i = 0; i < scroll; ++i)
      fillRow(Y1 + i, X1, X2, color);

  }
}


// scroll < 0 -> scroll LEFT
// scroll > 0 -> scroll RIGHT
void SSD1306Controller::HScroll(int scroll)
{
  hideSprites();
  uint8_t pattern = paintState().paintOptions.swapFGBG ? preparePixel(paintState().penColor) : preparePixel(paintState().brushColor);

  int Y1 = paintState().scrollingRegion.Y1;
  int Y2 = paintState().scrollingRegion.Y2;
  int X1 = paintState().scrollingRegion.X1;
  int X2 = paintState().scrollingRegion.X2;

  if (scroll < 0) {
    // scroll left
    for (int y = Y1; y <= Y2; ++y) {
      for (int x = X1; x <= X2 + scroll; ++x) {
        uint8_t c = SSD1306_GETPIXEL(x - scroll, y);
        SSD1306_SETPIXELCOLOR(x, y, c);
      }
      // fill right area with brush color
      for (int x = X2 + 1 + scroll; x <= X2; ++x)
        SSD1306_SETPIXELCOLOR(x, y, pattern);
    }
  } else if (scroll > 0) {
    // scroll right
    for (int y = Y1; y <= Y2; ++y) {
      for (int x = X2 - scroll; x >= X1; --x) {
        uint8_t c = SSD1306_GETPIXEL(x, y);
        SSD1306_SETPIXELCOLOR(x + scroll, y, c);
      }
      // fill left area with brush color
      for (int x = X1; x < X1 + scroll; ++x)
        SSD1306_SETPIXELCOLOR(x, y, pattern);
    }

  }
}


void SSD1306Controller::drawGlyph(Glyph const & glyph, GlyphOptions glyphOptions, RGB888 penColor, RGB888 brushColor)
{
  hideSprites();
  if (!glyphOptions.bold && !glyphOptions.italic && !glyphOptions.blank && !glyphOptions.underline && !glyphOptions.doubleWidth && glyph.width <= 32)
    drawGlyph_light(glyph, glyphOptions, penColor, brushColor);
  else
    drawGlyph_full(glyph, glyphOptions, penColor, brushColor);
}


// TODO: Italic doesn't work well when clipping rect is specified
void SSD1306Controller::drawGlyph_full(Glyph const & glyph, GlyphOptions glyphOptions, RGB888 penColor, RGB888 brushColor)
{
  const int clipX1 = paintState().absClippingRect.X1;
  const int clipY1 = paintState().absClippingRect.Y1;
  const int clipX2 = paintState().absClippingRect.X2;
  const int clipY2 = paintState().absClippingRect.Y2;

  const int origX = paintState().origin.X;
  const int origY = paintState().origin.Y;

  const int glyphX = glyph.X + origX;
  const int glyphY = glyph.Y + origY;

  if (glyphX > clipX2 || glyphY > clipY2)
    return;

  int16_t glyphWidth        = glyph.width;
  int16_t glyphHeight       = glyph.height;
  uint8_t const * glyphData = glyph.data;
  int16_t glyphWidthByte    = (glyphWidth + 7) / 8;
  int16_t glyphSize         = glyphHeight * glyphWidthByte;

  bool fillBackground = glyphOptions.fillBackground;
  bool bold           = glyphOptions.bold;
  bool italic         = glyphOptions.italic;
  bool blank          = glyphOptions.blank;
  bool underline      = glyphOptions.underline;
  int doubleWidth     = glyphOptions.doubleWidth;

  // modify glyph to handle top half and bottom half double height
  // doubleWidth = 1 is handled directly inside drawing routine
  if (doubleWidth > 1) {
    uint8_t * newGlyphData = (uint8_t*) alloca(glyphSize);
    // doubling top-half or doubling bottom-half?
    int offset = (doubleWidth == 2 ? 0 : (glyphHeight >> 1));
    for (int y = 0; y < glyphHeight ; ++y)
      for (int x = 0; x < glyphWidthByte; ++x)
        newGlyphData[x + y * glyphWidthByte] = glyphData[x + (offset + (y >> 1)) * glyphWidthByte];
    glyphData = newGlyphData;
  }

  // a very simple and ugly skew (italic) implementation!
  int skewAdder = 0, skewH1 = 0, skewH2 = 0;
  if (italic) {
    skewAdder = 2;
    skewH1 = glyphHeight / 3;
    skewH2 = skewH1 * 2;
  }

  int16_t X1 = 0;
  int16_t XCount = glyphWidth;
  int16_t destX = glyphX;

  if (destX < clipX1) {
    X1 = (clipX1 - destX) / (doubleWidth ? 2 : 1);
    destX = clipX1;
  }
  if (X1 >= glyphWidth)
    return;

  if (destX + XCount + skewAdder > clipX2 + 1)
    XCount = clipX2 + 1 - destX - skewAdder;
  if (X1 + XCount > glyphWidth)
    XCount = glyphWidth - X1;

  int16_t Y1 = 0;
  int16_t YCount = glyphHeight;
  int destY = glyphY;

  if (destY < clipY1) {
    Y1 = clipY1 - destY;
    destY = clipY1;
  }
  if (Y1 >= glyphHeight)
    return;

  if (destY + YCount > clipY2 + 1)
    YCount = clipY2 + 1 - destY;
  if (Y1 + YCount > glyphHeight)
    YCount = glyphHeight - Y1;

  if (glyphOptions.invert ^ paintState().paintOptions.swapFGBG)
    tswap(penColor, brushColor);

  // a very simple and ugly reduce luminosity (faint) implementation!
  if (glyphOptions.reduceLuminosity) {
    if (penColor.R > 128) penColor.R = 128;
    if (penColor.G > 128) penColor.G = 128;
    if (penColor.B > 128) penColor.B = 128;
  }

  uint8_t penPattern   = preparePixel(penColor);
  uint8_t brushPattern = preparePixel(brushColor);

  for (int y = Y1; y < Y1 + YCount; ++y, ++destY) {

    // true if previous pixel has been set
    bool prevSet = false;

    uint8_t const * srcrow = glyphData + y * glyphWidthByte;

    if (underline && y == glyphHeight - FABGLIB_UNDERLINE_POSITION - 1) {

      for (int x = X1, adestX = destX + skewAdder; x < X1 + XCount && adestX <= clipX2; ++x, ++adestX) {
        uint8_t c = blank ? brushPattern : penPattern;
        SSD1306_SETPIXELCOLOR(adestX, destY, c);
        if (doubleWidth) {
          ++adestX;
          if (adestX > clipX2)
            break;
          SSD1306_SETPIXELCOLOR(adestX, destY, c);
        }
      }

    } else {

      for (int x = X1, adestX = destX + skewAdder; x < X1 + XCount && adestX <= clipX2; ++x, ++adestX) {
        if ((srcrow[x >> 3] << (x & 7)) & 0x80 && !blank) {
          SSD1306_SETPIXELCOLOR(adestX, destY, penPattern);
          prevSet = true;
        } else if (bold && prevSet) {
          SSD1306_SETPIXELCOLOR(adestX, destY, penPattern);
          prevSet = false;
        } else if (fillBackground) {
          SSD1306_SETPIXELCOLOR(adestX, destY, brushPattern);
          prevSet = false;
        } else {
          prevSet = false;
        }
        if (doubleWidth) {
          ++adestX;
          if (adestX > clipX2)
            break;
          if (fillBackground) {
            uint8_t c = prevSet ? penPattern : brushPattern;
            SSD1306_SETPIXELCOLOR(adestX, destY, c);
          } else if (prevSet) {
            SSD1306_SETPIXELCOLOR(adestX, destY, penPattern);
          }
        }
      }

    }

    if (italic && (y == skewH1 || y == skewH2))
      --skewAdder;

  }
}


// assume:
//   glyph.width <= 32
//   glyphOptions.fillBackground = 0 or 1
//   glyphOptions.invert : 0 or 1
//   glyphOptions.reduceLuminosity: 0 or 1
//   glyphOptions.... others = 0
//   paintState().paintOptions.swapFGBG: 0 or 1
void SSD1306Controller::drawGlyph_light(Glyph const & glyph, GlyphOptions glyphOptions, RGB888 penColor, RGB888 brushColor)
{
  const int clipX1 = paintState().absClippingRect.X1;
  const int clipY1 = paintState().absClippingRect.Y1;
  const int clipX2 = paintState().absClippingRect.X2;
  const int clipY2 = paintState().absClippingRect.Y2;

  const int origX = paintState().origin.X;
  const int origY = paintState().origin.Y;

  const int glyphX = glyph.X + origX;
  const int glyphY = glyph.Y + origY;

  if (glyphX > clipX2 || glyphY > clipY2)
    return;

  int16_t glyphWidth        = glyph.width;
  int16_t glyphHeight       = glyph.height;
  uint8_t const * glyphData = glyph.data;
  int16_t glyphWidthByte    = (glyphWidth + 7) / 8;

  int16_t X1 = 0;
  int16_t XCount = glyphWidth;
  int16_t destX = glyphX;

  int16_t Y1 = 0;
  int16_t YCount = glyphHeight;
  int destY = glyphY;

  if (destX < clipX1) {
    X1 = clipX1 - destX;
    destX = clipX1;
  }
  if (X1 >= glyphWidth)
    return;

  if (destX + XCount > clipX2 + 1)
    XCount = clipX2 + 1 - destX;
  if (X1 + XCount > glyphWidth)
    XCount = glyphWidth - X1;

  if (destY < clipY1) {
    Y1 = clipY1 - destY;
    destY = clipY1;
  }
  if (Y1 >= glyphHeight)
    return;

  if (destY + YCount > clipY2 + 1)
    YCount = clipY2 + 1 - destY;
  if (Y1 + YCount > glyphHeight)
    YCount = glyphHeight - Y1;

  if (glyphOptions.invert ^ paintState().paintOptions.swapFGBG)
    tswap(penColor, brushColor);

  // a very simple and ugly reduce luminosity (faint) implementation!
  if (glyphOptions.reduceLuminosity) {
    if (penColor.R > 128) penColor.R = 128;
    if (penColor.G > 128) penColor.G = 128;
    if (penColor.B > 128) penColor.B = 128;
  }

  bool fillBackground = glyphOptions.fillBackground;

  uint8_t penPattern   = preparePixel(penColor);
  uint8_t brushPattern = preparePixel(brushColor);

  for (int y = Y1; y < Y1 + YCount; ++y, ++destY) {
    uint8_t const * srcrow = glyphData + y * glyphWidthByte;

    uint32_t src = (srcrow[0] << 24) | (srcrow[1] << 16) | (srcrow[2] << 8) | (srcrow[3]);
    src <<= X1;
    if (fillBackground) {
      // filled background
      for (int x = X1, adestX = destX; x < X1 + XCount; ++x, ++adestX, src <<= 1) {
        uint8_t c = src & 0x80000000 ? penPattern : brushPattern;
        SSD1306_SETPIXELCOLOR(adestX, destY, c);
      }
    } else {
      // transparent background
      for (int x = X1, adestX = destX; x < X1 + XCount; ++x, ++adestX, src <<= 1)
        if (src & 0x80000000)
          SSD1306_SETPIXELCOLOR(adestX, destY, penPattern);
    }
  }
}


void SSD1306Controller::copyRow(int x1, int x2, int srcY, int dstY)
{
  for (; x1 <= x2; ++x1) {
    uint8_t c = SSD1306_GETPIXEL(x1, srcY);
    SSD1306_SETPIXELCOLOR(x1, dstY, c);
  }
}


void SSD1306Controller::invertRect(Rect const & rect)
{
  hideSprites();

  const int origX = paintState().origin.X;
  const int origY = paintState().origin.Y;

  const int clipX1 = paintState().absClippingRect.X1;
  const int clipY1 = paintState().absClippingRect.Y1;
  const int clipX2 = paintState().absClippingRect.X2;
  const int clipY2 = paintState().absClippingRect.Y2;

  const int x1 = iclamp(rect.X1 + origX, clipX1, clipX2);
  const int y1 = iclamp(rect.Y1 + origY, clipY1, clipY2);
  const int x2 = iclamp(rect.X2 + origX, clipX1, clipX2);
  const int y2 = iclamp(rect.Y2 + origY, clipY1, clipY2);

  for (int y = y1; y <= y2; ++y)
    for (int x = x1; x <= x2; ++x)
      SSD1306_INVERTPIXEL(x, y);
}


void SSD1306Controller::swapFGBG(Rect const & rect)
{
  invertRect(rect);
}


// supports overlapping of source and dest rectangles
void SSD1306Controller::copyRect(Rect const & source)
{
  hideSprites();

  const int clipX1 = paintState().absClippingRect.X1;
  const int clipY1 = paintState().absClippingRect.Y1;
  const int clipX2 = paintState().absClippingRect.X2;
  const int clipY2 = paintState().absClippingRect.Y2;

  int origX = paintState().origin.X;
  int origY = paintState().origin.Y;

  int srcX = source.X1 + origX;
  int srcY = source.Y1 + origY;
  int width  = source.X2 - source.X1 + 1;
  int height = source.Y2 - source.Y1 + 1;
  int destX = paintState().position.X;
  int destY = paintState().position.Y;
  int deltaX = destX - srcX;
  int deltaY = destY - srcY;

  int incX = deltaX < 0 ? 1 : -1;
  int incY = deltaY < 0 ? 1 : -1;

  int startX = deltaX < 0 ? destX : destX + width - 1;
  int startY = deltaY < 0 ? destY : destY + height - 1;

  for (int y = startY, i = 0; i < height; y += incY, ++i) {
    if (y >= clipY1 && y <= clipY2) {
      for (int x = startX, j = 0; j < width; x += incX, ++j) {
        if (x >= clipX1 && x <= clipX2) {
          uint8_t c = SSD1306_GETPIXEL(x - deltaX, y - deltaY);
          SSD1306_SETPIXELCOLOR(x, y, c);
        }
      }
    }
  }
}


// no bounds check is done!
void SSD1306Controller::readScreen(Rect const & rect, RGB888 * destBuf)
{
  for (int y = rect.Y1; y <= rect.Y2; ++y)
    for (int x = rect.X1; x <= rect.X2; ++x, ++destBuf)
      *destBuf = SSD1306_GETPIXEL(x, y) ? RGB888(255, 255, 255) : RGB888(0, 0, 0);
}


void SSD1306Controller::drawBitmap_Mask(int destX, int destY, Bitmap const * bitmap, uint8_t * saveBackground, int X1, int Y1, int XCount, int YCount)
{
  const int width = bitmap->width;

  if (saveBackground) {

    // save background and draw the bitmap
    // sprites background is saved using one byte per pixel (to improve performance):
    //    0 = not saved (bitmap is transparent here)
    //    0xc0 = saved, black
    //    0xc1 = saved, white
    uint8_t foregroundColor = RGB888toMono(bitmap->foregroundColor);
    uint8_t const * data = bitmap->data;
    int rowlen = (width + 7) / 8;
    for (int y = Y1, adestY = destY; y < Y1 + YCount; ++y, ++adestY) {
      uint8_t * savePx = saveBackground + y * width + X1;
      uint8_t const * src = data + y * rowlen;
      for (int x = X1, adestX = destX; x < X1 + XCount; ++x, ++adestX, ++savePx) {
        if ((src[x >> 3] << (x & 7)) & 0x80) {
          *savePx = 0xc0 | SSD1306_GETPIXEL(adestX, adestY);
          SSD1306_SETPIXELCOLOR(adestX, adestY, foregroundColor);
        } else {
          *savePx = 0;
        }
      }
    }

  } else {

    // just draw the bitmap
    uint8_t foregroundColor = RGB888toMono(bitmap->foregroundColor);
    uint8_t const * data = bitmap->data;
    int rowlen = (bitmap->width + 7) / 8;
    for (int y = Y1, adestY = destY; y < Y1 + YCount; ++y, ++adestY) {
      uint8_t const * src = data + y * rowlen;
      for (int x = X1, adestX = destX; x < X1 + XCount; ++x, ++adestX) {
        if ((src[x >> 3] << (x & 7)) & 0x80)
          SSD1306_SETPIXELCOLOR(adestX, adestY, foregroundColor);
      }
    }

  }
}


void SSD1306Controller::drawBitmap_RGBA2222(int destX, int destY, Bitmap const * bitmap, uint8_t * saveBackground, int X1, int Y1, int XCount, int YCount)
{
  const int width = bitmap->width;

  if (saveBackground) {

    // save background and draw the bitmap
    // sprites background is saved using one byte per pixel (to improve performance):
    //    0 = not saved (bitmap is transparent here)
    //    0xc0 = saved, black
    //    0xc1 = saved, white
    uint8_t const * data = bitmap->data;
    for (int y = Y1, adestY = destY; y < Y1 + YCount; ++y, ++adestY) {
      uint8_t * savePx = saveBackground + y * width + X1;
      uint8_t const * src = data + y * width + X1;
      for (int x = X1, adestX = destX; x < X1 + XCount; ++x, ++adestX, ++savePx, ++src) {
        int alpha = *src >> 6;  // TODO?, alpha blending
        if (alpha) {
          *savePx = 0xc0 | SSD1306_GETPIXEL(adestX, adestY);
          SSD1306_SETPIXELCOLOR(adestX, adestY, RGBA2222toMono(*src));
        } else {
          *savePx = 0;
        }
      }
    }

  } else {

    // just draw the bitmap
    uint8_t const * data = bitmap->data;
    for (int y = Y1, adestY = destY; y < Y1 + YCount; ++y, ++adestY) {
      uint8_t const * src = data + y * width + X1;
      for (int x = X1, adestX = destX; x < X1 + XCount; ++x, ++adestX, ++src) {
        int alpha = *src >> 6;  // TODO?, alpha blending
        if (alpha)
          SSD1306_SETPIXELCOLOR(adestX, adestY, RGBA2222toMono(*src));
      }
    }

  }
}


void SSD1306Controller::drawBitmap_RGBA8888(int destX, int destY, Bitmap const * bitmap, uint8_t * saveBackground, int X1, int Y1, int XCount, int YCount)
{
  const int width = bitmap->width;

  if (saveBackground) {

    // save background and draw the bitmap
    // sprites background is saved using one byte per pixel (to improve performance):
    //    0 = not saved (bitmap is transparent here)
    //    0xc0 = saved, black
    //    0xc1 = saved, white
    RGBA8888 const * data = (RGBA8888 const *) bitmap->data;
    for (int y = Y1, adestY = destY; y < Y1 + YCount; ++y, ++adestY) {
      uint8_t * savePx = saveBackground + y * width + X1;
      RGBA8888 const * src = data + y * width + X1;
      for (int x = X1, adestX = destX; x < X1 + XCount; ++x, ++adestX, ++savePx, ++src) {
        if (src->A) {
          *savePx = 0xc0 | SSD1306_GETPIXEL(adestX, adestY);
          SSD1306_SETPIXELCOLOR(adestX, adestY, RGBA8888toMono(*src));
        } else {
          *savePx = 0;
        }
      }
    }

  } else {

    // just draw the bitmap
    RGBA8888 const * data = (RGBA8888 const *) bitmap->data;
    for (int y = Y1, adestY = destY; y < Y1 + YCount; ++y, ++adestY) {
      RGBA8888 const * src = data + y * width + X1;
      for (int x = X1, adestX = destX; x < X1 + XCount; ++x, ++adestX, ++src) {
        if (src->A)
          SSD1306_SETPIXELCOLOR(adestX, adestY, RGBA8888toMono(*src));
      }
    }

  }
}


void SSD1306Controller::swapBuffers()
{
  tswap(m_screenBuffer, m_altScreenBuffer);
}




} // end of namespace
