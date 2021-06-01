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
#include <stdio.h>

#include "emudevs/Z80.h"
#include "emudevs/i8080.h"


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
//
// 88-Disk 8'' inches has 77 tracks and 32 sectors (named here as Disk_338K)
// minidisk has 35 tracks and 16 sectors (naed here as MiniDisk_76K)


enum DiskFormat {
  Disk_338K,
  MiniDisk_76K,
};


class Mits88Disk : public Device {

private:

  static const int DISKCOUNT                    = 4;

  static const int sectorChangeDurationDisk     = 9500;  // us
  static const int sectorChangeDurationMiniDisk = 11000; // us
  static const int sectorChangeShortDuration    = 200;   // us
  static const int readByteDuration             = 32;    // us
  static const int sectorTrueDuration           = 90;    // us

  static const int SECTOR_SIZE                  = 137;   // number of bytes per sector

  static const int diskTracksCount              = 77;    // number of tracks (Disk_338K)
  static const int diskSectorsPerTrack          = 32;    // number of sectors per track (Disk_338K)

  static const int minidiskTracksCount          = 35;    // number of tracks (MiniDisk_76K)
  static const int minidiskSectorsPerTrack      = 16;    // number of sectors per track (MiniDisk_76K)


public:

  Mits88Disk(Machine * machine, DiskFormat diskFormat);
  ~Mits88Disk();

  DiskFormat diskFormat()    { return m_diskFormat; }
  size_t diskSize()          { return tracksCount() * sectorsPerTrack() * SECTOR_SIZE; }
  int sectorsPerTrack()      { return m_trackSize; }
  int tracksCount()          { return m_tracksCount; }

  void attachReadOnlyBuffer(int drive, uint8_t const * data);
  void attachFile(int drive, char const * filename);
  void attachFileFromImage(int drive, char const * filename, uint8_t const * data);

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


class Machine {

public:

  Machine();

  ~Machine();

  void load(int address, uint8_t const * data, int length);

  void attachDevice(Device * device);

  void attachRAM(int RAMSize);

  void setMenuCallback(MenuCallback value) { m_menuCallback = value; }

  void run(CPU cpu, int address);

  static int readByte(void * context, int address);
  static void writeByte(void * context, int address, int value);

  static int readWord(void * context, int addr)              { return readByte(context, addr) | (readByte(context, addr + 1) << 8); }
  static void writeWord(void * context, int addr, int value) { writeByte(context, addr, value & 0xFF); writeByte(context, addr + 1, value >> 8); }

  static int readIO(void * context, int address);
  static void writeIO(void * context, int address, int value);

  void setRealSpeed(bool value) { m_realSpeed = value; }
  bool realSpeed() { return m_realSpeed; }

private:

  int nextStep(CPU cpu);

  Device *     m_devices;
  bool         m_realSpeed;
  uint8_t *    m_RAM;
  MenuCallback m_menuCallback;
  fabgl::Z80   m_Z80;
  fabgl::i8080 m_i8080;
};

