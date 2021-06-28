
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "soc/i2s_struct.h"
#include "soc/i2s_reg.h"
#include "driver/periph_ctrl.h"
#include "soc/rtc.h"

#include "fabutils.h"
#include "vgatextcontroller.h"
#include "devdrivers/swgenerator.h"



#pragma GCC optimize ("O2")


namespace fabgl {


// statics

volatile int        VGATextController::s_scanLine;
uint32_t            VGATextController::s_blankPatternDWord;
uint32_t *          VGATextController::s_fgbgPattern = nullptr;
int                 VGATextController::s_textRow;
bool                VGATextController::s_upperRow;
lldesc_t volatile * VGATextController::s_frameResetDesc;

#if FABGLIB_VGAXCONTROLLER_PERFORMANCE_CHECK
  volatile uint64_t s_vgatxtcycles = 0;
#endif





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


void VGATextController::setTextMap(uint32_t const * map, int rows)
{
  // wait for the end of frame
  while (m_map != nullptr && s_scanLine < m_timings.VVisibleArea)
    ;
  m_rows = rows;
  m_map  = map;
}


void VGATextController::adjustMapSize(int * columns, int * rows)
{
  if (*columns > 0)
    *columns = VGATextController_COLUMNS;
  if (*rows > VGATextController_ROWS)
    *rows = VGATextController_ROWS;

}


void VGATextController::init(gpio_num_t VSyncGPIO)
{
  m_DMABuffers = nullptr;

  m_GPIOStream.begin();

  // load font into RAM
  auto font = getFont();
  int charDataSize = 256 * font->height * ((font->width + 7) / 8);
  m_charData = (uint8_t*) heap_caps_malloc(charDataSize, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
  memcpy(m_charData, font->data, charDataSize);
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
  configureGPIO(gpio, mode);
  gpio_matrix_out(gpio, I2S1O_DATA_OUT0_IDX + bit, false, false);
}


void VGATextController::setResolution(char const * modeline, int viewPortWidth, int viewPortHeight, bool doubleBuffered)
{
  VGATimings timings;
  if (VGABaseController::convertModelineToTimings(VGATextController_MODELINE, &timings))
    setResolution(timings);
}


void VGATextController::setResolution(VGATimings const& timings)
{
  // setResolution() already called? If so, stop and free buffers
  if (m_DMABuffers) {
    m_GPIOStream.stop();
    freeBuffers();
  }

  m_timings = timings;

  // inform base class about screen size
  setScreenSize(m_timings.HVisibleArea, m_timings.VVisibleArea);

  m_HVSync = packHVSync(false, false);

  m_DMABuffersCount = 2 * m_timings.VVisibleArea + m_timings.VFrontPorch + m_timings.VSyncPulse + m_timings.VBackPorch;

  m_DMABuffers = (lldesc_t*) heap_caps_malloc(m_DMABuffersCount * sizeof(lldesc_t), MALLOC_CAP_DMA);

  m_lines = (uint32_t*) heap_caps_malloc(VGATextController_CHARHEIGHT * VGATextController_WIDTH, MALLOC_CAP_DMA);

  int rawLineWidth = m_timings.HFrontPorch + m_timings.HSyncPulse + m_timings.HBackPorch + m_timings.HVisibleArea;
  m_blankLine = (uint8_t*) heap_caps_malloc(rawLineWidth, MALLOC_CAP_DMA);
  m_syncLine  = (uint8_t*) heap_caps_malloc(rawLineWidth, MALLOC_CAP_DMA);

  // horiz: FRONTPORCH -> SYNC -> BACKPORCH -> VISIBLEAREA
  //
  // vert:  VISIBLEAREA
  //        FRONTPORCH
  //        SYNC
  //        BACKPORCH

  for (int i = 0, visLine = 0, invLine = 0; i < m_DMABuffersCount; ++i) {

    if (i < m_timings.VVisibleArea * 2) {

      // first part is the same of a blank line
      m_DMABuffers[i].eof          = (visLine == 0 || visLine == VGATextController_CHARHEIGHT / 2 ? 1 : 0);
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
      m_DMABuffers[i].buf          = (uint8_t*)(m_lines) + visLine * VGATextController_WIDTH;

      ++visLine;
      if (visLine == VGATextController_CHARHEIGHT)
        visLine = 0;

    } else {

      // vertical porchs and sync

      bool frameResetDesc = (invLine == 0);

      if (frameResetDesc)
        s_frameResetDesc = &m_DMABuffers[i];

      m_DMABuffers[i].eof          = (frameResetDesc ? 1 : 0); // prepare for next frame
      m_DMABuffers[i].sosf         = 0;
      m_DMABuffers[i].offset       = 0;
      m_DMABuffers[i].owner        = 1;
      m_DMABuffers[i].qe.stqe_next = (lldesc_t*) (i ==  m_DMABuffersCount - 1 ? &m_DMABuffers[0] : &m_DMABuffers[i + 1]);
      m_DMABuffers[i].length       = rawLineWidth;
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
    s_fgbgPattern = (uint32_t*) heap_caps_malloc(16384, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
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


  // ESP_INTR_FLAG_LEVEL1: should be less than PS2Controller interrupt level, necessary when running on the same core
  CoreUsage::setBusiestCore(FABGLIB_VIDEO_CPUINTENSIVE_TASKS_CORE);
  esp_intr_alloc_pinnedToCore(ETS_I2S1_INTR_SOURCE, ESP_INTR_FLAG_LEVEL1 | ESP_INTR_FLAG_IRAM, ISRHandler, this, &m_isr_handle, FABGLIB_VIDEO_CPUINTENSIVE_TASKS_CORE);

  m_GPIOStream.play(m_timings.frequency, m_DMABuffers);

  I2S1.int_clr.val     = 0xFFFFFFFF;
  I2S1.int_ena.out_eof = 1;
}


void VGATextController::freeBuffers()
{
  heap_caps_free( (void*) m_DMABuffers );
  heap_caps_free((void*) m_lines);
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
  int rawLineWidth = m_timings.HFrontPorch + m_timings.HSyncPulse + m_timings.HBackPorch + m_timings.HVisibleArea;
  for (int rx = 0; x < rawLineWidth; ++x, ++rx) {
    VGA_PIXELINROW(m_blankLine, x) = preparePixelWithSync((RGB222){0, 0, 0}, false, false);
    VGA_PIXELINROW(m_syncLine, x)  = preparePixelWithSync((RGB222){0, 0, 0}, false, true);
    for (int i = 0; i < VGATextController_CHARHEIGHT; ++i)
      VGA_PIXELINROW( ((uint8_t*)(m_lines) + i * VGATextController_WIDTH), rx)  = preparePixelWithSync((RGB222){0, 0, 0}, false, false);
  }
}


void IRAM_ATTR VGATextController::ISRHandler(void * arg)
{
  #if FABGLIB_VGAXCONTROLLER_PERFORMANCE_CHECK
  auto s1 = getCycleCount();
  #endif

  VGATextController * ctrl = (VGATextController *) arg;

  if (I2S1.int_st.out_eof && ctrl->m_charData != nullptr) {

    auto desc = (volatile lldesc_t*) I2S1.out_eof_des_addr;

    if (desc == s_frameResetDesc) {

      s_scanLine = 0;
      s_textRow  = 0;
      s_upperRow = true;

      if (ctrl->m_cursorEnabled) {
        ++ctrl->m_cursorCounter;
        if (ctrl->m_cursorCounter >= ctrl->m_cursorSpeed)
          ctrl->m_cursorCounter = -ctrl->m_cursorSpeed;
      }

      if (ctrl->m_map == nullptr) {
        I2S1.int_clr.val = I2S1.int_st.val;
        #if FABGLIB_VGAXCONTROLLER_PERFORMANCE_CHECK
        s_vgatxtcycles += getCycleCount() - s1;
        #endif
        return;
      }

    } else if (s_scanLine == 0) {
      // out of sync, wait for next frame
      I2S1.int_clr.val = I2S1.int_st.val;
      #if FABGLIB_VGAXCONTROLLER_PERFORMANCE_CHECK
      s_vgatxtcycles += getCycleCount() - s1;
      #endif
      return;
    }

    int scanLine = s_scanLine;

    const int lineIndex = scanLine % VGATextController_CHARHEIGHT;

    auto lines = ctrl->m_lines;

    if (s_textRow < ctrl->m_rows) {

      int cursorCol = 0;
      int cursorFGBG = 0;
      const auto cursorVisible = (ctrl->m_cursorEnabled && ctrl->m_cursorCounter >= 0 && s_textRow == ctrl->m_cursorRow);
      if (cursorVisible) {
        cursorCol  = ctrl->m_cursorCol;
        cursorFGBG = (ctrl->m_cursorForeground << 4) | (ctrl->m_cursorBackground << 8);
      }

      const auto charData = ctrl->m_charData + (s_upperRow ? 0 : VGATextController_CHARHEIGHT / 2);
      auto mapItemPtr = ctrl->m_map + s_textRow * VGATextController_COLUMNS;

      for (int col = 0; col < VGATextController_COLUMNS; ++col, ++mapItemPtr) {

        const auto mapItem = *mapItemPtr;

        int fgbg = (mapItem >> 4) & 0b111111110000;

        const auto options = glyphMapItem_getOptions(mapItem);

        // invert?
        if (options.invert)
          fgbg = ((fgbg >> 4) & 0b11110000) | ((fgbg << 4) & 0b111100000000);

        // cursor?
        if (cursorVisible && col == cursorCol)
          fgbg = cursorFGBG;

        uint32_t * dest = lines + lineIndex * VGATextController_WIDTH / sizeof(uint32_t) + col * VGATextController_CHARWIDTHBYTES * 2;

        // blank?
        if (options.blank) {

          for (int rowInChar = 0; rowInChar < VGATextController_CHARHEIGHT / 2; ++rowInChar) {
            int v = s_fgbgPattern[fgbg];
            *dest       = v;
            *(dest + 1) = v;
            dest += VGATextController_WIDTH / sizeof(uint32_t);
          }

        } else {

          const bool underline = (s_upperRow == false && options.underline);
          const bool bold      = options.bold;

          auto charRowPtr = charData + glyphMapItem_getIndex(mapItem) * VGATextController_CHARHEIGHT * VGATextController_CHARWIDTHBYTES;

          for (int rowInChar = 0; rowInChar < VGATextController_CHARHEIGHT / 2; ++rowInChar) {
            auto charRowData = *charRowPtr;

            // bold?
            if (bold)
              charRowData |= charRowData >> 1;

            *dest       = s_fgbgPattern[(charRowData >> 4)  | fgbg];
            *(dest + 1) = s_fgbgPattern[(charRowData & 0xF) | fgbg];

            dest += VGATextController_WIDTH / sizeof(uint32_t);
            charRowPtr += VGATextController_CHARWIDTHBYTES;
          }

          // underline?
          if (underline) {
            dest -= VGATextController_WIDTH / sizeof(uint32_t);
            uint32_t v = s_fgbgPattern[0xF | fgbg];
            *dest       = v;
            *(dest + 1) = v;
          }

        }

      }

      if (s_upperRow) {
        s_upperRow = false;
      } else {
        s_upperRow = true;
        ++s_textRow;
      }

    } else {
      for (int i = 0; i < VGATextController_CHARHEIGHT / 2; ++i) {
        auto dest = lines + ((scanLine + i) % VGATextController_CHARHEIGHT) * VGATextController_WIDTH / sizeof(uint32_t);
        for (int i = 0; i < VGATextController_COLUMNS; ++i) {
          *dest++ = s_blankPatternDWord;
          *dest++ = s_blankPatternDWord;
        }
      }
    }

    scanLine += VGATextController_CHARHEIGHT / 2;

    s_scanLine = scanLine;

  }

  #if FABGLIB_VGAXCONTROLLER_PERFORMANCE_CHECK
  s_vgatxtcycles += getCycleCount() - s1;
  #endif

  I2S1.int_clr.val = I2S1.int_st.val;
}





}
