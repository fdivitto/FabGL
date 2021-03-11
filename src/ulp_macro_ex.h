#pragma once



#include "esp_attr.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp32/ulp.h"

#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/sens_reg.h"


////////////////////////////////////////////////////////////////////////////
// Support for missing macros for operations on STAGE register
// fabgl changes:

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

#define M_STAGEBL(label_num, imm_value) \
    M_BRANCH(label_num), \
    I_STAGEBL(0, imm_value)

#define M_STAGEBGE(label_num, imm_value) \
    M_BRANCH(label_num), \
    I_STAGEBGE(0, imm_value)

#define M_STAGEBLE(label_num, imm_value) \
    M_BRANCH(label_num), \
    I_STAGEBLE(0, imm_value)

////////////////////////////////////////////////////////////////////////////



esp_err_t ulp_process_macros_and_load_ex(uint32_t load_addr, const ulp_insn_t* program, size_t* psize);
