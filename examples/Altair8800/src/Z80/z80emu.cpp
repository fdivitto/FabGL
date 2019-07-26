/* z80emu.c
 * Z80 processor emulator. 
 *
 * Copyright (c) 2012-2017 Lin Ke-Fong
 *
 * This code is free, do whatever you want with it.
 */

#include "z80emu.h"
#include "z80user.h"
#include "instructions.h"
#include "macros.h"
#include "tables.h"

/* Indirect (HL) or prefixed indexed (IX + d) and (IY + d) memory operands are
 * encoded using the 3 bits "110" (0x06).
 */

#define INDIRECT_HL     0x06

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

static int	emulate (Z80_STATE * state, 
			int opcode,
			int elapsed_cycles, int number_cycles,
			void *context);

void Z80Reset (Z80_STATE *state)
{
        int     i;
        
        state->status = 0;
        AF = 0xffff;
        SP = 0xffff;
        state->i = state->pc = state->iff1 = state->iff2 = 0;
        state->im = Z80_INTERRUPT_MODE_0;
        
        /* Build register decoding tables for both 3-bit encoded 8-bit
         * registers and 2-bit encoded 16-bit registers. When an opcode is 
         * prefixed by 0xdd, HL is replaced by IX. When 0xfd prefixed, HL is
         * replaced by IY.
         */

        /* 8-bit "R" registers. */

        state->register_table[0] = &state->registers.byte[Z80_B];
        state->register_table[1] = &state->registers.byte[Z80_C];
        state->register_table[2] = &state->registers.byte[Z80_D];
        state->register_table[3] = &state->registers.byte[Z80_E];
        state->register_table[4] = &state->registers.byte[Z80_H];
        state->register_table[5] = &state->registers.byte[Z80_L];

        /* Encoding 0x06 is used for indexed memory operands and direct HL or
         * IX/IY register access.
         */

        state->register_table[6] = &state->registers.word[Z80_HL];
        state->register_table[7] = &state->registers.byte[Z80_A];
               
        /* "Regular" 16-bit "RR" registers. */

        state->register_table[8] = &state->registers.word[Z80_BC];
        state->register_table[9] = &state->registers.word[Z80_DE];
        state->register_table[10] = &state->registers.word[Z80_HL];
        state->register_table[11] = &state->registers.word[Z80_SP];
        
        /* 16-bit "SS" registers for PUSH and POP instructions (note that SP is
         * replaced by AF). 
         */

        state->register_table[12] = &state->registers.word[Z80_BC];
        state->register_table[13] = &state->registers.word[Z80_DE];
        state->register_table[14] = &state->registers.word[Z80_HL];
        state->register_table[15] = &state->registers.word[Z80_AF];

        /* 0xdd and 0xfd prefixed register decoding tables. */

        for (i = 0; i < 16; i++)

                state->dd_register_table[i] 
                        = state->fd_register_table[i] 
                        = state->register_table[i];

        state->dd_register_table[4] = &state->registers.byte[Z80_IXH];
        state->dd_register_table[5] = &state->registers.byte[Z80_IXL];
        state->dd_register_table[6] = &state->registers.word[Z80_IX];
        state->dd_register_table[10] = &state->registers.word[Z80_IX];
        state->dd_register_table[14] = &state->registers.word[Z80_IX];

        state->fd_register_table[4] = &state->registers.byte[Z80_IYH];
        state->fd_register_table[5] = &state->registers.byte[Z80_IYL];
        state->fd_register_table[6] = &state->registers.word[Z80_IY];
        state->fd_register_table[10] = &state->registers.word[Z80_IY];
        state->fd_register_table[14] = &state->registers.word[Z80_IY];        
}

int Z80Interrupt (Z80_STATE *state, int data_on_bus, void *context)
{
        state->status = 0;
        if (state->iff1) {
				
                state->iff1 = state->iff2 = 0;
                state->r = (state->r & 0x80) | ((state->r + 1) & 0x7f);
                switch (state->im) {

                        case Z80_INTERRUPT_MODE_0: {
                                
                                /* Assuming the opcode in data_on_bus is an
                                 * RST instruction, accepting the interrupt
                                 * should take 2 + 11 = 13 cycles.
                                 */

                                return emulate(state, 
					data_on_bus, 
					2, 4, 
					context);
                                
                        }

                        case Z80_INTERRUPT_MODE_1: {

				int	elapsed_cycles;

				elapsed_cycles = 0;
                                SP -= 2;
                                Z80_WRITE_WORD_INTERRUPT(SP, state->pc);
                                state->pc = 0x0038;
                                return elapsed_cycles + 13;

                        }

                        case Z80_INTERRUPT_MODE_2:
                        default: {

				int	elapsed_cycles, vector;

				elapsed_cycles = 0;
                                SP -= 2;
                                Z80_WRITE_WORD_INTERRUPT(SP, state->pc);
				vector = state->i << 8 | data_on_bus;

#ifdef Z80_MASK_IM2_VECTOR_ADDRESS

                                vector &= 0xfffe;

#endif

				Z80_READ_WORD_INTERRUPT(vector, state->pc);
                                return elapsed_cycles + 19;
                                
                        }

                }
                
        } else
                
                return 0;
}

int Z80NonMaskableInterrupt (Z80_STATE *state, void *context)
{
	int	elapsed_cycles;

        state->status = 0;

        state->iff2 = state->iff1;
        state->iff1 = 0;
        state->r = (state->r & 0x80) | ((state->r + 1) & 0x7f);

	elapsed_cycles = 0;
        SP -= 2;
        Z80_WRITE_WORD_INTERRUPT(SP, state->pc);
        state->pc = 0x0066;
        
        return elapsed_cycles + 11;
}

int Z80Emulate (Z80_STATE *state, int number_cycles, void *context)
{
        int     elapsed_cycles, pc, opcode;

        state->status = 0;
	elapsed_cycles = 0;
	pc = state->pc;
        Z80_FETCH_BYTE(pc, opcode);
        state->pc = pc + 1;

        return emulate(state, opcode, elapsed_cycles, number_cycles, context);
}

/* Actual emulation function. opcode is the first opcode to emulate, this is 
 * needed by Z80Interrupt() for interrupt mode 0.
 */

static int emulate (Z80_STATE * state, 
	int opcode, 
	int elapsed_cycles, int number_cycles, 
	void *context)
{
        int	pc, r;

        pc = state->pc;
        r = state->r & 0x7f;
        goto start_emulation;

        for ( ; ; ) {   

                void    **registers; 
                int     instruction;

                Z80_FETCH_BYTE(pc, opcode);
                pc++;

start_emulation:                

                registers = state->register_table;

emulate_next_opcode:

                instruction = INSTRUCTION_TABLE[opcode];

emulate_next_instruction:

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

                                if (registers == state->register_table) {

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

                                if (registers == state->register_table) {

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

                                if (registers == state->register_table) {

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
                                        ? state->i 
                                        : (state->r & 0x80) | (r & 0x7f);
                                f = SZYX_FLAGS_TABLE[a];

                                /* Note: On a real processor, if an interrupt
                                 * occurs during the execution of either 
                                 * "LD A, I" or "LD A, R", the parity flag is
                                 * reset. That can never happen here.
                                 */ 

                                f |= state->iff2 << Z80_P_FLAG_SHIFT;
                                f |= F & Z80_C_FLAG;

                                AF = (a << 8) | f;

                                elapsed_cycles++;

                                break;

                        }

                        case LD_I_A_LD_R_A: {

                                if (opcode == OPCODE_LD_I_A)

                                        state->i = A;

                                else {

                                        state->r = A;
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

                                EXCHANGE(AF, state->alternates[Z80_AF]);
                                break;

                        }

                        case EXX: {

                                EXCHANGE(BC, state->alternates[Z80_BC]);
                                EXCHANGE(DE, state->alternates[Z80_DE]);
                                EXCHANGE(HL, state->alternates[Z80_HL]);
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

                                        if (elapsed_cycles < number_cycles) 

                                                continue;

                                        else {
        
                                                f |= Z80_P_FLAG;
                                                pc -= 2;
                                                break;

                                        }

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

                                        if (elapsed_cycles < number_cycles) 

                                                continue;

                                        else {
        
                                                pc -= 2;
                                                break;

                                        }

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

                                if (registers == state->register_table) {

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

                                if (registers == state->register_table) {

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

                                state->status = Z80_STATUS_FLAG_HALT;

#else

				/* If an HALT instruction is executed, the Z80
				 * keeps executing NOPs until an interrupt is
				 * generated. Basically nothing happens for the
				 * remaining number of cycles.
				 */

				if (elapsed_cycles < number_cycles)
	
					elapsed_cycles = number_cycles;

#endif

				goto stop_emulation;

                        }

                        case DI: {

				state->iff1 = state->iff2 = 0;

#ifdef Z80_CATCH_DI

                                state->status = Z80_STATUS_FLAG_DI;
                                goto stop_emulation;

#else

                                /* No interrupt can be accepted right after
                                 * a DI or EI instruction on an actual Z80 
                                 * processor. By adding 4 cycles to 
                                 * number_cycles, at least one more 
                                 * instruction will be executed. However, this
                                 * will fail if the next instruction has
                                 * multiple 0xdd or 0xfd prefixes and
                                 * Z80_PREFIX_FAILSAFE is defined, but that
                                 * is an unlikely pathological case.
                                 */

                                number_cycles += 4;
                                break;

#endif                  

                        }

                        case EI: {

                                state->iff1 = state->iff2 = 1;

#ifdef Z80_CATCH_EI

                                state->status = Z80_STATUS_FLAG_EI;
                                goto stop_emulation;

#else

                                /* See comment for DI. */

                                number_cycles += 4;
                                break;

#endif

                        }

                        case IM_N: {

                                /* "IM 0/1" (0xed prefixed opcodes 0x4e and
                                 * 0x6e) is treated like a "IM 0".
                                 */
		
				if ((Y(opcode) & 0x03) <= 0x01)

                                        state->im = Z80_INTERRUPT_MODE_0;

                                else if (!(Y(opcode) & 1))

                                        state->im = Z80_INTERRUPT_MODE_1;

                                else

                                        state->im = Z80_INTERRUPT_MODE_2;

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

                                if (registers == state->register_table) {

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

                                if (registers == state->register_table) {

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

                                if (registers == state->register_table) {

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

                                if (registers == state->register_table) {

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

                                if (registers == state->register_table) {

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

                                if (registers == state->register_table) {

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

                                if (registers == state->register_table) {

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

                                if (registers == state->register_table) {

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
                                        
                                if (registers == state->register_table) {

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

                                if (registers == state->register_table) {

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

                                if (registers == state->register_table) {

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

                                state->iff1 = state->iff2;
                                POP(pc);        

#if defined(Z80_CATCH_RETI) && defined(Z80_CATCH_RETN)

                                state->status = opcode == OPCODE_RETI
                                        ? Z80_STATUS_FLAG_RETI
                                        : Z80_STATUS_FLAG_RETN;
                                goto stop_emulation;

#elif defined(Z80_CATCH_RETI)

                                state->status = Z80_STATUS_FLAG_RETI;
                                goto stop_emulation;

#elif defined(Z80_CATCH_RETN)

                                state->status = Z80_STATUS_FLAG_RETN;
                                goto stop_emulation;

#else

                                break;

#endif

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

                                        if (elapsed_cycles < number_cycles) 

                                                continue;

                                        else {

                                                f = SZYX_FLAGS_TABLE[b];
                                                pc -= 2;
                                                break;

                                        }

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

                                        if (elapsed_cycles < number_cycles) 

                                                continue;

                                        else {

                                                f = SZYX_FLAGS_TABLE[b];
                                                pc -= 2;
                                                break;

                                        }

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

                                if (registers != state->register_table) {

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

                                goto emulate_next_instruction;

                        }

                        case DD_PREFIX: {

                                registers = state->dd_register_table;

#ifdef Z80_PREFIX_FAILSAFE

                                /* Ensure that at least number_cycles cycles 
                                 * are executed.
                                 */

                                if (elapsed_cycles < number_cycles) {

                                        Z80_FETCH_BYTE(pc, opcode);
                                        pc++;
                                        goto emulate_next_opcode;

                                } else {

					state->status = Z80_STATUS_PREFIX;
                                        pc--;
                                        elapsed_cycles -= 4;
                                        goto stop_emulation;

                                }

#else

                                Z80_FETCH_BYTE(pc, opcode);
                                pc++;
                                goto emulate_next_opcode;

#endif                          

                        }

                        case FD_PREFIX: {

                                registers = state->fd_register_table;

#ifdef Z80_PREFIX_FAILSAFE

                                if (elapsed_cycles < number_cycles) {

                                        Z80_FETCH_BYTE(pc, opcode);
                                        pc++;
                                        goto emulate_next_opcode;

                                } else {

					state->status = Z80_STATUS_PREFIX;
                                        pc--;
                                        elapsed_cycles -= 4;
                                        goto stop_emulation;

                                }

#else
        
                                Z80_FETCH_BYTE(pc, opcode);
                                pc++;
                                goto emulate_next_opcode;

#endif                          

                        }

                        case ED_PREFIX: {

                                registers = state->register_table;
                                Z80_FETCH_BYTE(pc, opcode);
                                pc++;
                                instruction = ED_INSTRUCTION_TABLE[opcode];

                                goto emulate_next_instruction;

                        }

                        /* Special/pseudo instruction group. */

                        case ED_UNDEFINED: {

#ifdef Z80_CATCH_ED_UNDEFINED

                                state->status = Z80_STATUS_FLAG_ED_UNDEFINED;
                                pc -= 2;
                                goto stop_emulation;

#else

                                break;

#endif

                        }

                }

                if (elapsed_cycles >= number_cycles)

                        goto stop_emulation;

        }

stop_emulation:

        state->r = (state->r & 0x80) | (r & 0x7f);
        state->pc = pc & 0xffff;

        return elapsed_cycles;
}
