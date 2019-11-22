// Intel 8080 (KR580VM80A) microprocessor core model
//
// Copyright (C) 2012 Alexander Demin <alexander@demin.ws>
//
// Credits
//
// Viacheslav Slavinsky, Vector-06C FPGA Replica
// http://code.google.com/p/vector06cc/
//
// Dmitry Tselikov, Bashrikia-2M and Radio-86RK on Altera DE1
// http://bashkiria-2m.narod.ru/fpga.html
//
// Ian Bartholomew, 8080/8085 CPU Exerciser
// http://www.idb.me.uk/sunhillow/8080.html
//
// Frank Cringle, The origianal exerciser for the Z80.
//
// Thanks to zx.pk.ru and nedopc.org/forum communities.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or (at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

#ifndef I8080_H
#define I8080_H

#include "esp_attr.h"

void i8080_init(void * context);
IRAM_ATTR int i8080_instruction(void);

void i8080_jump(int addr);
int i8080_pc(void);

extern int i8080_regs_bc(void);
extern int i8080_regs_de(void);
extern int i8080_regs_hl(void);
extern int i8080_regs_sp(void);

extern int i8080_regs_a(void);
extern int i8080_regs_b(void);
extern int i8080_regs_c(void);
extern int i8080_regs_d(void);
extern int i8080_regs_e(void);
extern int i8080_regs_h(void);
extern int i8080_regs_l(void);



// implemented by app

extern int i8080_hal_memory_read_word(void * context, int addr);
extern void i8080_hal_memory_write_word(void * context, int addr, int word);

extern int i8080_hal_memory_read_byte(void * context, int addr);
extern void i8080_hal_memory_write_byte(void * context, int addr, int byte);

extern int i8080_hal_io_input(void * context, int port);
extern void i8080_hal_io_output(void * context, int port, int value);

extern void i8080_hal_iff(void * context, int on);



#endif
