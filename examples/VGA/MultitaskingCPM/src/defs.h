/*
  Created by Fabrizio Di Vittorio (fdivitto2013@gmail.com) - <http://www.fabgl.com>
  Copyright (c) 2019-2021 Fabrizio Di Vittorio.
  All rights reserved.

  This library and related software is available under GPL v3 or commercial license. It is always free for students, hobbyists, professors and researchers.
  It is not-free if embedded as firmware in commercial boards.


* Contact for commercial license: fdivitto2013@gmail.com


* GPL license version 3, for non-commercial use:

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


#pragma once


#include <stdint.h>


#define HAS_WIFI


// Start of system area
#define SYSTEM_ADDR    0xFD00 - 6   // some programs (catchum!) doesn't like 6 bytes serial number at start of bdos, but wants bdos entry at page start

// BDOS entry
#define BDOS_ENTRY     (SYSTEM_ADDR + 6)
#define BDOS_SIZE      1                        // needs one byte (RET)

// BIOS jump table
#define BIOS_ENTRY     (BDOS_ENTRY + BDOS_SIZE)
#define BIOS_SIZE      33 * 3                   // needs 33 * 3 bytes (JP XXXX)

// returns from BIOS calls
#define BIOS_RETS      (BIOS_ENTRY + BIOS_SIZE) // just 32 "RETs"
#define BIOS_RETS_SIZE 33

// Disc Parameter Block Address (one for all drivers)
#define DPB_ADDR       ((BIOS_RETS + BIOS_RETS_SIZE + 3) & ~3)  // 32 bit aligned (TODO: align required?)
#define DPB_SIZE       17

// Disc Parameter Header (one for all drivers)
#define DPH_ADDR       ((DPB_ADDR + DPB_SIZE + 3) & ~3)         // 32 bit aligned (TODO: align required?)
#define DPH_SIZE       25

// System Control Block
#define SCB_PAGEADDR   ((DPH_ADDR + DPH_SIZE + 255) & 0xFF00)   // page aligned
#define SCB_ADDR       (SCB_PAGEADDR + 0x9C)
#define SCB_SIZE       256

// BDOS temp buffer
#define BDOS_BUFADDR   (SCB_PAGEADDR + SCB_SIZE)
#define BDOS_BUFLEN    128    // minimum is 128 (used by BDOS_deleteFile for alternate DMA)

// chrtbl (physical devices table)
#define CHRTBL_ADDR    (BDOS_BUFADDR + BDOS_BUFLEN)
#define CHRTBL_DEVICES 5  // CP/M allows up to 12 devices (so maximum CHRTBL_SIZE is 12*8=96 bytes)
#define CHRTBL_SIZE    (CHRTBL_DEVICES * 8)


// Default stack
#define STACK_ADDR     SYSTEM_ADDR


// PAGE ZERO fields
#define PAGE0_WSTART         0x0000    // JMP to BIOS warm start
#define PAGE0_WSTARTADDR     0x0001    // address of WSTART function (BIOS+3)
#define PAGE0_IOBYTE         0x0003    // CP/M 2 I/O byte
#define PAGE0_CURDRVUSR      0x0004    // CCP drive (low nibble), CCP user (high nibble)
#define PAGE0_BDOS           0x0005    // JMP to BDOS
#define PAGE0_OSBASE         0x0006    // BDOS address (or first RSX address)
#define PAGE0_IRQ            0x0008    // start of IRQ area
#define PAGE0_LOADDRIVE      0x0050    // drive from which the transient program was loaded (0..15)
#define PAGE0_FCB1PASSADDR_W 0x0051    // absolute address (inside default DMA) of password of first file (or 0x0000 if no password specified)
#define PAGE0_FCB1PASSLEN    0x0053    // length of password specified in PAGE0_FCB1PASSADDR_W
#define PAGE0_FCB2PASSADDR_W 0x0054    // absolute address (inside default DMA) of password of second file (or 0x0000 if no password specified)
#define PAGE0_FCB2PASSLEN    0x0056    // length of password specified in PAGE0_FCB1PASSADDR_W
#define PAGE0_FCB1           0x005C    // default FCB1
#define PAGE0_FCB2           0x006C    // default FCB2
#define PAGE0_DMA            0x0080    // default DMA (and command tail)


// TPA Address
#define TPA_ADDR             0x0100


// SCB fields (_B = byte, _W = word)
#define SCB_BIOSPRINTCALL_3B    -0x1C  // undocumented call to BIOS print (3 bytes)
#define SCB_UNKNOWN1_B          -0x05  // unknown, always 0x07
#define SCB_BDOSBASE_W          -0x04  // undocumented, base address of BDOS
#define SCB_HASHL_B              0x00  // undocumented (Hash length. Can be 0, 2 or 3)
#define SCB_HASHENTRY1_W         0x01  // undocumented
#define SCB_HASH2_W              0x02  // undocumented (as called by ERASE.COM, etc...)
#define SCB_HASHENTRY2_W         0x03  // undocumented
#define SCB_HASH3_W              0x04  // undocumented (as called by ERASE.COM, etc...)
#define SCB_BDOSVERSION_B        0x05
#define SCB_USER1_B              0x06  // user reserved
#define SCB_USER2_B              0x07  // user reserved
#define SCB_USER3_B              0x08  // user reserved
#define SCB_USER4_B              0x09  // user reserved
#define SCB_DATEFORMAT           0x0C  // undocumented: field added by "DATE year 2000 fix", bit 0 and 1 -> 0 = US format, 1 = UK format, 2 = "YMD"
#define SCB_PROGRAMRETCODE_W     0x10
#define SCB_CCPDISK_B            0x13
#define SCB_CCPUSER_B            0x14
#define SCB_CCPFLAGS1_B          0x17  // undocumented
#define SCB_CCPFLAGS2_B          0x18  // undocumented
#define SCB_CCPFLAGS3_B          0x19  // undocumented
#define SCB_CONSOLECOLPOS_B      0x1B
#define SCB_CONSOLEWIDTH_B       0x1A
#define SCB_CONSOLEPAGELENGTH_B  0x1C
#define SCB_REDIRECTIONVECTS_W   0x22  // base of CIVEC, COVEC, etc...
#define SCB_CONINREDIRECT_W      0x22  // CIVEC (Console Input Redirection Vector)
#define SCB_CONOUTREDIRECTDEV_W  0x24  // COVEC (Console Output Redirection Vector)
#define SCB_AUXINREDIRECTDEV_W   0x26  // AIVEC (Auxiliary Input Redirection Vector)
#define SCB_AUXOUTREDIRECTDEV_W  0x28  // AOVEC (Auxiliary Output Redirection Vector)
#define SCB_LSTOUTREDIRECTDEV_W  0x2A  // LOVEC (List Output Redirection Vector)
#define SCB_PAGEMODE_B           0x2C  // 0 = one page at the time, !0 = no stop
#define SCB_DEFAULTPAGEMODE_B    0x2D  // undocumented: default for SCB_PAGEMODE_B
#define SCB_CTRLHMODE_B          0x2E
#define SCB_DELMODE_B            0x2E
#define SCB_CONSOLEMODE_W        0x33
#define SCB_BNKBUF               0x35  // undocumented: address of 128 byte buffer
#define SCB_OUTPUTDELIMETER_B    0x37
#define SCB_LISTOUTPUTFLAG_B     0x38
#define SCB_SCBADDR_W            0x3A  // undocumented (address of this)
#define SCB_CURRENTDMAADDR_W     0x3C
#define SCB_CURRENTDISK_B        0x3E
#define SCB_CURRENTUSER_B        0x44
#define SCB_DCNT_W               0x45  // undocumented (Holds the current directory entry number. Lower two bits are the search return code)
#define SCB_SEARCHA_W            0x47  // undocumented (Holds the FCB address of the last search for first/next operation)
#define SCB_SEARCHL_B            0x49  // undocumented (Search type flag. 0x00 = '?' in drive code search. 0x0F = normal search)
#define SCB_MULTISECTORCOUNT_B   0x4A
#define SCB_ERRORMODE_B          0x4B  // 0xFF = return error, 0xFE = return and display, others = display and terminate program
#define SCB_DRIVESEARCHCHAIN0_B  0x4C
#define SCB_DRIVESEARCHCHAIN1_B  0x4D
#define SCB_DRIVESEARCHCHAIN2_B  0x4E
#define SCB_DRIVESEARCHCHAIN3_B  0x4F
#define SCB_TEMPFILEDRIVE_B      0x50
#define SCB_ERRORDRIVE_B         0x51
#define SCB_BDOSFLAGS_B          0x57
#define SCB_DATEDAYS_W           0x58
#define SCB_HOUR_B               0x5A
#define SCB_MINUTES_B            0x5B
#define SCB_SECONDS_B            0x5C
#define SCB_COMMONBASEADDR_W     0x5D
#define SCB_TOPOFUSERTPA_W       0x62  // BDOS entry (MXTPA)

// bits of SCB_CCPFLAGS1_B
#define SCB_CCPFLAGS1_NULLRSX       0x02   // set when loading a COM with RSXs only
#define SCB_CCPFLAGS1_CHAINCHANGEDU 0x40   // set when drive/user must be changed to value set by the last program executed (when chaining)
#define SCB_CCOFLAGS1_CHAININING    0x80   // indicates to CCP that there is a command to chain to at DMA

// bits of SCB_CCPFLAGS2_B
#define SCB_CCPFLAGS2_CCPPRESENT              (0x20 | 0x80)   // got directly from CCP3.ASM, used calling BDOS 10 to signal function called by CCP
#define SCB_CCPFLAGS2_SUBMIT                  0x40            // "GET" RSX flag (set if GET RSX is redirecting)
#define SCB_CCPFLAGS2_FILESEARCHORDER_BIT     3               // file search order: 0 = (COM), 1 = (COM, SUB), 2 = (SUB, COM)
#define SCB_CCPFLAGS2_FILESEARCHORDER_COM     0
#define SCB_CCPFLAGS2_FILESEARCHORDER_COM_SUB 1
#define SCB_CCPFLAGS2_FILESEARCHORDER_SUB_COM 2

// bits of SCB_CCPFLAGS3_B
#define SCB_CCPFLAGS3_COLDSTART 0x01    // if 0 = cold start   (1 = not cold start)

// bits of SCB_BDOSFLAGS_B
#define SCB_BDOSFLAGS_B_EXPANDEDERRORMSG 0x80


// Console mode bits
#define CONSOLEMODE_FUN11_CTRLC_ONLY   0x01
#define CONSOLEMODE_DISABLE_STOPSCROLL 0x02
#define CONSOLEMODE_RAWCONSOLE_OUTMODE 0x04
#define CONSOLEMODE_DISABLE_CTRLC_EXIT 0x08


// some ASCII codes
#ifndef ASCII_CTRLA
#define ASCII_CTRLC 0x03
#define ASCII_BEL   0x07  // same of ASCII_CTRLG
#define ASCII_CTRLG 0x07  // same of ASCII_BEL
#define ASCII_TAB   0x09  // same of ASCII_CTRLI
#define ASCII_CTRLI 0x09  // same of ASCII_TAB
#define ASCII_LF    0x0A
#define ASCII_CR    0x0D
#define ASCII_CTRLP 0x10
#define ASCII_CTRLQ 0x11
#define ASCII_CTRLS 0x13
#endif


// logical devices
#define LOGICALDEV_CONIN  0    // console input
#define LOGICALDEV_CONOUT 1    // console output
#define LOGICALDEV_AUXIN  2    // aux input
#define LOGICALDEV_AUXOUT 3    // aux output
#define LOGICALDEV_LIST   4    // list (out)


// physical devices as ordered in BIOS.chrtbl
#define PHYSICALDEV_CRT   0    // display
#define PHYSICALDEV_KBD   1    // keyboard
#define PHYSICALDEV_LPT   2    // LPT (printer stream)
#define PHYSICALDEV_UART1 3    // serial 1
#define PHYSICALDEV_UART2 4    // serial 2


// Disc Parameter Block (DPB)
struct DiscParameterBlock {
  uint16_t  spt;  // Number of 128-byte records per track
  uint8_t   bsh;  // Block shift. 3 => 1k, 4 => 2k, 5 => 4k....
  uint8_t   blm;  // Block mask. 7 => 1k, 0Fh => 2k, 1Fh => 4k...
  uint8_t   exm;  // Extent mask, see later
  uint16_t  dsm;  // (no. of blocks on the disc)-1
  uint16_t  drm;  // (no. of directory entries)-1
  uint8_t   al0;  // Directory allocation bitmap, first byte
  uint8_t   al1;  // Directory allocation bitmap, second byte
  uint16_t  cks;  // Checksum vector size, 0 or 8000h for a fixed disc. No. directory entries/4, rounded up.
  uint16_t  off;  // Offset, number of reserved tracks
  uint8_t   psh;  // Physical sector shift, 0 => 128-byte sectors 1 => 256-byte sectors  2 => 512-byte sectors...
  uint8_t   phm;  // Physical sector mask,  0 => 128-byte sectors 1 => 256-byte sectors  3 => 512-byte sectors...
} __attribute__ ((packed));


// Disc Parameter Header
struct DiscParameterHeader {
  uint16_t XLT;
  uint8_t  dummy[9];
  uint8_t  MF;
  uint16_t DPB;
  uint16_t CSV;
  uint16_t ALV;
  uint16_t DIRBCB;
  uint16_t DTABCB;
  uint16_t HASH;
  uint8_t  HBANK;
} __attribute__ ((packed));


struct PhysicalDevice {
  char    name[6];
  uint8_t flags;    // see PHYSICALDEVICE_FLAG_...
  uint8_t baud;     // see PHYSICALDEVICE_BAUD_...
} __attribute__ ((packed));


// values for PhysicalDevice.flags
#define PHYSICALDEVICE_FLAG_INPUT           1    // device may do input
#define PHYSICALDEVICE_FLAG_OUTPUT          2    // device may do output
#define PHYSICALDEVICE_FLAG_INOUT  (PHYSICALDEVICE_FLAG_INPUT | PHYSICALDEVICE_FLAG_OUTPUT) // device may do both
#define PHYSICALDEVICE_FLAG_SOFTBAUD        4    // software selectable baud rates
#define PHYSICALDEVICE_FLAG_SERIAL          8    // device is serial
#define PHYSICALDEVICE_FLAG_SERIAL_XONXOFF  16   // XON/XOFF protocol enabled


// values for PhysicalDevice.baud
#define PHYSICALDEVICE_BAUD_NONE            0    // no baud rate associated with device
#define PHYSICALDEVICE_BAUD_50               1   // 50 baud
#define PHYSICALDEVICE_BAUD_75               2   // 75 baud
#define PHYSICALDEVICE_BAUD_110              3   // 110 baud
#define PHYSICALDEVICE_BAUD_134              4   // 134.5 baud
#define PHYSICALDEVICE_BAUD_150              5   // 150 baud
#define PHYSICALDEVICE_BAUD_300              6   // 300 baud
#define PHYSICALDEVICE_BAUD_600              7   // 600 baud
#define PHYSICALDEVICE_BAUD_1200             8   // 1200 baud
#define PHYSICALDEVICE_BAUD_1800             9   // 1800 baud
#define PHYSICALDEVICE_BAUD_2400            10   // 2400 baud
#define PHYSICALDEVICE_BAUD_3600            11   // 3600 baud
#define PHYSICALDEVICE_BAUD_4800            12   // 4800 baud
#define PHYSICALDEVICE_BAUD_7200            13   // 7200 baud
#define PHYSICALDEVICE_BAUD_9600            14   // 9600 baud
#define PHYSICALDEVICE_BAUD_19200           15   // 19.2k baud



#define DIRLABEL_FILENAME ".dirlabel"


// does not include fields handled directly by BDOS_getSetSystemControlBlock, because they aren't updated if accessed directly from memory
inline bool isSupportedSCBField(int field)
{
  switch (field) {
    case SCB_CONSOLEMODE_W:
    case SCB_CONSOLEMODE_W + 1:
    case SCB_COMMONBASEADDR_W:
    case SCB_COMMONBASEADDR_W + 1:
    case SCB_PROGRAMRETCODE_W:
    case SCB_PROGRAMRETCODE_W + 1:
    case SCB_SCBADDR_W:
    case SCB_SCBADDR_W + 1:
    case SCB_PAGEMODE_B:
    case SCB_DCNT_W:
    case SCB_DCNT_W + 1:
    case SCB_SEARCHA_W:
    case SCB_SEARCHA_W + 1:
    case SCB_SEARCHL_B:
    case SCB_HASHL_B:
    case SCB_HASHENTRY1_W:
    case SCB_HASHENTRY1_W + 1:
    case SCB_HASHENTRY2_W:
    case SCB_HASHENTRY2_W + 1:
    case SCB_CCPFLAGS1_B:
    case SCB_CCPFLAGS2_B:
    case SCB_CCPDISK_B:
    case SCB_CCPUSER_B:
    case SCB_CURRENTDISK_B:
    case SCB_CURRENTUSER_B:
    case SCB_OUTPUTDELIMETER_B:
    case SCB_BDOSVERSION_B:
    case SCB_DRIVESEARCHCHAIN0_B:
    case SCB_DRIVESEARCHCHAIN1_B:
    case SCB_DRIVESEARCHCHAIN2_B:
    case SCB_DRIVESEARCHCHAIN3_B:
    case SCB_MULTISECTORCOUNT_B:
    case SCB_ERRORMODE_B:
    case SCB_USER1_B:
    case SCB_USER2_B:
    case SCB_USER3_B:
    case SCB_USER4_B:
    case SCB_CONINREDIRECT_W:
    case SCB_CONINREDIRECT_W + 1:
    case SCB_CONOUTREDIRECTDEV_W:
    case SCB_CONOUTREDIRECTDEV_W + 1:
    case SCB_AUXINREDIRECTDEV_W:
    case SCB_AUXINREDIRECTDEV_W + 1:
    case SCB_AUXOUTREDIRECTDEV_W:
    case SCB_AUXOUTREDIRECTDEV_W + 1:
    case SCB_LSTOUTREDIRECTDEV_W:
    case SCB_LSTOUTREDIRECTDEV_W + 1:
    case SCB_CONSOLEWIDTH_B:
    case SCB_CONSOLEPAGELENGTH_B:
    case SCB_DATEDAYS_W:
    case SCB_DATEDAYS_W + 1:
    case SCB_HOUR_B:
    case SCB_MINUTES_B:
    case SCB_SECONDS_B:
    case SCB_DATEFORMAT:
    case SCB_BIOSPRINTCALL_3B:
    case SCB_BIOSPRINTCALL_3B + 1:
    case SCB_BIOSPRINTCALL_3B + 2:
    case SCB_UNKNOWN1_B:
    case SCB_BDOSBASE_W:
    case SCB_BDOSBASE_W + 1:
    case SCB_DEFAULTPAGEMODE_B:
    case SCB_BNKBUF:
    case SCB_BNKBUF + 1:
    case SCB_ERRORDRIVE_B:
    case SCB_TOPOFUSERTPA_W:
    case SCB_TOPOFUSERTPA_W + 1:
    case SCB_CURRENTDMAADDR_W:
    case SCB_CURRENTDMAADDR_W + 1:
    case SCB_CCPFLAGS3_B:
      return true;
    default:
      return false;
  }
}

