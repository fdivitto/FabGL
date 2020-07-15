
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "soc/i2s_struct.h"
#include "soc/i2s_reg.h"
#include "driver/periph_ctrl.h"
#include "rom/lldesc.h"
#include "soc/rtc.h"

#include "fabutils.h"
#include "vgatextcontroller.h"
#include "devdrivers/swgenerator.h"




namespace fabgl {


// statics

volatile int VGATextController::s_scanLine;
uint32_t     VGATextController::s_blankPatternDWord;
uint32_t *   VGATextController::s_fgbgPattern = nullptr;



VGATextController::VGATextController()
  : m_charData(nullptr),
    m_map(nullptr),
    m_cursorEnabled(false),
    m_cursorCounter(0),
    m_cursorSpeed(20),
    m_cursorRow(0),
    m_cursorCol(0),
    m_cursorForeground(0),
    m_cursorBackground(15)
{
}


VGATextController::~VGATextController()
{
  free((void*)m_charData);
}


void VGATextController::setTextMap(uint32_t const * map)
{
  // wait for the end of frame
  while (m_map != nullptr && s_scanLine < m_timings.VVisibleArea)
    ;
  m_map = map;
}


void VGATextController::init(gpio_num_t VSyncGPIO)
{
  m_DMABuffers = nullptr;

  m_GPIOStream.begin();

  // load font into RAM
  int charDataSize = 256 * FONT_8x14.height * (FONT_8x14.width + 7) / 8;
  m_charData = (uint8_t*) heap_caps_malloc(charDataSize, MALLOC_CAP_8BIT);
  memcpy(m_charData, FONT_8x14.data, charDataSize);
}


// initializer for 8 colors configuration
void VGATextController::begin(gpio_num_t redGPIO, gpio_num_t greenGPIO, gpio_num_t blueGPIO, gpio_num_t HSyncGPIO, gpio_num_t VSyncGPIO)
{
  init(VSyncGPIO);

  // GPIO configuration for bit 0
  setupGPIO(redGPIO,   VGA_RED_BIT,   GPIO_MODE_OUTPUT);
  setupGPIO(greenGPIO, VGA_GREEN_BIT, GPIO_MODE_OUTPUT);
  setupGPIO(blueGPIO,  VGA_BLUE_BIT,  GPIO_MODE_OUTPUT);

  // GPIO configuration for VSync and HSync
  setupGPIO(HSyncGPIO, VGA_HSYNC_BIT, GPIO_MODE_OUTPUT);
  setupGPIO(VSyncGPIO, VGA_VSYNC_BIT, GPIO_MODE_INPUT_OUTPUT);  // input/output so can be generated interrupt on falling/rising edge

  RGB222::lowBitOnly = true;
  m_bitsPerChannel = 1;
}


// initializer for 64 colors configuration
void VGATextController::begin(gpio_num_t red1GPIO, gpio_num_t red0GPIO, gpio_num_t green1GPIO, gpio_num_t green0GPIO, gpio_num_t blue1GPIO, gpio_num_t blue0GPIO, gpio_num_t HSyncGPIO, gpio_num_t VSyncGPIO)
{
  begin(red0GPIO, green0GPIO, blue0GPIO, HSyncGPIO, VSyncGPIO);

  // GPIO configuration for bit 1
  setupGPIO(red1GPIO,   VGA_RED_BIT + 1,   GPIO_MODE_OUTPUT);
  setupGPIO(green1GPIO, VGA_GREEN_BIT + 1, GPIO_MODE_OUTPUT);
  setupGPIO(blue1GPIO,  VGA_BLUE_BIT + 1,  GPIO_MODE_OUTPUT);

  RGB222::lowBitOnly = false;
  m_bitsPerChannel = 2;
}


// initializer for default configuration
void VGATextController::begin()
{
  begin(GPIO_NUM_22, GPIO_NUM_21, GPIO_NUM_19, GPIO_NUM_18, GPIO_NUM_5, GPIO_NUM_4, GPIO_NUM_23, GPIO_NUM_15);
}


void VGATextController::setCursorForeground(Color value)
{
  m_cursorForeground = (int) value;
}


void VGATextController::setCursorBackground(Color value)
{
  m_cursorBackground = (int) value;
}


void VGATextController::setupGPIO(gpio_num_t gpio, int bit, gpio_mode_t mode)
{
  PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[gpio], PIN_FUNC_GPIO);
  gpio_set_direction(gpio, mode);
  gpio_matrix_out(gpio, I2S1O_DATA_OUT0_IDX + bit, false, false);
}


void VGATextController::setResolution(char const * modeline)
{
  VGATimings timings;
  if (VGAController::convertModelineToTimings(VGATextController_MODELINE, &timings))
    setResolution(timings);
}


void VGATextController::setResolution(VGATimings const& timings)
{
  if (m_DMABuffers) {
    m_GPIOStream.stop();
    freeBuffers();
  }

  m_timings = timings;

  m_HVSync = packHVSync(false, false);

  m_rawLineWidth   = m_timings.HFrontPorch + m_timings.HSyncPulse + m_timings.HBackPorch + m_timings.HVisibleArea;
  m_rawFrameHeight = m_timings.VVisibleArea + m_timings.VFrontPorch + m_timings.VSyncPulse + m_timings.VBackPorch;

  m_HSyncPos       = m_timings.HFrontPorch;
  m_HBackPorchPos  = m_HSyncPos + m_timings.HSyncPulse;
  m_HVisiblePos    = m_HBackPorchPos + m_timings.HBackPorch;

  m_DMABuffersCount = m_rawFrameHeight + m_timings.VVisibleArea;

  m_DMABuffers = (lldesc_t*) heap_caps_malloc(m_DMABuffersCount * sizeof(lldesc_t), MALLOC_CAP_DMA);

  for (int i = 0; i < VGATextController_LINES; ++i)
    m_lines[i] = (uint32_t*) heap_caps_malloc(m_timings.HVisibleArea, MALLOC_CAP_DMA);

  m_blankLine = (uint8_t*) heap_caps_malloc(m_rawLineWidth, MALLOC_CAP_DMA);
  m_syncLine  = (uint8_t*) heap_caps_malloc(m_rawLineWidth, MALLOC_CAP_DMA);

  // horiz: FRONTPORCH -> SYNC -> BACKPORCH -> VISIBLEAREA
  //
  // vert:  VISIBLEAREA
  //        FRONTPORCH
  //        SYNC
  //        BACKPORCH

  for (int i = 0, visLine = 0, invLine = 0; i < m_DMABuffersCount; ++i) {

    if (i < m_timings.VVisibleArea * 2) {

      // first part is the same of a blank line
      m_DMABuffers[i].eof          = (visLine == 0 || visLine == VGATextController_LINES / 2 ? 1 : 0);
      m_DMABuffers[i].sosf         = 0;
      m_DMABuffers[i].offset       = 0;
      m_DMABuffers[i].owner        = 1;
      m_DMABuffers[i].qe.stqe_next = (lldesc_t*) &m_DMABuffers[i + 1];
      m_DMABuffers[i].length       = m_timings.HFrontPorch + m_timings.HSyncPulse + m_timings.HBackPorch;
      m_DMABuffers[i].size         = (m_DMABuffers[i].length + 3) & (~3);
      m_DMABuffers[i].buf          = (uint8_t*) m_blankLine;

      ++i;

      // second part is the visible line
      m_DMABuffers[i].eof          = 0;
      m_DMABuffers[i].sosf         = 0;
      m_DMABuffers[i].offset       = 0;
      m_DMABuffers[i].owner        = 1;
      m_DMABuffers[i].qe.stqe_next = (lldesc_t*) &m_DMABuffers[i + 1];
      m_DMABuffers[i].length       = m_timings.HVisibleArea;
      m_DMABuffers[i].size         = (m_DMABuffers[i].length + 3) & (~3);
      m_DMABuffers[i].buf          = (uint8_t*) m_lines[visLine];

      ++visLine;
      if (visLine == VGATextController_LINES)
        visLine = 0;

    } else {

      // vertical porchs and sync

      bool frameResetDesc = (invLine == 0);

      if (frameResetDesc)
        m_frameResetDesc = &m_DMABuffers[i];

      m_DMABuffers[i].eof          = (frameResetDesc ? 1 : 0); // prepare for next frame
      m_DMABuffers[i].sosf         = 0;
      m_DMABuffers[i].offset       = 0;
      m_DMABuffers[i].owner        = 1;
      m_DMABuffers[i].qe.stqe_next = (lldesc_t*) (i ==  m_DMABuffersCount - 1 ? &m_DMABuffers[0] : &m_DMABuffers[i + 1]);
      m_DMABuffers[i].length       = m_rawLineWidth;
      m_DMABuffers[i].size         = (m_DMABuffers[i].length + 3) & (~3);

      if (invLine < m_timings.VFrontPorch || invLine >= m_timings.VFrontPorch + m_timings.VSyncPulse)
        m_DMABuffers[i].buf = (uint8_t*) m_blankLine;
      else
        m_DMABuffers[i].buf = (uint8_t*) m_syncLine;

      ++invLine;

    }
  }

  fillDMABuffers();

  s_scanLine = 0;

  s_blankPatternDWord = m_HVSync | (m_HVSync << 8) | (m_HVSync << 16) | (m_HVSync << 24);

  if (s_fgbgPattern == nullptr) {
    s_fgbgPattern = (uint32_t*) heap_caps_malloc(16384, MALLOC_CAP_8BIT);
    for (int i = 0; i < 16; ++i)
      for (int fg = 0; fg < 16; ++fg)
        for (int bg = 0; bg < 16; ++bg) {
          uint8_t fg_pat = preparePixel(RGB222((Color)fg));
          uint8_t bg_pat = preparePixel(RGB222((Color)bg));
          s_fgbgPattern[i | (bg << 4) | (fg << 8)] = (i & 0b1000 ? (fg_pat << 16) : (bg_pat << 16)) |
                                                     (i & 0b0100 ? (fg_pat << 24) : (bg_pat << 24)) |
                                                     (i & 0b0010 ? (fg_pat << 0) : (bg_pat << 0)) |
                                                     (i & 0b0001 ? (fg_pat << 8) : (bg_pat << 8));
        }
  }


  // ESP_INTR_FLAG_LEVEL1: should be less than PS2Controller interrupt level
  esp_intr_alloc_pinnedToCore(ETS_I2S1_INTR_SOURCE, ESP_INTR_FLAG_LEVEL1 | ESP_INTR_FLAG_IRAM, I2SInterrupt, this, &m_isr_handle, 1);

  m_GPIOStream.play(m_timings.frequency, m_DMABuffers);

  I2S1.int_clr.val     = 0xFFFFFFFF;
  I2S1.int_ena.out_eof = 1;
}


void VGATextController::freeBuffers()
{
  heap_caps_free( (void*) m_DMABuffers );
  for (int i = 0; i < VGATextController_LINES; ++i) {
    heap_caps_free((void*) m_lines[i]);
  }
  m_DMABuffers = nullptr;
  heap_caps_free((void*) m_blankLine);
  heap_caps_free((void*) m_syncLine);
}


uint8_t IRAM_ATTR VGATextController::packHVSync(bool HSync, bool VSync)
{
  uint8_t hsync_value = (m_timings.HSyncLogic == '+' ? (HSync ? 1 : 0) : (HSync ? 0 : 1));
  uint8_t vsync_value = (m_timings.VSyncLogic == '+' ? (VSync ? 1 : 0) : (VSync ? 0 : 1));
  return (vsync_value << VGA_VSYNC_BIT) | (hsync_value << VGA_HSYNC_BIT);
}


uint8_t IRAM_ATTR VGATextController::preparePixelWithSync(RGB222 rgb, bool HSync, bool VSync)
{
  return packHVSync(HSync, VSync) | (rgb.B << VGA_BLUE_BIT) | (rgb.G << VGA_GREEN_BIT) | (rgb.R << VGA_RED_BIT);
}


void VGATextController::fillDMABuffers()
{
  int x = 0;
  for (; x < m_timings.HFrontPorch; ++x) {
    VGA_PIXELINROW(m_blankLine, x) = preparePixelWithSync((RGB222){0, 0, 0}, false, false);
    VGA_PIXELINROW(m_syncLine, x)  = preparePixelWithSync((RGB222){0, 0, 0}, false, true);
  }
  for (; x < m_timings.HFrontPorch + m_timings.HSyncPulse; ++x) {
    VGA_PIXELINROW(m_blankLine, x) = preparePixelWithSync((RGB222){0, 0, 0}, true, false);
    VGA_PIXELINROW(m_syncLine, x)  = preparePixelWithSync((RGB222){0, 0, 0}, true, true);
  }
  for (; x < m_timings.HFrontPorch + m_timings.HSyncPulse + m_timings.HBackPorch; ++x) {
    VGA_PIXELINROW(m_blankLine, x) = preparePixelWithSync((RGB222){0, 0, 0}, false, false);
    VGA_PIXELINROW(m_syncLine, x)  = preparePixelWithSync((RGB222){0, 0, 0}, false, true);
  }
  for (int rx = 0; x < m_rawLineWidth; ++x, ++rx) {
    VGA_PIXELINROW(m_blankLine, x) = preparePixelWithSync((RGB222){0, 0, 0}, false, false);
    VGA_PIXELINROW(m_syncLine, x)  = preparePixelWithSync((RGB222){0, 0, 0}, false, true);
    for (int i = 0; i < VGATextController_LINES; ++i)
      VGA_PIXELINROW(((uint8_t*)(m_lines[i])), rx)  = preparePixelWithSync((RGB222){0, 0, 0}, false, false);
  }
}


void IRAM_ATTR VGATextController::I2SInterrupt(void * arg)
{
  VGATextController * ctrl = (VGATextController *) arg;

  if (I2S1.int_st.out_eof && ctrl->m_charData != nullptr) {

    auto desc = (volatile lldesc_t*)I2S1.out_eof_des_addr;

    if (desc == ctrl->m_frameResetDesc) {

      s_scanLine = 0;

      if (ctrl->m_cursorEnabled) {
        ++ctrl->m_cursorCounter;
        if (ctrl->m_cursorCounter >= ctrl->m_cursorSpeed)
          ctrl->m_cursorCounter = -ctrl->m_cursorSpeed;
      }

      if (ctrl->m_map == nullptr) {
        I2S1.int_clr.val = I2S1.int_st.val;
        return;
      }

    } else if (s_scanLine == 0) {
      // out of sync, wait for next frame
      I2S1.int_clr.val = I2S1.int_st.val;
      return;
    }

    int scanLine = s_scanLine;

    int cursorRow = 0, cursorCol = 0;
    int cursorFG = 0, cursorBG = 0;
    const auto cursorVisible = (ctrl->m_cursorEnabled && ctrl->m_cursorCounter >= 0);
    if (cursorVisible) {
      cursorRow = ctrl->m_cursorRow;
      cursorCol = ctrl->m_cursorCol;
      cursorFG  = ctrl->m_cursorForeground;
      cursorBG  = ctrl->m_cursorBackground;
    }

    constexpr int CHARWIDTHBYTES = (VGATextController_CHARWIDTH + 7) / 8;
    const auto charData = ctrl->m_charData;
    const auto map      = ctrl->m_map;

    for (int i = 0; i < VGATextController_LINES / 2; ++i) {

      auto dest = ctrl->m_lines[scanLine % VGATextController_LINES];

      int textRow = scanLine / VGATextController_CHARHEIGHT;

      if (textRow < VGATextController_ROWS) {

        const int rowInChar = scanLine % VGATextController_CHARHEIGHT;

        auto mapItemPtr = map + textRow * VGATextController_COLUMNS;

        for (int col = 0; col < VGATextController_COLUMNS; ++col, ++mapItemPtr) {

          const auto mapItem = *mapItemPtr;

          const auto options = glyphMapItem_getOptions(mapItem);

          if (options.blank) {

            *dest++ = s_blankPatternDWord;
            *dest++ = s_blankPatternDWord;

          } else {

            int fgbg = (mapItem >> 4) & 0b111111110000;

            // invert?
            if (options.invert) {
              fgbg = ((fgbg >> 4) & 0b11110000) | ((fgbg << 4) & 0b111100000000);
            }

            // cursor?
            if (cursorVisible && textRow == cursorRow && col == cursorCol) {
              fgbg = (cursorFG << 4) | (cursorBG << 8);
            }

            // max char width is 8
            uint8_t charRowData = charData[glyphMapItem_getIndex(mapItem) * VGATextController_CHARHEIGHT * CHARWIDTHBYTES + rowInChar * CHARWIDTHBYTES];

            // bold?
            if (options.bold) {
              charRowData |= charRowData >> 1;
            }

            // underline?
            if (options.underline && rowInChar == VGATextController_CHARHEIGHT - 1) {
              charRowData = 0xFF;
            }

            *dest++ = s_fgbgPattern[(charRowData >> 4)  | fgbg];
            *dest++ = s_fgbgPattern[(charRowData & 0xF) | fgbg];

          }

        }

      } else {
        for (int i = 0; i < 80; ++i) {
          *dest++ = s_blankPatternDWord;
          *dest++ = s_blankPatternDWord;
        }
      }

      ++scanLine;

    }

    s_scanLine = scanLine;

  }

  I2S1.int_clr.val = I2S1.int_st.val;
}





}
