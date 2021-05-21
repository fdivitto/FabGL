/*
  Created by Fabrizio Di Vittorio (fdivitto2013@gmail.com) - <http://www.fabgl.com>
  Copyright (c) 2019-2021 Fabrizio Di Vittorio.
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


#include <strings.h>

#include "freertos/FreeRTOS.h"

#include "esp32/ulp.h"
#include "driver/rtc_io.h"
#include "soc/sens_reg.h"
#if __has_include("soc/rtc_io_periph.h")
  #include "soc/rtc_io_periph.h"
#endif
#include "esp_log.h"

#include "ps2controller.h"
#include "fabutils.h"
#include "ulp_macro_ex.h"
#include "devdrivers/keyboard.h"
#include "devdrivers/mouse.h"



#pragma GCC optimize ("O2")
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"


namespace fabgl {




////////////////////////////////////////////////////////////////////////////
// placeholders

#define OPCODE_PLACEHOLDER             12 // 12 is an unused ULP opcode we can use as placeholder

#define SUB_OPCODE_DAT_ENABLE_OUTPUT    0
#define SUB_OPCODE_DAT_ENABLE_INPUT     1
#define SUB_OPCODE_CLK_ENABLE_OUTPUT    2
#define SUB_OPCODE_CLK_ENABLE_INPUT     3
#define SUB_OPCODE_READ_CLK             4
#define SUB_OPCODE_READ_DAT             5
#define SUB_OPCODE_WRITE_CLK            6
#define SUB_OPCODE_WRITE_DAT            7

#define PS2_PORT0 0
#define PS2_PORT1 1


#define DAT_ENABLE_OUTPUT(ps2port, value) { .macro = { \
    .label      = value, \
    .unused     = ps2port, \
    .sub_opcode = SUB_OPCODE_DAT_ENABLE_OUTPUT, \
    .opcode     = OPCODE_PLACEHOLDER } }

#define DAT_ENABLE_INPUT(ps2port, value) { .macro = { \
    .label      = value, \
    .unused     = ps2port, \
    .sub_opcode = SUB_OPCODE_DAT_ENABLE_INPUT, \
    .opcode     = OPCODE_PLACEHOLDER } }

#define CLK_ENABLE_OUTPUT(ps2port, value) { .macro = { \
    .label      = value, \
    .unused     = ps2port, \
    .sub_opcode = SUB_OPCODE_CLK_ENABLE_OUTPUT, \
    .opcode     = OPCODE_PLACEHOLDER } }

#define CLK_ENABLE_INPUT(ps2port, value) { .macro = { \
    .label      = value, \
    .unused     = ps2port, \
    .sub_opcode = SUB_OPCODE_CLK_ENABLE_INPUT, \
    .opcode     = OPCODE_PLACEHOLDER } }

// equivalent to rtc_gpio_get_level()
#define READ_CLK(ps2port) { .macro = { \
    .label      = 0, \
    .unused     = ps2port, \
    .sub_opcode = SUB_OPCODE_READ_CLK, \
    .opcode     = OPCODE_PLACEHOLDER } }

// equivalent to rtc_gpio_get_level()
#define READ_DAT(ps2port) { .macro = { \
    .label      = 0, \
    .unused     = ps2port, \
    .sub_opcode = SUB_OPCODE_READ_DAT, \
    .opcode     = OPCODE_PLACEHOLDER } }

// equivalent to rtc_gpio_set_level()
#define WRITE_CLK(ps2port, value) { .macro = { \
    .label      = value, \
    .unused     = ps2port, \
    .sub_opcode = SUB_OPCODE_WRITE_CLK, \
    .opcode     = OPCODE_PLACEHOLDER } }

// equivalent to rtc_gpio_set_level()
#define WRITE_DAT(ps2port, value) { .macro = { \
    .label      = value, \
    .unused     = ps2port, \
    .sub_opcode = SUB_OPCODE_WRITE_DAT, \
    .opcode     = OPCODE_PLACEHOLDER } }


////////////////////////////////////////////////////////////////////////////
// macro instructions

// equivalent to rtc_gpio_set_direction(..., RTC_GPIO_MODE_INPUT_ONLY)
#define CONFIGURE_DAT_INPUT(ps2port) \
    DAT_ENABLE_OUTPUT(ps2port, 0), \
    DAT_ENABLE_INPUT(ps2port, 1)

// equivalent to rtc_gpio_set_direction(..., RTC_GPIO_MODE_OUTPUT_ONLY)
#define CONFIGURE_DAT_OUTPUT(ps2port) \
    DAT_ENABLE_OUTPUT(ps2port, 1), \
    DAT_ENABLE_INPUT(ps2port, 0)

// equivalent to rtc_gpio_set_direction(..., RTC_GPIO_MODE_INPUT_ONLY)
#define CONFIGURE_CLK_INPUT(ps2port) \
    CLK_ENABLE_OUTPUT(ps2port, 0), \
    CLK_ENABLE_INPUT(ps2port, 1)

// equivalent to rtc_gpio_set_direction(..., RTC_GPIO_MODE_OUTPUT_ONLY)
#define CONFIGURE_CLK_OUTPUT(ps2port) \
    CLK_ENABLE_OUTPUT(ps2port, 1), \
    CLK_ENABLE_INPUT(ps2port, 0)

// equivalent to rtc_gpio_set_level()
// WRITE bit 0 of R0
#define WRITE_DAT_R0(ps2port) \
    I_BL(3, 1), \
    WRITE_DAT(ps2port, 1), \
    I_BGE(2, 1), \
    WRITE_DAT(ps2port, 0)

// performs [addr] = value
// changes: R0, R1
#define MEM_WRITEI(addr, value) \
    I_MOVI(R0, addr), \
    I_MOVI(R1, value), \
    I_ST(R1, R0, 0)

// performs [[addr]] = value
// changes: R0, R1
#define MEM_INDWRITEI(addr, value) \
    I_MOVI(R0, addr), \
    I_LD(R0, R0, 0), \
    I_MOVI(R1, value), \
    I_ST(R1, R0, 0)

// performs [addr] = reg  (reg cannot be R0)
// changes R0
#define MEM_WRITER(addr, reg) \
    I_MOVI(R0, addr), \
    I_ST(reg, R0, 0)

// performs [[addr]] = reg  (reg cannot be R0)
// changes R0
#define MEM_INDWRITER(addr, reg) \
    I_MOVI(R0, addr), \
    I_LD(R0, R0, 0), \
    I_ST(reg, R0, 0)

// performs reg = [addr]
#define MEM_READR(reg, addr) \
    I_MOVI(reg, addr), \
    I_LD(reg, reg, 0)

// performs reg = [[addr]]  (reg cannot be R0)
#define MEM_INDREADR(reg, addr) \
    I_MOVI(reg, addr), \
    I_LD(reg, reg, 0), \
    I_LD(reg, reg, 0)

// performs [addr] = [addr] + 1
// changes: R0, R1
#define MEM_INC(addr) \
    I_MOVI(R0, addr),   \
    I_LD(R1, R0, 0),    \
    I_ADDI(R1, R1, 1),  \
    I_ST(R1, R0, 0)

// jump to "label" if [addr] < value
// changes: R0
#define MEM_BL(label, addr, value) \
    I_MOVI(R0, addr), \
    I_LD(R0, R0, 0), \
    M_BL(label, value)

// jump to "label" if [addr] >= value
// changes: R0
#define MEM_BGE(label, addr, value) \
    I_MOVI(R0, addr), \
    I_LD(R0, R0, 0), \
    M_BGE(label, value)

// long jump version of M_BGE
#define M_LONG_BGE(label, value) \
    I_BL(2, value), \
    M_BX(label)

// long jump version of M_BL
#define M_LONG_BL(label, value) \
    I_BGE(2, value), \
    M_BX(label)

// long jump to "label" if R0 == value
#define M_LONG_BE(label, value) \
    I_BL(3, value), \
    I_BGE(2, value + 1), \
    M_BX(label)

// long jump to "label" if R0 != value
#define M_LONG_BNE(label, value) \
    I_BGE(2, value), \
    M_BX(label), \
    I_BL(2, value + 1), \
    M_BX(label)

#define M_LONG_STAGEBLE(label_num, imm_value) \
    I_STAGEBLE(2, imm_value), \
    I_STAGEBGE(2, imm_value), \
    M_BX(label_num)

#define M_LONG_STAGEBL(label_num, imm_value) \
    I_STAGEBGE(2, imm_value), \
    M_BX(label_num)

#define M_LONG_STAGEBGE(label_num, imm_value) \
    I_STAGEBL(2, imm_value), \
    M_BX(label_num)

// ULP clock is 8MHz, so every cycle takes 0.125us and 1us = 8 cycles
#define M_DELAY_US(us) I_DELAY(us * 8)


////////////////////////////////////////////////////////////////////////////



// Locations inside RTC low speed memory

#define RTCMEM_PROG_START           0x000  // where the program begins
#define RTCMEM_VARS_START           0x200  // where the variables begin

// commands (set by CPU, reset by ULP)
#define RTCMEM_PORT0_TX             (RTCMEM_VARS_START +  0)  // 1 = send data to port 0
#define RTCMEM_PORT1_TX             (RTCMEM_VARS_START +  1)  // 1 = send data to port 1
#define RTCMEM_PORT0_RX_ENABLE      (RTCMEM_VARS_START +  2)  // 1 = Request to enable RX from port 0
#define RTCMEM_PORT1_RX_ENABLE      (RTCMEM_VARS_START +  3)  // 1 = Request to enable RX from port 1
#define RTCMEM_PORT0_RX_DISABLE     (RTCMEM_VARS_START +  4)  // 1 = Request to disable RX from port 0
#define RTCMEM_PORT1_RX_DISABLE     (RTCMEM_VARS_START +  5)  // 1 = Request to disable RX from port 1
// commands parameters (set by CPU)
#define RTCMEM_PORT0_DATAOUT        (RTCMEM_VARS_START +  6)  // Data to send to port 0 (when RTCMEM_PORT0_TX = 1)
#define RTCMEM_PORT1_DATAOUT        (RTCMEM_VARS_START +  7)  // Data to send to port 1 (when RTCMEM_PORT1_TX = 1)

// flags (set by ULP, reset by CPU), generate interrupt
#define RTCMEM_PORT0_RX             (RTCMEM_VARS_START +  8)  // Data received from port 0
#define RTCMEM_PORT1_RX             (RTCMEM_VARS_START +  9)  // Data received from port 1
#define RTCMEM_PORT0_RX_CLK_TIMEOUT (RTCMEM_VARS_START + 10)  // RX port 0 CLK timeout
#define RTCMEM_PORT1_RX_CLK_TIMEOUT (RTCMEM_VARS_START + 11)  // RX port 1 CLK timeout
// flags parameters (set by ULP)
#define RTCMEM_PORT0_DATAIN         (RTCMEM_VARS_START + 12)  // Data received from port 0 (when RTCMEM_PORT0_RX = 1)
#define RTCMEM_PORT1_DATAIN         (RTCMEM_VARS_START + 13)  // Data received from port 1 (when RTCMEM_PORT1_RX = 1)

// internal variables
#define RTCMEM_PORT0_RX_ENABLED     (RTCMEM_VARS_START + 14)  // 0 = port 0 not configured for RX, 1 = port 0 configured for RX
#define RTCMEM_PORT1_RX_ENABLED     (RTCMEM_VARS_START + 15)  // 0 = port 1 not configured for RX, 1 = port 1 configured for RX

#define RTCMEM_LASTVAR              (RTCMEM_VARS_START + 15)


// RX maximum time between CLK cycles (reliable minimum is about 15)
#define CLK_RX_TIMEOUT_VAL 100

// TX maximum time between CLK cycles
#define CLK_TX_TIMEOUT_VAL 1200    // fine tuned to work with PERIBOARD 409P

// counter (R2) re-wake value
#define WAKE_THRESHOLD     3000


// check RTC memory occupation
#if RTCMEM_LASTVAR >= 0x800
#error "ULP program too big"
#endif


// ULP program labels

#define LABEL_WAIT_COMMAND             0

#define LABEL_RX                       1
#define LABEL_RX_NEXT                  2

#define LABEL_PORT0_ENABLE_RX          3
#define LABEL_PORT0_STOP_RX            4
#define LABEL_PORT0_TX                 5
#define LABEL_PORT0_TX_NEXT_BIT        6
#define LABEL_PORT0_TX_WAIT_CLK_HIGH   7
#define LABEL_PORT0_TX_EXIT            8
#define LABEL_PORT0_RX_WAIT_LOOP       9
#define LABEL_PORT0_RX_CLK_IS_HIGH    10
#define LABEL_PORT0_RX_CLK_IS_LOW     11
#define LABEL_PORT0_RX_CLK_TIMEOUT    12
#define LABEL_PORT0_RX_CHECK_CLK      13

#define LABEL_PORT1_ENABLE_RX         14
#define LABEL_PORT1_STOP_RX           15
#define LABEL_PORT1_TX                16
#define LABEL_PORT1_TX_NEXT_BIT       17
#define LABEL_PORT1_TX_WAIT_CLK_HIGH  18
#define LABEL_PORT1_TX_EXIT           19
#define LABEL_PORT1_RX_WAIT_LOOP      20
#define LABEL_PORT1_RX_CLK_IS_HIGH    21
#define LABEL_PORT1_RX_CLK_IS_LOW     22
#define LABEL_PORT1_RX_CLK_TIMEOUT    23
#define LABEL_PORT1_RX_CHECK_CLK      24


// Helpers

// temporarily disable port 0
// note: before disable check if it was enabled. If it wasn't enabled just exit.
// changes: R0
#define TEMP_PORT0_DISABLE()              \
  I_LD(R0, R3, RTCMEM_PORT0_RX_ENABLED),  \
  I_BL(4, 1),                             \
  CONFIGURE_CLK_OUTPUT(PS2_PORT0),        \
  WRITE_CLK(PS2_PORT0, 0)

// temporarily enable port 0
// note: before enable check if it was enabled. If it wasn't enabled just exit
// changes: R0
#define TEMP_PORT0_ENABLE()               \
  I_LD(R0, R3, RTCMEM_PORT0_RX_ENABLED),  \
  I_BL(3, 1),                             \
  CONFIGURE_CLK_INPUT(PS2_PORT0)

// temporarily disable port 1
// note: before disable check if it was enabled. If it wasn't enabled just exit.
// changes: R0
#define TEMP_PORT1_DISABLE()              \
  I_LD(R0, R3, RTCMEM_PORT1_RX_ENABLED),  \
  I_BL(4, 1),                             \
  CONFIGURE_CLK_OUTPUT(PS2_PORT1),        \
  WRITE_CLK(PS2_PORT1, 0)

// temporarily enable port 1
// note: before enable check if it was enabled. If it wasn't enabled just exit
// changes: R0
#define TEMP_PORT1_ENABLE()               \
  I_LD(R0, R3, RTCMEM_PORT1_RX_ENABLED),  \
  I_BL(3, 1),                             \
  CONFIGURE_CLK_INPUT(PS2_PORT1)

// permanently disable port 0
// changes: R0
#define PERM_PORT0_DISABLE()              \
  I_MOVI(R0, 0),                          \
  I_ST(R0, R3, RTCMEM_PORT0_RX_ENABLED),  \
  CONFIGURE_CLK_OUTPUT(PS2_PORT0),        \
  WRITE_CLK(PS2_PORT0, 0)

// permanently disable port 1
// changes: R0
#define PERM_PORT1_DISABLE()              \
  I_MOVI(R0, 0),                          \
  I_ST(R0, R3, RTCMEM_PORT1_RX_ENABLED),  \
  CONFIGURE_CLK_OUTPUT(PS2_PORT1),        \
  WRITE_CLK(PS2_PORT1, 0)


/*
  Notes about ULP registers usage

    R0:
      General purpose temporary accumulator

    R1:
      General purpose temporary register

    R2:
      LABEL_PORT0_TX / LABEL_PORT1_TX: Word to send. Reset to 0 on exit.
      LABEL_RX: Re-wake counter register, when port 0 or port 1 is disabled.
                General purpose temporary register on receiving data (reset at the end or timeout).

    R3:
      Base address for variables (0x0000)
      LABEL_PORT0_TX / LABEL_PORT1_TX: timeout counter waiting for CLK to be Low
      LABEL_RX: timeout counter waiting for CLK changes

    STAGE:
      RX, TX bit counter

*/


const ulp_insn_t ULP_code[] = {

  // Stop ULP timer, not necessary because this routine never ends
  I_END(),

  // R3 contains just 0x0000, for entire execution, which is the base address of all variabiles load, so it is possible to do
  // I_LD(R#, R3, RTC_MEM_XX) instead of MEM_READR(), saving one instruction
  I_MOVI(R3, 0x0000),


  /////////////////////////////////////////////////////////////////////////////////////////////
  // Command wait main loop

M_LABEL(LABEL_WAIT_COMMAND),

  // port 0 TX?
  I_LD(R0, R3, RTCMEM_PORT0_TX),
  M_BGE(LABEL_PORT0_TX, 1),                               // yes, jump to LABEL_PORT0_TX

  // port 0 enable RX?
  I_LD(R0, R3, RTCMEM_PORT0_RX_ENABLE),
  M_BGE(LABEL_PORT0_ENABLE_RX, 1),                        // yes, jump to LABEL_PORT0_ENABLE_RX

  // port 0 disable RX?
  I_LD(R0, R3, RTCMEM_PORT0_RX_DISABLE),
  M_BGE(LABEL_PORT0_STOP_RX, 1),                          // yes, jump to LABEL_PORT0_STOP_RX

  // port 1 TX?
  I_LD(R0, R3, RTCMEM_PORT1_TX),
  M_BGE(LABEL_PORT1_TX, 1),                               // yes, jump to LABEL_PORT1_TX

  // port 1 enable RX?
  I_LD(R0, R3, RTCMEM_PORT1_RX_ENABLE),
  M_BGE(LABEL_PORT1_ENABLE_RX, 1),                        // yes, jump to LABEL_PORT1_ENABLE_RX

  // port 1 disable RX?
  I_LD(R0, R3, RTCMEM_PORT1_RX_DISABLE),
  M_BGE(LABEL_PORT1_STOP_RX, 1),                          // yes, jump to LABEL_PORT1_STOP_RX

  // check RX from port 0 or port 1
  M_BX(LABEL_RX),


  /////////////////////////////////////////////////////////////////////////////////////////////
  // LABEL_PORT0_ENABLE_RX - Configure port 0 as RX

M_LABEL(LABEL_PORT0_ENABLE_RX),

  // set RX flag for port 0
  I_MOVI(R0, 1),
  I_ST(R0, R3, RTCMEM_PORT0_RX_ENABLED),                  // [RTCMEM_PORT0_RX_ENABLED] = 1

  // Configure CLK and DAT as inputs
  CONFIGURE_CLK_INPUT(PS2_PORT0),
  CONFIGURE_DAT_INPUT(PS2_PORT0),

  // Reset command
  I_MOVI(R0, 0),
  I_ST(R0, R3, RTCMEM_PORT0_RX_ENABLE),                   // [RTCMEM_PORT0_RX_ENABLE] = 0

  M_BX(LABEL_RX),


  /////////////////////////////////////////////////////////////////////////////////////////////
  // LABEL_PORT1_ENABLE_RX - Configure port 1 as RX

M_LABEL(LABEL_PORT1_ENABLE_RX),

  // set RX flag for port 1
  I_MOVI(R0, 1),
  I_ST(R0, R3, RTCMEM_PORT1_RX_ENABLED),                  // [RTCMEM_PORT1_RX_ENABLED] = 1

  // Configure CLK and DAT as inputs
  CONFIGURE_CLK_INPUT(PS2_PORT1),
  CONFIGURE_DAT_INPUT(PS2_PORT1),

  // Reset command
  I_MOVI(R0, 0),
  I_ST(R0, R3, RTCMEM_PORT1_RX_ENABLE),                   // [RTCMEM_PORT1_RX_ENABLE] = 0

  M_BX(LABEL_RX),


  /////////////////////////////////////////////////////////////////////////////////////////////
  // LABEL_PORT0_STOP_RX - Stop port 0 RX (drive CLK low)

M_LABEL(LABEL_PORT0_STOP_RX),

  PERM_PORT0_DISABLE(),

  // Reset command
  I_MOVI(R0, 0),
  I_ST(R0, R3, RTCMEM_PORT0_RX_DISABLE),                 // [RTCMEM_PORT0_RX_DISABLE] = 0

  M_BX(LABEL_RX),


  /////////////////////////////////////////////////////////////////////////////////////////////
  // LABEL_PORT1_STOP_RX - Stop port 1 RX (drive CLK low)

M_LABEL(LABEL_PORT1_STOP_RX),

  PERM_PORT1_DISABLE(),

  // Reset command
  I_MOVI(R0, 0),
  I_ST(R0, R3, RTCMEM_PORT1_RX_DISABLE),                 // [RTCMEM_PORT1_RX_DISABLE] = 0

  M_BX(LABEL_RX),


  /////////////////////////////////////////////////////////////////////////////////////////////
  // LABEL_PORT0_TX - Send data
  // Port 0 is automatically enabled for RX at the end

M_LABEL(LABEL_PORT0_TX),

  // Send the word in RTCMEM_PORT0_DATAOUT

  // put in R2 the word to send (10 bits: data, parity and stop bit)
  I_LD(R2, R3, RTCMEM_PORT0_DATAOUT),                  // R2 = [RTCMEM_PORT0_DATAOUT]

  // reset the bit counter (STAGE register: 0...7 = data0, 8 = parity, 9 = stop bit)
  I_STAGERSTI(),                                       // STAGE = 0

  // disable port 1?
  TEMP_PORT1_DISABLE(),

  // maintain CLK and DAT low for 200us
  CONFIGURE_CLK_OUTPUT(PS2_PORT0),
  WRITE_CLK(PS2_PORT0, 0),
  M_DELAY_US(200),
  CONFIGURE_DAT_OUTPUT(PS2_PORT0),
  WRITE_DAT(PS2_PORT0, 0),

  // configure CLK as input
  CONFIGURE_CLK_INPUT(PS2_PORT0),

M_LABEL(LABEL_PORT0_TX_NEXT_BIT),

  // wait for CLK = LOW

  // temporarily use R3 (which is 0) as timeout counter
  I_ADDI(R3, R3, 1),
  I_MOVR(R0, R3),
  M_BGE(LABEL_PORT0_TX_EXIT, CLK_TX_TIMEOUT_VAL),      // jump to LABEL_PORT0_TX_EXIT on CLK timeout

  // read CLK
  READ_CLK(PS2_PORT0),                                 // R0 = CLK

  // repeat if CLK is high
  M_BGE(LABEL_PORT0_TX_NEXT_BIT, 1),                   // jump to LABEL_PORT0_TX_NEXT_BIT if R0 >= 1

  // R3 is no more timeout counter, but variables base
  I_MOVI(R3, 0),

  // bit 10 is the ACK from keyboard, don't send anything, just bypass
  M_STAGEBGE(LABEL_PORT0_TX_WAIT_CLK_HIGH, 10),        // jump to LABEL_PORT0_TX_WAIT_CLK_HIGH if STAGE >= 10

  // CLK is LOW, we are ready to send the bit (LSB of R0)
  I_ANDI(R0, R2, 1),                                   // R0 = R2 & 1
  WRITE_DAT_R0(PS2_PORT0),                             // DAT = LSB of R0

M_LABEL(LABEL_PORT0_TX_WAIT_CLK_HIGH),

  // Wait for CLK = HIGH

  // read CLK
  READ_CLK(PS2_PORT0),                                 // R0 = CLK

  // repeat if CLK is low
  M_BL(LABEL_PORT0_TX_WAIT_CLK_HIGH, 1),               // jump to LABEL_PORT0_TX_WAIT_CLK_HIGH if R0 < 1

  // shift the sending word 1 bit to the right (prepare next bit to send)
  I_RSHI(R2, R2, 1),                                   // R2 = R2 >> 1

  // increment bit count
  I_STAGEINCI(1),                                      // STAGE = STAGE + 1

  // end of word? if not send then another bit
  M_STAGEBL(LABEL_PORT0_TX_NEXT_BIT, 11),              // jump to LABEL_PORT0_TX_NEXT_BIT if STAGE < 11

M_LABEL(LABEL_PORT0_TX_EXIT),

  // R3 is no more timeout counter, but variables base
  I_MOVI(R3, 0),

  // back to RX mode
  CONFIGURE_DAT_INPUT(PS2_PORT0),
  I_MOVI(R0, 1),
  I_ST(R0, R3, RTCMEM_PORT0_RX_ENABLED),               // [RTCMEM_PORT0_RX_ENABLED] = 1

  // reset command field
  I_MOVI(R0, 0),
  I_ST(R0, R3, RTCMEM_PORT0_TX),                       // [RTCMEM_PORT0_TX] = 0

  // re-enable port 1?
  TEMP_PORT1_ENABLE(),

  // from now R2 is used as CPU re-wake timeout
  I_MOVI(R2, 0),

  // go to command loop
  M_BX(LABEL_RX),


  /////////////////////////////////////////////////////////////////////////////////////////////
  // LABEL_PORT1_TX - Send data
  // Port 1 is automatically enabled for RX at the end

M_LABEL(LABEL_PORT1_TX),

  // Send the word in RTCMEM_PORT1_DATAOUT

  // put in R2 the word to send (10 bits: data, parity and stop bit)
  I_LD(R2, R3, RTCMEM_PORT1_DATAOUT),                  // R2 = [RTCMEM_PORT1_DATAOUT]

  // reset the bit counter (STAGE register: 0...7 = data0, 8 = parity, 9 = stop bit)
  I_STAGERSTI(),                                       // STAGE = 0

  // disable port 0?
  TEMP_PORT0_DISABLE(),

  // maintain CLK and DAT low for 200us
  CONFIGURE_CLK_OUTPUT(PS2_PORT1),
  WRITE_CLK(PS2_PORT1, 0),
  M_DELAY_US(200),
  CONFIGURE_DAT_OUTPUT(PS2_PORT1),
  WRITE_DAT(PS2_PORT1, 0),

  // configure CLK as input
  CONFIGURE_CLK_INPUT(PS2_PORT1),

M_LABEL(LABEL_PORT1_TX_NEXT_BIT),

  // wait for CLK = LOW

  // temporarily use R3 (which is 0) as timeout counter
  I_ADDI(R3, R3, 1),
  I_MOVR(R0, R3),
  M_BGE(LABEL_PORT1_TX_EXIT, CLK_TX_TIMEOUT_VAL),      // jump to LABEL_PORT1_TX_EXIT on CLK timeout

  // read CLK
  READ_CLK(PS2_PORT1),                                 // R0 = CLK

  // repeat if CLK is high
  M_BGE(LABEL_PORT1_TX_NEXT_BIT, 1),                   // jump to LABEL_PORT1_TX_NEXT_BIT if R0 >= 1

  // R3 is no more timeout counter, but variables base
  I_MOVI(R3, 0),

  // bit 10 is the ACK from keyboard, don't send anything, just bypass
  M_STAGEBGE(LABEL_PORT1_TX_WAIT_CLK_HIGH, 10),        // jump to LABEL_PORT1_TX_WAIT_CLK_HIGH if STAGE >= 10

  // CLK is LOW, we are ready to send the bit (LSB of R0)
  I_ANDI(R0, R2, 1),                                   // R0 = R2 & 1
  WRITE_DAT_R0(PS2_PORT1),                             // DAT = LSB of R0

M_LABEL(LABEL_PORT1_TX_WAIT_CLK_HIGH),

  // Wait for CLK = HIGH

  // read CLK
  READ_CLK(PS2_PORT1),                                 // R0 = CLK

  // repeat if CLK is low
  M_BL(LABEL_PORT1_TX_WAIT_CLK_HIGH, 1),               // jump to LABEL_PORT1_TX_WAIT_CLK_HIGH if R0 < 1

  // shift the sending word 1 bit to the right (prepare next bit to send)
  I_RSHI(R2, R2, 1),                                   // R2 = R2 >> 1

  // increment bit count
  I_STAGEINCI(1),                                      // STAGE = STAGE + 1

  // end of word? if not send then another bit
  M_STAGEBL(LABEL_PORT1_TX_NEXT_BIT, 11),              // jump to LABEL_PORT1_TX_NEXT_BIT if STAGE < 11

M_LABEL(LABEL_PORT1_TX_EXIT),

  // R3 is no more timeout counter, but variables base
  I_MOVI(R3, 0),

  // back to RX mode
  CONFIGURE_DAT_INPUT(PS2_PORT1),
  I_MOVI(R0, 1),
  I_ST(R0, R3, RTCMEM_PORT1_RX_ENABLED),               // [RTCMEM_PORT1_RX_ENABLED] = 1

  // reset command field
  I_MOVI(R0, 0),
  I_ST(R0, R3, RTCMEM_PORT1_TX),                       // [RTCMEM_PORT0_TX] = 0

  // re-enable port 0?
  TEMP_PORT0_ENABLE(),

  // from now R2 is used as CPU re-wake timeout
  I_MOVI(R2, 0),

  // go to command loop
  M_BX(LABEL_RX),


  /////////////////////////////////////////////////////////////////////////////////////////////
  // LABEL_PORT0_RX_CLK_TIMEOUT

M_LABEL(LABEL_PORT0_RX_CLK_TIMEOUT),

  // R3 is no more timeout counter, but variables base
  I_MOVI(R3, 0),

  // disable port 0
  PERM_PORT0_DISABLE(),

  // re-enable port 1?
  TEMP_PORT1_ENABLE(),

  // set port 0 timeout flag
  I_MOVI(R0, 1),
  I_ST(R0, R3, RTCMEM_PORT0_RX_CLK_TIMEOUT),

  I_WAKE(),

  // from now R2 is used as CPU re-wake timeout
  I_MOVI(R2, 0),

  M_BX(LABEL_RX),


  /////////////////////////////////////////////////////////////////////////////////////////////
  // LABEL_PORT1_RX_CLK_TIMEOUT

M_LABEL(LABEL_PORT1_RX_CLK_TIMEOUT),

  // R3 is no more timeout counter, but variables base
  I_MOVI(R3, 0),

  // disable port 1
  PERM_PORT1_DISABLE(),

  // re-enable port 0?
  TEMP_PORT0_ENABLE(),

  // set port 1 timeout flag
  I_MOVI(R0, 1),
  I_ST(R0, R3, RTCMEM_PORT1_RX_CLK_TIMEOUT),

  I_WAKE(),

  // from now R2 is used as CPU re-wake timeout
  I_MOVI(R2, 0),

  //M_BX(LABEL_RX),


  /////////////////////////////////////////////////////////////////////////////////////////////
  // LABEL_RX - Check for new data from port 0 and port 1
  // During reception port 1 is disabled for RX. It will be enabled at the end.
  // After received data the port 0 remains disabled for RX.

M_LABEL(LABEL_RX),

  // Check for RX from port 0?
  I_LD(R0, R3, RTCMEM_PORT0_RX_ENABLED),
  M_BGE(LABEL_PORT0_RX_CHECK_CLK, 1),                       // yes, jump to LABEL_PORT0_RX_CHECK_CLK

  // no, port 0 is not enabled, maybe we are waiting for an ack from SoC, increment and check the re-wake timeout counter (R2)
  I_ADDI(R2, R2, 1),                                        // R2 = R2 + 1 (increment counter)
  I_MOVR(R0, R2),
  M_BL(LABEL_RX_NEXT, WAKE_THRESHOLD),                      // check the other port if counter is below WAKE_THRESHOLD
  I_WAKE(),                                                 // need to wake CPU
  I_MOVI(R2, 0),                                            // reset counter
  M_BX(LABEL_RX_NEXT),                                      // check the other port

M_LABEL(LABEL_PORT0_RX_CHECK_CLK),

  // read CLK
  READ_CLK(PS2_PORT0),                                      // R0 = CLK

  // CLK is low?
  M_BGE(LABEL_RX_NEXT, 1),                                  // no, jump to LABEL_RX_NEXT

  // yes, from here we are receiving data from port 0

  // reset state variables
  I_MOVI(R0, 0),
  I_ST(R0, R3, RTCMEM_PORT0_DATAIN),                        // [RTCMEM_PORT0_DATAIN] = 0
  I_STAGERSTI(),                                            // STAGE = 0 (STAGE is the bit count)
  I_MOVI(R2, 0),                                            // R2 = 0 (R2 represents last read value from CLK)

  // disable port 1?
  TEMP_PORT1_DISABLE(),

  // CLK is low so jump directly to DAT capture without recheck
  M_BX(LABEL_PORT0_RX_CLK_IS_LOW),

M_LABEL(LABEL_PORT0_RX_WAIT_LOOP),

  // temporarily use R3 (which is 0) as timeout counter
  I_ADDI(R3, R3, 1),
  I_MOVR(R0, R3),
  M_BGE(LABEL_PORT0_RX_CLK_TIMEOUT, CLK_RX_TIMEOUT_VAL),    // jump to LABEL_PORT0_RX_CLK_TIMEOUT on CLK timeout

  // read CLK
  READ_CLK(PS2_PORT0),                                      // R0 = CLK

  // CLK changed?
  I_SUBR(R1, R2, R0),                                       // R1 = R2 - R0
  M_BXZ(LABEL_PORT0_RX_WAIT_LOOP),                          // no, repeat to LABEL_PORT0_RX_WAIT_LOOP

  // R3 is no more timeout counter, but variables base
  I_MOVI(R3, 0),

  // update last bit received
  I_MOVR(R2, R0),                                           // R2 = R0

  // is CLK high?
  M_BGE(LABEL_PORT0_RX_CLK_IS_HIGH, 1),

M_LABEL(LABEL_PORT0_RX_CLK_IS_LOW),

  // CLK is Low, capture DAT value
  READ_DAT(PS2_PORT0),                                      // R0 = DAT

  // merge with data word and shift right by 1 the received word
  I_LSHI(R0, R0, 11),                                       // R0 = R0 << 11
  I_LD(R1, R3, RTCMEM_PORT0_DATAIN),                        // R1 = [RTCMEM_PORT0_DATAIN]
  I_ORR(R1, R1, R0),                                        // R1 = R1 | R0
  I_RSHI(R1, R1, 1),                                        // R1 = R1 >> 1
  I_ST(R1, R3, RTCMEM_PORT0_DATAIN),                        // [RTCMEM_PORT0_DATAIN] = R1

  // repeat
  M_BX(LABEL_PORT0_RX_WAIT_LOOP),

M_LABEL(LABEL_PORT0_RX_CLK_IS_HIGH),

  // increment bit count
  I_STAGEINCI(1),                                           // STAGE = STAGE + 1

  // end of word? if not get another bit
  M_STAGEBL(LABEL_PORT0_RX_WAIT_LOOP, 11),                  // jump to LABEL_PORT0_RX_WAIT_LOOP if STAGE < 11

  // End of word, disable port 0
  PERM_PORT0_DISABLE(),

  // set RX flag
  I_MOVI(R0, 1),
  I_ST(R0, R3, RTCMEM_PORT0_RX),                            // [RTCMEM_PORT0_RX] = 1

  // re-enable port 1?
  TEMP_PORT1_ENABLE(),

  // don't check RTC_CNTL_RDY_FOR_WAKEUP_S of RTC_CNTL_LOW_POWER_ST_REG because it never gets active, due
  // the fact ULP never calls HALT or SoC is always active.
  I_WAKE(),

  // from now R2 is used as CPU re-wake timeout
  I_MOVI(R2, 0),


  /////////////////////////////////////////////////////////////////////////////////////////////
  // Check for new data from port 1
  // During reception port 0 is disabled for RX. It will be enabled at the end.
  // After received data the port 0 remains disabled for RX.

M_LABEL(LABEL_RX_NEXT),

  // Check for RX from port 1?
  I_LD(R0, R3, RTCMEM_PORT1_RX_ENABLED),
  M_BGE(LABEL_PORT1_RX_CHECK_CLK, 1),                       // yes, jump to LABEL_PORT1_RX_CHECK_CLK

  // no, port 1 is not enabled, maybe we are waiting for an ack from SoC, increment and check the re-wake timeout counter (R2)
  I_ADDI(R2, R2, 1),                                        // R2 = R2 + 1 (increment counter)
  I_MOVR(R0, R2),
  M_LONG_BL(LABEL_WAIT_COMMAND, WAKE_THRESHOLD),            // return to main loop if counter is below WAKE_THRESHOLD
  I_WAKE(),                                                 // need to wake CPU
  I_MOVI(R2, 0),                                            // reset counter
  M_BX(LABEL_WAIT_COMMAND),                                 // return to main loop

M_LABEL(LABEL_PORT1_RX_CHECK_CLK),

  // read CLK
  READ_CLK(PS2_PORT1),                                      // R0 = CLK

  // CLK is low?
  M_LONG_BGE(LABEL_WAIT_COMMAND, 1),                        // no, jump to LABEL_WAIT_COMMAND

  // yes, from here we are receiving data from port 0

  // reset state variables
  I_MOVI(R0, 0),
  I_ST(R0, R3, RTCMEM_PORT1_DATAIN),                        // [RTCMEM_PORT1_DATAIN] = 0
  I_STAGERSTI(),                                            // STAGE = 0 (STAGE is the bit count)
  I_MOVI(R2, 0),                                            // R2 = 0 (R2 represents last read value from CLK)

  // disable port 0?
  TEMP_PORT0_DISABLE(),

  // CLK is low so jump directly to DAT capture without recheck
  M_BX(LABEL_PORT1_RX_CLK_IS_LOW),

M_LABEL(LABEL_PORT1_RX_WAIT_LOOP),

  // temporarily use R3 (which is 0) as timeout counter
  I_ADDI(R3, R3, 1),
  I_MOVR(R0, R3),
  M_BGE(LABEL_PORT1_RX_CLK_TIMEOUT, CLK_RX_TIMEOUT_VAL),    // jump to LABEL_PORT1_RX_CLK_TIMEOUT on CLK timeout

  // read CLK
  READ_CLK(PS2_PORT1),                                      // R0 = CLK

  // CLK changed?
  I_SUBR(R1, R2, R0),                                       // R1 = R2 - R0
  M_BXZ(LABEL_PORT1_RX_WAIT_LOOP),                          // no, repeat to LABEL_PORT1_RX_WAIT_LOOP

  // R3 is no more timeout counter, but variables base
  I_MOVI(R3, 0),

  // update last bit received
  I_MOVR(R2, R0),                                           // R2 = R0

  // is CLK high?
  M_BGE(LABEL_PORT1_RX_CLK_IS_HIGH, 1),

M_LABEL(LABEL_PORT1_RX_CLK_IS_LOW),

  // CLK is Low, capture DAT value
  READ_DAT(PS2_PORT1),                                      // R0 = DAT

  // merge with data word and shift right by 1 the received word
  I_LSHI(R0, R0, 11),                                       // R0 = R0 << 11
  I_LD(R1, R3, RTCMEM_PORT1_DATAIN),                        // R1 = [RTCMEM_PORT1_DATAIN]
  I_ORR(R1, R1, R0),                                        // R1 = R1 | R0
  I_RSHI(R1, R1, 1),                                        // R1 = R1 >> 1
  I_ST(R1, R3, RTCMEM_PORT1_DATAIN),                        // [RTCMEM_PORT1_DATAIN] = R1

  // repeat
  M_BX(LABEL_PORT1_RX_WAIT_LOOP),

M_LABEL(LABEL_PORT1_RX_CLK_IS_HIGH),

  // increment bit count
  I_STAGEINCI(1),                                           // STAGE = STAGE + 1

  // end of word? if not get another bit
  M_STAGEBL(LABEL_PORT1_RX_WAIT_LOOP, 11),                  // jump to LABEL_PORT1_RX_WAIT_LOOP if STAGE < 11

  // End of word, disable port 1
  PERM_PORT1_DISABLE(),

  // set RX flag
  I_MOVI(R0, 1),
  I_ST(R0, R3, RTCMEM_PORT1_RX),                            // [RTCMEM_PORT1_RX] = 1

  // re-enable port 0?
  TEMP_PORT0_ENABLE(),

  // don't check RTC_CNTL_RDY_FOR_WAKEUP_S of RTC_CNTL_LOW_POWER_ST_REG because it never gets active, due
  // the fact ULP never calls HALT or SoC is always active.
  I_WAKE(),

  // from now R2 is used as CPU re-wake timeout
  I_MOVI(R2, 0),

  // return to main loop
  M_BX(LABEL_WAIT_COMMAND),

};




// Allowed GPIOs: GPIO_NUM_0, GPIO_NUM_2, GPIO_NUM_4, GPIO_NUM_12, GPIO_NUM_13, GPIO_NUM_14, GPIO_NUM_15, GPIO_NUM_25, GPIO_NUM_26, GPIO_NUM_27, GPIO_NUM_32, GPIO_NUM_33
// Not allowed from GPIO_NUM_34 to GPIO_NUM_39
// prg_start in 32 bit words
// size in 32 bit words
static void replace_placeholders(uint32_t prg_start, int size, bool port0Enabled, gpio_num_t port0_clkGPIO, gpio_num_t port0_datGPIO, bool port1Enabled, gpio_num_t port1_clkGPIO, gpio_num_t port1_datGPIO)
{
  uint32_t CLK_rtc_gpio_num[2], CLK_rtc_gpio_reg[2], CLK_rtc_gpio_ie_s[2];
  uint32_t DAT_rtc_gpio_num[2], DAT_rtc_gpio_reg[2], DAT_rtc_gpio_ie_s[2];

  if (port0Enabled) {
    #if FABGL_ESP_IDF_VERSION <= FABGL_ESP_IDF_VERSION_VAL(3, 3, 5)
    CLK_rtc_gpio_num[0]  = (uint32_t) rtc_gpio_desc[port0_clkGPIO].rtc_num;
    CLK_rtc_gpio_reg[0]  = rtc_gpio_desc[port0_clkGPIO].reg;
    CLK_rtc_gpio_ie_s[0] = (uint32_t) ffs(rtc_gpio_desc[port0_clkGPIO].ie) - 1;
    DAT_rtc_gpio_num[0]  = (uint32_t) rtc_gpio_desc[port0_datGPIO].rtc_num;
    DAT_rtc_gpio_reg[0]  = rtc_gpio_desc[port0_datGPIO].reg;
    DAT_rtc_gpio_ie_s[0] = (uint32_t) ffs(rtc_gpio_desc[port0_datGPIO].ie) - 1;
    #else
    int port0_clkIO = rtc_io_number_get(port0_clkGPIO);
    CLK_rtc_gpio_num[0]  = port0_clkIO;
    CLK_rtc_gpio_reg[0]  = rtc_io_desc[port0_clkIO].reg;
    CLK_rtc_gpio_ie_s[0] = (uint32_t) ffs(rtc_io_desc[port0_clkIO].ie) - 1;
    int port0_datIO = rtc_io_number_get(port0_datGPIO);
    DAT_rtc_gpio_num[0]  = port0_datIO;
    DAT_rtc_gpio_reg[0]  = rtc_io_desc[port0_datIO].reg;
    DAT_rtc_gpio_ie_s[0] = (uint32_t) ffs(rtc_io_desc[port0_datIO].ie) - 1;
    #endif
  }

  if (port1Enabled) {
    #if FABGL_ESP_IDF_VERSION <= FABGL_ESP_IDF_VERSION_VAL(3, 3, 5)
    CLK_rtc_gpio_num[1]  = (uint32_t) rtc_gpio_desc[port1_clkGPIO].rtc_num;
    CLK_rtc_gpio_reg[1]  = rtc_gpio_desc[port1_clkGPIO].reg;
    CLK_rtc_gpio_ie_s[1] = (uint32_t) ffs(rtc_gpio_desc[port1_clkGPIO].ie) - 1;
    DAT_rtc_gpio_num[1]  = (uint32_t) rtc_gpio_desc[port1_datGPIO].rtc_num;
    DAT_rtc_gpio_reg[1]  = rtc_gpio_desc[port1_datGPIO].reg;
    DAT_rtc_gpio_ie_s[1] = (uint32_t) ffs(rtc_gpio_desc[port1_datGPIO].ie) - 1;
    #else
    int port1_clkIO = rtc_io_number_get(port1_clkGPIO);
    CLK_rtc_gpio_num[1]  = port1_clkIO;
    CLK_rtc_gpio_reg[1]  = rtc_io_desc[port1_clkIO].reg;
    CLK_rtc_gpio_ie_s[1] = (uint32_t) ffs(rtc_io_desc[port1_clkIO].ie) - 1;
    int port1_datIO = rtc_io_number_get(port1_datGPIO);
    DAT_rtc_gpio_num[1]  = port1_datIO;
    DAT_rtc_gpio_reg[1]  = rtc_io_desc[port1_datIO].reg;
    DAT_rtc_gpio_ie_s[1] = (uint32_t) ffs(rtc_io_desc[port1_datIO].ie) - 1;
    #endif
  }

  for (uint32_t i = 0; i < size; ++i) {
    ulp_insn_t * ins = (ulp_insn_t *) RTC_SLOW_MEM + i;
    if (ins->macro.opcode == OPCODE_PLACEHOLDER) {
      int ps2port = ins->macro.unused;
      if ((port0Enabled && ps2port == 0) || (port1Enabled && ps2port == 1)) {
        ins->macro.unused = 0;
        switch (ins->macro.sub_opcode) {
          case SUB_OPCODE_DAT_ENABLE_OUTPUT:
            if (ins->macro.label)
              *ins = (ulp_insn_t) I_WR_REG_BIT(RTC_GPIO_ENABLE_W1TS_REG, DAT_rtc_gpio_num[ps2port] + RTC_GPIO_ENABLE_W1TS_S, 1);
            else
              *ins = (ulp_insn_t) I_WR_REG_BIT(RTC_GPIO_ENABLE_W1TC_REG, DAT_rtc_gpio_num[ps2port] + RTC_GPIO_ENABLE_W1TC_S, 1);
            break;
          case SUB_OPCODE_DAT_ENABLE_INPUT:
            *ins = (ulp_insn_t) I_WR_REG_BIT(DAT_rtc_gpio_reg[ps2port], DAT_rtc_gpio_ie_s[ps2port], ins->macro.label);
            break;
          case SUB_OPCODE_CLK_ENABLE_OUTPUT:
            if (ins->macro.label)
              *ins = (ulp_insn_t) I_WR_REG_BIT(RTC_GPIO_ENABLE_W1TS_REG, CLK_rtc_gpio_num[ps2port] + RTC_GPIO_ENABLE_W1TS_S, 1);
            else
              *ins = (ulp_insn_t) I_WR_REG_BIT(RTC_GPIO_ENABLE_W1TC_REG, CLK_rtc_gpio_num[ps2port] + RTC_GPIO_ENABLE_W1TC_S, 1);
            break;
          case SUB_OPCODE_CLK_ENABLE_INPUT:
            *ins = (ulp_insn_t) I_WR_REG_BIT(CLK_rtc_gpio_reg[ps2port], CLK_rtc_gpio_ie_s[ps2port], ins->macro.label);
            break;
          case SUB_OPCODE_READ_CLK:
            *ins = (ulp_insn_t) I_RD_REG(RTC_GPIO_IN_REG, CLK_rtc_gpio_num[ps2port] + RTC_GPIO_IN_NEXT_S, CLK_rtc_gpio_num[ps2port] + RTC_GPIO_IN_NEXT_S);
            break;
          case SUB_OPCODE_READ_DAT:
            *ins = (ulp_insn_t) I_RD_REG(RTC_GPIO_IN_REG, DAT_rtc_gpio_num[ps2port] + RTC_GPIO_IN_NEXT_S, DAT_rtc_gpio_num[ps2port] + RTC_GPIO_IN_NEXT_S);
            break;
          case SUB_OPCODE_WRITE_CLK:
            *ins = (ulp_insn_t) I_WR_REG_BIT(RTC_GPIO_OUT_REG, CLK_rtc_gpio_num[ps2port] + RTC_GPIO_IN_NEXT_S, ins->macro.label);
            break;
          case SUB_OPCODE_WRITE_DAT:
            *ins = (ulp_insn_t) I_WR_REG_BIT(RTC_GPIO_OUT_REG, DAT_rtc_gpio_num[ps2port] + RTC_GPIO_IN_NEXT_S, ins->macro.label);
            break;
        }
      }
    }
  }
}


PS2Controller *    PS2Controller::s_instance = nullptr;
Keyboard *         PS2Controller::s_keyboard = nullptr;
Mouse *            PS2Controller::s_mouse    = nullptr;
bool               PS2Controller::s_keyboardAllocated = false;
bool               PS2Controller::s_mouseAllocated    = false;
bool               PS2Controller::s_portEnabled[2];
intr_handle_t      PS2Controller::s_ULPWakeISRHandle;
bool               PS2Controller::s_parityError[2];
bool               PS2Controller::s_syncError[2];
bool               PS2Controller::s_CLKTimeOutError[2];
QueueHandle_t      PS2Controller::s_dataIn[2];
SemaphoreHandle_t  PS2Controller::s_portLock[2];
bool               PS2Controller::s_initDone = false;


PS2Controller::PS2Controller()
{
  if (!s_instance)
    s_instance = this;
}


PS2Controller::~PS2Controller()
{
  end();
  if (this == s_instance)
    s_instance = nullptr;
}


// Note: GPIO_UNUSED is a placeholder used to disable PS/2 port 1.
void PS2Controller::begin(gpio_num_t port0_clkGPIO, gpio_num_t port0_datGPIO, gpio_num_t port1_clkGPIO, gpio_num_t port1_datGPIO)
{
  // ULP stuff is always active, even when end() is called
  if (!s_initDone) {

    s_portEnabled[0] = (port0_clkGPIO != GPIO_UNUSED && port0_datGPIO != GPIO_UNUSED);
    s_portEnabled[1] = (port1_clkGPIO != GPIO_UNUSED && port1_datGPIO != GPIO_UNUSED);

    if (s_portEnabled[0]) {
      if (!rtc_gpio_is_valid_gpio(port0_clkGPIO) || !rtc_gpio_is_valid_gpio(port0_datGPIO)) {
        ESP_LOGE("FabGL", "Invalid PS/2 Port 0 pins");
        s_portEnabled[0] = false;
      } else {
        rtc_gpio_init(port0_clkGPIO);
        rtc_gpio_init(port0_datGPIO);
      }
    }

    if (s_portEnabled[1]) {
      if (!rtc_gpio_is_valid_gpio(port1_clkGPIO) || !rtc_gpio_is_valid_gpio(port1_datGPIO)) {
        ESP_LOGE("FabGL", "Invalid PS/2 Port 1 pins");
        s_portEnabled[1] = false;
      } else {
        rtc_gpio_init(port1_clkGPIO);
        rtc_gpio_init(port1_datGPIO);
      }
    }

    // clear ULP memory (without this may fail to run ULP on softreset)
    for (int i = RTCMEM_PROG_START; i < RTCMEM_LASTVAR; ++i)
      RTC_SLOW_MEM[i] = 0x0000;

    // process, load and execute ULP program
    size_t size = sizeof(ULP_code) / sizeof(ulp_insn_t);
    ulp_process_macros_and_load_ex(RTCMEM_PROG_START, ULP_code, &size);         // convert macros to ULP code
    replace_placeholders(RTCMEM_PROG_START, size, s_portEnabled[0], port0_clkGPIO, port0_datGPIO, s_portEnabled[1], port1_clkGPIO, port1_datGPIO); // replace GPIO placeholders
    //printf("prog size = %d\n", size);
    assert(size < RTCMEM_VARS_START && "ULP Program too long, increase RTCMEM_VARS_START");

    REG_SET_FIELD(SENS_SAR_START_FORCE_REG, SENS_PC_INIT, RTCMEM_PROG_START);   // set entry point
    SET_PERI_REG_MASK(SENS_SAR_START_FORCE_REG, SENS_ULP_CP_FORCE_START_TOP);   // enable FORCE START

    for (int p = 0; p < 2; ++p) {
      RTC_SLOW_MEM[RTCMEM_PORT0_TX + p]         = 0;
      RTC_SLOW_MEM[RTCMEM_PORT0_RX_ENABLE + p]  = 0;
      RTC_SLOW_MEM[RTCMEM_PORT0_RX_DISABLE + p] = 0;
      RTC_SLOW_MEM[RTCMEM_PORT0_RX + p]         = 0;
      RTC_SLOW_MEM[RTCMEM_PORT0_RX_ENABLED + p] = 0;
      s_parityError[p] = s_syncError[p] = false;
      s_dataIn[p]   = (s_portEnabled[p] ? xQueueCreate(1, sizeof(uint16_t)) : nullptr);
      s_portLock[p] = (s_portEnabled[p] ? xSemaphoreCreateRecursiveMutex() : nullptr);
      enableRX(p);
    }

    // ULP start
    SET_PERI_REG_MASK(SENS_SAR_START_FORCE_REG, SENS_ULP_CP_START_TOP);

    // install RTC interrupt handler (on ULP Wake() instruction)
    // note about ESP_INTR_FLAG_LEVEL2: this is necessary in order to work reliably with interrupt intensive VGATextController, when running on the same core
    // On some boards only core "1" can -read- RTC slow memory and receive interrupts, hence we have to force core 1.
    esp_intr_alloc_pinnedToCore(ETS_RTC_CORE_INTR_SOURCE, ESP_INTR_FLAG_LEVEL2, ULPWakeISR, nullptr, &s_ULPWakeISRHandle, 1);
    SET_PERI_REG_MASK(RTC_CNTL_INT_ENA_REG, RTC_CNTL_ULP_CP_INT_ENA);

    s_initDone = true;

  } else {

    // ULP already initialized
    for (int p = 0; p < 2; ++p) {
      RTC_SLOW_MEM[RTCMEM_PORT0_TX + p]         = 0;
      RTC_SLOW_MEM[RTCMEM_PORT0_RX_ENABLE + p]  = 0;
      RTC_SLOW_MEM[RTCMEM_PORT0_RX_DISABLE + p] = 0;
      RTC_SLOW_MEM[RTCMEM_PORT0_RX + p]         = 0;
      RTC_SLOW_MEM[RTCMEM_PORT0_RX_ENABLED + p] = 0;
      s_parityError[p] = s_syncError[p] = false;
      if (s_portEnabled[p]) {
        xQueueReset(s_dataIn[p]);
        xSemaphoreGiveRecursive(s_portLock[p]);
        enableRX(p);
      }
    }
  }
}


void PS2Controller::begin(PS2Preset preset, KbdMode keyboardMode)
{
  end();

  bool generateVirtualKeys = (keyboardMode == KbdMode::GenerateVirtualKeys || keyboardMode == KbdMode::CreateVirtualKeysQueue);
  bool createVKQueue       = (keyboardMode == KbdMode::CreateVirtualKeysQueue);
  switch (preset) {
    case PS2Preset::KeyboardPort0_MousePort1:
      // both keyboard (port 0) and mouse configured (port 1)
      begin(GPIO_NUM_33, GPIO_NUM_32, GPIO_NUM_26, GPIO_NUM_27);
      setKeyboard(new Keyboard);
      keyboard()->begin(generateVirtualKeys, createVKQueue, 0);
      setMouse(new Mouse);
      mouse()->begin(1);
      s_keyboardAllocated = s_mouseAllocated = true;
      break;
    case PS2Preset::KeyboardPort1_MousePort0:
      // both keyboard (port 1) and mouse configured (port 0)
      begin(GPIO_NUM_33, GPIO_NUM_32, GPIO_NUM_26, GPIO_NUM_27);
      setMouse(new Mouse);
      mouse()->begin(0);
      setKeyboard(new Keyboard);
      keyboard()->begin(generateVirtualKeys, createVKQueue, 1);
      s_keyboardAllocated = s_mouseAllocated = true;
      break;
    case PS2Preset::KeyboardPort0:
      // only keyboard configured on port 0
      // this will call setKeyboard and begin()
      (new Keyboard)->begin(GPIO_NUM_33, GPIO_NUM_32, generateVirtualKeys, createVKQueue);
      s_keyboardAllocated = true;
      break;
    case PS2Preset::KeyboardPort1:
      // only keyboard configured on port 1
      // this will call setKeyboard and begin()
      (new Keyboard)->begin(GPIO_NUM_26, GPIO_NUM_27, generateVirtualKeys, createVKQueue);
      s_keyboardAllocated = true;
      break;
    case PS2Preset::MousePort0:
      // only mouse configured on port 0
      // this will call setMouse and begin()
      (new Mouse)->begin(GPIO_NUM_33, GPIO_NUM_32);
      s_mouseAllocated = true;
      break;
    case PS2Preset::MousePort1:
      // only mouse configured on port 1
      // this will call setMouse and begin()
      (new Mouse)->begin(GPIO_NUM_26, GPIO_NUM_27);
      s_mouseAllocated = true;
      break;
  };
}


void PS2Controller::end()
{
  if (s_initDone) {
    if (s_keyboardAllocated)
      delete s_keyboard;
    s_keyboard = nullptr;

    if (s_mouseAllocated)
      delete s_mouse;
    s_mouse = nullptr;

    for (int p = 0; p < 2; ++p)
      disableRX(p);
  }
}


void PS2Controller::disableRX(int PS2Port)
{
  if (s_portEnabled[PS2Port])
    RTC_SLOW_MEM[RTCMEM_PORT0_RX_DISABLE + PS2Port] = 1;
}


void PS2Controller::enableRX(int PS2Port)
{
  if (s_portEnabled[PS2Port]) {
    // enable RX only if there is not data waiting
    if (!dataAvailable(PS2Port))
      RTC_SLOW_MEM[RTCMEM_PORT0_RX_ENABLE + PS2Port] = 1;
  }
}


bool PS2Controller::dataAvailable(int PS2Port)
{
  return uxQueueMessagesWaiting(s_dataIn[PS2Port]);
}


// return -1 when no data is available
int PS2Controller::getData(int PS2Port, int timeOutMS)
{
  int r = -1;

  uint16_t w;
  if (xQueueReceive(s_dataIn[PS2Port], &w, msToTicks(timeOutMS))) {

    // check CLK timeout, parity, start and stop bits
    s_CLKTimeOutError[PS2Port] = (w == 0xffff);
    if (!s_CLKTimeOutError[PS2Port]) {
      uint8_t startBit = w & 1;
      uint8_t stopBit  = (w >> 10) & 1;
      uint8_t parity   = (w >> 9) & 1;
      r = (w >> 1) & 0xff;
      s_parityError[PS2Port] = (parity != !calcParity(r));
      s_syncError[PS2Port]   = (startBit != 0 || stopBit != 1);
      if (s_parityError[PS2Port] || s_syncError[PS2Port])
        r = -1;
    }

    // ULP leaves RX disables whenever receives data or CLK timeout, so we need to enable it here
    RTC_SLOW_MEM[RTCMEM_PORT0_RX_ENABLE + PS2Port] = 1;

  }

  return r;
}


void PS2Controller::sendData(uint8_t data, int PS2Port)
{
  if (s_portEnabled[PS2Port]) {
    RTC_SLOW_MEM[RTCMEM_PORT0_DATAOUT + PS2Port] = 0x200 | ((!calcParity(data) & 1) << 8) | data;  // 0x200 = stop bit. Start bit is not specified here.
    RTC_SLOW_MEM[RTCMEM_PORT0_TX + PS2Port]      = 1;
  }
}


bool PS2Controller::lock(int PS2Port, int timeOutMS)
{
  return s_portEnabled[PS2Port] ? xSemaphoreTakeRecursive(s_portLock[PS2Port], msToTicks(timeOutMS)) : true;
}


void PS2Controller::unlock(int PS2Port)
{
  if (s_portEnabled[PS2Port])
    xSemaphoreGiveRecursive(s_portLock[PS2Port]);
}



void IRAM_ATTR PS2Controller::ULPWakeISR(void * arg)
{
  uint32_t rtc_intr = READ_PERI_REG(RTC_CNTL_INT_ST_REG);

  if (rtc_intr & RTC_CNTL_SAR_INT_ST) {

    for (int p = 0; p < 2; ++p) {
      if (RTC_SLOW_MEM[RTCMEM_PORT0_RX + p] & 0xffff) {
        // RX
        uint16_t d = RTC_SLOW_MEM[RTCMEM_PORT0_DATAIN + p] & 0xffff;
        xQueueOverwriteFromISR(PS2Controller::s_dataIn[p], &d, nullptr);
        RTC_SLOW_MEM[RTCMEM_PORT0_RX + p] = 0;
      } else if (RTC_SLOW_MEM[RTCMEM_PORT0_RX_CLK_TIMEOUT + p] & 0xffff) {
        // CLK timeout
        uint16_t d = 0xffff;
        xQueueOverwriteFromISR(PS2Controller::s_dataIn[p], &d, nullptr);
        RTC_SLOW_MEM[RTCMEM_PORT0_RX_CLK_TIMEOUT + p] = 0;
      }
    }

  }

  // clear interrupt
  WRITE_PERI_REG(RTC_CNTL_INT_CLR_REG, rtc_intr);
}


} // end of namespace


