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
//
//
// 2020 adapted by Fabrizio Di Vittorio for fabgl ESP32 library


#pragma once


/**
 * @file
 *
 * @brief This file contains fabgl::i8080 definition.
 */



#include <stdint.h>


namespace fabgl {




/**
 * @brief Intel 8080 CPU emulator
 */
class i8080 {

  typedef union {
    struct {
      uint8_t l, h;
    } b;
    uint16_t w;
  } reg_pair;


  typedef struct {
    uint8_t carry_flag;
    uint8_t unused1;
    uint8_t parity_flag;
    uint8_t unused3;
    uint8_t half_carry_flag;
    uint8_t unused5;
    uint8_t zero_flag;
    uint8_t sign_flag;
  } flag_reg;


  struct Regs {
    flag_reg f;
    reg_pair af, bc, de, hl;
    reg_pair sp, pc;
    uint16_t iff;
    uint16_t last_pc;
  };

public:


  // callbacks
  typedef int  (*ReadByteCallback)(void * context, int addr);
  typedef void (*WriteByteCallback)(void * context, int addr, int value);
  typedef int  (*ReadWordCallback)(void * context, int addr);
  typedef void (*WriteWordCallback)(void * context, int addr, int value);
  typedef int  (*ReadIOCallback)(void * context, int addr);
  typedef void (*WriteIOCallback)(void * context, int addr, int value);


  void setCallbacks(void * context, ReadByteCallback readByte, WriteByteCallback writeByte, ReadWordCallback readWord, WriteWordCallback writeWord, ReadIOCallback readIO, WriteIOCallback writeIO) {
    m_context   = context;
    m_readByte  = readByte;
    m_writeByte = writeByte;
    m_readWord  = readWord;
    m_writeWord = writeWord;
    m_readIO    = readIO;
    m_writeIO   = writeIO;
  }

  void reset();
  int step();

  void setPC(int addr)  { cpu.pc.w = addr & 0xffff; }
  int getPC()           { return cpu.pc.w; }

  int regs_bc()  { return cpu.bc.w; }
  int regs_de()  { return cpu.de.w; }
  int regs_hl()  { return cpu.hl.w; }
  int regs_sp()  { return cpu.sp.w; }

  int regs_a()   { return cpu.af.b.h;  }
  int regs_b()   { return cpu.bc.b.h;  }
  int regs_c()   { return cpu.bc.b.l;  }
  int regs_d()   { return cpu.de.b.h;  }
  int regs_e()   { return cpu.de.b.l;  }
  int regs_h()   { return cpu.hl.b.h;  }
  int regs_l()   { return cpu.hl.b.l;  }


private:

  void store_flags();
  void retrieve_flags();


  Regs     cpu;

  uint32_t work32;
  uint16_t work16;
  uint8_t  work8;
  int      index;
  uint8_t  carry;
  uint8_t  add;


  // callbacks

  void *             m_context;

  ReadByteCallback   m_readByte;
  WriteByteCallback  m_writeByte;
  ReadWordCallback   m_readWord;
  WriteWordCallback  m_writeWord;
  ReadIOCallback     m_readIO;
  WriteIOCallback    m_writeIO;

};


};  // fabgl namespace

