/*
  Created by Fabrizio Di Vittorio (fdivitto2013@gmail.com) - www.fabgl.com
  Copyright (c) 2019-2020 Fabrizio Di Vittorio.
  All rights reserved.

  This file is part of FabGL Library.

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
#include <stdio.h>

#include "Z80/Z80.h"


// Altair 88-DSK Boot ROM (starts at 0xFF00)
extern const uint8_t Altair88DiskBootROM[256];
const int Altair88DiskBootROMAddr = 0xFF00;
const int Altair88DiskBootROMRun  = 0xFF00;


class Machine;
class Stream;



////////////////////////////////////////////////////////////////////////////////////
// Device


struct Device {
  Device(Machine * machine) : m_machine(machine) { }

  Device * next = nullptr;
  virtual bool read(int address, int * result) = 0;   // ret true = handled
  virtual bool write(int address, int value) = 0;     // ret true = handled

  Machine * machine() { return m_machine; }

  virtual void tick(int ticks) = 0;

private:
  Machine * m_machine;
};



////////////////////////////////////////////////////////////////////////////////////
// SIO


class SIO : public Device {

public:

  SIO(Machine * machine, int address);

  void attachStream(Stream * value);

  void tick(int ticks) { }

private:

  bool read(int address, int * result);
  bool write(int address, int value);

  int                 m_address;
  Stream *            m_stream;

};



////////////////////////////////////////////////////////////////////////////////////
// Mits88Disk (88-disk)


enum DiskFormat {
  Disk_338K,
  MiniDisk_76K,
};


class Mits88Disk : public Device {

public:

  static const int sectorChangeDurationDisk     = 9500;  // us
  static const int sectorChangeDurationMiniDisk = 11000; // us
  static const int sectorChangeShortDuration    = 200;    // us
  static const int readByteDuration      = 32;    // us
  static const int sectorTrueDuration    = 90;    // us
  static const int DISKCOUNT             = 4;
  static const int SECTOR_SIZE           = 137;   // number of bytes per sector

  Mits88Disk(Machine * machine, DiskFormat diskFormat);
  ~Mits88Disk();

  void attachReadOnlyBuffer(int drive, uint8_t const * data);
  void attachFile(int drive, char const * filename);

  void detach(int drive);

  void sendDiskImageToStream(int drive, Stream * stream);
  void receiveDiskImageFromStream(int drive, Stream * stream);

  void setDrive(int value);
  int  drive() { return m_drive; }

  void flush();
  void detachAll();

  void tick(int ticks);

private:

  bool read(int address, int * result);
  bool write(int address, int value);

  int  readByteFromDisk();
  void writeByteToDisk(int value);

  uint64_t        m_tick;
  DiskFormat      m_diskFormat;
  uint8_t const * m_readOnlyBuffer[DISKCOUNT];    // when using attachReadOnlyBuffer
  FILE *          m_file[DISKCOUNT];              // when using attachFile
  uint8_t *       m_fileSectorBuffer[DISKCOUNT];  // when using attachFile (sector buffer)
  int8_t          m_drive;
  uint8_t         m_track[DISKCOUNT];
  int8_t          m_sector[DISKCOUNT];
  uint8_t         m_pos[DISKCOUNT];               // read/write position
  uint8_t         m_trackSize;                    // number of sectors per track
  uint8_t         m_tracksCount;                  // number of tracks per disc
  // status word (0 = active, 1 = inactive)
  int8_t          m_readByteReady[DISKCOUNT];
  int8_t          m_sectorTrue[DISKCOUNT];
  int8_t          m_headLoaded[DISKCOUNT];
  // state changes timings (in ticks)
  uint64_t        m_readByteTime[DISKCOUNT];
  uint64_t        m_sectorChangeTime[DISKCOUNT];
  bool            m_enabled;
  int32_t         m_sectorChangeDuration; // us
};



////////////////////////////////////////////////////////////////////////////////////
// Machine


enum class CPU {
  i8080,
  Z80
};


typedef void (*MenuCallback)();


class Machine : public Z80Interface {

public:

  Machine();

  ~Machine();

  void load(int address, uint8_t const * data, int length);

  void attachDevice(Device * device);

  void attachRAM(int RAMSize);

  void setMenuCallback(MenuCallback value) { m_menuCallback = value; }

  void run(CPU cpu, int address);

  uint8_t readByte(uint16_t address);
  void writeByte(uint16_t address, uint8_t value);

  uint16_t readWord(uint16_t addr)              { return readByte(addr) | (readByte(addr + 1) << 8); }
  void writeWord(uint16_t addr, uint16_t value) { writeByte(addr, value & 0xFF); writeByte(addr + 1, value >> 8); }


  uint8_t readIO(uint16_t address);
  void writeIO(uint16_t address, uint8_t value);

  void setRealSpeed(bool value) { m_realSpeed = value; }
  bool realSpeed() { return m_realSpeed; }

private:

  int nextStep(CPU cpu);

  Device *     m_devices;
  bool         m_realSpeed;
  uint8_t *    m_RAM;
  MenuCallback m_menuCallback;
  Z80          m_Z80;
};

