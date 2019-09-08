/*
  Created by Fabrizio Di Vittorio (fdivitto2013@gmail.com) - www.fabgl.com
  Copyright (c) 2019 Fabrizio Di Vittorio.
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

private:
  Machine * m_machine;
};



////////////////////////////////////////////////////////////////////////////////////
// SIO

typedef int (*GetCharPreprocessor)(int);


class SIO : public Device {

public:

  SIO(Machine * machine, int address);

  void attachStream(Stream * value, GetCharPreprocessor getCharPreprocessor = nullptr);

private:

  bool read(int address, int * result);
  bool write(int address, int value);

  int                 m_address;
  GetCharPreprocessor m_getCharPreprocessor;
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

  static const int sectorPositionedDelay = 1;
  static const int DISKCOUNT    = 4;
  static const int SECTOR_SIZE  = 137;   // number of bytes per sector

  Mits88Disk(Machine * machine, DiskFormat diskFormat);
  ~Mits88Disk();

  void attachReadOnlyBuffer(int drive, uint8_t const * data);
  void attachFile(int drive, char const * filename);

  void detach(int drive);

  void sendDiskImageToStream(int drive, Stream * stream);
  void receiveDiskImageFromStream(int drive, Stream * stream);

  void setDrive(int value);
  int drive() { return m_drive; }

  void flush();
  void detachAll();

private:

  bool read(int address, int * result);
  bool write(int address, int value);

  int readByteFromDisk();
  void writeByteToDisk(int value);

  DiskFormat      m_diskFormat;
  uint8_t const * m_readOnlyBuffer[DISKCOUNT];    // when using attachReadOnlyBuffer
  FILE *          m_file[DISKCOUNT];              // when using attachFile
  uint8_t *       m_fileSectorBuffer[DISKCOUNT];  // when using attachFile (sector buffer)
  int8_t          m_drive;
  uint8_t         m_track[DISKCOUNT];
  int8_t          m_sector[DISKCOUNT];
  int8_t          m_sectorPositioned[DISKCOUNT];
  int8_t          m_readByteReady[DISKCOUNT];
  uint8_t         m_pos[DISKCOUNT]; // read/write position
  uint8_t         m_trackSize;       // number of sectors per track
  uint8_t         m_tracksCount;     // number of tracks per disc
};



////////////////////////////////////////////////////////////////////////////////////
// Machine


enum CPU {
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

  int readByte(int address);
  void writeByte(int address, int value);

  int readIO(int address);
  void writeIO(int address, int value);

  void setRealSpeed(bool value) { m_realSpeed = value; }
  bool realSpeed() { return m_realSpeed; }

private:

  int nextStep(CPU cpu, void * param);

  Device *     m_devices;
  bool         m_realSpeed;
  uint8_t *    m_RAM;
  MenuCallback m_menuCallback;
};

