//============================================================================
// Name        : mos6502 
// Author      : Gianluca Ghettini
// Version     : 1.0
// Copyright   : 
// Description : A MOS 6502 CPU emulator written in C++
//============================================================================
// 2019, Fabrizio Di Vittorio adaption to a personal project:
//    - Cycle exact counting
//    - Some undocumented 6502 instructions

#pragma once

#include <stdint.h>


#define F_NEGATIVE  0x80
#define F_OVERFLOW  0x40
#define F_CONSTANT  0x20
#define F_BREAK     0x10
#define F_DECIMAL   0x08
#define F_INTERRUPT 0x04
#define F_ZERO      0x02
#define F_CARRY     0x01

#define SET_NEGATIVE(x) (x ? (status |= F_NEGATIVE) : (status &= (~F_NEGATIVE)) )
#define SET_OVERFLOW(x) (x ? (status |= F_OVERFLOW) : (status &= (~F_OVERFLOW)) )
#define SET_CONSTANT(x) (x ? (status |= F_CONSTANT) : (status &= (~F_CONSTANT)) )
#define SET_BREAK(x) (x ? (status |= F_BREAK) : (status &= (~F_BREAK)) )
#define SET_DECIMAL(x) (x ? (status |= F_DECIMAL) : (status &= (~F_DECIMAL)) )
#define SET_INTERRUPT(x) (x ? (status |= F_INTERRUPT) : (status &= (~F_INTERRUPT)) )
#define SET_ZERO(x) (x ? (status |= F_ZERO) : (status &= (~F_ZERO)) )
#define SET_CARRY(x) (x ? (status |= F_CARRY) : (status &= (~F_CARRY)) )

#define IF_NEGATIVE() ((status & F_NEGATIVE) ? true : false)
#define IF_OVERFLOW() ((status & F_OVERFLOW) ? true : false)
#define IF_CONSTANT() ((status & CONSTANT) ? true : false)
#define IF_BREAK() ((status & F_BREAK) ? true : false)
#define IF_DECIMAL() ((status & F_DECIMAL) ? true : false)
#define IF_INTERRUPT() ((status & F_INTERRUPT) ? true : false)
#define IF_ZERO() ((status & F_ZERO) ? true : false)
#define IF_CARRY() ((status & F_CARRY) ? true : false)



class mos6502
{
private:
	
	// registers
	uint8_t A; // accumulator
	uint8_t X; // X-index
	uint8_t Y; // Y-index
	
	// stack pointer
	uint8_t sp;
	
	// program counter
	uint16_t pc;
	
	// status register
	uint8_t status;

  uint8_t addCycles;  // additional required cycles

	typedef void (mos6502::*CodeExec)(uint16_t);
	typedef uint16_t (mos6502::*AddrExec)();

	struct Instr
	{
		AddrExec addr;
		CodeExec code;
    uint8_t  cycl;
	};
	
	Instr InstrTable[256];
	
	bool illegalOpcode;
	
	// addressing modes
	uint16_t Addr_ACC(); // ACCUMULATOR
	uint16_t IRAM_ATTR Addr_IMM(); // IMMEDIATE
	uint16_t Addr_ABS(); // ABSOLUTE
	uint16_t Addr_ZER(); // ZERO PAGE
	uint16_t Addr_ZEX(); // INDEXED-X ZERO PAGE
	uint16_t Addr_ZEY(); // INDEXED-Y ZERO PAGE
	uint16_t Addr_ABX(); // INDEXED-X ABSOLUTE
  uint16_t Addr_ABX_ex(); // INDEXED-X ABSOLUTE (non increment cycles on boundary cross, like DEC)
	uint16_t Addr_ABY(); // INDEXED-Y ABSOLUTE
  uint16_t Addr_ABY_ex(); // INDEXED-Y ABSOLUTE (non increment cycles on boundary cross, like STA)
	uint16_t Addr_IMP(); // IMPLIED
	uint16_t Addr_REL(); // RELATIVE
	uint16_t Addr_INX(); // INDEXED-X INDIRECT
	uint16_t Addr_INY(); // INDEXED-Y INDIRECT
  uint16_t Addr_INY_ex(); // INDEXED-Y INDIRECT (non increment cycles on boundary cross, like STA)
	uint16_t Addr_ABI(); // ABSOLUTE INDIRECT
	
	// opcodes (grouped as per datasheet)
	void Op_ADC(uint16_t src);
	void Op_AND(uint16_t src);
	void Op_ASL(uint16_t src);
  void Op_ASL_ACC(uint16_t src);
	void Op_BCC(uint16_t src);
	void Op_BCS(uint16_t src);
	
	void Op_BEQ(uint16_t src);
	void Op_BIT(uint16_t src);
	void Op_BMI(uint16_t src);
	void Op_BNE(uint16_t src);
	void Op_BPL(uint16_t src);
	
	void Op_BRK(uint16_t src);
	void Op_BVC(uint16_t src);
	void Op_BVS(uint16_t src);
	void Op_CLC(uint16_t src);
	void Op_CLD(uint16_t src);
	
	void Op_CLI(uint16_t src);
	void Op_CLV(uint16_t src);
	void Op_CMP(uint16_t src);
	void Op_CPX(uint16_t src);
	void Op_CPY(uint16_t src);
	
	void Op_DEC(uint16_t src);
	void Op_DEX(uint16_t src);
	void Op_DEY(uint16_t src);
	void Op_EOR(uint16_t src);
	void Op_INC(uint16_t src);
	
	void Op_INX(uint16_t src);
	void Op_INY(uint16_t src);
	void Op_JMP(uint16_t src);
	void Op_JSR(uint16_t src);
	void Op_LDA(uint16_t src);
	
	void Op_LDX(uint16_t src);
	void Op_LDY(uint16_t src);
	void Op_LSR(uint16_t src);
  void Op_LSR_ACC(uint16_t src);
	void Op_NOP(uint16_t src);
	void Op_ORA(uint16_t src);
	
	void Op_PHA(uint16_t src);
	void Op_PHP(uint16_t src);
	void Op_PLA(uint16_t src);
	void Op_PLP(uint16_t src);
	void Op_ROL(uint16_t src);
  void Op_ROL_ACC(uint16_t src);
	
	void Op_ROR(uint16_t src);
  void Op_ROR_ACC(uint16_t src);
	void Op_RTI(uint16_t src);
	void Op_RTS(uint16_t src);
	void Op_SBC(uint16_t src);
	void Op_SEC(uint16_t src);
	void Op_SED(uint16_t src);
	
	void Op_SEI(uint16_t src);
	void Op_STA(uint16_t src);
	void Op_STX(uint16_t src);
	void Op_STY(uint16_t src);
	void Op_TAX(uint16_t src);
	
	void Op_TAY(uint16_t src);
	void Op_TSX(uint16_t src);
	void Op_TXA(uint16_t src);
	void Op_TXS(uint16_t src);
	void Op_TYA(uint16_t src);

  // undocumented instructions
  void Op_SKB(uint16_t src);
  void Op_INS(uint16_t src);
	
	void Op_ILLEGAL(uint16_t src);
	
	// IRQ, reset, NMI vectors
	static const uint16_t irqVectorH = 0xFFFF;
	static const uint16_t irqVectorL = 0xFFFE;
	static const uint16_t rstVectorH = 0xFFFD;
	static const uint16_t rstVectorL = 0xFFFC;
	static const uint16_t nmiVectorH = 0xFFFB;
	static const uint16_t nmiVectorL = 0xFFFA;
	
  void * Context;
	
	// stack operations
	inline void StackPush(uint8_t byte);
	inline uint8_t StackPop();
	
public:
	
	mos6502(void * context);
	int NMI();
	int IRQ();
	int Reset();
	int Run();
  void setPC(int addr) { pc = addr; }
  int PC() { return pc; }
  bool IRQEnabled() { return !IF_INTERRUPT(); }
};
