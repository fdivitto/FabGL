
#include "esp_attr.h"
#include "../../machine.h"

#include "mos6502.h"




#define READ(addr)         ((Machine*)Context)->busRead((addr))
#define WRITE(addr, value) ((Machine*)Context)->busWrite((addr), (value))


mos6502::mos6502(void * context)
{
  Context = context;
	Instr instr;
	
	// fill jump table with ILLEGALs
	instr.addr = &mos6502::Addr_IMP;
	instr.code = &mos6502::Op_ILLEGAL;
	for(int i = 0; i < 256; i++)
	{
		InstrTable[i] = instr;
	}
	
	// insert opcodes

  // ADC

	instr.addr = &mos6502::Addr_IMM;
	instr.code = &mos6502::Op_ADC;
  instr.cycl = 2;
	InstrTable[0x69] = instr;

  instr.addr = &mos6502::Addr_ZER;
  instr.code = &mos6502::Op_ADC;
  instr.cycl = 3;
  InstrTable[0x65] = instr;

  instr.addr = &mos6502::Addr_ZEX;
  instr.code = &mos6502::Op_ADC;
  instr.cycl = 4;
  InstrTable[0x75] = instr;

	instr.addr = &mos6502::Addr_ABS;
	instr.code = &mos6502::Op_ADC;
  instr.cycl = 4;
	InstrTable[0x6D] = instr;

  instr.addr = &mos6502::Addr_ABX;
  instr.code = &mos6502::Op_ADC;
  instr.cycl = 4;
  InstrTable[0x7D] = instr;

  instr.addr = &mos6502::Addr_ABY;
  instr.code = &mos6502::Op_ADC;
  instr.cycl = 4;
  InstrTable[0x79] = instr;

	instr.addr = &mos6502::Addr_INX;
	instr.code = &mos6502::Op_ADC;
  instr.cycl = 6;
	InstrTable[0x61] = instr;

	instr.addr = &mos6502::Addr_INY;
	instr.code = &mos6502::Op_ADC;
  instr.cycl = 5;
	InstrTable[0x71] = instr;

  // AND

	instr.addr = &mos6502::Addr_IMM;
	instr.code = &mos6502::Op_AND;
  instr.cycl = 2;
	InstrTable[0x29] = instr;

  instr.addr = &mos6502::Addr_ZER;
  instr.code = &mos6502::Op_AND;
  instr.cycl = 3;
  InstrTable[0x25] = instr;

  instr.addr = &mos6502::Addr_ZEX;
  instr.code = &mos6502::Op_AND;
  instr.cycl = 4;
  InstrTable[0x35] = instr;

	instr.addr = &mos6502::Addr_ABS;
	instr.code = &mos6502::Op_AND;
  instr.cycl = 4;
	InstrTable[0x2D] = instr;

  instr.addr = &mos6502::Addr_ABX;
  instr.code = &mos6502::Op_AND;
  instr.cycl = 4;
  InstrTable[0x3D] = instr;

  instr.addr = &mos6502::Addr_ABY;
  instr.code = &mos6502::Op_AND;
  instr.cycl = 4;
  InstrTable[0x39] = instr;

	instr.addr = &mos6502::Addr_INX;
	instr.code = &mos6502::Op_AND;
  instr.cycl = 6;
	InstrTable[0x21] = instr;

	instr.addr = &mos6502::Addr_INY;
	instr.code = &mos6502::Op_AND;
  instr.cycl = 5;
	InstrTable[0x31] = instr;

  // ASL

  instr.addr = &mos6502::Addr_ACC;
  instr.code = &mos6502::Op_ASL_ACC;
  instr.cycl = 2;
  InstrTable[0x0A] = instr;

	instr.addr = &mos6502::Addr_ZER;
	instr.code = &mos6502::Op_ASL;
  instr.cycl = 5;
	InstrTable[0x06] = instr;

	instr.addr = &mos6502::Addr_ZEX;
	instr.code = &mos6502::Op_ASL;
  instr.cycl = 6;
	InstrTable[0x16] = instr;

  instr.addr = &mos6502::Addr_ABS;
  instr.code = &mos6502::Op_ASL;
  instr.cycl = 6;
  InstrTable[0x0E] = instr;

	instr.addr = &mos6502::Addr_ABX;
	instr.code = &mos6502::Op_ASL;
  instr.cycl = 7;
	InstrTable[0x1E] = instr;

  // BCC

	instr.addr = &mos6502::Addr_REL;
	instr.code = &mos6502::Op_BCC;
  instr.cycl = 2;
	InstrTable[0x90] = instr;

  // BCS
	
	instr.addr = &mos6502::Addr_REL;
	instr.code = &mos6502::Op_BCS;
  instr.cycl = 2;
	InstrTable[0xB0] = instr;

  // BEQ

	instr.addr = &mos6502::Addr_REL;
	instr.code = &mos6502::Op_BEQ;
  instr.cycl = 2;
	InstrTable[0xF0] = instr;

  // BIT
	
  instr.addr = &mos6502::Addr_ZER;
  instr.code = &mos6502::Op_BIT;
  instr.cycl = 3;
  InstrTable[0x24] = instr;

	instr.addr = &mos6502::Addr_ABS;
	instr.code = &mos6502::Op_BIT;
  instr.cycl = 4;
	InstrTable[0x2C] = instr;

  // BMI
	
	instr.addr = &mos6502::Addr_REL;
	instr.code = &mos6502::Op_BMI;
  instr.cycl = 2;
	InstrTable[0x30] = instr;

  // BNE

	instr.addr = &mos6502::Addr_REL;
	instr.code = &mos6502::Op_BNE;
  instr.cycl = 2;
	InstrTable[0xD0] = instr;

  // BPL

	instr.addr = &mos6502::Addr_REL;
	instr.code = &mos6502::Op_BPL;
  instr.cycl = 2;
	InstrTable[0x10] = instr;

  // BRK

	instr.addr = &mos6502::Addr_IMP;
	instr.code = &mos6502::Op_BRK;
  instr.cycl = 7;
	InstrTable[0x00] = instr;

  // BVC

	instr.addr = &mos6502::Addr_REL;
	instr.code = &mos6502::Op_BVC;
  instr.cycl = 2;
	InstrTable[0x50] = instr;

  // BVS

	instr.addr = &mos6502::Addr_REL;
	instr.code = &mos6502::Op_BVS;
  instr.cycl = 2;
	InstrTable[0x70] = instr;

  // CLC

	instr.addr = &mos6502::Addr_IMP;
	instr.code = &mos6502::Op_CLC;
  instr.cycl = 2;
	InstrTable[0x18] = instr;

  // CLD

	instr.addr = &mos6502::Addr_IMP;
	instr.code = &mos6502::Op_CLD;
  instr.cycl = 2;
	InstrTable[0xD8] = instr;

  // CLI

	instr.addr = &mos6502::Addr_IMP;
	instr.code = &mos6502::Op_CLI;
  instr.cycl = 2;
	InstrTable[0x58] = instr;

  // CLV

	instr.addr = &mos6502::Addr_IMP;
	instr.code = &mos6502::Op_CLV;
  instr.cycl = 2;
	InstrTable[0xB8] = instr;

  // CMP

	instr.addr = &mos6502::Addr_IMM;
	instr.code = &mos6502::Op_CMP;
  instr.cycl = 2;
	InstrTable[0xC9] = instr;

  instr.addr = &mos6502::Addr_ZER;
  instr.code = &mos6502::Op_CMP;
  instr.cycl = 3;
  InstrTable[0xC5] = instr;

  instr.addr = &mos6502::Addr_ZEX;
  instr.code = &mos6502::Op_CMP;
  instr.cycl = 4;
  InstrTable[0xD5] = instr;

	instr.addr = &mos6502::Addr_ABS;
	instr.code = &mos6502::Op_CMP;
  instr.cycl = 4;
	InstrTable[0xCD] = instr;

  instr.addr = &mos6502::Addr_ABX;
  instr.code = &mos6502::Op_CMP;
  instr.cycl = 4;
  InstrTable[0xDD] = instr;

  instr.addr = &mos6502::Addr_ABY;
  instr.code = &mos6502::Op_CMP;
  instr.cycl = 4;
  InstrTable[0xD9] = instr;

	instr.addr = &mos6502::Addr_INX;
	instr.code = &mos6502::Op_CMP;
  instr.cycl = 6;
	InstrTable[0xC1] = instr;

	instr.addr = &mos6502::Addr_INY;
	instr.code = &mos6502::Op_CMP;
  instr.cycl = 5;
	InstrTable[0xD1] = instr;

  // CPX

	instr.addr = &mos6502::Addr_IMM;
	instr.code = &mos6502::Op_CPX;
  instr.cycl = 2;
	InstrTable[0xE0] = instr;

  instr.addr = &mos6502::Addr_ZER;
  instr.code = &mos6502::Op_CPX;
  instr.cycl = 3;
  InstrTable[0xE4] = instr;

	instr.addr = &mos6502::Addr_ABS;
	instr.code = &mos6502::Op_CPX;
  instr.cycl = 4;
	InstrTable[0xEC] = instr;

  // CPY

	instr.addr = &mos6502::Addr_IMM;
	instr.code = &mos6502::Op_CPY;
  instr.cycl = 2;
	InstrTable[0xC0] = instr;

  instr.addr = &mos6502::Addr_ZER;
  instr.code = &mos6502::Op_CPY;
  instr.cycl = 3;
  InstrTable[0xC4] = instr;

	instr.addr = &mos6502::Addr_ABS;
	instr.code = &mos6502::Op_CPY;
  instr.cycl = 4;
	InstrTable[0xCC] = instr;

  // DEC

	instr.addr = &mos6502::Addr_ZER;
	instr.code = &mos6502::Op_DEC;
  instr.cycl = 5;
	InstrTable[0xC6] = instr;

	instr.addr = &mos6502::Addr_ZEX;
	instr.code = &mos6502::Op_DEC;
  instr.cycl = 6;
	InstrTable[0xD6] = instr;

  instr.addr = &mos6502::Addr_ABS;
  instr.code = &mos6502::Op_DEC;
  instr.cycl = 6;
  InstrTable[0xCE] = instr;

	instr.addr = &mos6502::Addr_ABX_ex;
	instr.code = &mos6502::Op_DEC;
  instr.cycl = 7;
	InstrTable[0xDE] = instr;

  // DEX
	
	instr.addr = &mos6502::Addr_IMP;
	instr.code = &mos6502::Op_DEX;
  instr.cycl = 2;
	InstrTable[0xCA] = instr;

  // DEY

	instr.addr = &mos6502::Addr_IMP;
	instr.code = &mos6502::Op_DEY;
  instr.cycl = 2;
	InstrTable[0x88] = instr;

  // EOR

	instr.addr = &mos6502::Addr_IMM;
	instr.code = &mos6502::Op_EOR;
  instr.cycl = 2;
	InstrTable[0x49] = instr;

	instr.addr = &mos6502::Addr_ZER;
	instr.code = &mos6502::Op_EOR;
  instr.cycl = 3;
	InstrTable[0x45] = instr;

  instr.addr = &mos6502::Addr_ZEX;
  instr.code = &mos6502::Op_EOR;
  instr.cycl = 4;
  InstrTable[0x55] = instr;

  instr.addr = &mos6502::Addr_ABS;
  instr.code = &mos6502::Op_EOR;
  instr.cycl = 4;
  InstrTable[0x4D] = instr;

  instr.addr = &mos6502::Addr_ABX;
  instr.code = &mos6502::Op_EOR;
  instr.cycl = 4;
  InstrTable[0x5D] = instr;

  instr.addr = &mos6502::Addr_ABY;
  instr.code = &mos6502::Op_EOR;
  instr.cycl = 4;
  InstrTable[0x59] = instr;

	instr.addr = &mos6502::Addr_INX;
	instr.code = &mos6502::Op_EOR;
  instr.cycl = 6;
	InstrTable[0x41] = instr;

	instr.addr = &mos6502::Addr_INY;
	instr.code = &mos6502::Op_EOR;
  instr.cycl = 5;
	InstrTable[0x51] = instr;

  // INC

	instr.addr = &mos6502::Addr_ZER;
	instr.code = &mos6502::Op_INC;
  instr.cycl = 5;
	InstrTable[0xE6] = instr;

	instr.addr = &mos6502::Addr_ZEX;
	instr.code = &mos6502::Op_INC;
  instr.cycl = 6;
	InstrTable[0xF6] = instr;

  instr.addr = &mos6502::Addr_ABS;
  instr.code = &mos6502::Op_INC;
  instr.cycl = 6;
  InstrTable[0xEE] = instr;

	instr.addr = &mos6502::Addr_ABX_ex;
	instr.code = &mos6502::Op_INC;
  instr.cycl = 7;
	InstrTable[0xFE] = instr;

  // INX
	
	instr.addr = &mos6502::Addr_IMP;
	instr.code = &mos6502::Op_INX;
  instr.cycl = 2;
	InstrTable[0xE8] = instr;

  // INY

	instr.addr = &mos6502::Addr_IMP;
	instr.code = &mos6502::Op_INY;
  instr.cycl = 2;
	InstrTable[0xC8] = instr;

  // JMP

	instr.addr = &mos6502::Addr_ABS;
	instr.code = &mos6502::Op_JMP;
  instr.cycl = 3;
	InstrTable[0x4C] = instr;

	instr.addr = &mos6502::Addr_ABI;
	instr.code = &mos6502::Op_JMP;
  instr.cycl = 5;
	InstrTable[0x6C] = instr;

  // JSR

	instr.addr = &mos6502::Addr_ABS;
	instr.code = &mos6502::Op_JSR;
  instr.cycl = 6;
	InstrTable[0x20] = instr;

  // LDA

	instr.addr = &mos6502::Addr_IMM;
	instr.code = &mos6502::Op_LDA;
  instr.cycl = 2;
	InstrTable[0xA9] = instr;

  instr.addr = &mos6502::Addr_ZER;
  instr.code = &mos6502::Op_LDA;
  instr.cycl = 3;
  InstrTable[0xA5] = instr;

  instr.addr = &mos6502::Addr_ZEX;
  instr.code = &mos6502::Op_LDA;
  instr.cycl = 4;
  InstrTable[0xB5] = instr;

	instr.addr = &mos6502::Addr_ABS;
	instr.code = &mos6502::Op_LDA;
  instr.cycl = 4;
	InstrTable[0xAD] = instr;

  instr.addr = &mos6502::Addr_ABX;
  instr.code = &mos6502::Op_LDA;
  instr.cycl = 4;
  InstrTable[0xBD] = instr;

  instr.addr = &mos6502::Addr_ABY;
  instr.code = &mos6502::Op_LDA;
  instr.cycl = 4;
  InstrTable[0xB9] = instr;

	instr.addr = &mos6502::Addr_INX;
	instr.code = &mos6502::Op_LDA;
  instr.cycl = 6;
	InstrTable[0xA1] = instr;

	instr.addr = &mos6502::Addr_INY;
	instr.code = &mos6502::Op_LDA;
  instr.cycl = 5;
	InstrTable[0xB1] = instr;

  // LDX

	instr.addr = &mos6502::Addr_IMM;
	instr.code = &mos6502::Op_LDX;
  instr.cycl = 2;
	InstrTable[0xA2] = instr;

  instr.addr = &mos6502::Addr_ZER;
  instr.code = &mos6502::Op_LDX;
  instr.cycl = 3;
  InstrTable[0xA6] = instr;

  instr.addr = &mos6502::Addr_ZEY;
  instr.code = &mos6502::Op_LDX;
  instr.cycl = 4;
  InstrTable[0xB6] = instr;

	instr.addr = &mos6502::Addr_ABS;
	instr.code = &mos6502::Op_LDX;
  instr.cycl = 4;
	InstrTable[0xAE] = instr;

	instr.addr = &mos6502::Addr_ABY;
	instr.code = &mos6502::Op_LDX;
  instr.cycl = 4;
	InstrTable[0xBE] = instr;

  // LDY

	instr.addr = &mos6502::Addr_IMM;
	instr.code = &mos6502::Op_LDY;
  instr.cycl = 2;
	InstrTable[0xA0] = instr;

	instr.addr = &mos6502::Addr_ZER;
	instr.code = &mos6502::Op_LDY;
  instr.cycl = 3;
	InstrTable[0xA4] = instr;

	instr.addr = &mos6502::Addr_ZEX;
	instr.code = &mos6502::Op_LDY;
  instr.cycl = 4;
	InstrTable[0xB4] = instr;

  instr.addr = &mos6502::Addr_ABS;
  instr.code = &mos6502::Op_LDY;
  instr.cycl = 4;
  InstrTable[0xAC] = instr;

	instr.addr = &mos6502::Addr_ABX;
	instr.code = &mos6502::Op_LDY;
  instr.cycl = 4;
	InstrTable[0xBC] = instr;

  // LSR

  instr.addr = &mos6502::Addr_ACC;
  instr.code = &mos6502::Op_LSR_ACC;
  instr.cycl = 2;
  InstrTable[0x4A] = instr;

  instr.addr = &mos6502::Addr_ZER;
  instr.code = &mos6502::Op_LSR;
  instr.cycl = 5;
  InstrTable[0x46] = instr;

  instr.addr = &mos6502::Addr_ZEX;
  instr.code = &mos6502::Op_LSR;
  instr.cycl = 6;
  InstrTable[0x56] = instr;

	instr.addr = &mos6502::Addr_ABS;
	instr.code = &mos6502::Op_LSR;
  instr.cycl = 6;
	InstrTable[0x4E] = instr;

	instr.addr = &mos6502::Addr_ABX_ex;
	instr.code = &mos6502::Op_LSR;
  instr.cycl = 7;
	InstrTable[0x5E] = instr;

  // NOP

	instr.addr = &mos6502::Addr_IMP;
	instr.code = &mos6502::Op_NOP;
  instr.cycl = 2;
	InstrTable[0xEA] = instr;

  // ORA

	instr.addr = &mos6502::Addr_IMM;
	instr.code = &mos6502::Op_ORA;
  instr.cycl = 2;
	InstrTable[0x09] = instr;

	instr.addr = &mos6502::Addr_ZER;
	instr.code = &mos6502::Op_ORA;
  instr.cycl = 3;
	InstrTable[0x05] = instr;

  instr.addr = &mos6502::Addr_ZEX;
  instr.code = &mos6502::Op_ORA;
  instr.cycl = 4;
  InstrTable[0x15] = instr;

  instr.addr = &mos6502::Addr_ABS;
  instr.code = &mos6502::Op_ORA;
  instr.cycl = 4;
  InstrTable[0x0D] = instr;

	instr.addr = &mos6502::Addr_ABX;
	instr.code = &mos6502::Op_ORA;
  instr.cycl = 4;
	InstrTable[0x1D] = instr;

	instr.addr = &mos6502::Addr_ABY;
	instr.code = &mos6502::Op_ORA;
  instr.cycl = 4;
	InstrTable[0x19] = instr;

  instr.addr = &mos6502::Addr_INX;
  instr.code = &mos6502::Op_ORA;
  instr.cycl = 6;
  InstrTable[0x01] = instr;

  instr.addr = &mos6502::Addr_INY;
  instr.code = &mos6502::Op_ORA;
  instr.cycl = 5;
  InstrTable[0x11] = instr;

  // PHA

	instr.addr = &mos6502::Addr_IMP;
	instr.code = &mos6502::Op_PHA;
  instr.cycl = 3;
	InstrTable[0x48] = instr;

  // PHP

	instr.addr = &mos6502::Addr_IMP;
	instr.code = &mos6502::Op_PHP;
  instr.cycl = 3;
	InstrTable[0x08] = instr;

  // PLA

	instr.addr = &mos6502::Addr_IMP;
	instr.code = &mos6502::Op_PLA;
  instr.cycl = 4;
	InstrTable[0x68] = instr;

  // PLP

	instr.addr = &mos6502::Addr_IMP;
	instr.code = &mos6502::Op_PLP;
  instr.cycl = 4;
	InstrTable[0x28] = instr;

  // ROL

  instr.addr = &mos6502::Addr_ACC;
  instr.code = &mos6502::Op_ROL_ACC;
  instr.cycl = 2;
  InstrTable[0x2A] = instr;

	instr.addr = &mos6502::Addr_ZER;
	instr.code = &mos6502::Op_ROL;
  instr.cycl = 5;
	InstrTable[0x26] = instr;

	instr.addr = &mos6502::Addr_ZEX;
	instr.code = &mos6502::Op_ROL;
  instr.cycl = 6;
	InstrTable[0x36] = instr;

  instr.addr = &mos6502::Addr_ABS;
  instr.code = &mos6502::Op_ROL;
  instr.cycl = 6;
  InstrTable[0x2E] = instr;

	instr.addr = &mos6502::Addr_ABX_ex;
	instr.code = &mos6502::Op_ROL;
  instr.cycl = 7;
	InstrTable[0x3E] = instr;

  // ROR

  instr.addr = &mos6502::Addr_ACC;
  instr.code = &mos6502::Op_ROR_ACC;
  instr.cycl = 2;
  InstrTable[0x6A] = instr;

	instr.addr = &mos6502::Addr_ZER;
	instr.code = &mos6502::Op_ROR;
  instr.cycl = 5;
	InstrTable[0x66] = instr;

	instr.addr = &mos6502::Addr_ZEX;
	instr.code = &mos6502::Op_ROR;
  instr.cycl = 6;
	InstrTable[0x76] = instr;

  instr.addr = &mos6502::Addr_ABS;
  instr.code = &mos6502::Op_ROR;
  instr.cycl = 6;
  InstrTable[0x6E] = instr;

	instr.addr = &mos6502::Addr_ABX_ex;
	instr.code = &mos6502::Op_ROR;
  instr.cycl = 7;
	InstrTable[0x7E] = instr;

  // RTI

	instr.addr = &mos6502::Addr_IMP;
	instr.code = &mos6502::Op_RTI;
  instr.cycl = 6;
	InstrTable[0x40] = instr;

  // RTS

	instr.addr = &mos6502::Addr_IMP;
	instr.code = &mos6502::Op_RTS;
  instr.cycl = 6;
	InstrTable[0x60] = instr;

  // SBC

	instr.addr = &mos6502::Addr_IMM;
	instr.code = &mos6502::Op_SBC;
  instr.cycl = 2;
	InstrTable[0xE9] = instr;

	instr.addr = &mos6502::Addr_ZER;
	instr.code = &mos6502::Op_SBC;
  instr.cycl = 3;
	InstrTable[0xE5] = instr;

  instr.addr = &mos6502::Addr_ZEX;
  instr.code = &mos6502::Op_SBC;
  instr.cycl = 4;
  InstrTable[0xF5] = instr;

  instr.addr = &mos6502::Addr_ABS;
  instr.code = &mos6502::Op_SBC;
  instr.cycl = 4;
  InstrTable[0xED] = instr;

  instr.addr = &mos6502::Addr_ABX;
  instr.code = &mos6502::Op_SBC;
  instr.cycl = 4;
  InstrTable[0xFD] = instr;

  instr.addr = &mos6502::Addr_ABY;
  instr.code = &mos6502::Op_SBC;
  instr.cycl = 4;
  InstrTable[0xF9] = instr;

	instr.addr = &mos6502::Addr_INX;
	instr.code = &mos6502::Op_SBC;
  instr.cycl = 6;
	InstrTable[0xE1] = instr;

	instr.addr = &mos6502::Addr_INY;
	instr.code = &mos6502::Op_SBC;
  instr.cycl = 5;
	InstrTable[0xF1] = instr;

  // SEC

	instr.addr = &mos6502::Addr_IMP;
	instr.code = &mos6502::Op_SEC;
  instr.cycl = 2;
	InstrTable[0x38] = instr;

  // SED

	instr.addr = &mos6502::Addr_IMP;
	instr.code = &mos6502::Op_SED;
  instr.cycl = 2;
	InstrTable[0xF8] = instr;

  // SEI

	instr.addr = &mos6502::Addr_IMP;
	instr.code = &mos6502::Op_SEI;
  instr.cycl = 2;
	InstrTable[0x78] = instr;

  // STA

  instr.addr = &mos6502::Addr_ZER;
  instr.code = &mos6502::Op_STA;
  instr.cycl = 3;
  InstrTable[0x85] = instr;

  instr.addr = &mos6502::Addr_ZEX;
  instr.code = &mos6502::Op_STA;
  instr.cycl = 4;
  InstrTable[0x95] = instr;

	instr.addr = &mos6502::Addr_ABS;
	instr.code = &mos6502::Op_STA;
  instr.cycl = 4;
	InstrTable[0x8D] = instr;

  instr.addr = &mos6502::Addr_ABX_ex;
  instr.code = &mos6502::Op_STA;
  instr.cycl = 5;
  InstrTable[0x9D] = instr;

  instr.addr = &mos6502::Addr_ABY_ex;
  instr.code = &mos6502::Op_STA;
  instr.cycl = 5;
  InstrTable[0x99] = instr;

	instr.addr = &mos6502::Addr_INX;
	instr.code = &mos6502::Op_STA;
  instr.cycl = 6;
	InstrTable[0x81] = instr;

	instr.addr = &mos6502::Addr_INY_ex;
	instr.code = &mos6502::Op_STA;
  instr.cycl = 6;
	InstrTable[0x91] = instr;

  // STX

	instr.addr = &mos6502::Addr_ZER;
	instr.code = &mos6502::Op_STX;
  instr.cycl = 3;
	InstrTable[0x86] = instr;

	instr.addr = &mos6502::Addr_ZEY;
	instr.code = &mos6502::Op_STX;
  instr.cycl = 4;
	InstrTable[0x96] = instr;

  instr.addr = &mos6502::Addr_ABS;
  instr.code = &mos6502::Op_STX;
  instr.cycl = 4;
  InstrTable[0x8E] = instr;

  // STY

  instr.addr = &mos6502::Addr_ZER;
  instr.code = &mos6502::Op_STY;
  instr.cycl = 3;
  InstrTable[0x84] = instr;

  instr.addr = &mos6502::Addr_ZEX;
  instr.code = &mos6502::Op_STY;
  instr.cycl = 4;
  InstrTable[0x94] = instr;

	instr.addr = &mos6502::Addr_ABS;
	instr.code = &mos6502::Op_STY;
  instr.cycl = 4;
	InstrTable[0x8C] = instr;

  // TAX

	instr.addr = &mos6502::Addr_IMP;
	instr.code = &mos6502::Op_TAX;
  instr.cycl = 2;
	InstrTable[0xAA] = instr;

  // TAY

	instr.addr = &mos6502::Addr_IMP;
	instr.code = &mos6502::Op_TAY;
  instr.cycl = 2;
	InstrTable[0xA8] = instr;

  // TSX

	instr.addr = &mos6502::Addr_IMP;
	instr.code = &mos6502::Op_TSX;
  instr.cycl = 2;
	InstrTable[0xBA] = instr;

  // TXA

	instr.addr = &mos6502::Addr_IMP;
	instr.code = &mos6502::Op_TXA;
  instr.cycl = 2;
	InstrTable[0x8A] = instr;

  // TXS

	instr.addr = &mos6502::Addr_IMP;
	instr.code = &mos6502::Op_TXS;
  instr.cycl = 2;
	InstrTable[0x9A] = instr;

  // TYA

	instr.addr = &mos6502::Addr_IMP;
	instr.code = &mos6502::Op_TYA;
  instr.cycl = 2;
	InstrTable[0x98] = instr;

  //// UNDOCUMENTED INSTRUCTIONS

  // NOP

  InstrTable[0x1A] = InstrTable[0x3A] = InstrTable[0x5A] = InstrTable[0x7A] = InstrTable[0xDA] = InstrTable[0xFA] = InstrTable[0xEA];

  // SKB (skip next byte)

  instr.addr = &mos6502::Addr_ZEX;
  instr.code = &mos6502::Op_SKB;
  instr.cycl = 4;
  InstrTable[0x14] = instr;

  // INS

  instr.addr = &mos6502::Addr_ZER;
  instr.code = &mos6502::Op_INS;
  instr.cycl = 5;
  InstrTable[0xE7] = instr;

  instr.addr = &mos6502::Addr_ZEX;
  instr.code = &mos6502::Op_INS;
  instr.cycl = 6;
  InstrTable[0xF7] = instr;

  instr.addr = &mos6502::Addr_ABS;
  instr.code = &mos6502::Op_INS;
  instr.cycl = 6;
  InstrTable[0xEF] = instr;

  instr.addr = &mos6502::Addr_ABX_ex;
  instr.code = &mos6502::Op_INS;
  instr.cycl = 7;
  InstrTable[0xFF] = instr;

  instr.addr = &mos6502::Addr_ABY_ex;
  instr.code = &mos6502::Op_INS;
  instr.cycl = 7;
  InstrTable[0xFB] = instr;

  instr.addr = &mos6502::Addr_INX;
  instr.code = &mos6502::Op_INS;
  instr.cycl = 8;
  InstrTable[0xE3] = instr;

  instr.addr = &mos6502::Addr_INY_ex;
  instr.code = &mos6502::Op_INS;
  instr.cycl = 8;
  InstrTable[0xF3] = instr;


	Reset();
	
	return;
}

uint16_t mos6502::Addr_ACC()
{
	return 0; // not used
}

uint16_t mos6502::Addr_IMM()
{
	return pc++;
}

uint16_t mos6502::Addr_ABS()
{
	uint16_t addrL;
	uint16_t addrH;
	uint16_t addr;
	
	addrL = READ(pc++);
	addrH = READ(pc++);
	
	addr = addrL + (addrH << 8);
		
	return addr;
}

uint16_t mos6502::Addr_ZER()
{
	return READ(pc++);
}

uint16_t mos6502::Addr_IMP()
{
	return 0; // not used
}

uint16_t mos6502::Addr_REL()
{
	uint16_t offset;
	uint16_t addr;
	
	offset = (uint16_t)READ(pc++);
  if (offset & 0x80) offset |= 0xFF00;
  addr = pc + (int16_t)offset;
	return addr;
}

uint16_t mos6502::Addr_ABI()
{
	uint16_t addrL;
	uint16_t addrH;
	uint16_t effL;
	uint16_t effH;
	uint16_t abs;
	uint16_t addr;
	
	addrL = READ(pc++);
	addrH = READ(pc++);
	
	abs = (addrH << 8) | addrL;
	
	effL = READ(abs);

#ifndef CMOS_INDIRECT_JMP_FIX
	effH = READ((abs & 0xFF00) + ((abs + 1) & 0x00FF) );
#else
	effH = READ(abs + 1);
#endif

	addr = effL + 0x100 * effH;
	
	return addr;
}

uint16_t mos6502::Addr_ZEX()
{
	uint16_t addr = (READ(pc++) + X) % 256;
	return addr;
}

uint16_t mos6502::Addr_ZEY()
{
	uint16_t addr = (READ(pc++) + Y) % 256;
	return addr;
}

uint16_t mos6502::Addr_ABX()
{
	uint16_t addr = Addr_ABX_ex();
  addCycles += (addr & 0xff00) == (pc & 0xff00) ? 0 : 1;
	return addr;
}

// doesn't change addCycles
uint16_t mos6502::Addr_ABX_ex()
{
  uint16_t addr;
  uint16_t addrL;
  uint16_t addrH;

  addrL = READ(pc++);
  addrH = READ(pc++);

  addr = addrL + (addrH << 8) + X;

  return addr;
}

uint16_t mos6502::Addr_ABY()
{
	uint16_t addr = Addr_ABY_ex();
  addCycles += (addr & 0xff00) == (pc & 0xff00) ? 0 : 1;
	return addr;
}

uint16_t mos6502::Addr_ABY_ex()
{
  uint16_t addr;
  uint16_t addrL;
  uint16_t addrH;

  addrL = READ(pc++);
  addrH = READ(pc++);

  addr = addrL + (addrH << 8) + Y;
  return addr;
}

uint16_t mos6502::Addr_INX()
{
	uint16_t zeroL;
	uint16_t zeroH;
	uint16_t addr;
	
	zeroL = (READ(pc++) + X) % 256;
	zeroH = (zeroL + 1) % 256;
	addr = READ(zeroL) + (READ(zeroH) << 8);
	
	return addr;
}

uint16_t mos6502::Addr_INY()
{
  uint16_t addr = Addr_INY_ex();
  addCycles += (addr & 0xff00) == (pc & 0xff00) ? 0 : 1;
	return addr;
}

// doesn't change addCycles
uint16_t mos6502::Addr_INY_ex()
{
  uint16_t zeroL;
  uint16_t zeroH;
  uint16_t addr;

  zeroL = READ(pc++);
  zeroH = (zeroL + 1) % 256;
  addr = READ(zeroL) + (READ(zeroH) << 8) + Y;

  return addr;
}

int mos6502::Reset()
{
	A = 0x00;
	Y = 0x00;
	X = 0x00;
	
	pc = (READ(rstVectorH) << 8) + READ(rstVectorL); // load PC from reset vector
		
  sp = 0xFD;
    
  status |= F_CONSTANT;
    
	illegalOpcode = false;
	
	return 6;  // according to the datasheet, the reset routine takes 6 clock cycles
}

void mos6502::StackPush(uint8_t byte)
{	
	WRITE(0x0100 + sp, byte);
	if(sp == 0x00) sp = 0xFF;
	else sp--;
}

uint8_t mos6502::StackPop()
{
	if(sp == 0xFF) sp = 0x00;
	else sp++;
	return READ(0x0100 + sp);
}

int mos6502::IRQ()
{
  int cycles = 0;
	if(!IF_INTERRUPT())
	{
		SET_BREAK(0);
		StackPush((pc >> 8) & 0xFF);
		StackPush(pc & 0xFF);
		StackPush(status);
		SET_INTERRUPT(1);
		pc = (READ(irqVectorH) << 8) + READ(irqVectorL);
    cycles = 7;
	}
	return cycles;
}

int mos6502::NMI()
{
	SET_BREAK(0);
	StackPush((pc >> 8) & 0xFF);
	StackPush(pc & 0xFF);
	StackPush(status);
	SET_INTERRUPT(1);
	pc = (READ(nmiVectorH) << 8) + READ(nmiVectorL);
	return 7;
}

// ret = number of cycles
int mos6502::Run()
{
  Instr const * instr = InstrTable + READ(pc++);

  addCycles = 0;

  uint16_t src = (this->*instr->addr)();
  (this->*instr->code)(src);

  return instr->cycl + addCycles;
}

void mos6502::Op_ILLEGAL(uint16_t src)
{
  //printf("illegal 0x%04x 0x%02x\n", pc-1,READ(pc-1));
	illegalOpcode = true;
}


void mos6502::Op_ADC(uint16_t src)
{
	uint8_t m = READ(src);
	unsigned int tmp = m + A + (IF_CARRY() ? 1 : 0);
	SET_ZERO(!(tmp & 0xFF));
  if (IF_DECIMAL())
  {
    if (((A & 0xF) + (m & 0xF) + (IF_CARRY() ? 1 : 0)) > 9) tmp += 6;
    SET_NEGATIVE(tmp & 0x80);
    SET_OVERFLOW(!((A ^ m) & 0x80) && ((A ^ tmp) & 0x80));
    if (tmp > 0x99)
    {
      tmp += 96;
    }
    SET_CARRY(tmp > 0x99);
  }
	else
	{
		SET_NEGATIVE(tmp & 0x80);
		SET_OVERFLOW(!((A ^ m) & 0x80) && ((A ^ tmp) & 0x80));
		SET_CARRY(tmp > 0xFF);
  }
	
  A = tmp & 0xFF;
}



void mos6502::Op_AND(uint16_t src)
{
	uint8_t m = READ(src);
	uint8_t res = m & A;
	SET_NEGATIVE(res & 0x80);
	SET_ZERO(!res);
	A = res;
}


void mos6502::Op_ASL(uint16_t src)
{
	uint8_t m = READ(src);
  SET_CARRY(m & 0x80);
  m <<= 1;
  m &= 0xFF;
  SET_NEGATIVE(m & 0x80);
  SET_ZERO(!m);
  WRITE(src, m);
}

void mos6502::Op_ASL_ACC(uint16_t src)
{
	uint8_t m = A;
  SET_CARRY(m & 0x80);
  m <<= 1;
  m &= 0xFF;
  SET_NEGATIVE(m & 0x80);
  SET_ZERO(!m);
  A = m;
}

void mos6502::Op_BCC(uint16_t src)
{
  if (!IF_CARRY())
  {
    addCycles += (src & 0xff00) == (pc & 0xff00) ? 1 : 2;
    pc = src;
  }
}


void mos6502::Op_BCS(uint16_t src)
{
  if (IF_CARRY())
  {
    addCycles += (src & 0xff00) == (pc & 0xff00) ? 1 : 2;
    pc = src;
  }
}

void mos6502::Op_BEQ(uint16_t src)
{
  if (IF_ZERO())
  {
    addCycles += (src & 0xff00) == (pc & 0xff00) ? 1 : 2;
    pc = src;
  }
}

void mos6502::Op_BIT(uint16_t src)
{
	uint8_t m = READ(src);
	uint8_t res = m & A;
	SET_NEGATIVE(res & 0x80);
	status = (status & 0x3F) | (uint8_t)(m & 0xC0);
	SET_ZERO(!res);
}

void mos6502::Op_BMI(uint16_t src)
{
  if (IF_NEGATIVE())
  {
    addCycles += (src & 0xff00) == (pc & 0xff00) ? 1 : 2;
    pc = src;
  }
}

void mos6502::Op_BNE(uint16_t src)
{
  if (!IF_ZERO())
  {
    addCycles += (src & 0xff00) == (pc & 0xff00) ? 1 : 2;
    pc = src;
  }
}

void mos6502::Op_BPL(uint16_t src)
{
  if (!IF_NEGATIVE())
  {
    addCycles += (src & 0xff00) == (pc & 0xff00) ? 1 : 2;
    pc = src;
  }
}

void mos6502::Op_BRK(uint16_t src)
{
	pc++;
	StackPush((pc >> 8) & 0xFF);
	StackPush(pc & 0xFF);
	StackPush(status | F_BREAK);
	SET_INTERRUPT(1);
	pc = (READ(irqVectorH) << 8) + READ(irqVectorL);
}

void mos6502::Op_BVC(uint16_t src)
{
  if (!IF_OVERFLOW())
  {
    addCycles += (src & 0xff00) == (pc & 0xff00) ? 1 : 2;
    pc = src;
  }
}

void mos6502::Op_BVS(uint16_t src)
{
  if (IF_OVERFLOW())
  {
    addCycles += (src & 0xff00) == (pc & 0xff00) ? 1 : 2;
    pc = src;
  }
}

void mos6502::Op_CLC(uint16_t src)
{
	SET_CARRY(0);
}

void mos6502::Op_CLD(uint16_t src)
{
	SET_DECIMAL(0);
}

void mos6502::Op_CLI(uint16_t src)
{
	SET_INTERRUPT(0);
}

void mos6502::Op_CLV(uint16_t src)
{
	SET_OVERFLOW(0);
}

void mos6502::Op_CMP(uint16_t src)
{
	unsigned int tmp = A - READ(src);
	SET_CARRY(tmp < 0x100);
	SET_NEGATIVE(tmp & 0x80);
	SET_ZERO(!(tmp & 0xFF));
}

void mos6502::Op_CPX(uint16_t src)
{
	unsigned int tmp = X - READ(src);
	SET_CARRY(tmp < 0x100);
	SET_NEGATIVE(tmp & 0x80);
	SET_ZERO(!(tmp & 0xFF));
}

void mos6502::Op_CPY(uint16_t src)
{
	unsigned int tmp = Y - READ(src);
	SET_CARRY(tmp < 0x100);
	SET_NEGATIVE(tmp & 0x80);
	SET_ZERO(!(tmp & 0xFF));
}

void mos6502::Op_DEC(uint16_t src)
{
	uint8_t m = READ(src);
	m = (m - 1) % 256;
  SET_NEGATIVE(m & 0x80);
  SET_ZERO(!m);
  WRITE(src, m);
}

void mos6502::Op_DEX(uint16_t src)
{
	uint8_t m = X;
	m = (m - 1) % 256;
	SET_NEGATIVE(m & 0x80);
  SET_ZERO(!m);
  X = m;
}

void mos6502::Op_DEY(uint16_t src)
{
	uint8_t m = Y;
	m = (m - 1) % 256;
	SET_NEGATIVE(m & 0x80);
  SET_ZERO(!m);
  Y = m;
}

void mos6502::Op_EOR(uint16_t src)
{
	uint8_t m = READ(src);
	m = A ^ m;
	SET_NEGATIVE(m & 0x80);
  SET_ZERO(!m);
  A = m;
}

void mos6502::Op_INC(uint16_t src)
{
	uint8_t m = READ(src);
	m = (m + 1) % 256;
	SET_NEGATIVE(m & 0x80);
  SET_ZERO(!m);
  WRITE(src, m);
}

void mos6502::Op_INX(uint16_t src)
{
	uint8_t m = X;
	m = (m + 1) % 256;
	SET_NEGATIVE(m & 0x80);
  SET_ZERO(!m);
  X = m;
}

void mos6502::Op_INY(uint16_t src)
{
	uint8_t m = Y;
	m = (m + 1) % 256;
	SET_NEGATIVE(m & 0x80);
  SET_ZERO(!m);
  Y = m;
}

void mos6502::Op_JMP(uint16_t src)
{
	pc = src;
}

void mos6502::Op_JSR(uint16_t src)
{
	--pc;
	StackPush((pc >> 8) & 0xFF);
	StackPush(pc & 0xFF);
	pc = src;
}

void mos6502::Op_LDA(uint16_t src)
{
	uint8_t m = READ(src);
	SET_NEGATIVE(m & 0x80);
  SET_ZERO(!m);
	A = m;
}

void mos6502::Op_LDX(uint16_t src)
{
	uint8_t m = READ(src);
	SET_NEGATIVE(m & 0x80);
  SET_ZERO(!m);
	X = m;
}

void mos6502::Op_LDY(uint16_t src)
{
	uint8_t m = READ(src);
	SET_NEGATIVE(m & 0x80);
  SET_ZERO(!m);
	Y = m;
}

void mos6502::Op_LSR(uint16_t src)
{
	uint8_t m = READ(src);
	SET_CARRY(m & 0x01);
	m >>= 1;
	SET_NEGATIVE(0);
	SET_ZERO(!m);
	WRITE(src, m);
}

void mos6502::Op_LSR_ACC(uint16_t src)
{
	uint8_t m = A;
	SET_CARRY(m & 0x01);
	m >>= 1;
	SET_NEGATIVE(0);
	SET_ZERO(!m);
	A = m;
}

void mos6502::Op_NOP(uint16_t src)
{
}

void mos6502::Op_ORA(uint16_t src)
{
	uint8_t m = READ(src);
	m = A | m;
	SET_NEGATIVE(m & 0x80);
  SET_ZERO(!m);
  A = m;
}

void mos6502::Op_PHA(uint16_t src)
{
	StackPush(A);
}

void mos6502::Op_PHP(uint16_t src)
{	
	StackPush(status | F_BREAK);
}

void mos6502::Op_PLA(uint16_t src)
{
	A = StackPop();
	SET_NEGATIVE(A & 0x80);
  SET_ZERO(!A);
}

void mos6502::Op_PLP(uint16_t src)
{
	status = StackPop();
	SET_CONSTANT(1);
}

void mos6502::Op_ROL(uint16_t src)
{
	uint16_t m = READ(src);
  m <<= 1;
  if (IF_CARRY()) m |= 0x01;
  SET_CARRY(m > 0xFF);
  m &= 0xFF;
  SET_NEGATIVE(m & 0x80);
  SET_ZERO(!m);
  WRITE(src, m);
}

void mos6502::Op_ROL_ACC(uint16_t src)
{
	uint16_t m = A;
  m <<= 1;
  if (IF_CARRY()) m |= 0x01;
  SET_CARRY(m > 0xFF);
  m &= 0xFF;
  SET_NEGATIVE(m & 0x80);
  SET_ZERO(!m);
  A = m;
}

void mos6502::Op_ROR(uint16_t src)
{
	uint16_t m = READ(src);
  if (IF_CARRY()) m |= 0x100;
  SET_CARRY(m & 0x01);
  m >>= 1;
  m &= 0xFF;
  SET_NEGATIVE(m & 0x80);
  SET_ZERO(!m);
  WRITE(src, m);
}

void mos6502::Op_ROR_ACC(uint16_t src)
{
	uint16_t m = A;
  if (IF_CARRY()) m |= 0x100;
  SET_CARRY(m & 0x01);
  m >>= 1;
  m &= 0xFF;
  SET_NEGATIVE(m & 0x80);
  SET_ZERO(!m);
  A = m;
}

void mos6502::Op_RTI(uint16_t src)
{
	uint8_t lo, hi;
	
	status = StackPop();
	
	lo = StackPop();
	hi = StackPop();
	
	pc = (hi << 8) | lo;
}

void mos6502::Op_RTS(uint16_t src)
{
	uint8_t lo, hi;
	
	lo = StackPop();
	hi = StackPop();
	
	pc = ((hi << 8) | lo) + 1;
}

void mos6502::Op_SBC(uint16_t src)
{
	uint8_t m = READ(src);
	unsigned int tmp = A - m - (IF_CARRY() ? 0 : 1);
	SET_NEGATIVE(tmp & 0x80);
	SET_ZERO(!(tmp & 0xFF));
  SET_OVERFLOW(((A ^ tmp) & 0x80) && ((A ^ m) & 0x80));

  if (IF_DECIMAL())
  {
    if ( ((A & 0x0F) - (IF_CARRY() ? 0 : 1)) < (m & 0x0F)) tmp -= 6;
    if (tmp > 0x99)
    {
      tmp -= 0x60;
    }
  }
  SET_CARRY(tmp < 0x100);
  A = (tmp & 0xFF);
}

void mos6502::Op_SEC(uint16_t src)
{
	SET_CARRY(1);
}

void mos6502::Op_SED(uint16_t src)
{
	SET_DECIMAL(1);
}

void mos6502::Op_SEI(uint16_t src)
{
	SET_INTERRUPT(1);
}

void mos6502::Op_STA(uint16_t src)
{
	WRITE(src, A);
}

void mos6502::Op_STX(uint16_t src)
{
	WRITE(src, X);
}

void mos6502::Op_STY(uint16_t src)
{
	WRITE(src, Y);
}

void mos6502::Op_TAX(uint16_t src)
{
	uint8_t m = A;
  SET_NEGATIVE(m & 0x80);
  SET_ZERO(!m);
	X = m;
}

void mos6502::Op_TAY(uint16_t src)
{
	uint8_t m = A;
  SET_NEGATIVE(m & 0x80);
  SET_ZERO(!m);
	Y = m;
}

void mos6502::Op_TSX(uint16_t src)
{
	uint8_t m = sp;
  SET_NEGATIVE(m & 0x80);
  SET_ZERO(!m);
	X = m;
}

void mos6502::Op_TXA(uint16_t src)
{
	uint8_t m = X;
  SET_NEGATIVE(m & 0x80);
  SET_ZERO(!m);
	A = m;
}

void mos6502::Op_TXS(uint16_t src)
{
	sp = X;
}

void mos6502::Op_TYA(uint16_t src)
{
	uint8_t m = Y;
  SET_NEGATIVE(m & 0x80);
  SET_ZERO(!m);
	A = m;
}

void mos6502::Op_SKB(uint16_t src)
{
}

void mos6502::Op_INS(uint16_t src)
{
  Op_INC(src);
  Op_SBC(src);
}

