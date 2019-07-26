/* z80config.h 
 * Define or comment out macros in this file to configure the emulator. 
 *
 * Copyright (c) 2016, 2017 Lin Ke-Fong
 *
 * This code is free, do whatever you want with it.
 */

#ifndef __Z80CONFIG_INCLUDED__
#define __Z80CONFIG_INCLUDED__

/* Define this macro if the host processor is big endian. */

/* #define Z80_BIG_ENDIAN */

/* Emulation can be speed up a little bit by emulating only the documented
 * flags.
 */     

/* #define Z80_DOCUMENTED_FLAGS_ONLY */

/* HALT, DI, EI, RETI, and RETN instructions can be catched. When such an
 * instruction is catched, the emulator is stopped and the PC register points
 * at the opcode to be executed next. The catched instruction can be determined
 * from the Z80_STATE's status value. Keep in mind that no interrupt can be 
 * accepted at the instruction right after a DI or EI on an actual processor. 
 */

/*      
#define Z80_CATCH_HALT
#define Z80_CATCH_DI
#define Z80_CATCH_EI
#define Z80_CATCH_RETI
#define Z80_CATCH_RETN
*/

/* Undefined 0xed prefixed opcodes may be catched, otherwise they are treated
 * like NOP instructions. When one is catched, Z80_STATUS_ED_UNDEFINED is set 
 * in Z80_STATE's status member and the PC register points at the 0xed prefix 
 * before the undefined opcode.
 */

/* #define Z80_CATCH_ED_UNDEFINED */

/* The emulator cannot be stopped between prefixed opcodes. This can be a 
 * problem if there is a long sequence of 0xdd and/or 0xfd prefixes. But if
 * Z80_PREFIX_FAILSAFE is defined, it will always be able to stop after at 
 * least numbers_cycles are executed, in which case Z80_STATE's status is set 
 * to Z80_STATUS_PREFIX. Note that if the memory where the opcodes are read, 
 * has wait states (slow memory), then the additional cycles for a one byte 
 * fetch (the non executed prefix) must be substracted. Even if it is safer, 
 * most program won't need this feature.
 */

/* #define Z80_PREFIX_FAILSAFE */
     
/* By defining this macro, the emulator will always fetch the displacement or 
 * address of a conditionnal jump or call instruction, even if the condition 
 * is false and the fetch can be avoided. Define this macro if you need to 
 * account for memory wait states on code read.
 */

/* #define Z80_FALSE_CONDITION_FETCH */

/* It may be possible to overwrite the opcode of the currently executing LDIR, 
 * LDDR, INIR, or OTDR instruction. Define this macro if you need to handle 
 * these pathological cases.
 */

/* #define Z80_HANDLE_SELF_MODIFYING_CODE */

/* For interrupt mode 2, bit 0 of the 16-bit address to the interrupt vector 
 * can be masked to zero. Some documentation states that this bit is forced to 
 * zero. For instance, Zilog's application note about interrupts, states that
 * "only 7 bits are required" and "the least significant bit is zero". Yet, 
 * this is quite unclear, even from Zilog's manuals. So this is left as an 
 * option.
 */

/* #define Z80_MASK_IM2_VECTOR_ADDRESS */

#endif
