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


#include <strings.h>

#include "freertos/FreeRTOS.h"

#include "esp32/ulp.h"
#include "driver/rtc_io.h"
#include "soc/sens_reg.h"

#include "ps2controller.h"
#include "fabutils.h"
#include "ulp_macro_ex.h"
#include "keyboard.h"
#include "mouse.h"


fabgl::PS2ControllerClass PS2Controller;



namespace fabgl {


////////////////////////////////////////////////////////////////////////////
// Support for missing macros for operations on STAGE register

#define ALU_SEL_STAGE_INC 0
#define ALU_SEL_STAGE_DEC 1
#define ALU_SEL_STAGE_RST 2

#define SUB_OPCODE_STAGEB 2

// Increment stage register by immediate value (8 bit)
#define I_STAGEINCI(imm_) { .alu_imm = { \
    .dreg = 0, \
    .sreg = 0, \
    .imm = imm_, \
    .unused = 0, \
    .sel = ALU_SEL_STAGE_INC, \
    .sub_opcode = SUB_OPCODE_ALU_CNT, \
    .opcode = OPCODE_ALU } }

// Decrement stage register by immediate value (8 bit)
#define I_STAGEDECI(imm_) { .alu_imm = { \
    .dreg = 0, \
    .sreg = 0, \
    .imm = imm_, \
    .unused = 0, \
    .sel = ALU_SEL_STAGE_DEC, \
    .sub_opcode = SUB_OPCODE_ALU_CNT, \
    .opcode = OPCODE_ALU } }

// Reset stage register to zero
#define I_STAGERSTI() { .alu_imm = { \
    .dreg = 0, \
    .sreg = 0, \
    .imm = 0, \
    .unused = 0, \
    .sel = ALU_SEL_STAGE_RST, \
    .sub_opcode = SUB_OPCODE_ALU_CNT, \
    .opcode = OPCODE_ALU } }

// Branch relative if STAGE less than immediate value (8 bit)
#define I_STAGEBL(pc_offset, imm_value) { .b = { \
    .imm = imm_value, \
    .cmp = 0, \
    .offset = abs(pc_offset), \
    .sign = (pc_offset >= 0) ? 0 : 1, \
    .sub_opcode = SUB_OPCODE_STAGEB, \
    .opcode = OPCODE_BRANCH } }

// Branch relative if STAGE less or equal than immediate value (8 bit)
#define I_STAGEBLE(pc_offset, imm_value) { .b = { \
    .imm = imm_value, \
    .cmp = 1, \
    .offset = abs(pc_offset), \
    .sign = (pc_offset >= 0) ? 0 : 1, \
    .sub_opcode = SUB_OPCODE_STAGEB, \
    .opcode = OPCODE_BRANCH } }

// Branch relative if STAGE greater or equal than immediate value (8 bit)
#define I_STAGEBGE(pc_offset, imm_value) { .b = { \
    .imm = 0x8000 | imm_value, \
    .cmp = 0, \
    .offset = abs(pc_offset), \
    .sign = (pc_offset >= 0) ? 0 : 1, \
    .sub_opcode = SUB_OPCODE_STAGEB, \
    .opcode = OPCODE_BRANCH } }

// STAGE register branches to labels.
// ulp_process_macros_and_load() doesn't recognize labels on I_STAGEB## instructions, hence
// we use more than one instruction

#define M_STAGEBL(label_num, imm_value) \
    I_STAGEBGE(2, imm_value), \
    M_BX(label_num)

#define M_STAGEBGE(label_num, imm_value) \
    I_STAGEBL(2, imm_value), \
    M_BX(label_num)

#define M_STAGEBLE(label_num, imm_value) \
    I_STAGEBLE(2, imm_value), \
    I_STAGEBGE(2, imm_value), \
    M_BX(label_num)


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


////////////////////////////////////////////////////////////////////////////


#define PORT0_RX_BUFFER_SIZE 128
#define PORT1_RX_BUFFER_SIZE 1644


// Locations inside RTC low speed memory

#define RTCMEM_PROG_START           0x000  // where the program begins
#define RTCMEM_VARS_START           0x100  // where the variables begin

#define RTCMEM_PORT0_ENABLED        (RTCMEM_VARS_START +  0)  // if 1 then port 0 is enabled
#define RTCMEM_PORT0_MODE           (RTCMEM_VARS_START +  1)  // MODE_RECEIVE or MODE_SEND
#define RTCMEM_PORT0_WRITE_POS      (RTCMEM_VARS_START +  2)  // position of the next word to receive
#define RTCMEM_PORT0_WORD_RX_READY  (RTCMEM_VARS_START +  3)  // 1 when a word has been received (reset by ISR)
#define RTCMEM_PORT0_BIT            (RTCMEM_VARS_START +  4)  // send bit counter
#define RTCMEM_PORT0_STATE          (RTCMEM_VARS_START +  5)  // STATE_WAIT_CLK_LOW or STATE_WAIT_CLK_HIGH
#define RTCMEM_PORT0_SEND_WORD      (RTCMEM_VARS_START +  6)  // contains the word to send
#define RTCMEM_PORT0_WORD_SENT_FLAG (RTCMEM_VARS_START +  7)  // 1 when word has been sent (reset by ISR)

#define RTCMEM_PORT1_ENABLED        (RTCMEM_VARS_START +  8)  // if 1 then port 1 is enabled
#define RTCMEM_PORT1_MODE           (RTCMEM_VARS_START +  9)  // MODE_RECEIVE or MODE_SEND
#define RTCMEM_PORT1_WRITE_POS      (RTCMEM_VARS_START + 10)  // position of the next word to receive
#define RTCMEM_PORT1_WORD_RX_READY  (RTCMEM_VARS_START + 11)  // 1 when a word has been received (reset by ISR)
#define RTCMEM_PORT1_BIT            (RTCMEM_VARS_START + 12)  // receive or send bit counter
#define RTCMEM_PORT1_STATE          (RTCMEM_VARS_START + 13)  // STATE_WAIT_CLK_LOW or STATE_WAIT_CLK_HIGH
#define RTCMEM_PORT1_SEND_WORD      (RTCMEM_VARS_START + 14)  // contains the word to send
#define RTCMEM_PORT1_WORD_SENT_FLAG (RTCMEM_VARS_START + 15)  // 1 when word has been sent (reset by ISR)

#define RTCMEM_PORT0_BUFFER_START   (RTCMEM_VARS_START + 16)  // where the receive buffer begins
#define RTCMEM_PORT0_BUFFER_END     (RTCMEM_PORT0_BUFFER_START + PORT0_RX_BUFFER_SIZE)  // where the receive buffer ends

#define RTCMEM_PORT1_BUFFER_START   RTCMEM_PORT0_BUFFER_END  // where the receive buffer begins
#define RTCMEM_PORT1_BUFFER_END     (RTCMEM_PORT1_BUFFER_START + PORT1_RX_BUFFER_SIZE)  // where the receive buffer ends


// check RTC memory occupation
#if RTCMEM_PORT1_BUFFER_END >= 0x800
#error "Port 1 ending buffer overflow"
#endif


// values for RTCMEM_PORTX_MODE
#define MODE_RECEIVE                  0
#define MODE_SEND                     1

// values for RTCMEM_PORTX_STATE
#define STATE_WAIT_CLK_HIGH           0
#define STATE_WAIT_CLK_LOW            1


// ULP program labels
#define READY_TO_RECEIVE              0
#define PORT0_RECEIVE_WORD_READY      1
#define PORT0_SEND_WORD               2
#define PORT0_SEND_NEXT_BIT           3
#define PORT0_SEND_WAIT_FOR_CLK_HIGH  4
#define PORT0_CLK_IS_HIGH             5
#define PORT1_RECEIVE_WORD_READY      6
#define PORT1_SEND_WORD               7
#define PORT1_SEND_NEXT_BIT           8
#define PORT1_SEND_WAIT_FOR_CLK_HIGH  9
#define PORT1_RECEIVE                10
#define PORT1_CLK_IS_HIGH            11
#define MAIN_LOOP                    12
#define PORT1_INIT                   13







const ulp_insn_t ULP_code[] = {

  // Stop ULP timer, not necessary because this routine never ends
  I_END(),

M_LABEL(READY_TO_RECEIVE),

  /////////////////////////////////////////////////////////////////////////////////////////////
  // PORT0 Initialization

  // port 0 enabled?
  MEM_READR(R0, RTCMEM_PORT0_ENABLED),                      // R0 = [RTCMEM_PORT0_ENABLED]
  M_BL(PORT1_INIT, 1),                                      // go PORT1_INIT if R0 < 1  (port 0 not enabled)

  // Configure CLK and DAT as inputs
  CONFIGURE_CLK_INPUT(PS2_PORT0),
  CONFIGURE_DAT_INPUT(PS2_PORT0),

  MEM_WRITEI(RTCMEM_PORT0_STATE, STATE_WAIT_CLK_LOW),       // [RTCMEM_PORT0_STATE] = STATE_WAIT_CLK_LOW

  // reset the word that will contain the received data
  MEM_INDWRITEI(RTCMEM_PORT0_WRITE_POS, 0),                 // [[RTCMEM_PORT0_WRITE_POS]] = 0

  // reset the bit counters (0 = start bit, 1 = data0 .... 9 = parity, 10 = stop bit)
  MEM_WRITEI(RTCMEM_PORT0_BIT, 0),                          // [RTCMEM_PORT0_BIT] = 0
  I_MOVI(R2, 0),                                            // R2 = 0

  //
  /////////////////////////////////////////////////////////////////////////////////////////////


  /////////////////////////////////////////////////////////////////////////////////////////////
  // PORT1 Initialization

M_LABEL(PORT1_INIT),

  // port 1 enabled?
  MEM_READR(R0, RTCMEM_PORT1_ENABLED),                      // R0 = [RTCMEM_PORT1_ENABLED]
  M_BL(MAIN_LOOP, 1),                                       // go MAIN_LOOP if R0 < 1  (port 1 not enabled)

  // Configure CLK and DAT as inputs
  CONFIGURE_CLK_INPUT(PS2_PORT1),
  CONFIGURE_DAT_INPUT(PS2_PORT1),

  MEM_WRITEI(RTCMEM_PORT1_STATE, STATE_WAIT_CLK_LOW),       // [RTCMEM_PORT1_STATE] = STATE_WAIT_CLK_LOW

  // reset the word that will contain the received data
  MEM_INDWRITEI(RTCMEM_PORT1_WRITE_POS, 0),                 // [[RTCMEM_PORT1_WRITE_POS]] = 0

  // reset the bit counters (0 = start bit, 1 = data0 .... 9 = parity, 10 = stop bit)
  MEM_WRITEI(RTCMEM_PORT1_BIT, 0),                          // [RTCMEM_PORT1_BIT] = 0
  I_MOVI(R3, 0),                                            // R3 = 0

  //
  /////////////////////////////////////////////////////////////////////////////////////////////


M_LABEL(MAIN_LOOP),

  // is there something to SEND on port 0?
  MEM_READR(R0, RTCMEM_PORT0_MODE),                         // R0 = [RTCMEM_PORT0_MODE]
  M_LONG_BGE(PORT0_SEND_WORD, MODE_SEND),                   // jump to PORT0_SEND_WORD if R0 >= MODE_SEND

  // is there something to SEND on port 1?
  MEM_READR(R0, RTCMEM_PORT1_MODE),                         // R0 = [RTCMEM_PORT1_MODE]
  M_LONG_BGE(PORT1_SEND_WORD, MODE_SEND),                   // jump to PORT1_SEND_WORD if R0 >= MODE_SEND


  /////////////////////////////////////////////////////////////////////////////////////////////
  // PORT0 Receive

  // port 0 enabled?
  MEM_READR(R0, RTCMEM_PORT0_ENABLED),                      // R0 = [RTCMEM_PORT0_ENABLED]
  M_BL(PORT1_RECEIVE, 1),                                   // go PORT1_RECEIVE if R0 < 1  (port 0 not enabled)

  // wait for CLK low or high?
  MEM_READR(R1, RTCMEM_PORT0_STATE),                        // R1 = [RTCMEM_PORT0_STATE]

  // read CLK
  READ_CLK(PS2_PORT0),                                      // R0 = CLK

  // ALU result is Zero when [RTCMEM_PORT0_STATE] = CLK, that means "need to wait"
  I_SUBR(R1, R1, R0),                                       // R1 = R1 - R0
  M_BXZ(PORT1_RECEIVE),                                     // bypass if ALU is ZERO

  // is CLK high?
  M_BGE(PORT0_CLK_IS_HIGH, 1),

  // CLK is LOW
  MEM_WRITEI(RTCMEM_PORT0_STATE, STATE_WAIT_CLK_HIGH),      // [RTCMEM_PORT0_STATE] = STATE_WAIT_CLK_HIGH

  // get DAT value
  READ_DAT(PS2_PORT0),                                      // R0 = DAT

  // merge with data word and shift right by 1 the received word
  I_LSHI(R0, R0, 11),                                       // R0 = R0 << 11
  MEM_INDREADR(R1, RTCMEM_PORT0_WRITE_POS),                 // R1 = [[RTCMEM_PORT0_WRITE_POS]]
  I_ORR(R1, R1, R0),                                        // R1 = R1 | R0
  I_RSHI(R1, R1, 1),                                        // R1 = R1 >> 1
  MEM_INDWRITER(RTCMEM_PORT0_WRITE_POS, R1),                // [[RTCMEM_PORT0_WRITE_POS]] = R1

  // check port 1
  M_BX(PORT1_RECEIVE),

M_LABEL(PORT0_CLK_IS_HIGH),

  // CLK is high
  MEM_WRITEI(RTCMEM_PORT0_STATE, STATE_WAIT_CLK_LOW),       // [RTCMEM_PORT0_STATE] = STATE_WAIT_CLK_LOW

  // increment bit count
  I_ADDI(R2, R2, 1),                                        // R2 = R2 + 1

  // end of word? if not get another bit
  I_MOVR(R0, R2),                                           // R0 = R2
  M_BL(PORT1_RECEIVE, 11),                                  // jump to PORT1_RECEIVE if R0 (=R2) < 11

  // End of word

  // increments RTCMEM_PORT0_WRITE_POS and check if reached the upper bound
  MEM_INC(RTCMEM_PORT0_WRITE_POS),                                                    // [RTCMEM_PORT0_WRITE_POS] = [RTCMEM_PORT0_WRITE_POS] + 1
  MEM_BL(PORT0_RECEIVE_WORD_READY, RTCMEM_PORT0_WRITE_POS, RTCMEM_PORT0_BUFFER_END),  // jump to PORT0_RECEIVE_WORD_READY if RTCMEM_PORT0_WRITE_POS < RTCMEM_PORT0_BUFFER_END

  // reset RTCMEM_PORT0_WRITE_POS
  MEM_WRITEI(RTCMEM_PORT0_WRITE_POS, RTCMEM_PORT0_BUFFER_START),   // [RTCMEM_PORT0_WRITE_POS] = RTCMEM_PORT0_BUFFER_START

M_LABEL(PORT0_RECEIVE_WORD_READY),

  // set word received flag (RTCMEM_PORT0_WORD_RX_READY)
  MEM_WRITEI(RTCMEM_PORT0_WORD_RX_READY, 1),                // [RTCMEM_PORT0_WORD_RX_READY] = 1

  // trig ETS_RTC_CORE_INTR_SOURCE interrupt
  I_WAKE(),

  // reset the word that will contain the received data
  MEM_INDWRITEI(RTCMEM_PORT0_WRITE_POS, 0),                 // [[RTCMEM_PORT0_WRITE_POS]] = 0

  // reset the bit counter (0 = start bit, 1 = data0 .... 9 = parity, 10 = stop bit)
  I_MOVI(R2, 0),                                            // R2 = 0

  // do the next job
  //M_BX(PORT1_RECEIVE),                                      // jump to PORT1_RECEIVE

  //
  /////////////////////////////////////////////////////////////////////////////////////////////



  /////////////////////////////////////////////////////////////////////////////////////////////
  // PORT1 Receive

M_LABEL(PORT1_RECEIVE),

  // port 1 enabled?
  MEM_READR(R0, RTCMEM_PORT1_ENABLED),                      // R0 = [RTCMEM_PORT1_ENABLED]
  M_BL(MAIN_LOOP, 1),                                       // go MAIN_LOOP if R0 < 1  (port 1 not enabled)

  // wait for CLK low or high?
  MEM_READR(R1, RTCMEM_PORT1_STATE),                        // R1 = [RTCMEM_PORT1_STATE]

  // read CLK
  READ_CLK(PS2_PORT1),                                      // R0 = CLK

  // ALU result is Zero when [RTCMEM_PORT1_STATE] = CLK, that means "need to wait"
  I_SUBR(R1, R1, R0),                                       // R1 = R1 - R0
  M_BXZ(MAIN_LOOP),                                         // repeat if ALU is ZERO

  // is CLK high?
  M_BGE(PORT1_CLK_IS_HIGH, 1),

  // CLK is LOW
  MEM_WRITEI(RTCMEM_PORT1_STATE, STATE_WAIT_CLK_HIGH),      // [RTCMEM_PORT1_STATE] = STATE_WAIT_CLK_HIGH

  // get DAT value
  READ_DAT(PS2_PORT1),                                      // R0 = DAT

  // merge with data word and shift right by 1 the received word
  I_LSHI(R0, R0, 11),                                       // R0 = R0 << 11
  MEM_INDREADR(R1, RTCMEM_PORT1_WRITE_POS),                 // R1 = [[RTCMEM_PORT1_WRITE_POS]]
  I_ORR(R1, R1, R0),                                        // R1 = R1 | R0
  I_RSHI(R1, R1, 1),                                        // R1 = R1 >> 1
  MEM_INDWRITER(RTCMEM_PORT1_WRITE_POS, R1),                // [[RTCMEM_PORT1_WRITE_POS]] = R1

  // go to main loop
  M_BX(MAIN_LOOP),

M_LABEL(PORT1_CLK_IS_HIGH),

  // CLK is high
  MEM_WRITEI(RTCMEM_PORT1_STATE, STATE_WAIT_CLK_LOW),       // [RTCMEM_PORT1_STATE] = STATE_WAIT_CLK_LOW

  // increment bit count
  I_ADDI(R3, R3, 1),                                        // R3 = R3 + 1

  // end of word? if not get another bit
  I_MOVR(R0, R3),                                           // R0 = R3
  M_BL(MAIN_LOOP, 11),                                      // jump to MAIN_LOOP if R0 (=R3) < 11

  // End of word

  // increments RTCMEM_PORT1_WRITE_POS and check if reached the upper bound
  MEM_INC(RTCMEM_PORT1_WRITE_POS),                          // [RTCMEM_PORT1_WRITE_POS] = [RTCMEM_PORT1_WRITE_POS] + 1
  MEM_BL(PORT1_RECEIVE_WORD_READY, RTCMEM_PORT1_WRITE_POS, RTCMEM_PORT1_BUFFER_END),  // jump to PORT1_RECEIVE_WORD_READY if RTCMEM_PORT1_WRITE_POS < RTCMEM_PORT1_BUFFER_END

  // reset RTCMEM_PORT1_WRITE_POS
  MEM_WRITEI(RTCMEM_PORT1_WRITE_POS, RTCMEM_PORT1_BUFFER_START),   // [RTCMEM_PORT1_WRITE_POS] = RTCMEM_PORT1_BUFFER_START

M_LABEL(PORT1_RECEIVE_WORD_READY),

  // set word received flag (RTCMEM_PORT1_WORD_RX_READY)
  MEM_WRITEI(RTCMEM_PORT1_WORD_RX_READY, 1),                // [RTCMEM_PORT1_WORD_RX_READY] = 1

  // trig ETS_RTC_CORE_INTR_SOURCE interrupt
  I_WAKE(),

  // reset the word that will contain the received data
  MEM_INDWRITEI(RTCMEM_PORT1_WRITE_POS, 0),                 // [[RTCMEM_PORT1_WRITE_POS]] = 0

  // reset the bit counter (0 = start bit, 1 = data0 .... 9 = parity, 10 = stop bit)
  I_MOVI(R3, 0),                                            // R3 = 0

  // go to the main loop
  M_BX(MAIN_LOOP),

  //
  /////////////////////////////////////////////////////////////////////////////////////////////


  /////////////////////////////////////////////////////////////////////////////////////////////
  // PORT0 Send

M_LABEL(PORT0_SEND_WORD),

  // Send the word in RTCMEM_PORT0_SEND_WORD

  // maintain CLK low for about 200us
  CONFIGURE_CLK_OUTPUT(PS2_PORT0),
  WRITE_CLK(PS2_PORT0, 0),
  I_DELAY(1600),

  // set DAT low
  CONFIGURE_DAT_OUTPUT(PS2_PORT0),
  WRITE_DAT(PS2_PORT0, 0),

  // configure CLK as input
  CONFIGURE_CLK_INPUT(PS2_PORT0),

  // put in R3 the word to send (10 bits: data, parity and stop bit)
  MEM_READR(R3, RTCMEM_PORT0_SEND_WORD),               // R3 = [RTCMEM_PORT0_SEND_WORD]

  // reset the bit counter (0...7 = data0, 8 = parity, 9 = stop bit)
  MEM_WRITEI(RTCMEM_PORT0_BIT, 0),                     // [RTCMEM_PORT0_BIT] = 0

M_LABEL(PORT0_SEND_NEXT_BIT),

  // wait for CLK = LOW

  // are still we in sending mode?
  MEM_READR(R0, RTCMEM_PORT0_MODE),                    // R0 = [RTCMEM_PORT0_MODE]
  M_LONG_BL(READY_TO_RECEIVE, MODE_SEND),              // jump to READY_TO_RECEIVE if R0 != MODE_SEND

  // read CLK
  READ_CLK(PS2_PORT0),                                 // R0 = CLK

  // repeat if CLK is high
  M_BGE(PORT0_SEND_NEXT_BIT, 1),                       // jump to PORT0_SEND_NEXT_BIT if R0 >= 1

  // bit 10 is the ACK from keyboard, don't send anything, just bypass
  MEM_BGE(PORT0_SEND_WAIT_FOR_CLK_HIGH, RTCMEM_PORT0_BIT, 10),  // jump to PORT0_SEND_WAIT_FOR_CLK_HIGH if [RTCMEM_PORT0_BIT] >= 10

  // CLK is LOW, we are ready to send the bit (LSB of R0)
  I_ANDI(R0, R3, 1),                                   // R0 = R3 & 1
  WRITE_DAT_R0(PS2_PORT0),                             // DAT = LSB of R0

M_LABEL(PORT0_SEND_WAIT_FOR_CLK_HIGH),

  // Wait for CLK = HIGH

  // are still we in sending mode?
  MEM_READR(R0, RTCMEM_PORT0_MODE),                    // R0 = [RTCMEM_PORT0_MODE]
  M_LONG_BL(READY_TO_RECEIVE, MODE_SEND),              // jump to READY_TO_RECEIVE if R0 != MODE_SEND

  // read CLK
  READ_CLK(PS2_PORT0),                                 // R0 = CLK

  // repeat if CLK is low
  M_BL(PORT0_SEND_WAIT_FOR_CLK_HIGH, 1),               // jump to PORT0_SEND_WAIT_FOR_CLK_HIGH if R0 < 1

  // shift the sending word 1 bit to the right (prepare next bit to send)
  I_RSHI(R3, R3, 1),                                   // R3 = R3 >> 1

  // increment bit count
  MEM_INC(RTCMEM_PORT0_BIT),                           // [RTCMEM_PORT0_BIT] = [RTCMEM_PORT0_BIT] + 1

  // end of word? if not send another bit
  MEM_BL(PORT0_SEND_NEXT_BIT, RTCMEM_PORT0_BIT, 11),   // jump to PORT0_SEND_NEXT_BIT if [RTCMEM_PORT0_BIT] < 11

  // switch to receive mode
  MEM_WRITEI(RTCMEM_PORT0_MODE, MODE_RECEIVE),         // [RTCMEM_PORT0_MODE] = MODE_RECEIVE

  // set word sent flag (RTCMEM_PORT0_WORD_SENT_FLAG)
  MEM_WRITEI(RTCMEM_PORT0_WORD_SENT_FLAG, 1),          // [RTCMEM_PORT0_WORD_SENT_FLAG] = 1

  // trig ETS_RTC_CORE_INTR_SOURCE interrupt
  I_WAKE(),

  // perform another job
  M_BX(READY_TO_RECEIVE),                              // jump to READY_TO_RECEIVE

  //
  /////////////////////////////////////////////////////////////////////////////////////////////



  /////////////////////////////////////////////////////////////////////////////////////////////
  // PORT1 SEND

M_LABEL(PORT1_SEND_WORD),

  // Send the word in RTCMEM_PORT1_SEND_WORD

  // maintain CLK low for about 200us
  CONFIGURE_CLK_OUTPUT(PS2_PORT1),
  WRITE_CLK(PS2_PORT1, 0),
  I_DELAY(1600),

  // set DAT low
  CONFIGURE_DAT_OUTPUT(PS2_PORT1),
  WRITE_DAT(PS2_PORT1, 0),

  // configure CLK as input
  CONFIGURE_CLK_INPUT(PS2_PORT1),

  // put in R3 the word to send (10 bits: data, parity and stop bit)
  MEM_READR(R3, RTCMEM_PORT1_SEND_WORD),               // R3 = [RTCMEM_PORT1_SEND_WORD]

  // reset the bit counter (0...7 = data0, 8 = parity, 9 = stop bit)
  MEM_WRITEI(RTCMEM_PORT1_BIT, 0),                     // [RTCMEM_PORT1_BIT] = 0

M_LABEL(PORT1_SEND_NEXT_BIT),

  // wait for CLK = LOW

  // are still we in sending mode?
  MEM_READR(R0, RTCMEM_PORT1_MODE),                    // R0 = [RTCMEM_PORT1_MODE]
  M_LONG_BL(READY_TO_RECEIVE, MODE_SEND),              // jump to READY_TO_RECEIVE if R0 != MODE_SEND

  // read CLK
  READ_CLK(PS2_PORT1),                                 // R0 = CLK

  // repeat if CLK is high
  M_BGE(PORT1_SEND_NEXT_BIT, 1),                       // jump to PORT1_SEND_NEXT_BIT if R0 >= 1

  // bit 10 is the ACK from keyboard, don't send anything, just bypass
  MEM_BGE(PORT1_SEND_WAIT_FOR_CLK_HIGH, RTCMEM_PORT1_BIT, 10),  // jump to PORT1_SEND_WAIT_FOR_CLK_HIGH if [RTCMEM_PORT1_BIT] >= 10

  // CLK is LOW, we are ready to send the bit (LSB of R0)
  I_ANDI(R0, R3, 1),                                   // R0 = R3 & 1
  WRITE_DAT_R0(PS2_PORT1),                             // DAT = LSB of R0

M_LABEL(PORT1_SEND_WAIT_FOR_CLK_HIGH),

  // Wait for CLK = HIGH

  // are still we in sending mode?
  MEM_READR(R0, RTCMEM_PORT1_MODE),                    // R0 = [RTCMEM_PORT1_MODE]
  M_LONG_BL(READY_TO_RECEIVE, MODE_SEND),              // jump to READY_TO_RECEIVE if R0 != MODE_SEND

  // read CLK
  READ_CLK(PS2_PORT1),                                 // R0 = CLK

  // repeat if CLK is low
  M_BL(PORT1_SEND_WAIT_FOR_CLK_HIGH, 1),               // jump to PORT1_SEND_WAIT_FOR_CLK_HIGH if R0 < 1

  // shift the sending word 1 bit to the right (prepare next bit to send)
  I_RSHI(R3, R3, 1),                                   // R3 = R3 >> 1

  // increment bit count
  MEM_INC(RTCMEM_PORT1_BIT),                           // [RTCMEM_PORT1_BIT] = [RTCMEM_PORT1_BIT] + 1

  // end of word? if not send another bit
  MEM_BL(PORT1_SEND_NEXT_BIT, RTCMEM_PORT1_BIT, 11),   // jump to PORT1_SEND_NEXT_BIT if [RTCMEM_PORT1_BIT] < 11

  // switch to receive mode
  MEM_WRITEI(RTCMEM_PORT1_MODE, MODE_RECEIVE),         // [RTCMEM_PORT1_MODE] = MODE_RECEIVE

  // set word sent flag (RTCMEM_PORT1_WORD_SENT_FLAG)
  MEM_WRITEI(RTCMEM_PORT1_WORD_SENT_FLAG, 1),          // [RTCMEM_PORT1_WORD_SENT_FLAG] = 1

  // trig ETS_RTC_CORE_INTR_SOURCE interrupt
  I_WAKE(),

  // perform another job
  M_BX(READY_TO_RECEIVE),                              // jump to READY_TO_RECEIVE

  //
  /////////////////////////////////////////////////////////////////////////////////////////////

};




// Allowed GPIOs: GPIO_NUM_0, GPIO_NUM_2, GPIO_NUM_4, GPIO_NUM_12, GPIO_NUM_13, GPIO_NUM_14, GPIO_NUM_15, GPIO_NUM_25, GPIO_NUM_26, GPIO_NUM_27, GPIO_NUM_32, GPIO_NUM_33
// Not allowed from GPIO_NUM_34 to GPIO_NUM_39
// prg_start in 32 bit words
// size in 32 bit words
void replace_placeholders(uint32_t prg_start, int size, gpio_num_t port0_clkGPIO, gpio_num_t port0_datGPIO, gpio_num_t port1_clkGPIO, gpio_num_t port1_datGPIO)
{
  uint32_t CLK_rtc_gpio_num[2] = { (uint32_t) rtc_gpio_desc[port0_clkGPIO].rtc_num, (uint32_t) rtc_gpio_desc[port1_clkGPIO].rtc_num };
  uint32_t DAT_rtc_gpio_num[2] = { (uint32_t) rtc_gpio_desc[port0_datGPIO].rtc_num, (uint32_t) rtc_gpio_desc[port1_datGPIO].rtc_num };

  uint32_t CLK_rtc_gpio_reg[2] = { rtc_gpio_desc[port0_clkGPIO].reg, rtc_gpio_desc[port1_clkGPIO].reg };
  uint32_t DAT_rtc_gpio_reg[2] = { rtc_gpio_desc[port0_datGPIO].reg, rtc_gpio_desc[port1_datGPIO].reg };

  // just to remember, ffs() finds first set bit in an integer!
  uint32_t CLK_rtc_gpio_ie_s[2] = { (uint32_t) ffs(rtc_gpio_desc[port0_clkGPIO].ie) - 1, (uint32_t) ffs(rtc_gpio_desc[port1_clkGPIO].ie) - 1 };
  uint32_t DAT_rtc_gpio_ie_s[2] = { (uint32_t) ffs(rtc_gpio_desc[port0_datGPIO].ie) - 1, (uint32_t) ffs(rtc_gpio_desc[port1_datGPIO].ie) - 1 };

  for (uint32_t i = 0; i < size; ++i) {
    ulp_insn_t * ins = (ulp_insn_t *) RTC_SLOW_MEM + i;
    if (ins->macro.opcode == OPCODE_PLACEHOLDER) {
      int ps2port = ins->macro.unused;
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


// Note: GPIO_NUM_39 is a placeholder used to disable PS/2 port 1.
void PS2ControllerClass::begin(gpio_num_t port0_clkGPIO, gpio_num_t port0_datGPIO, gpio_num_t port1_clkGPIO, gpio_num_t port1_datGPIO)
{
  m_TXWaitTask[0] = m_TXWaitTask[1] = nullptr;
  m_RXWaitTask[0] = m_RXWaitTask[1] = nullptr;

  rtc_gpio_init(port0_clkGPIO);
  rtc_gpio_init(port0_datGPIO);

  if (port1_clkGPIO != GPIO_NUM_39) {
    rtc_gpio_init(port1_clkGPIO);
    rtc_gpio_init(port1_datGPIO);
  }

  // clear ULP memory (without this may fail to run ULP on softreset)
  for (int i = RTCMEM_PROG_START; i < RTCMEM_PORT1_BUFFER_END; ++i)
    RTC_SLOW_MEM[i] = 0x0000;

  // PS/2 port 1 enabled?
  RTC_SLOW_MEM[RTCMEM_PORT0_ENABLED] = (port0_clkGPIO != GPIO_NUM_39 ? 1 : 0);
  RTC_SLOW_MEM[RTCMEM_PORT1_ENABLED] = (port1_clkGPIO != GPIO_NUM_39 ? 1 : 0);

  warmInit();

  // process, load and execute ULP program
  size_t size = sizeof(ULP_code) / sizeof(ulp_insn_t);
  ulp_process_macros_and_load_ex(RTCMEM_PROG_START, ULP_code, &size);         // convert macros to ULP code
  replace_placeholders(RTCMEM_PROG_START, size, port0_clkGPIO, port0_datGPIO, port1_clkGPIO, port1_datGPIO); // replace GPIO placeholders
  assert(size < RTCMEM_VARS_START && "ULP Program too long, increase RTCMEM_VARS_START");
  REG_SET_FIELD(SENS_SAR_START_FORCE_REG, SENS_PC_INIT, RTCMEM_PROG_START);   // set entry point
  SET_PERI_REG_MASK(SENS_SAR_START_FORCE_REG, SENS_ULP_CP_FORCE_START_TOP);   // enable FORCE START
  SET_PERI_REG_MASK(SENS_SAR_START_FORCE_REG, SENS_ULP_CP_START_TOP);         // start

  // install RTC interrupt handler (on ULP Wake() instruction)
  esp_intr_alloc(ETS_RTC_CORE_INTR_SOURCE, 0, rtc_isr, nullptr, &m_isrHandle);
  SET_PERI_REG_MASK(RTC_CNTL_INT_ENA_REG, RTC_CNTL_ULP_CP_INT_ENA);

  m_suspendCount = 0;
}


void PS2ControllerClass::begin(PS2Preset preset, KbdMode keyboardMode)
{
  bool generateVirtualKeys = (keyboardMode == KbdMode::GenerateVirtualKeys || keyboardMode == KbdMode::CreateVirtualKeysQueue);
  bool createVKQueue       = (keyboardMode == KbdMode::CreateVirtualKeysQueue);
  switch (preset) {
    case PS2Preset::KeyboardPort0_MousePort1:
      // both keyboard (port 0) and mouse configured (port 1)
      PS2Controller.begin(GPIO_NUM_33, GPIO_NUM_32, GPIO_NUM_26, GPIO_NUM_27);
      Keyboard.begin(generateVirtualKeys, createVKQueue, 0);
      Mouse.begin(1);
      break;
    case PS2Preset::KeyboardPort0:
      // only keyboard configured on port 0
      Keyboard.begin(GPIO_NUM_33, GPIO_NUM_32, generateVirtualKeys, createVKQueue);
      break;
    case PS2Preset::MousePort0:
      // only mouse configured on port 0
      Mouse.begin(GPIO_NUM_33, GPIO_NUM_32);
      break;
  };
}


void PS2ControllerClass::suspend()
{
  if (m_suspendCount == 0) {
    CLEAR_PERI_REG_MASK(RTC_CNTL_INT_ENA_REG, RTC_CNTL_ULP_CP_INT_ENA);
    ets_delay_us(50);
    WRITE_PERI_REG(RTC_CNTL_INT_CLR_REG, READ_PERI_REG(RTC_CNTL_INT_ST_REG));
  }
  ++m_suspendCount;
}

void PS2ControllerClass::resume()
{
  --m_suspendCount;
  if (m_suspendCount <= 0) {
    m_suspendCount = 0;
    SET_PERI_REG_MASK(RTC_CNTL_INT_ENA_REG, RTC_CNTL_ULP_CP_INT_ENA);
  }
}


int PS2ControllerClass::dataAvailable(int PS2Port)
{
  uint32_t RTCMEM_PORTX_WRITE_POS    = (PS2Port == 0 ? RTCMEM_PORT0_WRITE_POS    : RTCMEM_PORT1_WRITE_POS);
  uint32_t RTCMEM_PORTX_BUFFER_END   = (PS2Port == 0 ? RTCMEM_PORT0_BUFFER_END   : RTCMEM_PORT1_BUFFER_END);
  uint32_t RTCMEM_PORTX_BUFFER_START = (PS2Port == 0 ? RTCMEM_PORT0_BUFFER_START : RTCMEM_PORT1_BUFFER_START);

  int writePos = RTC_SLOW_MEM[RTCMEM_PORTX_WRITE_POS] & 0xFFFF;
  if (m_readPos[PS2Port] <= writePos)
    return writePos - m_readPos[PS2Port];
  else
    return (RTCMEM_PORTX_BUFFER_END - m_readPos[PS2Port]) + (writePos - RTCMEM_PORTX_BUFFER_START);
}


// return -1 when no data is available
int PS2ControllerClass::getData(int PS2Port)
{
  uint32_t RTCMEM_PORTX_WRITE_POS    = (PS2Port == 0 ? RTCMEM_PORT0_WRITE_POS    : RTCMEM_PORT1_WRITE_POS);
  uint32_t RTCMEM_PORTX_BUFFER_END   = (PS2Port == 0 ? RTCMEM_PORT0_BUFFER_END   : RTCMEM_PORT1_BUFFER_END);
  uint32_t RTCMEM_PORTX_BUFFER_START = (PS2Port == 0 ? RTCMEM_PORT0_BUFFER_START : RTCMEM_PORT1_BUFFER_START);

  int data = -1;

  int writePos = RTC_SLOW_MEM[RTCMEM_PORTX_WRITE_POS] & 0xFFFF;
  if (m_readPos[PS2Port] != writePos) {
    uint16_t data16 = (RTC_SLOW_MEM[m_readPos[PS2Port]] & 0xFFFF);
    data = data16 >> 1 & 0xFF;
    // check parity
    if ((data16 >> 9 & 1) != !calcParity(data)) {
      // parity error
      sendData(0xFE, PS2Port);  // request to resend last byte
      warmInit();
      data = -1;
    } else {
      // parity OK
      ++m_readPos[PS2Port];
      if (m_readPos[PS2Port] == RTCMEM_PORTX_BUFFER_END)
        m_readPos[PS2Port] = RTCMEM_PORTX_BUFFER_START;
    }
  }

  return data;
}


void PS2ControllerClass::warmInit()
{
  m_readPos[0] = RTCMEM_PORT0_BUFFER_START;
  m_readPos[1] = RTCMEM_PORT1_BUFFER_START;

  // initialize the receiving word pointer at the bottom of the buffer
  RTC_SLOW_MEM[RTCMEM_PORT0_WRITE_POS] = RTCMEM_PORT0_BUFFER_START;
  RTC_SLOW_MEM[RTCMEM_PORT1_WRITE_POS] = RTCMEM_PORT1_BUFFER_START;

  // select receive mode
  RTC_SLOW_MEM[RTCMEM_PORT0_MODE] = MODE_RECEIVE;
  RTC_SLOW_MEM[RTCMEM_PORT1_MODE] = MODE_RECEIVE;

  // initialize flags
  RTC_SLOW_MEM[RTCMEM_PORT0_WORD_SENT_FLAG] = 0;
  RTC_SLOW_MEM[RTCMEM_PORT1_WORD_SENT_FLAG] = 0;
  RTC_SLOW_MEM[RTCMEM_PORT0_WORD_RX_READY]  = 0;
  RTC_SLOW_MEM[RTCMEM_PORT1_WORD_RX_READY]  = 0;
}


void PS2ControllerClass::injectInRXBuffer(int value, int PS2Port)
{
  uint32_t RTCMEM_PORTX_WRITE_POS    = (PS2Port == 0 ? RTCMEM_PORT0_WRITE_POS    : RTCMEM_PORT1_WRITE_POS);
  uint32_t RTCMEM_PORTX_BUFFER_END   = (PS2Port == 0 ? RTCMEM_PORT0_BUFFER_END   : RTCMEM_PORT1_BUFFER_END);
  uint32_t RTCMEM_PORTX_BUFFER_START = (PS2Port == 0 ? RTCMEM_PORT0_BUFFER_START : RTCMEM_PORT1_BUFFER_START);

  int writePos = RTC_SLOW_MEM[RTCMEM_PORTX_WRITE_POS] & 0xFFFF;
  RTC_SLOW_MEM[writePos] = value << 1;
  ++writePos;
  if (writePos == RTCMEM_PORTX_BUFFER_END)
    writePos = RTCMEM_PORTX_BUFFER_START;
  RTC_SLOW_MEM[RTCMEM_PORTX_WRITE_POS] = writePos;
}


bool PS2ControllerClass::waitData(int timeOutMS, int PS2Port)
{
  m_RXWaitTask[PS2Port] = xTaskGetCurrentTaskHandle();
  return ulTaskNotifyTake(pdTRUE, timeOutMS < 0 ? portMAX_DELAY : pdMS_TO_TICKS(timeOutMS));
}


void PS2ControllerClass::sendData(uint8_t data, int PS2Port)
{
  uint32_t RTCMEM_PORTX_SEND_WORD = (PS2Port == 0 ? RTCMEM_PORT0_SEND_WORD : RTCMEM_PORT1_SEND_WORD);
  uint32_t RTCMEM_PORTX_MODE      = (PS2Port == 0 ? RTCMEM_PORT0_MODE      : RTCMEM_PORT1_MODE);

  RTC_SLOW_MEM[RTCMEM_PORTX_SEND_WORD]  = 0x200 | ((~calcParity(data) & 1) << 8) | data;  // 0x200 = stop bit. Start bit is not specified here.
  RTC_SLOW_MEM[RTCMEM_PORTX_MODE] = MODE_SEND;
  m_TXWaitTask[PS2Port] = xTaskGetCurrentTaskHandle();
  if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(10)) == pdFALSE) {
    warmInit();
  }
}


void IRAM_ATTR PS2ControllerClass::rtc_isr(void * arg)
{
  for (int PS2Port = 0; PS2Port < 2; ++PS2Port) {

    uint32_t RTCMEM_PORTX_WORD_SENT_FLAG = (PS2Port == 0 ? RTCMEM_PORT0_WORD_SENT_FLAG : RTCMEM_PORT1_WORD_SENT_FLAG);
    uint32_t RTCMEM_PORTX_WRITE_POS      = (PS2Port == 0 ? RTCMEM_PORT0_WRITE_POS      : RTCMEM_PORT1_WRITE_POS);
    uint32_t RTCMEM_PORTX_WORD_RX_READY  = (PS2Port == 0 ? RTCMEM_PORT0_WORD_RX_READY  : RTCMEM_PORT1_WORD_RX_READY);

    // Received end of send interrupt?
    if (RTC_SLOW_MEM[RTCMEM_PORTX_WORD_SENT_FLAG]) {
      // reset flag and awake waiting task
      RTC_SLOW_MEM[RTCMEM_PORTX_WORD_SENT_FLAG] = 0;
      PS2Controller.m_readPos[PS2Port] = RTC_SLOW_MEM[RTCMEM_PORTX_WRITE_POS] & 0xFFFF;
      if (PS2Controller.m_TXWaitTask[PS2Port]) {
        vTaskNotifyGiveFromISR(PS2Controller.m_TXWaitTask[PS2Port], nullptr);
        PS2Controller.m_TXWaitTask[PS2Port] = nullptr;
      }
    }

    // Received new RX word interrupt?
    if (RTC_SLOW_MEM[RTCMEM_PORTX_WORD_RX_READY]) {
      // reset flag and awake waiting task
      RTC_SLOW_MEM[RTCMEM_PORTX_WORD_RX_READY] = 0;
      if (PS2Controller.m_RXWaitTask[PS2Port]) {
        vTaskNotifyGiveFromISR(PS2Controller.m_RXWaitTask[PS2Port], nullptr);
        PS2Controller.m_RXWaitTask[PS2Port] = nullptr;
      }
    }

  }
}




} // end of namespace


