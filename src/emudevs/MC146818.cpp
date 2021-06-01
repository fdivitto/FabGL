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



#include "MC146818.h"


#define NSVKEY_REGS   "MC146818"


// MC146818 registers
#define REG_SECONDS       0x00    // bin: 0..59, bcd: 00..59
#define REG_SECONDS_ALARM 0x01    // like REG_SECONDS or >=0xc0 for don't care
#define REG_MINUTES       0x02    // bin: 0..59, bcd: 00..59
#define REG_MINUTES_ALARM 0x03    // like REG_MINUTES or >=0xc0 for don't care
#define REG_HOURS         0x04    // bin: 1..12 or 0..23, bcd: 01..12 or 00..23 (ORed with 0x80 for PM when range is 1..12)
#define REG_HOURS_ALARM   0x05    // like REG_HOURS or >=0xc0 for don't care
#define REG_DAYOFWEEK     0x06    // bin: 1..7, bcd: 01..07, (sunday = 1)
#define REG_DAYOFMONTH    0x07    // bin: 1..31, bcd: 01..31
#define REG_MONTH         0x08    // bin: 1..12, bcd: 01..12
#define REG_YEAR          0x09    // bin: 0..99, bcd: 00..99

// not MC146818 but anyway filled (to avoid Y2K bug)
#define REG_CENTURY       0x32    // bcd: 19 or 20

// status and control registers
#define REG_A             0x0a
#define REG_B             0x0b
#define REG_C             0x0c
#define REG_D             0x0d

// bits of register A
#define REGA_RS0          0x01  // R/W, rate selection for square wave gen and Periodic Interrupt
#define REGA_RS1          0x02  // R/W
#define REGA_RS2          0x04  // R/W
#define REGA_RS3          0x08  // R/W
#define REGA_DV0          0x10  // R/W, input freq divider
#define REGA_DV1          0x20  // R/W
#define REGA_DV2          0x40  // R/W
#define REGA_UIP          0x80  // R/O, 1 = update in progress

// bits of register B
#define REGB_DSE          0x01  // R/W, 1 = enabled daylight save
#define REGB_H24          0x02  // R/W, 1 = 24h mode, 0 = 12h mode
#define REGB_DM           0x04  // R/W, 1 = binary format, 0 = BCD format
#define REGB_SQWE         0x08  // R/W, 1 = enable SQWE output
#define REGB_UIE          0x10  // R/W, 1 = enable update ended interrupt
#define REGB_AIE          0x20  // R/W, 1 = enable alarm interrupt
#define REGB_PIE          0x40  // R/W, 1 = enable period interrupts
#define REGB_SET          0x80  // R/W, 1 = halt time updates

// bits of register C
#define REGC_UF           0x10  // R/O, 1 = update ended interrupt flag
#define REGC_AF           0x20  // R/O, 1 = alarm interrupt flag
#define REGC_PF           0x40  // R/O, 1 = period interrupt flag
#define REGC_IRQF         0x80  // R/O, this is "UF & UIE | AF & AIE | PF & PIE"

// bits of register D
#define REGD_VRT          0x80  // R/O, 1 = valid RAM and time



namespace fabgl {


MC146818::MC146818()
  : m_nvs(0),
    m_interruptCallback(nullptr),
    m_periodicIntTimerHandle(nullptr),
    m_endUpdateIntTimerHandle(nullptr)
{
}


MC146818::~MC146818()
{
  stopPeriodicTimer();
  stopEndUpdateTimer();
  if (m_nvs)
    nvs_close(m_nvs);
}


void MC146818::init(char const * NVSNameSpace)
{
  // load registers from NVS
  if (!m_nvs) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      nvs_flash_erase();
      nvs_flash_init();
    }
    nvs_open(NVSNameSpace, NVS_READWRITE, &m_nvs);
  }
  if (m_nvs) {
    size_t len = sizeof(m_regs);
    if (nvs_get_blob(m_nvs, NSVKEY_REGS, m_regs, &len) != ESP_OK) {
      // first time initialization
      memset(m_regs, 0, sizeof(m_regs));
    }
  }
}


// reload data from NVS
void MC146818::reset()
{
  // set registers bits affected by reset
  m_regs[REG_B] &= ~(REGB_PIE | REGB_AIE | REGB_UIE | REGB_SQWE);
  m_regs[REG_C] &= ~(REGC_IRQF | REGC_PF | REGC_AF | REGC_UF);
  m_regs[REG_D]  = REGD_VRT; // power ok

  m_regSel = 0;
}


// saves all data to NVS
void MC146818::commit()
{
  if (m_nvs) {
    nvs_set_blob(m_nvs, NSVKEY_REGS, m_regs, sizeof(m_regs));
  }
}


// address:
//    1 : register read
uint8_t MC146818::read(int address)
{
  uint8_t retval = 0;
  if (address == 1) {
    if (m_regSel <= REG_YEAR || m_regSel == REG_CENTURY)
      updateTime();
    retval = m_regs[m_regSel];
    if (m_regSel == REG_C) {
      // timers are enabled when flags are read
      enableTimers();
      // flags are cleared on read (but after retval has been assigned!)
      m_regs[REG_C] = 0;
    }
    //printf("MC146818::read(%02X) => %02X (sel=%02X)\n", address, retval, m_regSel);
  }
  return retval;
}


// address:
//    0 : register address port (bits 0-6)
//    1 : register write
void MC146818::write(int address, uint8_t value)
{
  switch (address) {
    case 0:
      m_regSel = value & 0x7f;
      break;
    case 1:
      m_regs[m_regSel] = value;
      if ( (m_regSel == REG_A && (value & 0xf) != 0) ||
           (m_regSel == REG_B && (value & (REGB_UIE | REGB_AIE | REGB_PIE)) != 0) ) {
        // timers are enabled when Rate Selection > 0 or any interrupt is enabled
        enableTimers();
      }
      break;
  }
}


// convert decimal to packed BCD (v in range 0..99)
static uint8_t byteToBCD(uint8_t v)
{
  return (v % 10) | ((v / 10) << 4);
}


// get time from system and fill date/time registers
void MC146818::updateTime()
{
  if ((m_regs[REG_B] & REGB_SET) == 0) {

    time_t now;
    tm timeinfo;

    time(&now);
    localtime_r(&now, &timeinfo);

    bool binary = m_regs[REG_B] & REGB_DM;
    bool h24    = m_regs[REG_B] & REGB_H24;

    int year    = (1900 + timeinfo.tm_year);  // 1986, 2021, ...
    int century = year / 100;                 // 19, 20, ...

    m_regs[REG_CENTURY] = byteToBCD(century);

    if (binary) {
      // binary format
      m_regs[REG_SECONDS]    = imin(timeinfo.tm_sec, 59);
      m_regs[REG_MINUTES]    = timeinfo.tm_min;
      m_regs[REG_HOURS]      = h24 ? timeinfo.tm_hour : (((timeinfo.tm_hour - 1) % 12 + 1) | (timeinfo.tm_hour >= 12 ? 0x80 : 0x00));
      m_regs[REG_DAYOFWEEK]  = timeinfo.tm_wday + 1;
      m_regs[REG_DAYOFMONTH] = timeinfo.tm_mday;
      m_regs[REG_MONTH]      = timeinfo.tm_mon + 1;
      m_regs[REG_YEAR]       = year - century * 100;
    } else {
      // BCD format
      m_regs[REG_SECONDS]    = byteToBCD(imin(timeinfo.tm_sec, 59));
      m_regs[REG_MINUTES]    = byteToBCD(timeinfo.tm_min);
      m_regs[REG_HOURS]      = h24 ? byteToBCD(timeinfo.tm_hour) : (byteToBCD((timeinfo.tm_hour - 1) % 12 + 1) | (timeinfo.tm_hour >= 12 ? 0x80 : 0x00));
      m_regs[REG_DAYOFWEEK]  = byteToBCD(timeinfo.tm_wday + 1);
      m_regs[REG_DAYOFMONTH] = byteToBCD(timeinfo.tm_mday);
      m_regs[REG_MONTH]      = byteToBCD(timeinfo.tm_mon + 1);
      m_regs[REG_YEAR]       = byteToBCD(year - century * 100);
    }
  }
}


void MC146818::enableTimers()
{
  esp_timer_init(); // can be called multiple times

  // Setup Periodic Interrupt timer
  stopPeriodicTimer();
  int rate = m_regs[REG_A] & 0xf;
  if (rate > 0) {
    // supported divider (time base)?
    int divider = (m_regs[REG_A] >> 4) & 7;
    if (divider == 2) {
      // we just support 32768Hz time base
      static const int RATE2US[16] = { 0, 3906, 7812, 122, 244, 488, 976, 1953, 3906, 7812, 15625, 31250, 62500, 125000, 250000, 500000 };
      esp_timer_create_args_t args = { 0 };
      args.callback = periodIntTimerFunc;
      args.arg = this;
      args.dispatch_method = ESP_TIMER_TASK;
      esp_timer_create(&args, &m_periodicIntTimerHandle);
      esp_timer_start_periodic(m_periodicIntTimerHandle, RATE2US[rate]);
      //printf("MC146818: Periodic timer started\n");
    } else {
      printf("MC146818: Unsupported freq divider %d\n", divider);
    }
  }

  // Setup Alarm and End of Update timer
  if (!m_endUpdateIntTimerHandle) {
    esp_timer_create_args_t args = { 0 };
    args.callback = endUpdateIntTimerFunc;
    args.arg = this;
    args.dispatch_method = ESP_TIMER_TASK;
    esp_timer_create(&args, &m_endUpdateIntTimerHandle);
    esp_timer_start_periodic(m_endUpdateIntTimerHandle, 1000000); // 1 second
    //printf("MC146818: Alarm & End of Update timer started\n");
  }
}


void MC146818::stopPeriodicTimer()
{
  if (m_periodicIntTimerHandle) {
    esp_timer_stop(m_periodicIntTimerHandle);
    esp_timer_delete(m_periodicIntTimerHandle);
    m_periodicIntTimerHandle = nullptr;
  }
}


void MC146818::stopEndUpdateTimer()
{
  if (m_endUpdateIntTimerHandle) {
    esp_timer_stop(m_endUpdateIntTimerHandle);
    esp_timer_delete(m_endUpdateIntTimerHandle);
    m_endUpdateIntTimerHandle = nullptr;
  }
}


// Handles Periodic events at specified rate
void MC146818::periodIntTimerFunc(void * args)
{
  auto m = (MC146818 *) args;

  // set periodic flag
  m->m_regs[REG_C] |= REGC_PF;

  // trig interrupt?
  if (m->m_regs[REG_B] & REGB_PIE) {
    m->m_regs[REG_C] |= REGC_PF | REGC_IRQF;
    m->m_interruptCallback(m->m_context);
  }
}


// Fired every second
// Handles Alarm and End Update events
void MC146818::endUpdateIntTimerFunc(void * args)
{
  auto m = (MC146818 *) args;

  if ((m->m_regs[REG_B] & REGB_SET) == 0) {

    // updating flag
    m->m_regs[REG_A] |= REGA_UIP;

    m->updateTime();

    // alarm?
    if ( ((m->m_regs[REG_SECONDS_ALARM] & 0xc0) == 0xc0 || (m->m_regs[REG_SECONDS_ALARM] == m->m_regs[REG_SECONDS])) &&
         ((m->m_regs[REG_MINUTES_ALARM] & 0xc0) == 0xc0 || (m->m_regs[REG_MINUTES_ALARM] == m->m_regs[REG_MINUTES])) &&
         ((m->m_regs[REG_HOURS_ALARM] & 0xc0) == 0xc0 || (m->m_regs[REG_HOURS_ALARM] == m->m_regs[REG_HOURS])) ) {
      // yes, set flag
      m->m_regs[REG_C] |= REGC_AF;
    }

    // always signal end update
    m->m_regs[REG_C] |= REGC_UF;

    // updating flag
    m->m_regs[REG_A] &= ~REGA_UIP;

    // trig interrupt?
    if ( ((m->m_regs[REG_B] & REGB_UIE) && (m->m_regs[REG_C] & REGC_UF)) ||
         ((m->m_regs[REG_B] & REGB_AIE) && (m->m_regs[REG_C] & REGC_AF)) ) {
      // yes
      m->m_regs[REG_C] |= REGC_IRQF;
      m->m_interruptCallback(m->m_context);
    }

  }
}



}   // fabgl namespace
