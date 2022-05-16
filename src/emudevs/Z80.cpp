/*
 Z80 emulator code derived from Lin Ke-Fong source. Copyright says:

 Copyright (c) 2016, 2017 Lin Ke-Fong

 This code is free, do whatever you want with it.

 2020 adapted by Fabrizio Di Vittorio for fabgl ESP32 library
 */



#include "Z80.h"


#pragma GCC optimize ("O2")


namespace fabgl {


/* Write the following macros for memory access and input/output on the Z80.
 *
 * Z80_FETCH_BYTE() and Z80_FETCH_WORD() are used by the emulator to read the
 * code (opcode, constants, displacement, etc). The upper 16-bit of the address
 * parameters is undefined and must be reset to zero before actually reading
 * memory (use & 0xffff). The value x read, must be an unsigned 8-bit or 16-bit
 * value in the endianness of the host processor.
 *
 * Z80_READ_BYTE(), Z80_WRITE_BYTE(), Z80_READ_WORD(), and Z80_WRITE_WORD()
 * are used for general memory access. They obey the same rules as the code
 * reading macros. The upper bits of the value x to write may be non-zero.
 * Z80_READ_WORD_INTERRUPT() and Z80_WRITE_WORD_INTERRUPT() are same as
 * respectively Z80_READ_WORD() and Z80_WRITE_WORD(), except they are only used
 * for interrupt generation.
 *
 * Z80_INPUT_BYTE() and Z80_OUTPUT_BYTE() are for input and output. The upper
 * bits of the port number to read or write are always zero. The input byte x
 * must be an unsigned 8-bit value. The value x to write is an unsigned 8-bit
 * with its upper bits zeroed.
 *
 * All macros have access to the following three variables:
 *
 *      state           Pointer to the current Z80_STATE. Because the
 *			instruction is currently executing, its members may not
 *			be fully up to date, depending on when the macro is
 *			called in the process. It is rather suggested to access
 *			the state only when the emulator is stopped.
 *
 *      elapsed_cycles  Number of cycles emulated. If needed, you may add wait
 *			states to it for slow memory accesses. Because the
 *			macros are called during the execution of the current
 *			instruction, this number is only precise up to the
 *			previous one.
 *
 *
 * Except for Z80_READ_WORD_INTERRUPT and Z80_WRITE_WORD_INTERRUPT, all macros
 * also have access to:
 *
 *
 *      registers       Current register decoding table, use it to determine if
 * 			the current instruction is prefixed. It points on:
 *
 *				state.dd_register_table for 0xdd prefixes;
 *                      	state.fd_register_table for 0xfd prefixes;
 *				state.register_table otherwise.
 *
 *      pc              Current PC register (upper bits are undefined), points
 *                      on the opcode, the displacement or constant to read for
 *                      Z80_FETCH_BYTE() and Z80_FETCH_WORD(), or on the next
 *                      instruction otherwise.
 *
 * Except for Z80_FETCH_BYTE(), Z80_FETCH_WORD(), Z80_READ_WORD_INTERRUPT, and
 * Z80_WRITE_WORD_INTERRUPT, all other macros can know which instruction is
 * currently executing:
 *
 *      opcode          Opcode of the currently executing instruction.
 *
 *      instruction     Type of the currently executing instruction, see
 *                      instructions.h for a list.
 */

/* Here are macros for emulating zexdoc and zexall. Read/write memory macros
 * are written for a linear 64k RAM. Input/output port macros are used as
 * "traps" to simulate system calls.
 */


#define Z80_READ_BYTE(address, x)                                       \
{                                                                       \
  (x) = (m_readByte(m_context, (address) & 0xffff));                    \
}

#define Z80_FETCH_BYTE(address, x)    Z80_READ_BYTE((address), (x))

#define Z80_READ_WORD(address, x)                                       \
{                                                                       \
  (x) = (m_readByte(m_context, (address) & 0xffff) | (m_readByte(m_context, ((address) + 1) & 0xffff) << 8)); \
}

#define Z80_FETCH_WORD(address, x)    Z80_READ_WORD((address), (x))

#define Z80_WRITE_BYTE(address, x)                                        \
{                                                                         \
  m_writeByte(m_context, (address) & 0xffff, (x));                        \
}

#define Z80_WRITE_WORD(address, x)                                 \
{                                                                  \
  m_writeByte(m_context, (address) & 0xffff, (x));                 \
  m_writeByte(m_context, ((address) + 1) & 0xffff, (x) >> 8);      \
}

#define Z80_READ_WORD_INTERRUPT(address, x)  Z80_READ_WORD((address), (x))

#define Z80_WRITE_WORD_INTERRUPT(address, x)  Z80_WRITE_WORD((address), (x))

#define Z80_INPUT_BYTE(port, x)                                         \
{                                                                       \
  (x) = m_readIO(m_context, (port));                                    \
}

#define Z80_OUTPUT_BYTE(port, x)                                        \
{                                                                       \
  m_writeIO(m_context, (port), (x));                                    \
}






/* Some "instructions" handle two opcodes hence they need their encodings to
 * be able to distinguish them.
 */

#define OPCODE_LD_A_I           0x57
#define OPCODE_LD_I_A           0x47

#define OPCODE_LDI              0xa0
#define OPCODE_LDIR             0xb0
#define OPCODE_CPI              0xa1
#define OPCODE_CPIR             0xb1

#define OPCODE_RLD              0x6f

#if defined(Z80_CATCH_RETI) && defined(Z80_CATCH_RETN)
#       define OPCODE_RETI      0x4d
#endif

#define OPCODE_INI              0xa2
#define OPCODE_INIR             0xb2
#define OPCODE_OUTI             0xa3
#define OPCODE_OTIR             0xb3



/* Instruction numbers, opcodes are converted to these numbers using tables
 * generated by maketables.c.
 */

enum {

  /* 8-bit load group. */

  LD_R_R,
  LD_R_N,

  LD_R_INDIRECT_HL,
  LD_INDIRECT_HL_R,
  LD_INDIRECT_HL_N,

  LD_A_INDIRECT_BC,
  LD_A_INDIRECT_DE,
  LD_A_INDIRECT_NN,
  LD_INDIRECT_BC_A,
  LD_INDIRECT_DE_A,
  LD_INDIRECT_NN_A,

  LD_A_I_LD_A_R,          /* Handle "LD A, I" and "LD A, R". */
  LD_I_A_LD_R_A,          /* Handle "LD I, A" and "LD I, A". */

  /* 16-bit load group. */

  LD_RR_NN,

  LD_HL_INDIRECT_NN,
  LD_RR_INDIRECT_NN,
  LD_INDIRECT_NN_HL,
  LD_INDIRECT_NN_RR,

  LD_SP_HL,

  PUSH_SS,
  POP_SS,

  /* Exchange, block transfer, and search group. */

  EX_DE_HL,
  EX_AF_AF_PRIME,
  EXX,
  EX_INDIRECT_SP_HL,

  LDI_LDD,                /* Handle "LDI" and "LDD". */
  LDIR_LDDR,              /* Handle "LDIR" and "LDDR". */

  CPI_CPD,                /* Handle "CPI" and "CPD". */
  CPIR_CPDR,              /* Handle "CPIR" and "CPDR". */

  /* 8-bit arithmetic and logical group. */

  ADD_R,
  ADD_N,
  ADD_INDIRECT_HL,

  ADC_R,
  ADC_N,
  ADC_INDIRECT_HL,

  SUB_R,
  SUB_N,
  SUB_INDIRECT_HL,

  SBC_R,
  SBC_N,
  SBC_INDIRECT_HL,

  AND_R,
  AND_N,
  AND_INDIRECT_HL,

  XOR_R,
  XOR_N,
  XOR_INDIRECT_HL,

  OR_R,
  OR_N,
  OR_INDIRECT_HL,

  CP_R,
  CP_N,
  CP_INDIRECT_HL,

  INC_R,
  INC_INDIRECT_HL,
  DEC_R,
  DEC_INDIRECT_HL,

  /* 16-bit arithmetic group. */

  ADD_HL_RR,

  ADC_HL_RR,
  SBC_HL_RR,

  INC_RR,
  DEC_RR,

  /* General-purpose arithmetic and CPU control group. */

  DAA,

  CPL,
  NEG,

  CCF,
  SCF,

  NOP,
  HALT,

  DI,
  EI,

  IM_N,

  /* Rotate and shift group. */

  RLCA,
  RLA,
  RRCA,
  RRA,

  RLC_R,
  RLC_INDIRECT_HL,
  RL_R,
  RL_INDIRECT_HL,
  RRC_R,
  RRC_INDIRECT_HL,
  RR_R,
  RR_INDIRECT_HL,
  SLA_R,
  SLA_INDIRECT_HL,
  SLL_R,
  SLL_INDIRECT_HL,
  SRA_R,
  SRA_INDIRECT_HL,
  SRL_R,
  SRL_INDIRECT_HL,

  RLD_RRD,                /* Handle "RLD" and "RRD". */

  /* Bit set, reset, and test group. */

  BIT_B_R,
  BIT_B_INDIRECT_HL,
  SET_B_R,
  SET_B_INDIRECT_HL,
  RES_B_R,
  RES_B_INDIRECT_HL,

  /* Jump group. */

  JP_NN,
  JP_CC_NN,
  JR_E,
  JR_DD_E,
  JP_HL,
  DJNZ_E,

  /* Call and return group. */

  CALL_NN,
  CALL_CC_NN,
  RET,
  RET_CC,

  RETI_RETN,              /* Handle "RETI" and "RETN". */

  RST_P,

  /* Input and output group. */

  IN_A_N,
  IN_R_C,                 /* Correctly handle undocumented "IN F, (C)"
                           * instruction.
                           */

  INI_IND,                /* Handle "INI" and "IND". */
  INIR_INDR,              /* Handle "INIR" and "INDR". */

  OUT_N_A,
  OUT_C_R,                /* Correctly handle undocumented "OUT (C), 0"
                           * instruction.
                           */

  OUTI_OUTD,              /* Handle "OUTI" and "OUTD".*/
  OTIR_OTDR,              /* Handle "OTIR" and "OTDR". */

  /* Prefix group. */

  CB_PREFIX,
  DD_PREFIX,
  FD_PREFIX,
  ED_PREFIX,

  /* Special instruction group. */

  ED_UNDEFINED            /* ED_UNDEFINED is used to catch undefined
                           * 0xed prefixed opcodes.
                           */

};




/* Shortcuts for flags and registers. */

#define SZC_FLAGS       (Z80_S_FLAG | Z80_Z_FLAG | Z80_C_FLAG)
#define YX_FLAGS        (Z80_Y_FLAG | Z80_X_FLAG)
#define SZ_FLAGS        (Z80_S_FLAG | Z80_Z_FLAG)
#define SZPV_FLAGS      (Z80_S_FLAG | Z80_Z_FLAG | Z80_PV_FLAG)
#define SYX_FLAGS       (Z80_S_FLAG | Z80_Y_FLAG | Z80_X_FLAG)
#define HC_FLAGS        (Z80_H_FLAG | Z80_C_FLAG)

#define A               (state.registers.byte[Z80_A])
#define F               (state.registers.byte[Z80_F])
#define B               (state.registers.byte[Z80_B])
#define C               (state.registers.byte[Z80_C])

#define AF              (state.registers.word[Z80_AF])
#define BC              (state.registers.word[Z80_BC])
#define DE              (state.registers.word[Z80_DE])
#define HL              (state.registers.word[Z80_HL])
#define SP              (state.registers.word[Z80_SP])

#define HL_IX_IY        *((unsigned short *) registers[6])

/* Opcode decoding macros.  Y() is bits 5-3 of the opcode, Z() is bits 2-0,
 * P() bits 5-4, and Q() bits 4-3.
 */

#define Y(opcode)       (((opcode) >> 3) & 0x07)
#define Z(opcode)       ((opcode) & 0x07)
#define P(opcode)       (((opcode) >> 4) & 0x03)
#define Q(opcode)       (((opcode) >> 3) & 0x03)

/* Registers and conditions are decoded using tables in encodings.h.  S() is
 * for the special cases "LD H/L, (IX/Y + d)" and "LD (IX/Y + d), H/L".
 */

#define R(r)            *((unsigned char *) (registers[(r)]))
#define S(s)            *((unsigned char *) state.register_table[(s)])
#define RR(rr)          *((unsigned short *) registers[(rr) + 8])
#define SS(ss)          *((unsigned short *) registers[(ss) + 12])
#define CC(cc)          ((F ^ XOR_CONDITION_TABLE[(cc)])                \
& AND_CONDITION_TABLE[(cc)])
#define DD(dd)          CC(dd)

/* Macros to read constants, displacements, or addresses from code. */

#define READ_N(n)                                                       \
{                                                                       \
Z80_FETCH_BYTE(pc, (n));                                        \
pc++;                                                           \
elapsed_cycles += 3;                                            \
}

#define READ_NN(nn)                                                     \
{                                                                       \
Z80_FETCH_WORD(pc, (nn));                                       \
pc += 2;                                                        \
elapsed_cycles += 6;                                            \
}

#define READ_D(d)                                                       \
{                                                                       \
Z80_FETCH_BYTE(pc, (d));                                        \
(d) = (signed char) (d);					\
pc++;                                                           \
elapsed_cycles += 3;                                            \
}

/* Macros to read and write data. */

#define READ_BYTE(address, x)                                           \
{                                                                       \
Z80_READ_BYTE((address), (x));                                  \
elapsed_cycles += 3;                                            \
}

#define WRITE_BYTE(address, x)                                          \
{                                                                       \
Z80_WRITE_BYTE((address), (x));                                 \
elapsed_cycles += 3;                                            \
}

#define READ_WORD(address, x)                                           \
{                                                                       \
Z80_READ_WORD((address), (x));                                  \
elapsed_cycles += 6;                                            \
}

#define WRITE_WORD(address, x)                                          \
{                                                                       \
Z80_WRITE_WORD((address), (x));                                 \
elapsed_cycles += 6;                                            \
}

/* Indirect (HL) and indexed (IX + d) or (IY + d) memory operands read and
 * write macros.
 */

#define READ_INDIRECT_HL(x)                                             \
{                                                                       \
if (registers == state.register_table) {			\
\
READ_BYTE(HL, (x));                                     \
\
} else {                                                        \
\
int     d;                                              \
\
READ_D(d);                                              \
d += HL_IX_IY;                                          \
READ_BYTE(d, (x));                                      \
\
elapsed_cycles += 5;                                    \
\
}                                                               \
}

#define WRITE_INDIRECT_HL(x)                                            \
{                                                                       \
if (registers == state.register_table) {			\
\
WRITE_BYTE(HL, (x));                                    \
\
} else {                                                        \
\
int     d;                                              \
\
READ_D(d);                                              \
d += HL_IX_IY;                                          \
WRITE_BYTE(d, (x));                                     \
\
elapsed_cycles += 5;                                    \
\
}                                                               \
}

/* Stack operation macros. */

#define PUSH(x)                                                         \
{                                                                       \
SP -= 2;                                                        \
WRITE_WORD(SP, (x));                                            \
}

#define POP(x)                                                          \
{                                                                       \
READ_WORD(SP, (x));                                             \
SP += 2;                                                        \
}

/* Exchange macro. */

#define EXCHANGE(a, b)                                                  \
{                                                                       \
int     t;                                                      \
\
t = (a);                                                        \
(a) = (b);                                                      \
(b) = t;                                                        \
}

/* 8-bit arithmetic and logic operations. */

#define ADD(x)                                                          \
{                                                                       \
int     a, z, c, f;                                             \
\
a = A;                                                          \
z = a + (x);                                                    \
\
c = a ^ (x) ^ z;                                                \
f = c & Z80_H_FLAG;                                             \
f |= SZYX_FLAGS_TABLE[z & 0xff];                                \
f |= OVERFLOW_TABLE[c >> 7];                                    \
f |= z >> (8 - Z80_C_FLAG_SHIFT);                               \
\
A = z;                                                          \
F = f;                                                          \
}

#define ADC(x)                                                          \
{                                                                       \
int     a, z, c, f;                                             \
\
a = A;                                                          \
z = a + (x) + (F & Z80_C_FLAG);                                 \
\
c = a ^ (x) ^ z;                                                \
f = c & Z80_H_FLAG;                                             \
f |= SZYX_FLAGS_TABLE[z & 0xff];                                \
f |= OVERFLOW_TABLE[c >> 7];                                    \
f |= z >> (8 - Z80_C_FLAG_SHIFT);                               \
\
A = z;                                                          \
F = f;                                                          \
}

#define SUB(x)                                                          \
{                                                                       \
int     a, z, c, f;                                             \
\
a = A;                                                          \
z = a - (x);                                                    \
\
c = a ^ (x) ^ z;                                                \
f = Z80_N_FLAG | (c & Z80_H_FLAG);                              \
f |= SZYX_FLAGS_TABLE[z & 0xff];                                \
c &= 0x0180;                                                    \
f |= OVERFLOW_TABLE[c >> 7];                                    \
f |= c >> (8 - Z80_C_FLAG_SHIFT);                               \
\
A = z;                                                          \
F = f;                                                          \
}

#define SBC(x)                                                          \
{                                                                       \
int     a, z, c, f;                                             \
\
a = A;                                                          \
z = a - (x) - (F & Z80_C_FLAG);                                 \
\
c = a ^ (x) ^ z;                                                \
f = Z80_N_FLAG | (c & Z80_H_FLAG);                              \
f |= SZYX_FLAGS_TABLE[z & 0xff];                                \
c &= 0x0180;                                                    \
f |= OVERFLOW_TABLE[c >> 7];                                    \
f |= c >> (8 - Z80_C_FLAG_SHIFT);                               \
\
A = z;                                                          \
F = f;                                                          \
}

#define AND(x)                                                          \
{                                                                       \
F = SZYXP_FLAGS_TABLE[A &= (x)] | Z80_H_FLAG;                   \
}

#define OR(x)                                                           \
{                                                                       \
F = SZYXP_FLAGS_TABLE[A |= (x)];                                \
}

#define XOR(x)                                                          \
{                                                                       \
F = SZYXP_FLAGS_TABLE[A ^= (x)];                                \
}

#define CP(x)                                                           \
{                                                                       \
int     a, z, c, f;                                             \
\
a = A;                                                          \
z = a - (x);                                                    \
\
c = a ^ (x) ^ z;                                                \
f = Z80_N_FLAG | (c & Z80_H_FLAG);                              \
f |= SZYX_FLAGS_TABLE[z & 0xff] & SZ_FLAGS;                     \
f |= (x) & YX_FLAGS;                                            \
c &= 0x0180;                                                    \
f |= OVERFLOW_TABLE[c >> 7];                                    \
f |= c >> (8 - Z80_C_FLAG_SHIFT);                               \
\
F = f;                                                          \
}

#define INC(x)                                                          \
{                                                                       \
int     z, c, f;                                                \
\
z = (x) + 1;                                                    \
c = (x) ^ z;                                                    \
\
f = F & Z80_C_FLAG;                                             \
f |= c & Z80_H_FLAG;                                            \
f |= SZYX_FLAGS_TABLE[z & 0xff];                                \
f |= OVERFLOW_TABLE[(c >> 7) & 0x03];                           \
\
(x) = z;                                                        \
F = f;                                                          \
}

#define DEC(x)                                                          \
{                                                                       \
int     z, c, f;                                                \
\
z = (x) - 1;                                                    \
c = (x) ^ z;                                                    \
\
f = Z80_N_FLAG | (F & Z80_C_FLAG);                              \
f |= c & Z80_H_FLAG;                                            \
f |= SZYX_FLAGS_TABLE[z & 0xff];                                \
f |= OVERFLOW_TABLE[(c >> 7) & 0x03];                           \
\
(x) = z;                                                        \
F = f;                                                          \
}

/* 0xcb prefixed logical operations. */

#define RLC(x)                                                          \
{                                                                       \
int     c;                                                      \
\
c = (x) >> 7;                                                   \
(x) = ((x) << 1) | c;                                           \
F = SZYXP_FLAGS_TABLE[(x) & 0xff] | c;                          \
}

#define RL(x)                                                           \
{                                                                       \
int     c;                                                      \
\
c = (x) >> 7;                                                   \
(x) = ((x) << 1) | (F & Z80_C_FLAG);                            \
F = SZYXP_FLAGS_TABLE[(x) & 0xff] | c;                          \
}

#define RRC(x)                                                          \
{                                                                       \
int     c;                                                      \
\
c = (x) & 0x01;                                                 \
(x) = ((x) >> 1) | (c << 7);                                    \
F = SZYXP_FLAGS_TABLE[(x) & 0xff] | c;                          \
}

#define RR_INSTRUCTION(x)                                               \
{                                                                       \
int     c;                                                      \
\
c = (x) & 0x01;                                                 \
(x) = ((x) >> 1) | ((F & Z80_C_FLAG) << 7);                     \
F = SZYXP_FLAGS_TABLE[(x) & 0xff] | c;                          \
}

#define SLA(x)                                                          \
{                                                                       \
int     c;                                                      \
\
c = (x) >> 7;                                                   \
(x) <<= 1;                                                      \
F = SZYXP_FLAGS_TABLE[(x) & 0xff] | c;                          \
}

#define SLL(x)                                                          \
{                                                                       \
int     c;                                                      \
\
c = (x) >> 7;                                                   \
(x) = ((x) << 1) | 0x01;                                        \
F = SZYXP_FLAGS_TABLE[(x) & 0xff] | c;                          \
}

#define SRA(x)                                                          \
{                                                                       \
int     c;                                                      \
\
c = (x) & 0x01;                                                 \
(x) = ((signed char) (x)) >> 1;  				\
F = SZYXP_FLAGS_TABLE[(x) & 0xff] | c;                          \
}

#define SRL(x)                                                          \
{                                                                       \
int     c;                                                      \
\
c = (x) & 0x01;                                                 \
(x) >>= 1;                                                      \
F = SZYXP_FLAGS_TABLE[(x) & 0xff] | c;                          \
}





/* Generated file, see maketables.c. */

static const unsigned char INSTRUCTION_TABLE[256] = {

  NOP,
  LD_RR_NN,
  LD_INDIRECT_BC_A,
  INC_RR,
  INC_R,
  DEC_R,
  LD_R_N,
  RLCA,

  EX_AF_AF_PRIME,
  ADD_HL_RR,
  LD_A_INDIRECT_BC,
  DEC_RR,
  INC_R,
  DEC_R,
  LD_R_N,
  RRCA,

  DJNZ_E,
  LD_RR_NN,
  LD_INDIRECT_DE_A,
  INC_RR,
  INC_R,
  DEC_R,
  LD_R_N,
  RLA,

  JR_E,
  ADD_HL_RR,
  LD_A_INDIRECT_DE,
  DEC_RR,
  INC_R,
  DEC_R,
  LD_R_N,
  RRA,

  JR_DD_E,
  LD_RR_NN,
  LD_INDIRECT_NN_HL,
  INC_RR,
  INC_R,
  DEC_R,
  LD_R_N,
  DAA,

  JR_DD_E,
  ADD_HL_RR,
  LD_HL_INDIRECT_NN,
  DEC_RR,
  INC_R,
  DEC_R,
  LD_R_N,
  CPL,

  JR_DD_E,
  LD_RR_NN,
  LD_INDIRECT_NN_A,
  INC_RR,
  INC_INDIRECT_HL,
  DEC_INDIRECT_HL,
  LD_INDIRECT_HL_N,
  SCF,

  JR_DD_E,
  ADD_HL_RR,
  LD_A_INDIRECT_NN,
  DEC_RR,
  INC_R,
  DEC_R,
  LD_R_N,
  CCF,

  NOP,
  LD_R_R,
  LD_R_R,
  LD_R_R,
  LD_R_R,
  LD_R_R,
  LD_R_INDIRECT_HL,
  LD_R_R,

  LD_R_R,
  NOP,
  LD_R_R,
  LD_R_R,
  LD_R_R,
  LD_R_R,
  LD_R_INDIRECT_HL,
  LD_R_R,

  LD_R_R,
  LD_R_R,
  NOP,
  LD_R_R,
  LD_R_R,
  LD_R_R,
  LD_R_INDIRECT_HL,
  LD_R_R,

  LD_R_R,
  LD_R_R,
  LD_R_R,
  NOP,
  LD_R_R,
  LD_R_R,
  LD_R_INDIRECT_HL,
  LD_R_R,

  LD_R_R,
  LD_R_R,
  LD_R_R,
  LD_R_R,
  NOP,
  LD_R_R,
  LD_R_INDIRECT_HL,
  LD_R_R,

  LD_R_R,
  LD_R_R,
  LD_R_R,
  LD_R_R,
  LD_R_R,
  NOP,
  LD_R_INDIRECT_HL,
  LD_R_R,

  LD_INDIRECT_HL_R,
  LD_INDIRECT_HL_R,
  LD_INDIRECT_HL_R,
  LD_INDIRECT_HL_R,
  LD_INDIRECT_HL_R,
  LD_INDIRECT_HL_R,
  HALT,
  LD_INDIRECT_HL_R,

  LD_R_R,
  LD_R_R,
  LD_R_R,
  LD_R_R,
  LD_R_R,
  LD_R_R,
  LD_R_INDIRECT_HL,
  NOP,

  ADD_R,
  ADD_R,
  ADD_R,
  ADD_R,
  ADD_R,
  ADD_R,
  ADD_INDIRECT_HL,
  ADD_R,

  ADC_R,
  ADC_R,
  ADC_R,
  ADC_R,
  ADC_R,
  ADC_R,
  ADC_INDIRECT_HL,
  ADC_R,

  SUB_R,
  SUB_R,
  SUB_R,
  SUB_R,
  SUB_R,
  SUB_R,
  SUB_INDIRECT_HL,
  SUB_R,

  SBC_R,
  SBC_R,
  SBC_R,
  SBC_R,
  SBC_R,
  SBC_R,
  SBC_INDIRECT_HL,
  SBC_R,

  AND_R,
  AND_R,
  AND_R,
  AND_R,
  AND_R,
  AND_R,
  AND_INDIRECT_HL,
  AND_R,

  XOR_R,
  XOR_R,
  XOR_R,
  XOR_R,
  XOR_R,
  XOR_R,
  XOR_INDIRECT_HL,
  XOR_R,

  OR_R,
  OR_R,
  OR_R,
  OR_R,
  OR_R,
  OR_R,
  OR_INDIRECT_HL,
  OR_R,

  CP_R,
  CP_R,
  CP_R,
  CP_R,
  CP_R,
  CP_R,
  CP_INDIRECT_HL,
  CP_R,

  RET_CC,
  POP_SS,
  JP_CC_NN,
  JP_NN,
  CALL_CC_NN,
  PUSH_SS,
  ADD_N,
  RST_P,

  RET_CC,
  RET,
  JP_CC_NN,
  CB_PREFIX,
  CALL_CC_NN,
  CALL_NN,
  ADC_N,
  RST_P,

  RET_CC,
  POP_SS,
  JP_CC_NN,
  OUT_N_A,
  CALL_CC_NN,
  PUSH_SS,
  SUB_N,
  RST_P,

  RET_CC,
  EXX,
  JP_CC_NN,
  IN_A_N,
  CALL_CC_NN,
  DD_PREFIX,
  SBC_N,
  RST_P,

  RET_CC,
  POP_SS,
  JP_CC_NN,
  EX_INDIRECT_SP_HL,
  CALL_CC_NN,
  PUSH_SS,
  AND_N,
  RST_P,

  RET_CC,
  JP_HL,
  JP_CC_NN,
  EX_DE_HL,
  CALL_CC_NN,
  ED_PREFIX,
  XOR_N,
  RST_P,

  RET_CC,
  POP_SS,
  JP_CC_NN,
  DI,
  CALL_CC_NN,
  PUSH_SS,
  OR_N,
  RST_P,

  RET_CC,
  LD_SP_HL,
  JP_CC_NN,
  EI,
  CALL_CC_NN,
  FD_PREFIX,
  CP_N,
  RST_P,

};


static const unsigned char CB_INSTRUCTION_TABLE[256] = {

  RLC_R,
  RLC_R,
  RLC_R,
  RLC_R,
  RLC_R,
  RLC_R,
  RLC_INDIRECT_HL,
  RLC_R,

  RRC_R,
  RRC_R,
  RRC_R,
  RRC_R,
  RRC_R,
  RRC_R,
  RRC_INDIRECT_HL,
  RRC_R,

  RL_R,
  RL_R,
  RL_R,
  RL_R,
  RL_R,
  RL_R,
  RL_INDIRECT_HL,
  RL_R,

  RR_R,
  RR_R,
  RR_R,
  RR_R,
  RR_R,
  RR_R,
  RR_INDIRECT_HL,
  RR_R,

  SLA_R,
  SLA_R,
  SLA_R,
  SLA_R,
  SLA_R,
  SLA_R,
  SLA_INDIRECT_HL,
  SLA_R,

  SRA_R,
  SRA_R,
  SRA_R,
  SRA_R,
  SRA_R,
  SRA_R,
  SRA_INDIRECT_HL,
  SRA_R,

  SLL_R,
  SLL_R,
  SLL_R,
  SLL_R,
  SLL_R,
  SLL_R,
  SLL_INDIRECT_HL,
  SLL_R,

  SRL_R,
  SRL_R,
  SRL_R,
  SRL_R,
  SRL_R,
  SRL_R,
  SRL_INDIRECT_HL,
  SRL_R,

  BIT_B_R,
  BIT_B_R,
  BIT_B_R,
  BIT_B_R,
  BIT_B_R,
  BIT_B_R,
  BIT_B_INDIRECT_HL,
  BIT_B_R,

  BIT_B_R,
  BIT_B_R,
  BIT_B_R,
  BIT_B_R,
  BIT_B_R,
  BIT_B_R,
  BIT_B_INDIRECT_HL,
  BIT_B_R,

  BIT_B_R,
  BIT_B_R,
  BIT_B_R,
  BIT_B_R,
  BIT_B_R,
  BIT_B_R,
  BIT_B_INDIRECT_HL,
  BIT_B_R,

  BIT_B_R,
  BIT_B_R,
  BIT_B_R,
  BIT_B_R,
  BIT_B_R,
  BIT_B_R,
  BIT_B_INDIRECT_HL,
  BIT_B_R,

  BIT_B_R,
  BIT_B_R,
  BIT_B_R,
  BIT_B_R,
  BIT_B_R,
  BIT_B_R,
  BIT_B_INDIRECT_HL,
  BIT_B_R,

  BIT_B_R,
  BIT_B_R,
  BIT_B_R,
  BIT_B_R,
  BIT_B_R,
  BIT_B_R,
  BIT_B_INDIRECT_HL,
  BIT_B_R,

  BIT_B_R,
  BIT_B_R,
  BIT_B_R,
  BIT_B_R,
  BIT_B_R,
  BIT_B_R,
  BIT_B_INDIRECT_HL,
  BIT_B_R,

  BIT_B_R,
  BIT_B_R,
  BIT_B_R,
  BIT_B_R,
  BIT_B_R,
  BIT_B_R,
  BIT_B_INDIRECT_HL,
  BIT_B_R,

  RES_B_R,
  RES_B_R,
  RES_B_R,
  RES_B_R,
  RES_B_R,
  RES_B_R,
  RES_B_INDIRECT_HL,
  RES_B_R,

  RES_B_R,
  RES_B_R,
  RES_B_R,
  RES_B_R,
  RES_B_R,
  RES_B_R,
  RES_B_INDIRECT_HL,
  RES_B_R,

  RES_B_R,
  RES_B_R,
  RES_B_R,
  RES_B_R,
  RES_B_R,
  RES_B_R,
  RES_B_INDIRECT_HL,
  RES_B_R,

  RES_B_R,
  RES_B_R,
  RES_B_R,
  RES_B_R,
  RES_B_R,
  RES_B_R,
  RES_B_INDIRECT_HL,
  RES_B_R,

  RES_B_R,
  RES_B_R,
  RES_B_R,
  RES_B_R,
  RES_B_R,
  RES_B_R,
  RES_B_INDIRECT_HL,
  RES_B_R,

  RES_B_R,
  RES_B_R,
  RES_B_R,
  RES_B_R,
  RES_B_R,
  RES_B_R,
  RES_B_INDIRECT_HL,
  RES_B_R,

  RES_B_R,
  RES_B_R,
  RES_B_R,
  RES_B_R,
  RES_B_R,
  RES_B_R,
  RES_B_INDIRECT_HL,
  RES_B_R,

  RES_B_R,
  RES_B_R,
  RES_B_R,
  RES_B_R,
  RES_B_R,
  RES_B_R,
  RES_B_INDIRECT_HL,
  RES_B_R,

  SET_B_R,
  SET_B_R,
  SET_B_R,
  SET_B_R,
  SET_B_R,
  SET_B_R,
  SET_B_INDIRECT_HL,
  SET_B_R,

  SET_B_R,
  SET_B_R,
  SET_B_R,
  SET_B_R,
  SET_B_R,
  SET_B_R,
  SET_B_INDIRECT_HL,
  SET_B_R,

  SET_B_R,
  SET_B_R,
  SET_B_R,
  SET_B_R,
  SET_B_R,
  SET_B_R,
  SET_B_INDIRECT_HL,
  SET_B_R,

  SET_B_R,
  SET_B_R,
  SET_B_R,
  SET_B_R,
  SET_B_R,
  SET_B_R,
  SET_B_INDIRECT_HL,
  SET_B_R,

  SET_B_R,
  SET_B_R,
  SET_B_R,
  SET_B_R,
  SET_B_R,
  SET_B_R,
  SET_B_INDIRECT_HL,
  SET_B_R,

  SET_B_R,
  SET_B_R,
  SET_B_R,
  SET_B_R,
  SET_B_R,
  SET_B_R,
  SET_B_INDIRECT_HL,
  SET_B_R,

  SET_B_R,
  SET_B_R,
  SET_B_R,
  SET_B_R,
  SET_B_R,
  SET_B_R,
  SET_B_INDIRECT_HL,
  SET_B_R,

  SET_B_R,
  SET_B_R,
  SET_B_R,
  SET_B_R,
  SET_B_R,
  SET_B_R,
  SET_B_INDIRECT_HL,
  SET_B_R,

};


static const unsigned char ED_INSTRUCTION_TABLE[256] = {

  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,

  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,

  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,

  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,

  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,

  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,

  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,

  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,

  IN_R_C,
  OUT_C_R,
  SBC_HL_RR,
  LD_INDIRECT_NN_RR,
  NEG,
  RETI_RETN,
  IM_N,
  LD_I_A_LD_R_A,

  IN_R_C,
  OUT_C_R,
  ADC_HL_RR,
  LD_RR_INDIRECT_NN,
  NEG,
  RETI_RETN,
  IM_N,
  LD_I_A_LD_R_A,

  IN_R_C,
  OUT_C_R,
  SBC_HL_RR,
  LD_INDIRECT_NN_RR,
  NEG,
  RETI_RETN,
  IM_N,
  LD_A_I_LD_A_R,

  IN_R_C,
  OUT_C_R,
  ADC_HL_RR,
  LD_RR_INDIRECT_NN,
  NEG,
  RETI_RETN,
  IM_N,
  LD_A_I_LD_A_R,

  IN_R_C,
  OUT_C_R,
  SBC_HL_RR,
  LD_INDIRECT_NN_RR,
  NEG,
  RETI_RETN,
  IM_N,
  RLD_RRD,

  IN_R_C,
  OUT_C_R,
  ADC_HL_RR,
  LD_RR_INDIRECT_NN,
  NEG,
  RETI_RETN,
  IM_N,
  RLD_RRD,

  IN_R_C,
  OUT_C_R,
  SBC_HL_RR,
  LD_INDIRECT_NN_RR,
  NEG,
  RETI_RETN,
  IM_N,
  ED_UNDEFINED,

  IN_R_C,
  OUT_C_R,
  ADC_HL_RR,
  LD_RR_INDIRECT_NN,
  NEG,
  RETI_RETN,
  IM_N,
  ED_UNDEFINED,

  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,

  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,

  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,

  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,

  LDI_LDD,
  CPI_CPD,
  INI_IND,
  OUTI_OUTD,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,

  LDI_LDD,
  CPI_CPD,
  INI_IND,
  OUTI_OUTD,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,

  LDIR_LDDR,
  CPIR_CPDR,
  INIR_INDR,
  OTIR_OTDR,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,

  LDIR_LDDR,
  CPIR_CPDR,
  INIR_INDR,
  OTIR_OTDR,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,

  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,

  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,

  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,

  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,

  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,

  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,

  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,

  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,
  ED_UNDEFINED,

};

static const unsigned char SZYX_FLAGS_TABLE[256] = {

  0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
  0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28,
  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
  0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
  0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28,
  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
  0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
  0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0,
  0xa8, 0xa8, 0xa8, 0xa8, 0xa8, 0xa8, 0xa8, 0xa8,
  0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0,
  0xa8, 0xa8, 0xa8, 0xa8, 0xa8, 0xa8, 0xa8, 0xa8,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
  0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0,
  0xa8, 0xa8, 0xa8, 0xa8, 0xa8, 0xa8, 0xa8, 0xa8,
  0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0,
  0xa8, 0xa8, 0xa8, 0xa8, 0xa8, 0xa8, 0xa8, 0xa8,

};

static const unsigned char SZYXP_FLAGS_TABLE[256] = {

  0x44, 0x00, 0x00, 0x04, 0x00, 0x04, 0x04, 0x00,
  0x08, 0x0c, 0x0c, 0x08, 0x0c, 0x08, 0x08, 0x0c,
  0x00, 0x04, 0x04, 0x00, 0x04, 0x00, 0x00, 0x04,
  0x0c, 0x08, 0x08, 0x0c, 0x08, 0x0c, 0x0c, 0x08,
  0x20, 0x24, 0x24, 0x20, 0x24, 0x20, 0x20, 0x24,
  0x2c, 0x28, 0x28, 0x2c, 0x28, 0x2c, 0x2c, 0x28,
  0x24, 0x20, 0x20, 0x24, 0x20, 0x24, 0x24, 0x20,
  0x28, 0x2c, 0x2c, 0x28, 0x2c, 0x28, 0x28, 0x2c,
  0x00, 0x04, 0x04, 0x00, 0x04, 0x00, 0x00, 0x04,
  0x0c, 0x08, 0x08, 0x0c, 0x08, 0x0c, 0x0c, 0x08,
  0x04, 0x00, 0x00, 0x04, 0x00, 0x04, 0x04, 0x00,
  0x08, 0x0c, 0x0c, 0x08, 0x0c, 0x08, 0x08, 0x0c,
  0x24, 0x20, 0x20, 0x24, 0x20, 0x24, 0x24, 0x20,
  0x28, 0x2c, 0x2c, 0x28, 0x2c, 0x28, 0x28, 0x2c,
  0x20, 0x24, 0x24, 0x20, 0x24, 0x20, 0x20, 0x24,
  0x2c, 0x28, 0x28, 0x2c, 0x28, 0x2c, 0x2c, 0x28,
  0x80, 0x84, 0x84, 0x80, 0x84, 0x80, 0x80, 0x84,
  0x8c, 0x88, 0x88, 0x8c, 0x88, 0x8c, 0x8c, 0x88,
  0x84, 0x80, 0x80, 0x84, 0x80, 0x84, 0x84, 0x80,
  0x88, 0x8c, 0x8c, 0x88, 0x8c, 0x88, 0x88, 0x8c,
  0xa4, 0xa0, 0xa0, 0xa4, 0xa0, 0xa4, 0xa4, 0xa0,
  0xa8, 0xac, 0xac, 0xa8, 0xac, 0xa8, 0xa8, 0xac,
  0xa0, 0xa4, 0xa4, 0xa0, 0xa4, 0xa0, 0xa0, 0xa4,
  0xac, 0xa8, 0xa8, 0xac, 0xa8, 0xac, 0xac, 0xa8,
  0x84, 0x80, 0x80, 0x84, 0x80, 0x84, 0x84, 0x80,
  0x88, 0x8c, 0x8c, 0x88, 0x8c, 0x88, 0x88, 0x8c,
  0x80, 0x84, 0x84, 0x80, 0x84, 0x80, 0x80, 0x84,
  0x8c, 0x88, 0x88, 0x8c, 0x88, 0x8c, 0x8c, 0x88,
  0xa0, 0xa4, 0xa4, 0xa0, 0xa4, 0xa0, 0xa0, 0xa4,
  0xac, 0xa8, 0xa8, 0xac, 0xa8, 0xac, 0xac, 0xa8,
  0xa4, 0xa0, 0xa0, 0xa4, 0xa0, 0xa4, 0xa4, 0xa0,
  0xa8, 0xac, 0xac, 0xa8, 0xac, 0xa8, 0xa8, 0xac,

};




/* Indirect (HL) or prefixed indexed (IX + d) and (IY + d) memory operands are
 * encoded using the 3 bits "110" (0x06).
 */

#define INDIRECT_HL 0x06


/* Condition codes are encoded using 2 or 3 bits.  The xor table is needed for
 * negated conditions, it is used along with the and table.
 */

static const int XOR_CONDITION_TABLE[8] = {
  Z80_Z_FLAG,
  0,
  Z80_C_FLAG,
  0,
  Z80_P_FLAG,
  0,
  Z80_S_FLAG,
  0,
};


static const int AND_CONDITION_TABLE[8] = {
  Z80_Z_FLAG,
  Z80_Z_FLAG,
  Z80_C_FLAG,
  Z80_C_FLAG,
  Z80_P_FLAG,
  Z80_P_FLAG,
  Z80_S_FLAG,
  Z80_S_FLAG,
};


/* RST instruction restart addresses, encoded by Y() bits of the opcode. */

static const int RST_TABLE[8] = {
  0x00,
  0x08,
  0x10,
  0x18,
  0x20,
  0x28,
  0x30,
  0x38,
};


/* There is an overflow if the xor of the carry out and the carry of the most
 * significant bit is not zero.
 */

static const int OVERFLOW_TABLE[4] = {
  0,
  Z80_V_FLAG,
  Z80_V_FLAG,
  0,
};




void Z80::reset()
{
  state.status = 0;
  AF = 0xffff;
  SP = 0xffff;
  state.i = state.pc = state.iff1 = state.iff2 = 0;
  state.im = Z80_INTERRUPT_MODE_0;

  /* Build register decoding tables for both 3-bit encoded 8-bit
   * registers and 2-bit encoded 16-bit registers. When an opcode is
   * prefixed by 0xdd, HL is replaced by IX. When 0xfd prefixed, HL is
   * replaced by IY.
   */

  /* 8-bit "R" registers. */

  state.register_table[0] = &state.registers.byte[Z80_B];
  state.register_table[1] = &state.registers.byte[Z80_C];
  state.register_table[2] = &state.registers.byte[Z80_D];
  state.register_table[3] = &state.registers.byte[Z80_E];
  state.register_table[4] = &state.registers.byte[Z80_H];
  state.register_table[5] = &state.registers.byte[Z80_L];

  /* Encoding 0x06 is used for indexed memory operands and direct HL or
   * IX/IY register access.
   */

  state.register_table[6] = &state.registers.word[Z80_HL];
  state.register_table[7] = &state.registers.byte[Z80_A];

  /* "Regular" 16-bit "RR" registers. */

  state.register_table[8] = &state.registers.word[Z80_BC];
  state.register_table[9] = &state.registers.word[Z80_DE];
  state.register_table[10] = &state.registers.word[Z80_HL];
  state.register_table[11] = &state.registers.word[Z80_SP];

  /* 16-bit "SS" registers for PUSH and POP instructions (note that SP is
   * replaced by AF).
   */

  state.register_table[12] = &state.registers.word[Z80_BC];
  state.register_table[13] = &state.registers.word[Z80_DE];
  state.register_table[14] = &state.registers.word[Z80_HL];
  state.register_table[15] = &state.registers.word[Z80_AF];

  /* 0xdd and 0xfd prefixed register decoding tables. */

  for (int i = 0; i < 16; i++)
    state.dd_register_table[i] = state.fd_register_table[i] = state.register_table[i];

  state.dd_register_table[4] = &state.registers.byte[Z80_IXH];
  state.dd_register_table[5] = &state.registers.byte[Z80_IXL];
  state.dd_register_table[6] = &state.registers.word[Z80_IX];
  state.dd_register_table[10] = &state.registers.word[Z80_IX];
  state.dd_register_table[14] = &state.registers.word[Z80_IX];

  state.fd_register_table[4] = &state.registers.byte[Z80_IYH];
  state.fd_register_table[5] = &state.registers.byte[Z80_IYL];
  state.fd_register_table[6] = &state.registers.word[Z80_IY];
  state.fd_register_table[10] = &state.registers.word[Z80_IY];
  state.fd_register_table[14] = &state.registers.word[Z80_IY];
}



int Z80::IRQ(int data_on_bus)
{
  state.status = 0;
  if (state.iff1) {

    state.iff1 = state.iff2 = 0;
    state.r = (state.r & 0x80) | ((state.r + 1) & 0x7f);
    switch (state.im) {

      case Z80_INTERRUPT_MODE_0: {

        /* Assuming the opcode in data_on_bus is an
         * RST instruction, accepting the interrupt
         * should take 2 + 11 = 13 cycles.
         */

        return intemulate(data_on_bus, 2);

      }

      case Z80_INTERRUPT_MODE_1: {

        int	elapsed_cycles;

        elapsed_cycles = 0;
        SP -= 2;
        Z80_WRITE_WORD_INTERRUPT(SP, state.pc);
        state.pc = 0x0038;
        return elapsed_cycles + 13;

      }

      case Z80_INTERRUPT_MODE_2:
      default: {

        int	elapsed_cycles, vector;

        elapsed_cycles = 0;
        SP -= 2;
        Z80_WRITE_WORD_INTERRUPT(SP, state.pc);
        vector = state.i << 8 | data_on_bus;

#ifdef Z80_MASK_IM2_VECTOR_ADDRESS

        vector &= 0xfffe;

#endif

        Z80_READ_WORD_INTERRUPT(vector, state.pc);
        return elapsed_cycles + 19;

      }

    }

  } else

    return 0;
}


int Z80::NMI()
{
  int	elapsed_cycles;

  state.status = 0;

  state.iff2 = state.iff1;
  state.iff1 = 0;
  state.r = (state.r & 0x80) | ((state.r + 1) & 0x7f);

  elapsed_cycles = 0;
  SP -= 2;
  Z80_WRITE_WORD_INTERRUPT(SP, state.pc);
  state.pc = 0x0066;

  return elapsed_cycles + 11;
}


int Z80::step()
{
  state.status = 0;
  int elapsed_cycles = 0;
  int pc = state.pc;
  int opcode;
  Z80_FETCH_BYTE(pc, opcode);
  state.pc = pc + 1;

  return intemulate(opcode, elapsed_cycles);
}


/* Actual emulation function. opcode is the first opcode to emulate, this is
 * needed by Z80Interrupt() for interrupt mode 0.
 */

int Z80::intemulate(int opcode, int elapsed_cycles)
{
  int pc = state.pc;
  int r = state.r & 0x7f;

  void * * registers = state.register_table;

  int instruction = INSTRUCTION_TABLE[opcode];

  bool repeatLoop;

  do {

    repeatLoop = false;

    elapsed_cycles += 4;
    r++;
    switch (instruction) {

        /* 8-bit load group. */

      case LD_R_R: {

        R(Y(opcode)) = R(Z(opcode));
        break;

      }

      case LD_R_N: {

        READ_N(R(Y(opcode)));
        break;

      }

      case LD_R_INDIRECT_HL: {

        if (registers == state.register_table) {

          READ_BYTE(HL, R(Y(opcode)));

        } else {

          int     d;

          READ_D(d);
          d += HL_IX_IY;
          READ_BYTE(d, S(Y(opcode)));

          elapsed_cycles += 5;

        }
        break;

      }

      case LD_INDIRECT_HL_R: {

        if (registers == state.register_table) {

          WRITE_BYTE(HL, R(Z(opcode)));

        } else {

          int     d;

          READ_D(d);
          d += HL_IX_IY;
          WRITE_BYTE(d, S(Z(opcode)));

          elapsed_cycles += 5;

        }
        break;

      }

      case LD_INDIRECT_HL_N: {

        int     n;

        if (registers == state.register_table) {

          READ_N(n);
          WRITE_BYTE(HL, n);

        } else {

          int     d;

          READ_D(d);
          d += HL_IX_IY;
          READ_N(n);
          WRITE_BYTE(d, n);

          elapsed_cycles += 2;

        }

        break;

      }

      case LD_A_INDIRECT_BC: {

        READ_BYTE(BC, A);
        break;

      }

      case LD_A_INDIRECT_DE: {

        READ_BYTE(DE, A);
        break;

      }

      case LD_A_INDIRECT_NN: {

        int     nn;

        READ_NN(nn);
        READ_BYTE(nn, A);
        break;

      }

      case LD_INDIRECT_BC_A: {

        WRITE_BYTE(BC, A);
        break;

      }

      case LD_INDIRECT_DE_A: {

        WRITE_BYTE(DE, A);
        break;

      }

      case LD_INDIRECT_NN_A: {

        int     nn;

        READ_NN(nn);
        WRITE_BYTE(nn, A);
        break;

      }

      case LD_A_I_LD_A_R: {

        int     a, f;

        a = opcode == OPCODE_LD_A_I
        ? state.i
        : (state.r & 0x80) | (r & 0x7f);
        f = SZYX_FLAGS_TABLE[a];

        /* Note: On a real processor, if an interrupt
         * occurs during the execution of either
         * "LD A, I" or "LD A, R", the parity flag is
         * reset. That can never happen here.
         */

        f |= state.iff2 << Z80_P_FLAG_SHIFT;
        f |= F & Z80_C_FLAG;

        AF = (a << 8) | f;

        elapsed_cycles++;

        break;

      }

      case LD_I_A_LD_R_A: {

        if (opcode == OPCODE_LD_I_A)

          state.i = A;

        else {

          state.r = A;
          r = A & 0x7f;

        }

        elapsed_cycles++;

        break;

      }

        /* 16-bit load group. */

      case LD_RR_NN: {

        READ_NN(RR(P(opcode)));
        break;

      }

      case LD_HL_INDIRECT_NN: {

        int     nn;

        READ_NN(nn);
        READ_WORD(nn, HL_IX_IY);
        break;

      }

      case LD_RR_INDIRECT_NN: {

        int     nn;

        READ_NN(nn);
        READ_WORD(nn, RR(P(opcode)));
        break;

      }

      case LD_INDIRECT_NN_HL: {

        int     nn;

        READ_NN(nn);
        WRITE_WORD(nn, HL_IX_IY);
        break;

      }

      case LD_INDIRECT_NN_RR: {

        int     nn;

        READ_NN(nn);
        WRITE_WORD(nn, RR(P(opcode)));
        break;

      }

      case LD_SP_HL: {

        SP = HL_IX_IY;
        elapsed_cycles += 2;
        break;

      }

      case PUSH_SS: {

        PUSH(SS(P(opcode)));
        elapsed_cycles++;
        break;

      }

      case POP_SS: {

        POP(SS(P(opcode)));
        break;

      }

        /* Exchange, block transfer and search group. */

      case EX_DE_HL: {

        EXCHANGE(DE, HL);
        break;

      }

      case EX_AF_AF_PRIME: {

        EXCHANGE(AF, state.alternates[Z80_AF]);
        break;

      }

      case EXX: {

        EXCHANGE(BC, state.alternates[Z80_BC]);
        EXCHANGE(DE, state.alternates[Z80_DE]);
        EXCHANGE(HL, state.alternates[Z80_HL]);
        break;

      }

      case EX_INDIRECT_SP_HL: {

        int     t;

        READ_WORD(SP, t);
        WRITE_WORD(SP, HL_IX_IY);
        HL_IX_IY = t;

        elapsed_cycles += 3;

        break;
      }

      case LDI_LDD: {

        int     n, f, d;

        READ_BYTE(HL, n);
        WRITE_BYTE(DE, n);

        f = F & SZC_FLAGS;
        f |= --BC ? Z80_P_FLAG : 0;

  #ifndef Z80_DOCUMENTED_FLAGS_ONLY

        n += A;
        f |= n & Z80_X_FLAG;
        f |= (n << (Z80_Y_FLAG_SHIFT - 1))
        & Z80_Y_FLAG;

  #endif

        F = f;

        d = opcode == OPCODE_LDI ? +1 : -1;
        DE += d;
        HL += d;

        elapsed_cycles += 2;

        break;

      }

      case LDIR_LDDR: {

        int     d, f, bc, de, hl, n;

  #ifdef Z80_HANDLE_SELF_MODIFYING_CODE

        int     p, q;

        p = (pc - 2) & 0xffff;
        q = (pc - 1) & 0xffff;

  #endif

        d = opcode == OPCODE_LDIR ? +1 : -1;

        f = F & SZC_FLAGS;
        bc = BC;
        de = DE;
        hl = HL;

        r -= 2;
        elapsed_cycles -= 8;
        for ( ; ; ) {

          r += 2;

          Z80_READ_BYTE(hl, n);
          Z80_WRITE_BYTE(de, n);

          hl += d;
          de += d;

          if (--bc)

            elapsed_cycles += 21;

          else {

            elapsed_cycles += 16;
            break;

          }

  #ifdef Z80_HANDLE_SELF_MODIFYING_CODE

          if (((de - d) & 0xffff) == p
              || ((de - d) & 0xffff) == q) {

            f |= Z80_P_FLAG;
            pc -= 2;
            break;

          }

  #endif

          f |= Z80_P_FLAG;
          pc -= 2;
          break;

        }

        HL = hl;
        DE = de;
        BC = bc;

  #ifndef Z80_DOCUMENTED_FLAGS_ONLY

        n += A;
        f |= n & Z80_X_FLAG;
        f |= (n << (Z80_Y_FLAG_SHIFT - 1))
        & Z80_Y_FLAG;

  #endif

        F = f;

        break;

      }

      case CPI_CPD: {

        int     a, n, z, f;

        a = A;
        READ_BYTE(HL, n);
        z = a - n;

        HL += opcode == OPCODE_CPI ? +1 : -1;

        f = (a ^ n ^ z) & Z80_H_FLAG;

  #ifndef Z80_DOCUMENTED_FLAGS_ONLY

        n = z - (f >> Z80_H_FLAG_SHIFT);
        f |= (n << (Z80_Y_FLAG_SHIFT - 1))
        & Z80_Y_FLAG;
        f |= n & Z80_X_FLAG;

  #endif

        f |= SZYX_FLAGS_TABLE[z & 0xff] & SZ_FLAGS;
        f |= --BC ? Z80_P_FLAG : 0;
        F = f | Z80_N_FLAG | (F & Z80_C_FLAG);

        elapsed_cycles += 5;

        break;

      }

      case CPIR_CPDR: {

        int     d, a, bc, hl, n, z, f;

        d = opcode == OPCODE_CPIR ? +1 : -1;

        a = A;
        bc = BC;
        hl = HL;

        r -= 2;
        elapsed_cycles -= 8;
        for ( ; ; ) {

          r += 2;

          Z80_READ_BYTE(hl, n);
          z = a - n;

          hl += d;
          if (--bc && z)

            elapsed_cycles += 21;

          else {

            elapsed_cycles += 16;
            break;

          }

          pc -= 2;
          break;

        }

        HL = hl;
        BC = bc;

        f = (a ^ n ^ z) & Z80_H_FLAG;

  #ifndef Z80_DOCUMENTED_FLAGS_ONLY

        n = z - (f >> Z80_H_FLAG_SHIFT);
        f |= (n << (Z80_Y_FLAG_SHIFT - 1))
        & Z80_Y_FLAG;
        f |= n & Z80_X_FLAG;

  #endif

        f |= SZYX_FLAGS_TABLE[z & 0xff] & SZ_FLAGS;
        f |= bc ? Z80_P_FLAG : 0;
        F = f | Z80_N_FLAG | (F & Z80_C_FLAG);

        break;

      }

        /* 8-bit arithmetic and logical group. */

      case ADD_R: {

        ADD(R(Z(opcode)));
        break;

      }

      case ADD_N: {

        int     n;

        READ_N(n);
        ADD(n);
        break;

      }

      case ADD_INDIRECT_HL: {

        int     x;

        READ_INDIRECT_HL(x);
        ADD(x);
        break;

      }

      case ADC_R: {

        ADC(R(Z(opcode)));
        break;

      }

      case ADC_N: {

        int     n;

        READ_N(n);
        ADC(n);
        break;

      }

      case ADC_INDIRECT_HL: {

        int     x;

        READ_INDIRECT_HL(x);
        ADC(x);
        break;

      }

      case SUB_R: {

        SUB(R(Z(opcode)));
        break;

      }

      case SUB_N: {

        int     n;

        READ_N(n);
        SUB(n);
        break;

      }

      case SUB_INDIRECT_HL: {

        int     x;

        READ_INDIRECT_HL(x);
        SUB(x);
        break;

      }

      case SBC_R: {

        SBC(R(Z(opcode)));
        break;

      }

      case SBC_N: {

        int     n;

        READ_N(n);
        SBC(n);
        break;

      }

      case SBC_INDIRECT_HL: {

        int     x;

        READ_INDIRECT_HL(x);
        SBC(x);
        break;

      }

      case AND_R: {

        AND(R(Z(opcode)));
        break;

      }

      case AND_N: {

        int     n;

        READ_N(n);
        AND(n);
        break;

      }

      case AND_INDIRECT_HL: {

        int     x;

        READ_INDIRECT_HL(x);
        AND(x);
        break;

      }

      case OR_R: {

        OR(R(Z(opcode)));
        break;

      }

      case OR_N: {

        int     n;

        READ_N(n);
        OR(n);
        break;

      }

      case OR_INDIRECT_HL: {

        int     x;

        READ_INDIRECT_HL(x);
        OR(x);
        break;

      }

      case XOR_R: {

        XOR(R(Z(opcode)));
        break;

      }

      case XOR_N: {

        int     n;

        READ_N(n);
        XOR(n);
        break;

      }

      case XOR_INDIRECT_HL: {

        int     x;

        READ_INDIRECT_HL(x);
        XOR(x);
        break;

      }

      case CP_R: {

        CP(R(Z(opcode)));
        break;

      }

      case CP_N: {

        int     n;

        READ_N(n);
        CP(n);
        break;

      }

      case CP_INDIRECT_HL: {

        int     x;

        READ_INDIRECT_HL(x);
        CP(x);
        break;

      }

      case INC_R: {

        INC(R(Y(opcode)));
        break;

      }

      case INC_INDIRECT_HL: {

        int     x;

        if (registers == state.register_table) {

          READ_BYTE(HL, x);
          INC(x);
          WRITE_BYTE(HL, x);

          elapsed_cycles++;

        } else {

          int     d;

          READ_D(d);
          d += HL_IX_IY;
          READ_BYTE(d, x);
          INC(x);
          WRITE_BYTE(d, x);

          elapsed_cycles += 6;

        }
        break;

      }

      case DEC_R: {

        DEC(R(Y(opcode)));
        break;

      }

      case DEC_INDIRECT_HL: {

        int     x;

        if (registers == state.register_table) {

          READ_BYTE(HL, x);
          DEC(x);
          WRITE_BYTE(HL, x);

          elapsed_cycles++;

        } else {

          int     d;

          READ_D(d);
          d += HL_IX_IY;
          READ_BYTE(d, x);
          DEC(x);
          WRITE_BYTE(d, x);

          elapsed_cycles += 6;

        }
        break;

      }

        /* General-purpose arithmetic and CPU control group. */

      case DAA: {

        int     a, c, d;

        /* The following algorithm is from
         * comp.sys.sinclair's FAQ.
         */

        a = A;
        if (a > 0x99 || (F & Z80_C_FLAG)) {

          c = Z80_C_FLAG;
          d = 0x60;

        } else

          c = d = 0;

        if ((a & 0x0f) > 0x09 || (F & Z80_H_FLAG))

          d += 0x06;

        A += F & Z80_N_FLAG ? -d : +d;
        F = SZYXP_FLAGS_TABLE[A]
        | ((A ^ a) & Z80_H_FLAG)
        | (F & Z80_N_FLAG)
        | c;

        break;

      }

      case CPL: {

        A = ~A;
        F = (F & (SZPV_FLAGS | Z80_C_FLAG))

  #ifndef Z80_DOCUMENTED_FLAGS_ONLY

        | (A & YX_FLAGS)

  #endif

        | Z80_H_FLAG | Z80_N_FLAG;

        break;

      }

      case NEG: {

        int     a, f, z, c;

        a = A;
        z = -a;

        c = a ^ z;
        f = Z80_N_FLAG | (c & Z80_H_FLAG);
        f |= SZYX_FLAGS_TABLE[z &= 0xff];
        c &= 0x0180;
        f |= OVERFLOW_TABLE[c >> 7];
        f |= c >> (8 - Z80_C_FLAG_SHIFT);

        A = z;
        F = f;

        break;

      }

      case CCF: {

        int     c;

        c = F & Z80_C_FLAG;
        F = (F & SZPV_FLAGS)
        | (c << Z80_H_FLAG_SHIFT)

  #ifndef Z80_DOCUMENTED_FLAGS_ONLY

        | (A & YX_FLAGS)

  #endif

        | (c ^ Z80_C_FLAG);

        break;

      }

      case SCF: {

        F = (F & SZPV_FLAGS)

  #ifndef Z80_DOCUMENTED_FLAGS_ONLY

        | (A & YX_FLAGS)

  #endif

        | Z80_C_FLAG;

        break;

      }

      case NOP: {

        break;

      }

      case HALT: {

  #ifdef Z80_CATCH_HALT

        state.status = Z80_STATUS_HALT;

  #endif

        break;

      }

      case DI: {

        state.iff1 = state.iff2 = 0;

  #ifdef Z80_CATCH_DI

        state.status = Z80_STATUS_DI;

  #endif

        break;

      }

      case EI: {

        state.iff1 = state.iff2 = 1;

  #ifdef Z80_CATCH_EI

        state.status = Z80_STATUS_EI;

  #endif

        break;

      }

      case IM_N: {

        /* "IM 0/1" (0xed prefixed opcodes 0x4e and
         * 0x6e) is treated like a "IM 0".
         */

        if ((Y(opcode) & 0x03) <= 0x01)

          state.im = Z80_INTERRUPT_MODE_0;

        else if (!(Y(opcode) & 1))

          state.im = Z80_INTERRUPT_MODE_1;

        else

          state.im = Z80_INTERRUPT_MODE_2;

        break;

      }

        /* 16-bit arithmetic group. */

      case ADD_HL_RR: {

        int     x, y, z, f, c;

        x = HL_IX_IY;
        y = RR(P(opcode));
        z = x + y;

        c = x ^ y ^ z;
        f = F & SZPV_FLAGS;

  #ifndef Z80_DOCUMENTED_FLAGS_ONLY

        f |= (z >> 8) & YX_FLAGS;
        f |= (c >> 8) & Z80_H_FLAG;

  #endif

        f |= c >> (16 - Z80_C_FLAG_SHIFT);

        HL_IX_IY = z;
        F = f;

        elapsed_cycles += 7;

        break;

      }

      case ADC_HL_RR: {

        int     x, y, z, f, c;

        x = HL;
        y = RR(P(opcode));
        z = x + y + (F & Z80_C_FLAG);

        c = x ^ y ^ z;
        f = z & 0xffff
        ? (z >> 8) & SYX_FLAGS
        : Z80_Z_FLAG;

  #ifndef Z80_DOCUMENTED_FLAGS_ONLY

        f |= (c >> 8) & Z80_H_FLAG;

  #endif

        f |= OVERFLOW_TABLE[c >> 15];
        f |= z >> (16 - Z80_C_FLAG_SHIFT);

        HL = z;
        F = f;

        elapsed_cycles += 7;

        break;

      }

      case SBC_HL_RR: {

        int     x, y, z, f, c;

        x = HL;
        y = RR(P(opcode));
        z = x - y - (F & Z80_C_FLAG);

        c = x ^ y ^ z;
        f = Z80_N_FLAG;
        f |= z & 0xffff
        ? (z >> 8) & SYX_FLAGS
        : Z80_Z_FLAG;

  #ifndef Z80_DOCUMENTED_FLAGS_ONLY

        f |= (c >> 8) & Z80_H_FLAG;

  #endif

        c &= 0x018000;
        f |= OVERFLOW_TABLE[c >> 15];
        f |= c >> (16 - Z80_C_FLAG_SHIFT);

        HL = z;
        F = f;

        elapsed_cycles += 7;

        break;

      }

      case INC_RR: {

        int     x;

        x = RR(P(opcode));
        x++;
        RR(P(opcode)) = x;

        elapsed_cycles += 2;

        break;

      }

      case DEC_RR: {

        int     x;

        x = RR(P(opcode));
        x--;
        RR(P(opcode)) = x;

        elapsed_cycles += 2;

        break;

      }

        /* Rotate and shift group. */

      case RLCA: {

        A = (A << 1) | (A >> 7);
        F = (F & SZPV_FLAGS)
        | (A & (YX_FLAGS | Z80_C_FLAG));
        break;

      }

      case RLA: {

        int     a, f;

        a = A << 1;
        f = (F & SZPV_FLAGS)

  #ifndef Z80_DOCUMENTED_FLAGS_ONLY

        | (a & YX_FLAGS)

  #endif

        | (A >> 7);
        A = a | (F & Z80_C_FLAG);
        F = f;

        break;

      }

      case RRCA: {

        int     c;

        c = A & 0x01;
        A = (A >> 1) | (A << 7);
        F = (F & SZPV_FLAGS)

  #ifndef Z80_DOCUMENTED_FLAGS_ONLY

        | (A & YX_FLAGS)

  #endif

        | c;

        break;

      }

      case RRA: {

        int     c;

        c = A & 0x01;
        A = (A >> 1) | ((F & Z80_C_FLAG) << 7);
        F = (F & SZPV_FLAGS)

  #ifndef Z80_DOCUMENTED_FLAGS_ONLY

        | (A & YX_FLAGS)

  #endif

        | c;

        break;

      }

      case RLC_R: {

        RLC(R(Z(opcode)));
        break;

      }

      case RLC_INDIRECT_HL: {

        int     x;

        if (registers == state.register_table) {

          READ_BYTE(HL, x);
          RLC(x);
          WRITE_BYTE(HL, x);

          elapsed_cycles++;

        } else {

          int     d;

          Z80_FETCH_BYTE(pc, d);
          d = ((signed char) d) + HL_IX_IY;

          READ_BYTE(d, x);
          RLC(x);
          WRITE_BYTE(d, x);

          if (Z(opcode) != INDIRECT_HL)

            R(Z(opcode)) = x;

          pc += 2;

          elapsed_cycles += 5;

        }

        break;

      }

      case RL_R: {

        RL(R(Z(opcode)));
        break;

      }

      case RL_INDIRECT_HL: {

        int     x;

        if (registers == state.register_table) {

          READ_BYTE(HL, x);
          RL(x);
          WRITE_BYTE(HL, x);

          elapsed_cycles++;

        } else {

          int     d;

          Z80_FETCH_BYTE(pc, d);
          d = ((signed char) d) + HL_IX_IY;

          READ_BYTE(d, x);
          RL(x);
          WRITE_BYTE(d, x);

          if (Z(opcode) != INDIRECT_HL)

            R(Z(opcode)) = x;

          pc += 2;

          elapsed_cycles += 5;

        }
        break;

      }

      case RRC_R: {

        RRC(R(Z(opcode)));
        break;

      }

      case RRC_INDIRECT_HL: {

        int     x;

        if (registers == state.register_table) {

          READ_BYTE(HL, x);
          RRC(x);
          WRITE_BYTE(HL, x);

          elapsed_cycles++;

        } else {

          int     d;

          Z80_FETCH_BYTE(pc, d);
          d = ((signed char) d) + HL_IX_IY;

          READ_BYTE(d, x);
          RRC(x);
          WRITE_BYTE(d, x);

          if (Z(opcode) != INDIRECT_HL)

            R(Z(opcode)) = x;

          pc += 2;

          elapsed_cycles += 5;

        }
        break;

      }

      case RR_R: {

        RR_INSTRUCTION(R(Z(opcode)));
        break;

      }

      case RR_INDIRECT_HL: {

        int     x;

        if (registers == state.register_table) {

          READ_BYTE(HL, x);
          RR_INSTRUCTION(x);
          WRITE_BYTE(HL, x);

          elapsed_cycles++;

        } else {

          int     d;

          Z80_FETCH_BYTE(pc, d);
          d = ((signed char) d) + HL_IX_IY;

          READ_BYTE(d, x);
          RR_INSTRUCTION(x);
          WRITE_BYTE(d, x);

          if (Z(opcode) != INDIRECT_HL)

            R(Z(opcode)) = x;

          pc += 2;

          elapsed_cycles += 5;

        }
        break;

      }

      case SLA_R: {

        SLA(R(Z(opcode)));
        break;

      }

      case SLA_INDIRECT_HL: {

        int     x;

        if (registers == state.register_table) {

          READ_BYTE(HL, x);
          SLA(x);
          WRITE_BYTE(HL, x);

          elapsed_cycles++;

        } else {

          int     d;

          Z80_FETCH_BYTE(pc, d);
          d = ((signed char) d) + HL_IX_IY;

          READ_BYTE(d, x);
          SLA(x);
          WRITE_BYTE(d, x);

          if (Z(opcode) != INDIRECT_HL)

            R(Z(opcode)) = x;

          pc += 2;

          elapsed_cycles += 5;

        }
        break;

      }

      case SLL_R: {

        SLL(R(Z(opcode)));
        break;

      }

      case SLL_INDIRECT_HL: {

        int     x;

        if (registers == state.register_table) {

          READ_BYTE(HL, x);
          SLL(x);
          WRITE_BYTE(HL, x);

          elapsed_cycles++;

        } else {

          int     d;

          Z80_FETCH_BYTE(pc, d);
          d = ((signed char) d) + HL_IX_IY;

          READ_BYTE(d, x);
          SLL(x);
          WRITE_BYTE(d, x);

          if (Z(opcode) != INDIRECT_HL)

            R(Z(opcode)) = x;

          pc += 2;

          elapsed_cycles += 5;

        }
        break;

      }

      case SRA_R: {

        SRA(R(Z(opcode)));
        break;

      }

      case SRA_INDIRECT_HL: {

        int     x;

        if (registers == state.register_table) {

          READ_BYTE(HL, x);
          SRA(x);
          WRITE_BYTE(HL, x);

          elapsed_cycles++;

        } else {

          int     d;

          Z80_FETCH_BYTE(pc, d);
          d = ((signed char) d) + HL_IX_IY;

          READ_BYTE(d, x);
          SRA(x);
          WRITE_BYTE(d, x);

          if (Z(opcode) != INDIRECT_HL)

            R(Z(opcode)) = x;

          pc += 2;

          elapsed_cycles += 5;

        }
        break;

      }

      case SRL_R: {

        SRL(R(Z(opcode)));
        break;

      }

      case SRL_INDIRECT_HL: {

        int     x;

        if (registers == state.register_table) {

          READ_BYTE(HL, x);
          SRL(x);
          WRITE_BYTE(HL, x);

          elapsed_cycles++;

        } else {

          int     d;

          Z80_FETCH_BYTE(pc, d);
          d = ((signed char) d) + HL_IX_IY;

          READ_BYTE(d, x);
          SRL(x);
          WRITE_BYTE(d, x);

          if (Z(opcode) != INDIRECT_HL)

            R(Z(opcode)) = x;

          pc += 2;

          elapsed_cycles += 5;

        }
        break;

      }

      case RLD_RRD: {

        int     x, y;

        READ_BYTE(HL, x);
        y = (A & 0xf0) << 8;
        y |= opcode == OPCODE_RLD
        ? (x << 4) | (A & 0x0f)
        : ((x & 0x0f) << 8)
        | ((A & 0x0f) << 4)
        | (x >> 4);
        WRITE_BYTE(HL, y);
        y >>= 8;

        A = y;
        F = SZYXP_FLAGS_TABLE[y] | (F & Z80_C_FLAG);

        elapsed_cycles += 4;

        break;

      }

        /* Bit set, reset, and test group. */

      case BIT_B_R: {

        int     x;

        x = R(Z(opcode)) & (1 << Y(opcode));
        F = (x ? 0 : Z80_Z_FLAG | Z80_P_FLAG)

  #ifndef Z80_DOCUMENTED_FLAGS_ONLY

        | (x & Z80_S_FLAG)
        | (R(Z(opcode)) & YX_FLAGS)

  #endif

        | Z80_H_FLAG
        | (F & Z80_C_FLAG);

        break;

      }

      case BIT_B_INDIRECT_HL: {

        int     d, x;

        if (registers == state.register_table) {

          d = HL;

          elapsed_cycles++;

        } else {

          Z80_FETCH_BYTE(pc, d);
          d = ((signed char) d) + HL_IX_IY;

          pc += 2;

          elapsed_cycles += 5;

        }

        READ_BYTE(d, x);
        x &= 1 << Y(opcode);
        F = (x ? 0 : Z80_Z_FLAG | Z80_P_FLAG)

  #ifndef Z80_DOCUMENTED_FLAGS_ONLY

        | (x & Z80_S_FLAG)
        | (d & YX_FLAGS)

  #endif

        | Z80_H_FLAG
        | (F & Z80_C_FLAG);

        break;

      }

      case SET_B_R: {

        R(Z(opcode)) |= 1 << Y(opcode);
        break;

      }

      case SET_B_INDIRECT_HL: {

        int     x;

        if (registers == state.register_table) {

          READ_BYTE(HL, x);
          x |= 1 << Y(opcode);
          WRITE_BYTE(HL, x);

          elapsed_cycles++;

        } else {

          int     d;

          Z80_FETCH_BYTE(pc, d);
          d = ((signed char) d) + HL_IX_IY;

          READ_BYTE(d, x);
          x |= 1 << Y(opcode);
          WRITE_BYTE(d, x);

          if (Z(opcode) != INDIRECT_HL)

            R(Z(opcode)) = x;

          pc += 2;

          elapsed_cycles += 5;

        }
        break;

      }

      case RES_B_R: {

        R(Z(opcode)) &= ~(1 << Y(opcode));
        break;

      }

      case RES_B_INDIRECT_HL: {

        int     x;

        if (registers == state.register_table) {

          READ_BYTE(HL, x);
          x &= ~(1 << Y(opcode));
          WRITE_BYTE(HL, x);

          elapsed_cycles++;

        } else {

          int     d;

          Z80_FETCH_BYTE(pc, d);
          d = ((signed char) d) + HL_IX_IY;

          READ_BYTE(d, x);
          x &= ~(1 << Y(opcode));
          WRITE_BYTE(d, x);

          if (Z(opcode) != INDIRECT_HL)

            R(Z(opcode)) = x;

          pc += 2;

          elapsed_cycles += 5;

        }
        break;

      }

        /* Jump group. */

      case JP_NN: {

        int     nn;

        Z80_FETCH_WORD(pc, nn);
        pc = nn;

        elapsed_cycles += 6;

        break;

      }

      case JP_CC_NN: {

        int     nn;

        if (CC(Y(opcode))) {

          Z80_FETCH_WORD(pc, nn);
          pc = nn;

        } else {

  #ifdef Z80_FALSE_CONDITION_FETCH

          Z80_FETCH_WORD(pc, nn);

  #endif

          pc += 2;

        }

        elapsed_cycles += 6;

        break;

      }

      case JR_E: {

        int     e;

        Z80_FETCH_BYTE(pc, e);
        pc += ((signed char) e) + 1;

        elapsed_cycles += 8;

        break;

      }

      case JR_DD_E: {

        int     e;

        if (DD(Q(opcode))) {

          Z80_FETCH_BYTE(pc, e);
          pc += ((signed char) e) + 1;

          elapsed_cycles += 8;

        } else {

  #ifdef Z80_FALSE_CONDITION_FETCH

          Z80_FETCH_BYTE(pc, e);

  #endif

          pc++;

          elapsed_cycles += 3;

        }
        break;

      }

      case JP_HL: {

        pc = HL_IX_IY;
        break;

      }

      case DJNZ_E: {

        int     e;

        if (--B) {

          Z80_FETCH_BYTE(pc, e);
          pc += ((signed char) e) + 1;

          elapsed_cycles += 9;

        } else {

  #ifdef Z80_FALSE_CONDITION_FETCH

          Z80_FETCH_BYTE(pc, e);

  #endif

          pc++;

          elapsed_cycles += 4;

        }
        break;

      }

        /* Call and return group. */

      case CALL_NN: {

        int     nn;

        READ_NN(nn);
        PUSH(pc);
        pc = nn;

        elapsed_cycles++;

        break;

      }

      case CALL_CC_NN: {

        int     nn;

        if (CC(Y(opcode))) {

          READ_NN(nn);
          PUSH(pc);
          pc = nn;

          elapsed_cycles++;

        } else {

  #ifdef Z80_FALSE_CONDITION_FETCH

          Z80_FETCH_WORD(pc, nn);

  #endif

          pc += 2;

          elapsed_cycles += 6;

        }
        break;

      }

      case RET: {

        POP(pc);
        break;

      }

      case RET_CC: {

        if (CC(Y(opcode))) {

          POP(pc);

        }
        elapsed_cycles++;
        break;

      }

      case RETI_RETN: {

        state.iff1 = state.iff2;
        POP(pc);

  #if defined(Z80_CATCH_RETI) && defined(Z80_CATCH_RETN)

        state.status = opcode == OPCODE_RETI
        ? Z80_STATUS_RETI
        : Z80_STATUS_RETN;

  #elif defined(Z80_CATCH_RETI)

        state.status = Z80_STATUS_RETI;

  #elif defined(Z80_CATCH_RETN)

        state.status = Z80_STATUS_RETN;

  #endif

        break;

      }

      case RST_P: {

        PUSH(pc);
        pc = RST_TABLE[Y(opcode)];
        elapsed_cycles++;
        break;

      }

        /* Input and output group. */

      case IN_A_N: {

        int     n;

        READ_N(n);
        Z80_INPUT_BYTE(n, A);

        elapsed_cycles += 4;

        break;

      }

      case IN_R_C: {

        int     x;
        Z80_INPUT_BYTE(C, x);
        if (Y(opcode) != INDIRECT_HL)

          R(Y(opcode)) = x;

        F = SZYXP_FLAGS_TABLE[x] | (F & Z80_C_FLAG);

        elapsed_cycles += 4;

        break;

      }

        /* Some of the undocumented flags for "INI", "IND",
         * "INIR", "INDR",  "OUTI", "OUTD", "OTIR", and
         * "OTDR" are really really strange. The emulator
         * implements the specifications described in "The
         * Undocumented Z80 Documented Version 0.91".
         */

      case INI_IND: {

        int     x, f;

        Z80_INPUT_BYTE(C, x);
        WRITE_BYTE(HL, x);

        f = SZYX_FLAGS_TABLE[--B & 0xff]
        | (x >> (7 - Z80_N_FLAG_SHIFT));
        if (opcode == OPCODE_INI) {

          HL++;
          x += (C + 1) & 0xff;

        } else {

          HL--;
          x += (C - 1) & 0xff;

        }
        f |= x & 0x0100 ? HC_FLAGS : 0;
        f |= SZYXP_FLAGS_TABLE[(x & 0x07) ^ B]
        & Z80_P_FLAG;
        F = f;

        elapsed_cycles += 5;

        break;

      }

      case INIR_INDR: {

        int     d, b, hl, x, f;

  #ifdef Z80_HANDLE_SELF_MODIFYING_CODE

        int     p, q;

        p = (pc - 2) & 0xffff;
        q = (pc - 1) & 0xffff;

  #endif

        d = opcode == OPCODE_INIR ? +1 : -1;

        b = B;
        hl = HL;

        r -= 2;
        elapsed_cycles -= 8;
        for ( ; ; ) {

          r += 2;

          Z80_INPUT_BYTE(C, x);
          Z80_WRITE_BYTE(hl, x);

          hl += d;

          if (--b)

            elapsed_cycles += 21;

          else {

            f = Z80_Z_FLAG;
            elapsed_cycles += 16;
            break;

          }

  #ifdef Z80_HANDLE_SELF_MODIFYING_CODE

          if (((hl - d) & 0xffff) == p
              || ((hl - d) & 0xffff) == q) {

            f = SZYX_FLAGS_TABLE[b];
            pc -= 2;
            break;

          }

  #endif

          f = SZYX_FLAGS_TABLE[b];
          pc -= 2;
          break;

        }

        HL = hl;
        B = b;

        f |= x >> (7 - Z80_N_FLAG_SHIFT);
        x += (C + d) & 0xff;
        f |= x & 0x0100 ? HC_FLAGS : 0;
        f |= SZYXP_FLAGS_TABLE[(x & 0x07) ^ b]
        & Z80_P_FLAG;
        F = f;

        break;

      }

      case OUT_N_A: {

        int     n;

        READ_N(n);
        Z80_OUTPUT_BYTE(n, A);

        elapsed_cycles += 4;

        break;

      }

      case OUT_C_R: {

        int     x;

        x = Y(opcode) != INDIRECT_HL
        ? R(Y(opcode))
        : 0;
        Z80_OUTPUT_BYTE(C, x);

        elapsed_cycles += 4;

        break;

      }

      case OUTI_OUTD: {

        int     x, f;

        READ_BYTE(HL, x);
        Z80_OUTPUT_BYTE(C, x);

        HL += opcode == OPCODE_OUTI ? +1 : -1;

        f = SZYX_FLAGS_TABLE[--B & 0xff]
        | (x >> (7 - Z80_N_FLAG_SHIFT));
        x += HL & 0xff;
        f |= x & 0x0100 ? HC_FLAGS : 0;
        f |= SZYXP_FLAGS_TABLE[(x & 0x07) ^ B]
        & Z80_P_FLAG;
        F = f;

        break;

      }

      case OTIR_OTDR: {

        int     d, b, hl, x, f;

        d = opcode == OPCODE_OTIR ? +1 : -1;

        b = B;
        hl = HL;

        r -= 2;
        elapsed_cycles -= 8;
        for ( ; ; ) {

          r += 2;

          Z80_READ_BYTE(hl, x);
          Z80_OUTPUT_BYTE(C, x);

          hl += d;
          if (--b)

            elapsed_cycles += 21;

          else {

            f = Z80_Z_FLAG;
            elapsed_cycles += 16;
            break;

          }

          f = SZYX_FLAGS_TABLE[b];
          pc -= 2;
          break;

        }

        HL = hl;
        B = b;

        f |= x >> (7 - Z80_N_FLAG_SHIFT);
        x += hl & 0xff;
        f |= x & 0x0100 ? HC_FLAGS : 0;
        f |= SZYXP_FLAGS_TABLE[(x & 0x07) ^ b]
        & Z80_P_FLAG;
        F = f;

        break;

      }

        /* Prefix group. */

      case CB_PREFIX: {

        /* Special handling if the 0xcb prefix is
         * prefixed by a 0xdd or 0xfd prefix.
         */

        if (registers != state.register_table) {

          r--;

          /* Indexed memory access routine will
           * correctly update pc.
           */

          Z80_FETCH_BYTE(pc + 1, opcode);

        } else {

          Z80_FETCH_BYTE(pc, opcode);
          pc++;

        }
        instruction = CB_INSTRUCTION_TABLE[opcode];

        repeatLoop = true;
        break;

      }

      case DD_PREFIX: {

        registers = state.dd_register_table;

        Z80_FETCH_BYTE(pc, opcode);
        pc++;
        instruction = INSTRUCTION_TABLE[opcode];
        repeatLoop = true;
        break;

      }

      case FD_PREFIX: {

        registers = state.fd_register_table;

        Z80_FETCH_BYTE(pc, opcode);
        pc++;
        instruction = INSTRUCTION_TABLE[opcode];
        repeatLoop = true;
        break;

      }

      case ED_PREFIX: {

        registers = state.register_table;
        Z80_FETCH_BYTE(pc, opcode);
        pc++;
        instruction = ED_INSTRUCTION_TABLE[opcode];
        repeatLoop = true;
        break;

      }

        /* Special/pseudo instruction group. */

      case ED_UNDEFINED: {

  #ifdef Z80_CATCH_ED_UNDEFINED

        state.status = Z80_STATUS_FLAG_ED_UNDEFINED;
        pc -= 2;

  #endif

        break;

      }

    }

  } while (repeatLoop);

  state.r = (state.r & 0x80) | (r & 0x7f);
  state.pc = pc & 0xffff;

  return elapsed_cycles;
}


};  // fabgl namespace
