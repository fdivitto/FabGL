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
#include "utils.h"


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

#define OPCODE_PLACEHOLDER             12 // fortunately there is an unused ULP opcode we can use as placeholder
#define SUB_OPCODE_DAT_ENABLE_OUTPUT   0
#define SUB_OPCODE_DAT_ENABLE_INPUT    1
#define SUB_OPCODE_CLK_ENABLE_OUTPUT   2
#define SUB_OPCODE_CLK_ENABLE_INPUT    3
#define SUB_OPCODE_READ_CLK            4
#define SUB_OPCODE_READ_DAT            5
#define SUB_OPCODE_WRITE_CLK           6
#define SUB_OPCODE_WRITE_DAT           7


#define DAT_ENABLE_OUTPUT(value) { .macro = { \
    .label = value, \
    .unused = 0, \
    .sub_opcode = SUB_OPCODE_DAT_ENABLE_OUTPUT, \
    .opcode = OPCODE_PLACEHOLDER } }

#define DAT_ENABLE_INPUT(value) { .macro = { \
    .label = value, \
    .unused = 0, \
    .sub_opcode = SUB_OPCODE_DAT_ENABLE_INPUT, \
    .opcode = OPCODE_PLACEHOLDER } }

#define CLK_ENABLE_OUTPUT(value) { .macro = { \
    .label = value, \
    .unused = 0, \
    .sub_opcode = SUB_OPCODE_CLK_ENABLE_OUTPUT, \
    .opcode = OPCODE_PLACEHOLDER } }

#define CLK_ENABLE_INPUT(value) { .macro = { \
    .label = value, \
    .unused = 0, \
    .sub_opcode = SUB_OPCODE_CLK_ENABLE_INPUT, \
    .opcode = OPCODE_PLACEHOLDER } }

// equivalent to rtc_gpio_get_level()
#define READ_CLK() { .macro = { \
    .label = 0, \
    .unused = 0, \
    .sub_opcode = SUB_OPCODE_READ_CLK, \
    .opcode = OPCODE_PLACEHOLDER } }

// equivalent to rtc_gpio_get_level()
#define READ_DAT() { .macro = { \
    .label = 0, \
    .unused = 0, \
    .sub_opcode = SUB_OPCODE_READ_DAT, \
    .opcode = OPCODE_PLACEHOLDER } }

// equivalent to rtc_gpio_set_level()
#define WRITE_CLK(value) { .macro = { \
    .label = value, \
    .unused = 0, \
    .sub_opcode = SUB_OPCODE_WRITE_CLK, \
    .opcode = OPCODE_PLACEHOLDER } }

// equivalent to rtc_gpio_set_level()
#define WRITE_DAT(value) { .macro = { \
    .label = value, \
    .unused = 0, \
    .sub_opcode = SUB_OPCODE_WRITE_DAT, \
    .opcode = OPCODE_PLACEHOLDER } }


////////////////////////////////////////////////////////////////////////////
// macro instructions

// equivalent to rtc_gpio_set_direction(..., RTC_GPIO_MODE_INPUT_ONLY)
#define CONFIGURE_DAT_INPUT() \
    DAT_ENABLE_OUTPUT(0), \
    DAT_ENABLE_INPUT(1)

// equivalent to rtc_gpio_set_direction(..., RTC_GPIO_MODE_OUTPUT_ONLY)
#define CONFIGURE_DAT_OUTPUT() \
    DAT_ENABLE_OUTPUT(1), \
    DAT_ENABLE_INPUT(0)

// equivalent to rtc_gpio_set_direction(..., RTC_GPIO_MODE_INPUT_ONLY)
#define CONFIGURE_CLK_INPUT() \
    CLK_ENABLE_OUTPUT(0), \
    CLK_ENABLE_INPUT(1)

// equivalent to rtc_gpio_set_direction(..., RTC_GPIO_MODE_OUTPUT_ONLY)
#define CONFIGURE_CLK_OUTPUT() \
    CLK_ENABLE_OUTPUT(1), \
    CLK_ENABLE_INPUT(0)

// equivalent to rtc_gpio_set_level()
// WRITE bit 0 of R0
#define WRITE_DAT_R0() \
    I_BL(3, 1), \
    WRITE_DAT(1), \
    I_BGE(2, 1), \
    WRITE_DAT(0)


////////////////////////////////////////////////////////////////////////////


// Locations inside RTC low speed memory
#define RTC_MEM_PROG_START         0x000
#define RTC_MEM_MODE               0x400
#define RTC_MEM_SEND_WORD          0x401
#define RTC_MEM_WRITE_POS          0x402
#define RTC_MEM_WORD_SENT_FLAG     0x403  // 1 when word has been sent (reset by ISR)
#define RTC_MEM_WORD_RECEIVED_FLAG 0x404  // 1 when a word has been received (reset by ISR)
#define RTC_MEM_BUFFER_BTM         0x405
#define RTC_MEM_BUFFER_TOP         0x800

// values for RTC_MEM_MODE
#define MODE_RECEIVE       0
#define MODE_SEND          1


// ULP program labels
#define READY_TO_RECEIVE          0
#define RECEIVE_NEXT_WORD         1
#define RECEIVE_NEXT_BIT          2
#define RECEIVE_WAIT_FOR_CLK_HIGH 3
#define RECEIVE_WORD_READY        4
#define SEND_WORD                 5
#define SEND_NEXT_BIT             6
#define SEND_WAIT_FOR_CLK_HIGH    7



const ulp_insn_t ULPCode[] = {

  // Stop ULP timer, not necessary because this routine never ends
  I_END(),

M_LABEL(READY_TO_RECEIVE),

  // Configure CLK and DAT as inputs
  CONFIGURE_CLK_INPUT(),
  CONFIGURE_DAT_INPUT(),

M_LABEL(RECEIVE_NEXT_WORD),

  // reset the word that will contain the received word
  I_MOVI(R3, 0),                                       // R3 = 0

  // reset the bit counter (STAGE regiser, 0 = start bit, 1 = data0 .... 9 = parity, 10 = stop bit)
  I_STAGERSTI(),                                       // STAGE = 0

M_LABEL(RECEIVE_NEXT_BIT),

  // Wait for CLK = LOW or another job to do

  // is there something to SEND?
  // read RTC_MEM_MODE variable
  I_MOVI(R0, RTC_MEM_MODE),                            // R0 = RTC_MEM_MODE
  I_LD(R0, R0, 0),                                     // R0 = [R0 + 0]
  M_BGE(SEND_WORD, MODE_SEND),                         // jump to SEND_WORD if R0 >= MODE_SEND

  // RTC_MEM_MODE is MODE_RECEIVE

  // read CLK
  READ_CLK(),                                          // R0 = CLK

  // repeat if CLK is high
  M_BGE(RECEIVE_NEXT_BIT, 1),                          // jump to RECEIVE_NEXT_BIT if R0 >= 1

  // now CLK is LOW, get DAT value
  READ_DAT(),                                          // R0 = DAT

  // shift R0 left by 10
  I_LSHI(R0, R0, 11),                                  // R0 = R0 << 11

  // merge with data word and shift right by 1 the received word
  I_ORR(R3, R3, R0),                                   // R3 = R3 | R0
  I_RSHI(R3, R3, 1),                                   // R3 = R3 >> 1

M_LABEL(RECEIVE_WAIT_FOR_CLK_HIGH),

  // Wait for CLK = HIGH

  // read CLK
  READ_CLK(),                                          // R0 = CLK

  // repeat if CLK is low
  M_BL(RECEIVE_WAIT_FOR_CLK_HIGH, 1),                  // jump to RECEIVE_WAIT_FOR_CLK_HIGH if R0 < 1

  // increment bit count
  I_STAGEINCI(1),                                      // STAGE = STAGE + 1

  // end of word? if not get another bit
  M_STAGEBL(RECEIVE_NEXT_BIT, 11),                     // jump to RECEIVE_NEXT_BIT if STAGE < 11

  // End of word

  // put R3 into [RTC_MEM_WRITE_POS]
  I_MOVI(R1, RTC_MEM_WRITE_POS),                       // R1 = RTC_MEM_WRITE_POS
  I_LD(R0, R1, 0),                                     // R0 = [R1 + 0]
  I_ST(R3, R0, 0),                                     // [R0 + 0] = R3

  // increments RTC_MEM_WRITE_LOC and check if reched the upper bound
  I_ADDI(R0, R0, 1),                                   // R0 = R0 + 1
  M_BL(RECEIVE_WORD_READY, RTC_MEM_BUFFER_TOP),        // jump to RECEIVE_WORD_READY if R0 < RTC_MEM_BUFFER_TOP

  // reset RTC_MEM_WRITE_POS
  I_MOVI(R0, RTC_MEM_BUFFER_BTM),                      // R0 = RTC_MEM_BUFFER_BTM

M_LABEL(RECEIVE_WORD_READY),

  // update RTC_MEM_WRITE_POS actual value
  I_ST(R0, R1, 0),                                     // [R1 + 0] = R0

  // set word received flag (RTC_MEM_WORD_RECEIVED_FLAG)
  I_MOVI(R1, RTC_MEM_WORD_RECEIVED_FLAG),              // R1 = RTC_MEM_WORD_RECEIVED_FLAG
  I_MOVI(R0, 1),                                       // R0 = 1
  I_ST(R0, R1, 0),                                     // [R1 + 0] = R0

  // trig ETS_RTC_CORE_INTR_SOURCE interrupt
  I_WAKE(),

  // do the next job
  M_BX(RECEIVE_NEXT_WORD),                             // jump to RECEIVE_NEXT_WORD

M_LABEL(SEND_WORD),

  // Send the word in RTC_MEM_SEND_WORD

  // maintain CLK low for about 200us
  CONFIGURE_CLK_OUTPUT(),
  WRITE_CLK(0),
  I_DELAY(1600),

  // set DAT low
  CONFIGURE_DAT_OUTPUT(),
  WRITE_DAT(0),

  // configure CLK as input
  CONFIGURE_CLK_INPUT(),

  // put in R3 the word to send (10 bits: data, parity and stop bit)
  I_MOVI(R0, RTC_MEM_SEND_WORD),                       // R0 = RTC_MEM_SEND_WORD
  I_LD(R3, R0, 0),                                     // R3 = [R0 + 0]

  // reset the bit counter (STAGE register, 0...7 = data0, 8 = parity, 9 = stop bit)
  I_STAGERSTI(),                                       // STAGE = 0

M_LABEL(SEND_NEXT_BIT),

  // wait for CLK = LOW

  // read CLK
  READ_CLK(),                                          // R0 = CLK

  // repeat if CLK is high
  M_BGE(SEND_NEXT_BIT, 1),                             // jump to SEND_NEXT_BIT if R0 >= 1

  // bit 10 is the ACK from keyboard, don't send anything, just bypass
  M_STAGEBGE(SEND_WAIT_FOR_CLK_HIGH, 10),              // jump to SEND_WAIT_FOR_CLK_HIGH if STAGE >= 10

  // CLK is LOW, we are ready to send the bit (LSB of R0)
  I_ANDI(R0, R3, 1),                                   // R0 = R3 & 1
  WRITE_DAT_R0(),                                      // DAT = LSB of R0

M_LABEL(SEND_WAIT_FOR_CLK_HIGH),

  // Wait for CLK = HIGH

  // read CLK
  READ_CLK(),                                          // R0 = CLK

  // repeat if CLK is low
  M_BL(SEND_WAIT_FOR_CLK_HIGH, 1),                     // jump to SEND_WAIT_FOR_CLK_HIGH if R0 < 1

  // shift the sending word 1 bit to the right (prepare next bit to send)
  I_RSHI(R3, R3, 1),                                   // R3 = R3 >> 1

  // increment bit count
  I_STAGEINCI(1),                                      // STAGE = STAGE + 1

  // end of word? if not send another bit
  M_STAGEBL(SEND_NEXT_BIT, 11),                        // jump to SEND_NEXT_BIT if STAGE < 11

  // switch to receive mode
  I_MOVI(R0, RTC_MEM_MODE),                            // R0 = RTC_MEM_MODE
  I_MOVI(R1, MODE_RECEIVE),                            // R1 = MODE_RECEIVE
  I_ST(R1, R0, 0),                                     // [R0 + 0] = R1

  // set word sent flag (RTC_MEM_WORD_SENT_FLAG)
  I_MOVI(R1, RTC_MEM_WORD_SENT_FLAG),                  // R1 = RTC_MEM_WORD_SENT_FLAG
  I_MOVI(R0, 1),                                       // R0 = 1
  I_ST(R0, R1, 0),                                     // [R1 + 0] = R0

  // trig ETS_RTC_CORE_INTR_SOURCE interrupt
  I_WAKE(),

  // perform another job
  M_BX(READY_TO_RECEIVE),                              // jump to READY_TO_RECEIVE

};


////////////////////////////////////////////////////////////////////////////


// Allowed GPIOs: GPIO_NUM_0, GPIO_NUM_2, GPIO_NUM_4, GPIO_NUM_12, GPIO_NUM_13, GPIO_NUM_14, GPIO_NUM_15, GPIO_NUM_25, GPIO_NUM_26, GPIO_NUM_27, GPIO_NUM_32, GPIO_NUM_33
// Not allowed from GPIO_NUM_34 to GPIO_NUM_39 because these pins are Input Only (use only if Send is not required)
// prg_start in 32 bit words
// size in 32 bit words
void replace_placeholders(uint32_t prg_start, int size, gpio_num_t clkGPIO, gpio_num_t datGPIO)
{
  uint32_t CLK_rtc_gpio_num = rtc_gpio_desc[clkGPIO].rtc_num;
  uint32_t DAT_rtc_gpio_num = rtc_gpio_desc[datGPIO].rtc_num;

  uint32_t CLK_rtc_gpio_reg = rtc_gpio_desc[clkGPIO].reg;
  uint32_t DAT_rtc_gpio_reg = rtc_gpio_desc[datGPIO].reg;

  uint32_t CLK_rtc_gpio_ie_s = ffs(rtc_gpio_desc[clkGPIO].ie) - 1;  // ffs() because we need the bit position (from 0)
  uint32_t DAT_rtc_gpio_ie_s = ffs(rtc_gpio_desc[datGPIO].ie) - 1;  // ffs() because we need the bit position (from 0)

  for (uint32_t i = 0; i < size; ++i) {
    ulp_insn_t * ins = (ulp_insn_t *) RTC_SLOW_MEM + i;
    if (ins->macro.opcode == OPCODE_PLACEHOLDER) {
      switch (ins->macro.sub_opcode) {
        case SUB_OPCODE_DAT_ENABLE_OUTPUT:
          if (ins->macro.label)
            *ins = (ulp_insn_t) I_WR_REG_BIT(RTC_GPIO_ENABLE_W1TS_REG, DAT_rtc_gpio_num + RTC_GPIO_ENABLE_W1TS_S, 1);
          else
            *ins = (ulp_insn_t) I_WR_REG_BIT(RTC_GPIO_ENABLE_W1TC_REG, DAT_rtc_gpio_num + RTC_GPIO_ENABLE_W1TC_S, 1);
          break;
        case SUB_OPCODE_DAT_ENABLE_INPUT:
          *ins = (ulp_insn_t) I_WR_REG_BIT(DAT_rtc_gpio_reg, DAT_rtc_gpio_ie_s, ins->macro.label);
          break;
        case SUB_OPCODE_CLK_ENABLE_OUTPUT:
          if (ins->macro.label)
            *ins = (ulp_insn_t) I_WR_REG_BIT(RTC_GPIO_ENABLE_W1TS_REG, CLK_rtc_gpio_num + RTC_GPIO_ENABLE_W1TS_S, 1);
          else
            *ins = (ulp_insn_t) I_WR_REG_BIT(RTC_GPIO_ENABLE_W1TC_REG, CLK_rtc_gpio_num + RTC_GPIO_ENABLE_W1TC_S, 1);
          break;
        case SUB_OPCODE_CLK_ENABLE_INPUT:
          *ins = (ulp_insn_t) I_WR_REG_BIT(CLK_rtc_gpio_reg, CLK_rtc_gpio_ie_s, ins->macro.label);
          break;
        case SUB_OPCODE_READ_CLK:
          *ins = (ulp_insn_t) I_RD_REG(RTC_GPIO_IN_REG, CLK_rtc_gpio_num + RTC_GPIO_IN_NEXT_S, CLK_rtc_gpio_num + RTC_GPIO_IN_NEXT_S);
          break;
        case SUB_OPCODE_READ_DAT:
          *ins = (ulp_insn_t) I_RD_REG(RTC_GPIO_IN_REG, DAT_rtc_gpio_num + RTC_GPIO_IN_NEXT_S, DAT_rtc_gpio_num + RTC_GPIO_IN_NEXT_S);
          break;
        case SUB_OPCODE_WRITE_CLK:
          *ins = (ulp_insn_t) I_WR_REG_BIT(RTC_GPIO_OUT_REG, CLK_rtc_gpio_num + RTC_GPIO_IN_NEXT_S, ins->macro.label);
          break;
        case SUB_OPCODE_WRITE_DAT:
          *ins = (ulp_insn_t) I_WR_REG_BIT(RTC_GPIO_OUT_REG, DAT_rtc_gpio_num + RTC_GPIO_IN_NEXT_S, ins->macro.label);
          break;
      }
    }
  }
}


void PS2ControllerClass::begin(gpio_num_t clkGPIO, gpio_num_t datGPIO)
{
  m_TXWaitTask = NULL;
  m_RXWaitTask = NULL;

  rtc_gpio_init(clkGPIO);
  rtc_gpio_init(datGPIO);

  // clear ULP memory (without this may fail to run ULP on softreset)
  for (int i = RTC_MEM_PROG_START; i < RTC_MEM_BUFFER_TOP; ++i)
    RTC_SLOW_MEM[i] = 0x0000;

  // initialize the receiving word pointer at the bottom of the buffer
  RTC_SLOW_MEM[RTC_MEM_WRITE_POS] = RTC_MEM_BUFFER_BTM;

  // select receive mode
  RTC_SLOW_MEM[RTC_MEM_MODE] = MODE_RECEIVE;

  // initialize flags
  RTC_SLOW_MEM[RTC_MEM_WORD_SENT_FLAG]     = 0;
  RTC_SLOW_MEM[RTC_MEM_WORD_RECEIVED_FLAG] = 0;

  // process, load and execute ULP program
  size_t size = sizeof(ULPCode) / sizeof(ulp_insn_t);
  ulp_process_macros_and_load(RTC_MEM_PROG_START, ULPCode, &size);            // convert macros to ULP code
  replace_placeholders(RTC_MEM_PROG_START, size, GPIO_NUM_33, GPIO_NUM_32);   // replace GPIO placeholders
  REG_SET_FIELD(SENS_SAR_START_FORCE_REG, SENS_PC_INIT, RTC_MEM_PROG_START);  // set entry point
  SET_PERI_REG_MASK(SENS_SAR_START_FORCE_REG, SENS_ULP_CP_FORCE_START_TOP);   // enable FORCE START
  SET_PERI_REG_MASK(SENS_SAR_START_FORCE_REG, SENS_ULP_CP_START_TOP);         // start

  m_readPos = RTC_MEM_BUFFER_BTM;

  // install RTC interrupt handler (on ULP Wake() instruction)
  esp_intr_alloc(ETS_RTC_CORE_INTR_SOURCE, 0, rtc_isr, NULL, NULL);
  SET_PERI_REG_MASK(RTC_CNTL_INT_ENA_REG, RTC_CNTL_ULP_CP_INT_ENA);
}


int PS2ControllerClass::dataAvailable()
{
  int writePos = RTC_SLOW_MEM[RTC_MEM_WRITE_POS] & 0xFFFF;
  return m_readPos < writePos ? (writePos - m_readPos) : (RTC_MEM_BUFFER_TOP - m_readPos) + (writePos - RTC_MEM_BUFFER_BTM);
}


// return -1 when no data is available
int PS2ControllerClass::getData(int timeOutMS, bool isReply)
{
  // to avoid possible missings of RX interrupts a value of 200ms is used in place of portMAX_DELAY
  int aTimeOut = timeOutMS < 0 ? pdMS_TO_TICKS(200) : pdMS_TO_TICKS(timeOutMS);
  while (true) {
    int writePos = RTC_SLOW_MEM[RTC_MEM_WRITE_POS] & 0xFFFF;
    volatile int * readPos = isReply ? &m_replyReadPos : &m_readPos;
    if (*readPos != writePos) {
      int data = (RTC_SLOW_MEM[*readPos] & 0xFFFF) >> 1 & 0xFF;
      if (isReply) {
        // reset reply code to avoid to be used as normal received word
        RTC_SLOW_MEM[*readPos] = 0;
      }
      *readPos += 1;
      if (*readPos == RTC_MEM_BUFFER_TOP)
        *readPos = RTC_MEM_BUFFER_BTM;
      if (data == 0)
        continue; // not valid code (maybe a retry code)
      return data;
    } else {
      m_RXWaitTask = xTaskGetCurrentTaskHandle();
      if (ulTaskNotifyTake(pdTRUE, aTimeOut) == 0 && timeOutMS > -1)
        return -1;  // real timeout
    }
  }
}


void PS2ControllerClass::sendData(uint8_t data)
{
  RTC_SLOW_MEM[RTC_MEM_SEND_WORD] = 0x200 | ((~calcParity(data) & 1) << 8) | data;  // 0x200 = stop bit. Start bit is not specified here.
  RTC_SLOW_MEM[RTC_MEM_MODE]      = MODE_SEND;
  m_TXWaitTask = xTaskGetCurrentTaskHandle();
  ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(10));
}


void IRAM_ATTR PS2ControllerClass::rtc_isr(void * arg)
{
  // Received end of send interrupt?
  if (RTC_SLOW_MEM[RTC_MEM_WORD_SENT_FLAG]) {
    // reset flag and awake waiting task
    RTC_SLOW_MEM[RTC_MEM_WORD_SENT_FLAG] = 0;
    PS2Controller.m_replyReadPos = RTC_SLOW_MEM[RTC_MEM_WRITE_POS] & 0xFFFF;
    if (PS2Controller.m_TXWaitTask) {
      vTaskNotifyGiveFromISR(PS2Controller.m_TXWaitTask, NULL);
      PS2Controller.m_TXWaitTask = NULL;
    }
  }

  // Received new RX word interrupt?
  if (RTC_SLOW_MEM[RTC_MEM_WORD_RECEIVED_FLAG]) {
    // reset flag and awake waiting task
    RTC_SLOW_MEM[RTC_MEM_WORD_RECEIVED_FLAG] = 0;
    if (PS2Controller.m_RXWaitTask) {
      vTaskNotifyGiveFromISR(PS2Controller.m_RXWaitTask, NULL);
      PS2Controller.m_RXWaitTask = NULL;
    }
  }
}




} // end of namespace


