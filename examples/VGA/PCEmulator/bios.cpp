/*
  Created by Fabrizio Di Vittorio (fdivitto2013@gmail.com) - <http://www.fabgl.com>
  Copyright (c) 2019-2022 Fabrizio Di Vittorio.
  All rights reserved.


* Please contact fdivitto2013@gmail.com if you need a commercial license.


* This library and related software is available under GPL v3.

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



#include "bios.h"
#include "machine.h"
#include "emudevs/i8086.h"


using fabgl::i8086;


// To compile BIOS execute "cbios.sh"
static const uint8_t biosrom[] = {
  #include "biosrom.h"
};


BIOS::BIOS()
  : m_memory(nullptr)
{
}


void BIOS::init(Machine * machine)
{
  m_machine   = machine;
  m_memory    = m_machine->memory();
  m_i8042     = m_machine->getI8042();
  m_keyboard  = m_i8042->keyboard();
  m_mouse     = m_i8042->mouse();
  m_MC146818  = m_machine->getMC146818();

	// copy bios
  memcpy(m_memory + BIOS_ADDR, biosrom, sizeof(biosrom));

  // setup bootstrap code (starting from 0xFFFF0)
  // for example: "JMP FC00:0100"
  m_memory[0xffff0] = 0xea;
  m_memory[0xffff1] = BIOS_OFF & 0xff;
  m_memory[0xffff2] = BIOS_OFF >> 8;
  m_memory[0xffff3] = BIOS_SEG & 0xff;
  m_memory[0xffff4] = BIOS_SEG >> 8;
}


void BIOS::reset()
{
  m_kbdScancodeComp = 0;
  m_memory[BIOS_DATAAREA_ADDR + BIOS_NUMHD] = (bool)(m_machine->disk(2)) + (bool)(m_machine->disk(3));
}


// drive:
//   0 = floppy 0 (get address from INT 1E)
//   1 = floppy 1 (get address from INT 1E)
//   2 = HD 0     (get address from INT 41)
//   3 = HD 1     (get address from INT 46)
uint32_t BIOS::getDriveMediaTableAddr(int drive)
{
  int intNum = drive < 2 ? 0x1e : (drive == 2 ? 0x41 : 0x46);
  uint16_t * intAddr = (uint16_t*)(m_memory + intNum * 4);
  return intAddr[0] + intAddr[1] * 16;
}


bool BIOS::checkDriveMediaType(int drive)
{
  if (drive < 2) {
    // FDD
    if (m_mediaType[drive] == mediaUnknown) {
      MediaType mt = mediaUnknown;
      int h = m_machine->diskHeads(drive);
      int t = m_machine->diskCylinders(drive);
      int s = m_machine->diskSectors(drive);
      if (h == 1 && t == 40 && s == 8)
        mt = floppy160KB;
      else if (h == 1 && t == 40 && s == 9)
        mt = floppy180KB;
      else if (h == 2 && t == 40 && s == 8)
        mt = floppy320KB;
      else if (h == 2 && t == 40 && s == 9)
        mt = floppy360KB;
      else if (h == 2 && t == 80 && s == 9)
        mt = floppy720KB;
      else if (h == 2 && t == 80 && s == 15)
        mt = floppy1M2K;
      else if (h == 2 && t == 80 && s == 18)
        mt = floppy1M44K;
      else if (h == 2 && t == 80 && s == 36)
        mt = floppy2M88K;
      setDriveMediaType(drive, mt);
    }
  } else {
    // HDD
    if (m_machine->disk(drive))
      setDriveMediaType(drive, HDD);
  }
  return m_mediaType[drive] != mediaUnknown;
}


void BIOS::setDriveMediaType(int drive, MediaType media)
{
  m_mediaType[drive] = media;

  if (drive < 2) {
    // FDD

    // updates BIOS data area
    uint8_t knownMedia     = 0x10;        // default set bit 4 (known media)
    uint8_t doubleStepping = 0x00;        // reset bit 5 (double stepping)
    uint8_t dataRate       = 0x00;
    uint8_t defs           = 0x00;
    switch (media) {
      case floppy160KB:
      case floppy180KB:
      case floppy320KB:
      case floppy360KB:
        doubleStepping     = 0x20;        // set bit 5 (double stepping)
        dataRate           = 0b01000000;  // 300 KBS
        defs               = 0b00000100;  // Known 360K media in 1.2MB drive
        break;
      case floppy720KB:
        dataRate           = 0b10000000;  // 250 KBS
        defs               = 0b00000111;  // 720K media in 720K drive or 1.44MB media in 1.44MB drive
        break;
      case floppy1M2K:
        dataRate           = 0b00000000;  // 500 KBS
        defs               = 0b00000101;  // Known 1.2MB media in 1.2MB drive
        break;
      case floppy1M44K:
        dataRate           = 0b00000000;  // 500 KBS
        defs               = 0b00000111;  // 720K media in 720K drive or 1.44MB media in 1.44MB drive
        break;
      case floppy2M88K:
        dataRate           = 0b11000000;  // 1 MBS
        defs               = 0b00000111;  // right?
        break;
      case mediaUnknown:
      default:
        knownMedia         = 0x00;        // reset bit 4 (known media)
        break;
    }
    if (m_memory && drive < 2) {
      // BIOS data area
      m_memory[BIOS_DATAAREA_ADDR + BIOS_DRIVE0MEDIATYPE + drive] = knownMedia | doubleStepping | dataRate | defs;
      //printf("%05X = %02X\n", BIOS_DATAAREA_ADDR + BIOS_DRIVE0MEDIATYPE + drive, m_memory[BIOS_DATAAREA_ADDR + BIOS_DRIVE0MEDIATYPE + drive]);

      // INT 1E
      uint32_t maddr = getDriveMediaTableAddr(drive);
      m_memory[maddr + 0x04] = m_machine->diskSectors(drive);

      // original INT 1E (returned in ES:DI, int 13h, serv 08h)
      m_memory[m_origInt1EAddr + 0x04] = m_machine->diskSectors(drive);
      m_memory[m_origInt1EAddr + 0x0b] = m_machine->diskCylinders(drive) - 1;
    }

  } else if (media == HDD) {

    // HDD

    // fill tables pointed by INT 41h or 46h
    uint32_t mtableAddr = getDriveMediaTableAddr(drive);
    *(uint16_t*)(m_memory + mtableAddr + 0x00) = m_machine->diskCylinders(drive);
    *(uint8_t*)(m_memory + mtableAddr + 0x02)  = m_machine->diskHeads(drive);
    *(uint8_t*)(m_memory + mtableAddr + 0x0e)  = m_machine->diskSectors(drive);

  }
}


// AH = select the helper function
void BIOS::helpersEntry()
{
  switch (i8086::AH()) {

    // AH = 0x00, perform some INT 9 tasks (keyboard interrupt handler)
    case 0x00:
      getKeyFromKeyboard();
      break;

    // AH = 0x01, get or extract key from keyboard buffer
    case 0x01:
      getKeyFromBuffer();
      break;

    // AH = 0x02, get shift flags or extended shift flags
    case 0x02:
      getKeyboardFlags();
      break;

    // AH = 0x03, set keyboard typematic rate and delay
    case 0x03:
      setKeyboardTypematicAndDelay();
      break;

    // AH = 0x05, store keyboard key data
    case 0x05:
      storeKeyboardKeyData();
      break;

    // AH = 0x06, pointing device interface
    case 0x06:
      pointingDeviceInterface();
      break;

    // AH = 0x07, synchronize system ticks with RTC
    case 0x07:
      syncTicksWithRTC();
      break;

    // AH = 0x08, set media drive parameters tables address
    //   ES:BX = int 1Eh address (in bios.asm) for floppy drivers
    case 0x08:
      m_origInt1EAddr = i8086::ES() * 16 + i8086::BX();
      // set initial media
      for (int i = 0; i < DISKCOUNT; ++i)
        setDriveMediaType(i, mediaUnknown);
      break;

    default:
      break;

  };
}



// convert keypad scancode to number, or -1 if not applicable
int convKeypadScancodeToNum(uint8_t scancode)
{
  // LUT to convert scancode 0x47 to 0x52
  static const int8_t CONV[12] = { 7, 8, 9, -1, 4, 5, 6, -1, 1, 2, 3, 0 };
  return scancode >= 0x47 && scancode <= 0x52 ? CONV[scancode - 0x47] : -1;
}


// - update keyboard shift flags
// - adapt fabgl scancode to PC scancodes
// ret: true if the key need to be inserted into the keyboard buffer
bool BIOS::processScancode(int scancode, uint16_t * syscode)
{
  struct ScanCode2SysCode {
    uint8_t  scancode;
    uint16_t syscode[4];    // 0 = normal, 1 = shifted, 2 = control, 3 = alt
  };
  static const ScanCode2SysCode SCODE2SYSCODE[] = {
    { 0x29, 0x2960, 0x297e, 0xffff, 0x2900 }, { 0x02, 0x0231, 0x0221, 0xffff, 0x7800 }, { 0x03, 0x0332, 0x0340, 0x0300, 0x7900 },
    { 0x04, 0x0433, 0x0423, 0xffff, 0x7a00 }, { 0x05, 0x0534, 0x0524, 0xffff, 0x7b00 }, { 0x06, 0x0635, 0x0625, 0xffff, 0x7c00 },
    { 0x07, 0x0736, 0x075e, 0x071e, 0x7d00 }, { 0x08, 0x0837, 0x0826, 0xffff, 0x7e00 }, { 0x09, 0x0938, 0x092a, 0xffff, 0x7f00 },
    { 0x0a, 0x0a39, 0x0a28, 0xffff, 0x8000 }, { 0x0b, 0x0b30, 0x0b29, 0xffff, 0x8100 }, { 0x0c, 0x0c2d, 0x0c5f, 0x0c1f, 0x8200 },
    { 0x0d, 0x0d3d, 0x0d2b, 0xffff, 0x8300 }, { 0x0e, 0x0e08, 0x0e08, 0x0e7f, 0x0e00 }, { 0x0f, 0x0f09, 0x0f00, 0x9400, 0xa500 },
    { 0x10, 0x1071, 0x1051, 0x1011, 0x1000 }, { 0x11, 0x1177, 0x1157, 0x1117, 0x1100 }, { 0x12, 0x1265, 0x1245, 0x1205, 0x1200 },
    { 0x13, 0x1372, 0x1352, 0x1312, 0x1300 }, { 0x14, 0x1474, 0x1454, 0x1414, 0x1400 }, { 0x15, 0x1579, 0x1559, 0x1519, 0x1500 },
    { 0x16, 0x1675, 0x1655, 0x1615, 0x1600 }, { 0x17, 0x1769, 0x1749, 0x1709, 0x1700 }, { 0x18, 0x186f, 0x184f, 0x180f, 0x1800 },
    { 0x19, 0x1970, 0x1950, 0x1910, 0x1900 }, { 0x1a, 0x1a5b, 0x1a7b, 0x1a1b, 0x1a00 }, { 0x1b, 0x1b5d, 0x1b7d, 0x1b1d, 0x1b00 },
    { 0x2b, 0x2b5c, 0x2b7c, 0x2b1c, 0x2b00 }, { 0x1e, 0x1e61, 0x1e41, 0x1e01, 0x1e00 }, { 0x1f, 0x1f73, 0x1f53, 0x1f13, 0x1f00 },
    { 0x20, 0x2064, 0x2044, 0x2004, 0x2000 }, { 0x21, 0x2166, 0x2146, 0x2106, 0x2100 }, { 0x22, 0x2267, 0x2247, 0x2207, 0x2200 },
    { 0x23, 0x2368, 0x2348, 0x2308, 0x2300 }, { 0x24, 0x246a, 0x244a, 0x240a, 0x2400 }, { 0x25, 0x256b, 0x254b, 0x250b, 0x2500 },
    { 0x26, 0x266c, 0x264c, 0x260c, 0x2600 }, { 0x27, 0x273b, 0x273a, 0xffff, 0x2700 }, { 0x28, 0x2827, 0x2822, 0xffff, 0x2800 },
    { 0x2b, 0x2b5c, 0x2b7c, 0x2b1c, 0xffff }, { 0x1c, 0x1c0d, 0x1c0d, 0x1c0a, 0x1c00 }, { 0x56, 0x565c, 0x567c, 0xffff, 0xffff },
    { 0x2c, 0x2c7a, 0x2c5a, 0x2c1a, 0x2c00 }, { 0x2d, 0x2d78, 0x2d58, 0x2d18, 0x2d00 }, { 0x2e, 0x2e63, 0x2e43, 0x2e03, 0x2e00 },
    { 0x2f, 0x2f76, 0x2f56, 0x2f16, 0x2f00 }, { 0x30, 0x3062, 0x3042, 0x3002, 0x3000 }, { 0x31, 0x316e, 0x314e, 0x310e, 0x3100 },
    { 0x32, 0x326d, 0x324d, 0x320d, 0x3200 }, { 0x33, 0x332c, 0x333c, 0xffff, 0x3300 }, { 0x34, 0x342e, 0x343e, 0xffff, 0x3400 },
    { 0x35, 0x352f, 0x353f, 0xffff, 0x3500 }, { 0x39, 0x3920, 0x3920, 0x3920, 0x3920 }, { 0x47, 0x4700, 0x4737, 0x7700, 0xffff },
    { 0x4b, 0x4b00, 0x4b34, 0x7300, 0xffff }, { 0x4f, 0x4f00, 0x4f31, 0x7500, 0xffff }, { 0x48, 0x4800, 0x4838, 0x8d00, 0xffff },
    { 0x4c, 0x4c00, 0x4c35, 0x8f00, 0xffff }, { 0x50, 0x5000, 0x5032, 0x9100, 0xffff }, { 0x52, 0x5200, 0x5230, 0x9200, 0xffff },
    { 0x37, 0x372a, 0x372a, 0x9600, 0x3700 }, { 0x49, 0x4900, 0x4939, 0x8400, 0xffff }, { 0x4d, 0x4d00, 0x4d36, 0x7400, 0xffff },
    { 0x51, 0x5100, 0x5133, 0x7600, 0xffff }, { 0x53, 0x5300, 0x532e, 0x9300, 0xffff }, { 0x4a, 0x4a2d, 0x4a2d, 0x8e00, 0x4a00 },
    { 0x4e, 0x4e2b, 0x4e2b, 0x9000, 0x4e00 }, { 0x01, 0x011b, 0x011b, 0x011b, 0x0100 }, { 0x3b, 0x3b00, 0x5400, 0x5e00, 0x6800 },
    { 0x3c, 0x3c00, 0x5500, 0x5f00, 0x6900 }, { 0x3d, 0x3d00, 0x5600, 0x6000, 0x6a00 }, { 0x3e, 0x3e00, 0x5700, 0x6100, 0x6b00 },
    { 0x3f, 0x3f00, 0x5800, 0x6200, 0x6c00 }, { 0x40, 0x4000, 0x5900, 0x6300, 0x6d00 }, { 0x41, 0x4100, 0x5a00, 0x6400, 0x6e00 },
    { 0x42, 0x4200, 0x5b00, 0x6500, 0x6f00 }, { 0x43, 0x4300, 0x5c00, 0x6600, 0x7000 }, { 0x44, 0x4400, 0x5d00, 0x6700, 0x7100 },
    { 0x57, 0x8500, 0x8700, 0x8900, 0x8b00 }, { 0x58, 0x8600, 0x8800, 0x8a00, 0x8c00 },
    { 0x00, 0xffff, 0xffff, 0xffff, 0xffff } // ending code
  };
  static const ScanCode2SysCode ESCODE2SYSCODE[] = {
    { 0x52, 0x52e0, 0x52e0, 0x92e0, 0xa200 }, { 0x53, 0x53e0, 0x53e0, 0x93e0, 0xa300 }, { 0x4b, 0x4be0, 0x4be0, 0x73e0, 0x9b00 },
    { 0x47, 0x47e0, 0x47e0, 0x77e0, 0x9700 }, { 0x4f, 0x4fe0, 0x4fe0, 0x75e0, 0x9f00 }, { 0x48, 0x48e0, 0x48e0, 0x8de0, 0x9800 },
    { 0x50, 0x50e0, 0x50e0, 0x91e0, 0xa000 }, { 0x49, 0x49e0, 0x49e0, 0x84e0, 0x9900 }, { 0x51, 0x51e0, 0x51e0, 0x76e0, 0xa100 },
    { 0x4d, 0x4de0, 0x4de0, 0x74e0, 0x9d00 }, { 0x35, 0xe02f, 0xe02f, 0x9500, 0xa400 }, { 0x1c, 0xe00d, 0xe00d, 0xe00a, 0xa600 },
    { 0x37, 0xffff, 0xffff, 0x7200, 0xffff }, // CTRL + PRINTSCREEN
    { 0x46, 0xffff, 0xffff, 0x0000, 0xffff }, // CTRL + PAUSE (BREAK)
    { 0x00, 0xffff, 0xffff, 0xffff, 0xffff }  // ending code
  };

  //printf("  %02X\n", scancode);

  *syscode = 0xffff;

  // 3 = RALT, 2 = RCTRL, 1 = E0, 0 = E1
  uint8_t * mode = m_memory + BIOS_DATAAREA_ADDR + BIOS_KBDMODE;

  // save and reset e0 and e1 flags
  bool e0 = *mode & 0x02;
  bool e1 = *mode & 0x01;
  *mode &= 0xfc;

  // e0?
  if (scancode == 0xe0) {
    *mode |= 0x02;
    return false;
  }

  // e1?
  if (scancode == 0xe1) {
    *mode |= 0x01;
    return false;
  }

  bool down = !(scancode & 0x80); // down if bit 7 = 0
  scancode &= 0x7f;

  //printf("  e0 = %d, down = %d\n", e0, down);

  // 7 = INS ON, 6 = CAPS ON, 5 = NUMLCK ON, 4 = SCRLCK ON, 3 = ALT, 2 = CTRL, 1 = LSHIFT, 0 = RSHIFT
  uint8_t * flags1 = m_memory + BIOS_DATAAREA_ADDR + BIOS_KBDSHIFTFLAGS1;

  // 7 = INS, 6 = CAPS, 5 = NUMLCK, 4 = SCRLCK, 3 = CTRL+NUMLCK ON (PAUSE), 2 = SYSREQ, 1 = LALT, 0 = LCTRL
  uint8_t * flags2 = m_memory + BIOS_DATAAREA_ADDR + BIOS_KBDSHIFTFLAGS2;

  // 2 = CAPS LED, 1 = NUMLCK LED, 0 = SCRLCK LED
  uint8_t * LEDs   = m_memory + BIOS_DATAAREA_ADDR + BIOS_KBDLEDS;

  uint8_t * altKeypadEntry = m_memory + BIOS_DATAAREA_ADDR + BIOS_KBDALTKEYPADENTRY;

  if (e0) {
    // extended code (0xe0 ...)
    switch (scancode) {
      // RCTRL
      case 0x1d:
        *flags1  = (*flags1 & ~0x04) | (0x04 * down);  // bit 2 on flags1 (1 = down)
        *mode    = (*mode   & ~0x04) | (0x04 * down);  // bit 2 on mode   (1 = down)
        break;
      // RALT
      case 0x38:
        *flags1  = (*flags1 & ~0x08) | (0x08 * down);  // bit 3 on flags1 (1 = down)
        *mode    = (*mode   & ~0x08) | (0x08 * down);  // bit 3 on mode   (1 = down)
        break;
      // INSERT
      case 0x52:
        *flags1 ^= 0x80 * !down;                       // bit 7 on flags1 (toggle when up)
        *flags2  = (*flags2 & ~0x80) | (0x80 * down);  // bit 7 on flags2 (1 = down)
        break;
      // PRINTSCREEN or SYSREQ
      case 0x37:
        // not shifts, PRINTSCREEN
        if (down && (*flags1 & 0x0f) == 0)
          m_memory[BIOS_DATAAREA_ADDR + BIOS_PRINTSCREENFLAG] = 1;
        // ALT + PRINTSCREEN = SYSREQ
        else if (*flags1 & 0x08)
          *flags2 |= 0x04;
        break;
      // CTRL + BREAK (CTRL + PAUSE)
      case 0x46:
        emptyKbdBuffer();
        m_memory[BIOS_DATAAREA_ADDR + BIOS_CTRLBREAKFLAG] = 0x80;
        break;
      // bypass (e0 2a / e0 aa)
      case 0x2a:
        return false;
    }
  } else if (e1 || m_kbdScancodeComp > 0) {
    // extended code (0xe1 ...)
    if ((m_kbdScancodeComp == 0 && scancode == 0x1d) || (m_kbdScancodeComp == 1 && scancode == 0x45) || (m_kbdScancodeComp == 2 && scancode == 0x1d)) {
      ++m_kbdScancodeComp;
      return false;
    } else if (m_kbdScancodeComp == 3 && scancode == 0x45) {
      // PAUSE key completed (e1 1d 45 e1 9d c5)
      *flags2 |= 0x08;                                 // bit 3 on flags2 (always set)
      m_kbdScancodeComp = 0;
      return false;
    }
    m_kbdScancodeComp = 0;
  } else {
    // normal code
    m_kbdScancodeComp = 0;
    switch (scancode) {
      // LALT
      case 0x38:
        *flags1  = (*flags1 & ~0x08) | (0x08 * down);  // bit 3 on flags1 (1 = down)
        *flags2  = (*flags2 & ~0x02) | (0x02 * down);  // bit 1 on flags2 (1 = down)
        break;
      // LSHIFT
      case 0x2a:
        *flags1  = (*flags1 & ~0x02) | (0x02 * down);  // bit 1 on flags1 (1 = down)
        break;
      // RSHIFT
      case 0x36:
        *flags1  = (*flags1 & ~0x01) | (0x01 * down);  // bit 0 on flags1 (1 = down)
        break;
      // LCTRL
      case 0x1d:
        *flags1  = (*flags1 & ~0x04) | (0x04 * down);  // bit 2 on flags1 (1 = down)
        *flags2  = (*flags2 & ~0x01) | (0x01 * down);  // bit 0 on flags2 (1 = down)
        break;
      // SCROLLLOCK
      case 0x46:
        *flags1 ^= 0x10 * !down;                       // bit 4 on flags1 (toggle when up)
        *flags2  = (*flags2 & ~0x10) | (0x10 * down);  // bit 4 on flags2 (1 = down)
        *LEDs   ^= 0x01 * !down;                       // bit 0 on LEDs   (toggle when up)
        break;
      // NUMLOCK
      case 0x45:
        *flags1 ^= 0x20 * !down;                       // bit 5 on flags1 (toggle when up)
        *flags2  = (*flags2 & ~0x20) | (0x20 * down);  // bit 5 on flags2 (1 = down)
        *LEDs   ^= 0x02 * !down;                       // bit 1 on LEDs   (toggle when up)
        break;
      // CAPSLOCK
      case 0x3a:
        *flags1 ^= 0x40 * !down;                       // bit 6 on flags1 (toggle when up)
        *flags2  = (*flags2 & ~0x40) | (0x40 * down);  // bit 6 on flags2 (1 = down)
        *LEDs   ^= 0x04 * !down;                       // bit 2 on LEDs   (toggle when up)
        break;
      // KEYPAD INS (KEYPAD 0)
      case 0x52:
        // NUMLOCK = off ?, interpret keypad0 as INSERT toggle
        if ((*flags1 & 0x20) == 0)
          *flags1 ^= 0x80 * !down;                     // bit 7 on flags1 (toggle when up)
        break;
    }
  }

  bool lalt = *flags1 & 0x08;

  // manage LALT + KEYPAD NUM
  if (lalt && scancode != 0x38 && !e0) {
    // ALT was down, is this a keypad number?
    int num = convKeypadScancodeToNum(scancode);
    if (num >= 0) {
      // yes this is a keypad num, if down update altKeypadEntry
      if (down)
        *altKeypadEntry = (*altKeypadEntry * 10 + num) & 0xff;
      return false;
    } else {
      // no, back to normal case
      *altKeypadEntry = 0;
    }
  } else if (*altKeypadEntry > 0 && scancode == 0x38 && !down) {
    // ALT is up and altKeypadEntry contains a valid value, add it
    *syscode = *altKeypadEntry;  // high byte will be 0x00, low byte is the ascii value just typed
    *altKeypadEntry = 0;
    return true;
  }

  if (down) {
    bool shift    = *flags1 & 0x03;
    bool capslock = *flags1 & 0x40;
    bool numlock  = *flags1 & 0x20;
    bool ctrl     = *flags1 & 0x04;

    // CAPSLOCK enabled and letter
    if (capslock && ((scancode >= 0x10 && scancode <= 0x19) || (scancode >= 0x1e && scancode <= 0x26) || (scancode >= 0x2c && scancode <= 0x32)))
      shift = !shift;

    // NUMLOCK and keypad
    if (numlock && (scancode >= 0x47 && scancode <= 0x53))
      shift = !shift;

    // convert scancode to system code
    for (auto CONV = (e0 ? ESCODE2SYSCODE : SCODE2SYSCODE); CONV->scancode; ++CONV)
      if (CONV->scancode == scancode) {
        *syscode = CONV->syscode[shift ? 1 : ctrl ? 2 : lalt ? 3 : 0];
        return *syscode != 0xffff;
      }
  }

  return false;
}


// ret false on buffer full
bool BIOS::storeKeyInKbdBuffer(uint16_t syscode)
{
  // check space in BIOS keyboard buffer
  auto head = (uint16_t*)(m_memory + BIOS_DATAAREA_ADDR + BIOS_KBDBUFHEAD);
  auto tail = (uint16_t*)(m_memory + BIOS_DATAAREA_ADDR + BIOS_KBDBUFTAIL);
  if (*head - 2 != *tail && (*head != BIOS_KBDBUF || *tail != BIOS_KBDBUF + 30)) {
    // insert key into the keyboard buffer
    *(uint16_t*)(m_memory + BIOS_DATAAREA_ADDR + *tail) = syscode;
    *tail = (*tail == BIOS_KBDBUF + 30 ? BIOS_KBDBUF : *tail + 2);
    return true;  // success
  }
  // buffer full
  //printf("kbd buffer full\n");
  return false;
}


// perform some INT 9 tasks (keyboard interrupt handler)
// intput AL:
//    scancode as read from port 0x60
// output AH:
//    0 : normal key
//    2 : CTRL + ALT + DEL
//    3 : PRINTSCREEN
//    4 : CTRL-BREAK  (CTRL + PAUSE)
//    5 : SYSREQ      (ALT + PRINTSCREEN), down (AL = 0), up (AL = 1)
void BIOS::getKeyFromKeyboard()
{
  uint8_t * flags1 = m_memory + BIOS_DATAAREA_ADDR + BIOS_KBDSHIFTFLAGS1;
  uint8_t * flags2 = m_memory + BIOS_DATAAREA_ADDR + BIOS_KBDSHIFTFLAGS2;
  // saves current pause state
  bool onPause = *flags2 & 0x08;
  // update keyboard decoding state
  uint16_t syscode; // low byte = ASCII value, high byte = scancode
  bool toAdd = processScancode(i8086::AL(), &syscode);
  if (toAdd) {
    if (onPause) {
      // just disable pause state and discard key
      *flags2 &= ~0x08;
    } else {
      // we need to add this key to the keyboard buffer
      //printf("kbd store %04X\n", syscode);
      if (!storeKeyInKbdBuffer(syscode)) {
        // buffer full
        //printf("kbd buffer full\n");
      }
    }
  }
  // check for special syskeys
  if ((*flags1 & 0x04) && (*flags1 & 0x08) && (syscode == 0x53e0 || syscode == 0x93e0 || syscode == 0xa300)) {
    i8086::setAH(2);
  } else if (m_memory[BIOS_DATAAREA_ADDR + BIOS_PRINTSCREENFLAG] == 1) {
    i8086::setAH(3);
  } else if (syscode == 0x0000) {
    i8086::setAH(4);
  } else if (*flags2 & 0x04) {
    i8086::setAH(5);
    i8086::setAL((bool)i8086::AL());
  } else
    i8086::setAH(0);
}


// get or extract key from keyboard buffer
// updates keyboard LEDs
// input AL:
//    bit 0 : 0 = check only, 1 = extract
//    bit 1 : 0 = do not filter, 1 = filter extended keys
// output:
//    AX : ASCII (AL) and scancode (AH)
//    ZF : 0 = key present, 1 = key not present
void BIOS::getKeyFromBuffer()
{
  // this LUT transform extended keyboard syskeys to XT syscodes
  struct Ext2XT {
    uint16_t esyscode;
    uint16_t xsyscode;  // 0xffff = don't return
  };
  static const Ext2XT EXT2XT[] = {
    { 0x2900, 0xffff }, { 0x0e00, 0xffff }, { 0x9400, 0xffff }, { 0xa500, 0xffff }, { 0x1a00, 0xffff }, { 0x1b00, 0xffff }, { 0x2b00, 0xffff },
    { 0x2700, 0xffff }, { 0x2800, 0xffff }, { 0x1c00, 0xffff }, { 0x3300, 0xffff }, { 0x3400, 0xffff }, { 0x3500, 0xffff }, { 0x52e0, 0x5200 },
    { 0x92e0, 0xffff }, { 0xa200, 0xffff }, { 0x53e0, 0x5300 }, { 0x93e0, 0xffff }, { 0xa300, 0xffff }, { 0x4be0, 0x4b00 }, { 0x73e0, 0x7300 },
    { 0x9b00, 0xffff }, { 0x47e0, 0x4700 }, { 0x77e0, 0x7700 }, { 0x9700, 0xffff }, { 0x4fe0, 0x4f00 }, { 0x75e0, 0x7500 }, { 0x9f00, 0xffff },
    { 0x48e0, 0x4800 }, { 0x8de0, 0xffff }, { 0x9800, 0xffff }, { 0x50e0, 0x5000 }, { 0x91e0, 0xffff }, { 0xa000, 0xffff }, { 0x49e0, 0x4900 },
    { 0x84e0, 0x8400 }, { 0x9900, 0xffff }, { 0x51e0, 0x5100 }, { 0x76e0, 0x7600 }, { 0xa100, 0xffff }, { 0x4de0, 0x4d00 }, { 0x74e0, 0x7400 },
    { 0x9d00, 0xffff }, { 0xe02f, 0x352f }, { 0x9500, 0xffff }, { 0xa400, 0xffff }, { 0x8d00, 0xffff }, { 0x8f00, 0xffff }, { 0x9100, 0xffff },
    { 0x9200, 0xffff }, { 0x9600, 0xffff }, { 0x3700, 0xffff }, { 0x9300, 0xffff }, { 0x8e00, 0xffff }, { 0x4a00, 0xffff }, { 0x9000, 0xffff },
    { 0x4e00, 0xffff }, { 0xe00d, 0x1c0d }, { 0xe00a, 0x1c0a }, { 0xa600, 0xffff }, { 0x0100, 0xffff }, { 0x8500, 0xffff }, { 0x8700, 0xffff },
    { 0x8900, 0xffff }, { 0x8b00, 0xffff }, { 0x8600, 0xffff }, { 0x8800, 0xffff }, { 0x8a00, 0xffff }, { 0x8c00, 0xffff },
  };

  // return value is not valid (ZF = 1)
  i8086::setFlagZF(1);

  auto head = (uint16_t*)(m_memory + BIOS_DATAAREA_ADDR + BIOS_KBDBUFHEAD);
  auto tail = (uint16_t*)(m_memory + BIOS_DATAAREA_ADDR + BIOS_KBDBUFTAIL);

  if (*head != *tail) {

    // get key from buffer head
    uint16_t k = * (uint16_t*) (m_memory + BIOS_DATAAREA_ADDR + *head);

    bool filtered = false;

    // filter extended keys?
    if (i8086::AL() & 0x02) {
      for (int i = 0; i < sizeof(EXT2XT) / sizeof(Ext2XT); ++i) {
        if (EXT2XT[i].esyscode == k) {
          if (EXT2XT[i].xsyscode == 0xffff)
            filtered = true;        // don't return
          else
            k = EXT2XT[i].xsyscode; // replace
          break;
        }
      }
    }

    // remove from buffer?
    if (filtered || (i8086::AL() & 0x01))
      *head = (*head == BIOS_KBDBUF + 30 ? BIOS_KBDBUF : *head + 2);

    if (!filtered) {
      // return value is valid (ZF = 0)
      i8086::setFlagZF(0);
      i8086::setAX(k);
    }

  }

  // update LEDs
  bool numLockLED, capsLockLED, scrollLockLED;
  m_keyboard->getLEDs(&numLockLED, &capsLockLED, &scrollLockLED);
  uint8_t LEDs = m_memory[BIOS_DATAAREA_ADDR + BIOS_KBDLEDS];
  if (numLockLED != (bool)(LEDs & 0x02) || capsLockLED != (bool)(LEDs & 0x04) || scrollLockLED != (bool)(LEDs & 0x01))
    m_keyboard->setLEDs((bool)(LEDs & 0x02), (bool)(LEDs & 0x04), (bool)(LEDs & 0x01));
}


void BIOS::emptyKbdBuffer()
{
  auto head = (uint16_t*)(m_memory + BIOS_DATAAREA_ADDR + BIOS_KBDBUFHEAD);
  auto tail = (uint16_t*)(m_memory + BIOS_DATAAREA_ADDR + BIOS_KBDBUFTAIL);
  *tail = *head;
}


// get shift flags or extended shift flags
// input AL:
//   bit 0 : 0 = normal flags in AL, 1 = normal flags in AL and extended flags in AH
// output AL or AX
void BIOS::getKeyboardFlags()
{
  uint8_t * flags1 = m_memory + BIOS_DATAAREA_ADDR + BIOS_KBDSHIFTFLAGS1;
  uint8_t * flags2 = m_memory + BIOS_DATAAREA_ADDR + BIOS_KBDSHIFTFLAGS2;
  uint8_t * mode   = m_memory + BIOS_DATAAREA_ADDR + BIOS_KBDMODE;
  if (i8086::AL() & 1)
    i8086::setAH( (*flags2 & 0xf3) | (*mode & 0x0c) );
  i8086::setAL(*flags1);
}


// inputs:
//    AL : service (0x05 = set typematic rate and delay)
//    BH : delay value (as expected by BIOS, 0 = 250ms, 1 = 500ms, etc...)
//    BL : typematic rate (as expected by BIOS, 0 = 30, 1 = 26.7, etc...)
// notes:
//    this method doesn't use 0x60 and 0x64 ports, but directly interfaces to the Keyboard object
void BIOS::setKeyboardTypematicAndDelay()
{
  if (i8086::AL() == 0x05) {
    // send command "set typematic rate and delay" (0xF3) to the keyboard and wait for ACK (0xFA)
    if (!m_keyboard->sendCommand(0xF3, 0xFA))
      return;
    // send parameters
    m_keyboard->sendCommand(i8086::BL() | (i8086::BH() << 5), 0xFA);
  }
}


// inputs:
//    CL : ASCII character
//    CH : scan code
// outputs:
//    AL : 0 = no error, 1 = keyboard buffer full
//    CF : 0 = no error, 1 = keyboard buffer full
void BIOS::storeKeyboardKeyData()
{
  bool r = storeKeyInKbdBuffer(i8086::CX());
  i8086::setAL(!r);
  i8086::setFlagCF(!r);
}


// Implements all services of "INT 15 Function C2h"
// inputs:
//    AL : subfunction
//    .. : depends by the subfunction
// outputs:
//    AH : 0 = success, >0 = error (see "INT 15h Function C2h - Pointing Device Interface")
//    CF : 0 = successful, 1 = unsuccessful
//    .. : depends by the subfunction
void BIOS::pointingDeviceInterface()
{
  if (m_mouse->isMouseAvailable()) {

    i8086::setAH(0x00);
    i8086::setFlagCF(0);

    switch (i8086::AL()) {

      // Enable/disable pointing device
      // inputs:
      //    AL : 0x00
      //    BH : 0 = disable, 1 = enable
      case 0x00:
        m_i8042->enableMouse(i8086::BH());
        break;

      // Reset pointing device
      // inputs:
      //    AL : 0x01
      // outputs:
      //    BH : Device ID
      case 0x01:
        m_i8042->enableMouse(false);             // mouse disabled
        m_mouse->setSampleRate(100);             // 100 reports/second
        m_mouse->setResolution(2);               // 4 counts/millimeter
        m_mouse->setScaling(1);                  // 1:1 scaling
        i8086::setBH(m_mouse->deviceID() & 0xff);
        break;

      // Set sample rate
      // inputs:
      //    AL : 0x02
      //    BH : Sample rate
      case 0x02:
        m_mouse->setSampleRate(i8086::BH());
        break;

      // Set resolution
      // inputs:
      //    AL : 0x03
      //    BH : Resolution value
      case 0x03:
        m_mouse->setResolution(i8086::BH());
        break;

      // Read device type
      // inputs:
      //    AL : 0x04
      case 0x04:
        i8086::setBH(m_mouse->deviceID() & 0xff);
        break;

      // Initialize pointing device interface
      // inputs:
      //    AL : 0x05
      //    BH : Data package size (1-8, in bytes)
      //         note: this value is acqually ignored because we get actual packet size from Mouse object
      case 0x05:
      {
        m_i8042->enableMouse(false);              // mouse disabled
        m_mouse->setSampleRate(100);              // 100 reports/second
        m_mouse->setResolution(2);                // 4 counts/millimeter
        m_mouse->setScaling(1);                   // 1:1 scaling
        uint8_t * EBDA = m_memory + EBDA_ADDR;
        EBDA[EBDA_DRIVER_OFFSET] = 0x0000;
        EBDA[EBDA_DRIVER_SEG]    = 0x0000;
        EBDA[EBDA_FLAGS1]        = 0x00;
        EBDA[EBDA_FLAGS2]        = m_mouse->getPacketSize(); // instead of i8086::BH()!!
        break;
      }

      // Set scaling or get status
      // inputs:
      //    AL : 0x06
      //    BH : subfunction
      case 0x06:
        switch (i8086::BH()) {
          // Set scaling factor to 1:1
          // inputs:
          //    BH : 0x01
          case 0x01:
            m_mouse->setScaling(1);
            break;
          // Set scaling factor to 2:1
          // inputs:
          //    BH : 0x02
          case 0x02:
            m_mouse->setScaling(2);
            break;
          default:
            // not implements
            printf("Pointing device function 06:%02X not implemented\n", i8086::BH());
            i8086::setAH(0x86);
            i8086::setFlagCF(1);
            break;
        }
        break;

      // Set pointing device handler address
      // inputs:
      //    AL = 0x07
      //    ES:BX : Pointer to application-program's device driver
      case 0x07:
      {
        uint8_t * EBDA = m_memory + EBDA_ADDR;
        *(uint16_t*)(EBDA + EBDA_DRIVER_OFFSET) = i8086::BX();
        *(uint16_t*)(EBDA + EBDA_DRIVER_SEG)    = i8086::ES();
        EBDA[EBDA_FLAGS2] |= 0x80;  // set handler installed flag
        break;
      }

      default:
        // not implements
        printf("Pointing device function %02X not implemented\n", i8086::AL());
        i8086::setAH(0x86);
        i8086::setFlagCF(1);
        break;
    }

  } else {
    // mouse not available
    i8086::setAH(0x03);   // 0x03 = interface error
    i8086::setFlagCF(1);
  }
}


// convert packed BCD to decimal
static uint8_t BCDtoByte(uint8_t v)
{
  return (v & 0x0F) + (v >> 4) * 10;
}


// synchronize system ticks with RTC
void BIOS::syncTicksWithRTC()
{
  m_MC146818->updateTime();
  int ss = BCDtoByte(m_MC146818->reg(0x00));
  int mm = BCDtoByte(m_MC146818->reg(0x02));
  int hh = BCDtoByte(m_MC146818->reg(0x04));
  int totSecs = ss + mm * 60 + hh * 3600 + 1000;
  int64_t pitTicks = (int64_t)totSecs * PIT_TICK_FREQ;
  *(uint32_t*)(m_memory + BIOS_DATAAREA_ADDR + BIOS_SYSTICKS) = (uint32_t) (pitTicks / 65536);
}


// INT 13 services
void BIOS::diskHandlerEntry()
{
  if (i8086::DL() < 2)
    diskHandler_floppy();
  else
    diskHandler_HD();
}


void BIOS::diskHandler_floppy()
{
  int drive   = i8086::DL();
  int service = i8086::AH();

  //printf("INT 13h, FDD (%d), service %02X\n", drive, service);

  if (m_machine->disk(drive) == nullptr) {
    // invalid drive
    diskHandler_floppyExit(0x80, true);
    return;
  }

  switch (service) {

    // Reset Diskette System
    case 0x00:
      diskHandler_floppyExit(m_mediaType[drive] == mediaUnknown ? 0x06 : 0x00, true);
      return;

    // Read Diskette Status
    case 0x01:
      diskHandler_floppyExit(m_memory[BIOS_DATAAREA_ADDR + BIOS_DISKLASTSTATUS], false);
      m_memory[BIOS_DATAAREA_ADDR + BIOS_DISKLASTSTATUS] = 0; // this function resets BIOS_DISKLASTSTATUS
      return;

    // Read Diskette Sectors
    case 0x02:
    // Write Diskette Sectors
    case 0x03:
    // Verify Diskette Sectors
    case 0x04:
    {
      if (!checkDriveMediaType(drive)) {
        diskHandler_floppyExit(6, true);
        return;
      }
      uint32_t pos, dest, count;
      if (!diskHandler_calcAbsAddr(drive, &pos, &dest, &count)) {
        diskHandler_floppyExit(4, true);  // sector not found
        return;
      }
      fseek(m_machine->disk(drive), pos, 0);
      size_t sects = i8086::AL();
      if (service != 0x04) {
        sects = service == 0x02 ?
                  fread(m_memory + dest, 1, count, m_machine->disk(drive)) :
                  fwrite(m_memory + dest, 1, count, m_machine->disk(drive));
        sects /= 512;
      }
      i8086::setAL(sects);
      diskHandler_floppyExit(sects == 0 ? 4 : 0, true);
      return;
    }

    // Format Diskette Track
    case 0x05:
    {
      int sectsCountToFormat = i8086::AL();
      int track              = i8086::CH();
      int head               = i8086::DH();
      uint32_t tableAddr     = i8086::ES() * 16 + i8086::BX();

      int SPT = m_machine->diskSectors(drive);
      int tracksCount = m_machine->diskCylinders(drive);

      uint8_t fillByte = m_memory[getDriveMediaTableAddr(drive) + 8];

      uint8_t * buf = (uint8_t*) malloc(512);
      memset(buf, fillByte, 512);

      for (int i = 0; i < sectsCountToFormat; ++i) {
        int ttrack  = m_memory[tableAddr++];
        int thead   = m_memory[tableAddr++];
        int tsect   = m_memory[tableAddr++];
        int tsectSz = 128 << m_memory[tableAddr++];
        if (ttrack != track || thead > 1 || tsect > SPT || tsectSz != 512 || track >= tracksCount) {
          // error
          free(buf);
          diskHandler_floppyExit(0x04, true);
          return;
        }
        fseek(m_machine->disk(drive), 512 * ((track * 2 + head) * SPT + (tsect - 1)), 0);
        fwrite(buf, 1, 512, m_machine->disk(drive));
        //printf("  track=%d head=%d sect=%d pos=%d fill=%02X\n", track, head, tsect, 512 * ((track * 2 + head) * SPT + (tsect - 1)), fillByte);
      }
      free(buf);
      diskHandler_floppyExit(0x00, true);
      return;
    }

    // Read Drive Parameters
    case 0x08:
      i8086::setAX(0x0000);
      i8086::setBH(0x00);
      if (!checkDriveMediaType(drive)) {
        i8086::setCX(0x0000);
        i8086::setDX(0x0000);
        i8086::setES(0x0000);
        i8086::setDI(0x0000);
        diskHandler_floppyExit(6, true);
        return;
      }
      switch (m_mediaType[drive]) {
        case floppy160KB:
        case floppy180KB:
        case floppy320KB:
          // @TODO: check this
          i8086::setBL(0x01);
          break;
        case floppy360KB:
          i8086::setBL(0x01);
          break;
        case floppy720KB:
          i8086::setBL(0x03);
          break;
        case floppy1M2K:
          i8086::setBL(0x02);
          break;
        case floppy1M44K:
          i8086::setBL(0x04);
          break;
        case floppy2M88K:
          i8086::setBL(0x05);
          break;
        case mediaUnknown:
        default:
          // not possible here
          break;
      }
      i8086::setCH(m_machine->diskCylinders(drive) - 1);                      // max usable track number
      i8086::setCL(m_machine->diskSectors(drive));                            // max usable sector number
      i8086::setDH(m_machine->diskHeads(drive) - 1);                          // max usable head number
      i8086::setDL((bool)(m_machine->disk(0)) + (bool)(m_machine->disk(1)));  // number of diskette installed
      // Pointer to Diskette Parameters table for the maximum media type supported on the specified drive
      i8086::setES(BIOS_SEG);
      i8086::setDI(m_origInt1EAddr - BIOS_SEG * 16);
      diskHandler_floppyExit(0, true);
      //printf("  INT 13h, FDD, 08h: BL=%d, CH=%d, CL=%d, DL=%d\n", i8086::BL(), i8086::CH(), i8086::CL(), i8086::DL());
      return;

    // Read Drive Type
    case 0x15:
      diskHandler_floppyExit(0, true);
      i8086::setAH(m_machine->disk(drive) ? 0x02 : 0x00);
      return;

    // Detect Media Change
    case 0x16:
      diskHandler_floppyExit(m_mediaType[drive] == mediaUnknown ? 0x06 : 0x00, true);
      return;

    // Set Diskette Type
    case 0x17:
      switch (i8086::AL()) {

        // 320K/360K
        case 0x01:
          diskHandler_floppyExit(m_mediaType[drive] != floppy360KB &&
                                 m_mediaType[drive] != floppy320KB &&
                                 m_mediaType[drive] != floppy180KB &&
                                 m_mediaType[drive] != floppy160KB, true);
          break;

        // 360K
        case 0x02:
          diskHandler_floppyExit(m_mediaType[drive] != floppy360KB, true);
          break;

        // 1.2MB
        case 0x03:
          diskHandler_floppyExit(m_mediaType[drive] != floppy1M2K, true);
          break;

        // 720KB
        case 0x04:
          diskHandler_floppyExit(m_mediaType[drive] != floppy720KB, true);
          break;

        // error
        default:
          diskHandler_floppyExit(1, true);
          break;

      }
      return;

    // Set Media Type for Format
    case 0x18:
    {
      // check if proposed media type matches with current
      int propTracks = i8086::CH();
      int propSPT    = i8086::CL();
      int tracks     = m_machine->diskCylinders(drive) - 1;
      int SPT        = m_machine->diskSectors(drive);
      if (propTracks == tracks && propSPT == SPT) {
        // match ok
        diskHandler_floppyExit(0x00, true);
        i8086::setES(BIOS_SEG);
        i8086::setDI(m_origInt1EAddr - BIOS_SEG * 16);
      } else {
        // not supported
        diskHandler_floppyExit(0x0c, true);
        printf("  INT 13h, FDD, 18h: unsupported media type, t=%d (%d), s=%d (%d)\n", propTracks, tracks, propSPT, SPT);
      }
      return;
    }

    default:
      // invalid function
      diskHandler_floppyExit(1, true);
      printf("  INT 13h, FDD, invalid service %02X\n", service);
      return;

  }
}


// CH       : low 8 bits of track number
// CL 6...7 : high 2 bits of track number
// CL 0...5 : sector number
// DH       : head number
// AL       : number of sectors to read
// ES:BX    : destination address
bool BIOS::diskHandler_calcAbsAddr(int drive, uint32_t * pos, uint32_t * dest, uint32_t * count)
{
  int sectorsPerTrack = m_machine->diskSectors(drive);
  int heads           = m_machine->diskHeads(drive);
  int track  = i8086::CH() | (((uint16_t)i8086::CL() & 0xc0) << 2);
  int sector = i8086::CL() & 0x3f;
  int head   = i8086::DH();
  if (sector > sectorsPerTrack)
    return false;
  *pos   = 512 * ((track * heads + head) * sectorsPerTrack + (sector - 1));
  *dest  = i8086::ES() * 16 + i8086::BX();
  *count = i8086::AL() * 512;
  return true;
}


void BIOS::diskHandler_floppyExit(uint8_t err, bool setErrStat)
{
  i8086::setAH(err);
  i8086::setFlagCF(err ? 1 : 0);
  if (setErrStat)
    m_memory[BIOS_DATAAREA_ADDR + BIOS_DISKLASTSTATUS] = err;

  /*
  if (err > 0)
    printf("  err = %d\n", err);
    */
}


void BIOS::diskHandler_HD()
{
  int drive   = (i8086::DL() & 1) + 2;  // 2 = HD0, 3 = HD1
  int service = i8086::AH();

  //printf("INT 13h, HDD (%d), service %02X [AH=%02X DL=%02X]\n", drive, service, i8086::AH(), i8086::DL());

  if (m_machine->disk(drive) == nullptr || i8086::DL() > 0x81) {
    // invalid drive
    diskHandler_HDExit(0x80, true);
    return;
  }

  switch (service) {

    // Reset Fixed Disk System
    case 0x00:
      diskHandler_HDExit(checkDriveMediaType(drive) ? 0x00 : 0x80, true);
      return;

    // Read Disk Status
    case 0x01:
      diskHandler_HDExit(m_memory[BIOS_DATAAREA_ADDR + BIOS_HDLASTSTATUS], false);
      m_memory[BIOS_DATAAREA_ADDR + BIOS_HDLASTSTATUS] = 0; // this function resets BIOS_HDLASTSTATUS
      return;

    // Read Fixed Disk Sectors
    case 0x02:
    // Write Fixed Disk Sectors
    case 0x03:
    // Verify Fixed Disk Sectors
    case 0x04:
    {
      if (!checkDriveMediaType(drive)) {
        diskHandler_HDExit(0x80, true);
        return;
      }
      uint32_t pos, dest, count;
      if (!diskHandler_calcAbsAddr(drive, &pos, &dest, &count)) {
        diskHandler_HDExit(4, true);  // sector not found
        return;
      }
      fseek(m_machine->disk(drive), pos, 0);
      size_t sects = i8086::AL();
      if (service != 0x04) {
        sects = service == 0x02 ?
                  fread(m_memory + dest, 1, count, m_machine->disk(drive)) :
                  fwrite(m_memory + dest, 1, count, m_machine->disk(drive));
        sects /= 512;
      }
      i8086::setAL(sects);
      diskHandler_HDExit(sects == 0 ? 4 : 0, true);
      return;
    }

    // Format Disk Cylinder
    case 0x05:
      diskHandler_HDExit(0x00, true);
      return;

    // Read Drive Parameters
    case 0x08:
      i8086::setAL(0x00);
      if (checkDriveMediaType(drive)) {
        uint16_t maxUsableCylNum  = m_machine->diskCylinders(drive) - 1;
        uint8_t  maxUsableSecNum  = m_machine->diskSectors(drive);
        uint8_t  maxUsableHeadNum = m_machine->diskHeads(drive) - 1;
        i8086::setCH(maxUsableCylNum & 0xff);                                       // Maximum usable cylinder number (low 8 bits)
        i8086::setCL(((maxUsableCylNum >> 2) & 0xc0) |                              // Bits 7-6 = Maximum usable cylinder number (high 2 bits)
                     (maxUsableSecNum & 0x3f));                                     // Bits 5-0 = Maximum usable sector number
        i8086::setDH(maxUsableHeadNum);                                             // Maximum usable head number
        i8086::setDL((bool)(m_machine->disk(2)) + (bool)(m_machine->disk(3)));      // Number of drives (@TODO: correct? Docs not clear)
        // Address of Fixed Disk Parameters table
        // *** note: some texts tell ES:DI should return a pointer to parameters table. IBM docs don't. Actually
        //           returning ES:DI may crash old MSDOS versions!
        //i8086::setES(BIOS_SEG);
        //i8086::setDI(getDriveMediaTableAddr(drive) - BIOS_SEG * 16);
/*
        printf("CH = %02X CL = %02X DH = %02X DL = %02X\n", i8086::CH(), i8086::CL(), i8086::DH(), i8086::DL());
        printf("ES = %04X  DI = %04X\n", i8086::ES(), i8086::DI());
        printf("ES:DI [00-01] = %d\n", (int) *(uint16_t*)(m_memory + i8086::ES() * 16 + i8086::DI() + 0x00));
        printf("ES:DI    [02] = %d\n", (int) *(uint8_t*)(m_memory + i8086::ES() * 16 + i8086::DI() + 0x02));
        printf("ES:DI    [0e] = %d\n", (int) *(uint8_t*)(m_memory + i8086::ES() * 16 + i8086::DI() + 0x0e));
*/
        diskHandler_HDExit(0x00, true);
      } else {
        i8086::setCX(0x0000);
        i8086::setDX(0x0000);
        diskHandler_HDExit(0x80, true);
      }
      return;

    // Initialize Drive Parameters
    case 0x09:
    // Seek to Cylinder
    case 0x0c:
    // Recalibrate Drive
    case 0x11:
    // Controller Internal Diagnostic
    case 0x14:
      diskHandler_HDExit(checkDriveMediaType(drive) ? 0x00 : 0x80, true);
      return;

    // Test for Drive Ready
    case 0x10:
      diskHandler_HDExit(checkDriveMediaType(drive) ? 0x00 : 0xAA, true);
      return;

    // Read Disk Type
    case 0x15:
      if (checkDriveMediaType(drive)) {
        diskHandler_HDExit(0x00, true);
        i8086::setAH(0x03);// drive present
        auto sectors = m_machine->diskSize(drive) / 512;
        i8086::setDX(sectors & 0xffff);
        i8086::setCX(sectors >> 16);
      } else {
        i8086::setAX(0x0000);
        i8086::setCX(0x0000);
        i8086::setDX(0x0000);
        diskHandler_HDExit(0x00, true); // yes, it is 0x00!
      }
      return;

    default:
      // invalid function
      printf("INT 13h, HDD, invalid service %02X\n", service);
      diskHandler_HDExit(1, true);
      return;

  }
}


void BIOS::diskHandler_HDExit(uint8_t err, bool setErrStat)
{
  i8086::setAH(err);
  i8086::setFlagCF(err ? 1 : 0);
  if (setErrStat)
    m_memory[BIOS_DATAAREA_ADDR + BIOS_HDLASTSTATUS] = err;
}


void BIOS::videoHandlerEntry()
{
  auto ga = m_machine->graphicsAdapter();
  auto fb = m_machine->frameBuffer();

  switch (i8086::AH()) {

    // Write Pixel
    case 0x0c:
      switch (ga->emulation()) {

        case GraphicsAdapter::Emulation::PC_Graphics_320x200_4Colors:
        {
          constexpr uint32_t rowlen = 320 / 4;
          const uint8_t  value      = i8086::AL() & 0b11;
          const bool     xored      = i8086::AL() & 0x80;
          const uint32_t col        = i8086::CX();
          const uint32_t row        = i8086::DX();
          const uint32_t addr       = (row >> 1) * rowlen + (col >> 2) + (row & 1) * 0x2000;
          const uint32_t shift      = 6 - (col & 3) * 2;
          if (xored)
            fb[addr] ^= value << shift;
          else
            fb[addr]  = (fb[addr] & ~(0b11 << shift)) | (value << shift);
          break;
        }

        case GraphicsAdapter::Emulation::PC_Graphics_640x200_2Colors:
          printf("INT 10h, write pixel, unsupported 640x200x2 resolution\n");
          break;

        case GraphicsAdapter::Emulation::PC_Graphics_HGC_720x348:
          printf("INT 10h, write pixel, unsupported 720x348x2 resolution\n");
          break;

        default:
          printf("INT 10h, write pixel, unsupported resolution\n");
          break;

      }
      break;

    default:
      printf("unsupported INT 10h, AX = %04X\n", i8086::AX());
      break;

  }
}
