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


#include "Arduino.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>

#include "esp_int_wdt.h"

#include "machine.h"

#include "fabgl.h"
#include "fabutils.h"

#include "i8080/i8080.h"
#include "Z80/z80emu.h"




// Altair 88-DSK Boot ROM (starts at 0xFF00)
const uint8_t Altair88DiskBootROM[256] = {
  0x21, 0x00, 0x4c, 0x11, 0x18, 0xff, 0x0e, 0xe6, 0x1a, 0x77, 0x13, 0x23,
  0x0d, 0xc2, 0x08, 0xff, 0xc3, 0x00, 0x4c, 0x00, 0x00, 0x00, 0x00, 0x00,
  0xf3, 0x31, 0x62, 0x4d, 0xaf, 0xd3, 0x08, 0x3e, 0x04, 0xd3, 0x09, 0xc3,
  0x19, 0x4c, 0xdb, 0x08, 0xe6, 0x02, 0xc2, 0x0e, 0x4c, 0x3e, 0x02, 0xd3,
  0x09, 0xdb, 0x08, 0xe6, 0x40, 0xc2, 0x0e, 0x4c, 0x11, 0x00, 0x00, 0x06,
  0x00, 0xdb, 0x08, 0xe6, 0x04, 0xc2, 0x25, 0x4c, 0x3e, 0x10, 0xf5, 0xd5,
  0xc5, 0xd5, 0x11, 0x86, 0x80, 0x21, 0xd4, 0x4c, 0xdb, 0x09, 0x1f, 0xda,
  0x38, 0x4c, 0xe6, 0x1f, 0xb8, 0xc2, 0x38, 0x4c, 0xdb, 0x08, 0xb7, 0xfa,
  0x44, 0x4c, 0xdb, 0x0a, 0x77, 0x23, 0x1d, 0xca, 0x5a, 0x4c, 0x1d, 0xdb,
  0x0a, 0x77, 0x23, 0xc2, 0x44, 0x4c, 0xe1, 0x11, 0xd7, 0x4c, 0x01, 0x80,
  0x00, 0x1a, 0x77, 0xbe, 0xc2, 0xc1, 0x4c, 0x80, 0x47, 0x13, 0x23, 0x0d,
  0xc2, 0x61, 0x4c, 0x1a, 0xfe, 0xff, 0xc2, 0x78, 0x4c, 0x13, 0x1a, 0xb8,
  0xc1, 0xeb, 0xc2, 0xb5, 0x4c, 0xf1, 0xf1, 0x2a, 0xd5, 0x4c, 0xd5, 0x11,
  0x00, 0xff, 0xcd, 0xce, 0x4c, 0xd1, 0xda, 0xbe, 0x4c, 0xcd, 0xce, 0x4c,
  0xd2, 0xae, 0x4c, 0x04, 0x04, 0x78, 0xfe, 0x20, 0xda, 0x2c, 0x4c, 0x06,
  0x01, 0xca, 0x2c, 0x4c, 0xdb, 0x08, 0xe6, 0x02, 0xc2, 0xa0, 0x4c, 0x3e,
  0x01, 0xd3, 0x09, 0xc3, 0x23, 0x4c, 0x3e, 0x80, 0xd3, 0x08, 0xc3, 0x00,
  0x00, 0xd1, 0xf1, 0x3d, 0xc2, 0x2e, 0x4c, 0x3e, 0x43, 0x01, 0x3e, 0x4f,
  0x01, 0x3e, 0x4d, 0x47, 0x3e, 0x80, 0xd3, 0x08, 0x78, 0xd3, 0x01, 0xc3,
  0xc9, 0x4c, 0x7a, 0xbc, 0xc0, 0x7b, 0xbd, 0xc9, 0x84, 0x00, 0x4c, 0x24,
  0x16, 0x56, 0x16, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
};




////////////////////////////////////////////////////////////////////////////////////
// buffered file IO routines


FILE * bufferedFile                = nullptr;
uint8_t * bufferedFileData         = nullptr;
int bufferedFileDataPos            = -1; // position of bufferedFileData related to the actual file
bool bufferedFileChanged           = false;
constexpr int bufferedFileDataSize = 4388;


void diskFlush(FILE * file = nullptr)
{
  // flush bufferedFile
  if (bufferedFileChanged && bufferedFile && bufferedFileDataPos != -1) {
    fabgl::suspendInterrupts();
    fseek(bufferedFile, bufferedFileDataPos, SEEK_SET);
    fwrite(bufferedFileData, bufferedFileDataSize, 1, bufferedFile);
    fflush(bufferedFile);
    fsync(fileno(bufferedFile));  // workaround from forums...
    fabgl::resumeInterrupts();
    bufferedFileChanged = false;
  }
  if (file) {
    // flush specified file
    fabgl::suspendInterrupts();
    fflush(file);
    fsync(fileno(file));  // workaround from forums...
    fabgl::resumeInterrupts();
  }
}


// note: size cannot be > bufferedFileDataSize
void checkFile(FILE * file, int position, int size)
{
  if (bufferedFileData == nullptr)
    bufferedFileData = new uint8_t[bufferedFileDataSize];
  if (bufferedFile != file) {
    diskFlush();
    bufferedFileDataPos = -1;
    bufferedFile = file;
  }
  if (bufferedFileDataPos == -1 || position < bufferedFileDataPos || position + size >= bufferedFileDataPos + bufferedFileDataSize) {
    fabgl::suspendInterrupts();
    diskFlush();
    fseek(file, position, SEEK_SET);
    fread(bufferedFileData, bufferedFileDataSize, 1, file);
    fabgl::resumeInterrupts();
    bufferedFileDataPos = position;
  }
}


void diskRead(int position, void * buffer, int size, FILE * file)
{
  checkFile(file, position, size);
  memcpy(buffer, bufferedFileData + position - bufferedFileDataPos, size);
}


void diskWrite(int position, void * buffer, int size, FILE * file)
{
  checkFile(file, position, size);
  memcpy(bufferedFileData + position - bufferedFileDataPos, buffer, size);
  bufferedFileChanged = true;
}



/*

// NO BUFFERING

void diskRead(int position, void * buffer, int size, FILE * file)
{
  fabgl::suspendInterrupts();
  fseek(file, position, SEEK_SET);
  fread(buffer, size, 1, file);
  fabgl::resumeInterrupts();
}


void diskWrite(int position, void * buffer, int size, FILE * file)
{
  fabgl::suspendInterrupts();
  fseek(file, position, SEEK_SET);
  fwrite(buffer, size, 1, file);
  fabgl::resumeInterrupts();
}
*/




////////////////////////////////////////////////////////////////////////////////////
// Machine


Machine::Machine()
  : m_devices(nullptr),
    m_realSpeed(false),
    m_menuCallback(nullptr)
{
}


Machine::~Machine()
{
  delete [] m_RAM;
}


void Machine::attachDevice(Device * device)
{
  device->next = m_devices;
  m_devices = device;
}


void Machine::load(int address, uint8_t const * data, int length)
{
  for (int i = 0; i < length; ++i)
    m_RAM[address + i] = data[i];
}


void Machine::attachRAM(int RAMSize)
{
  m_RAM = new uint8_t[RAMSize];
}


IRAM_ATTR int Machine::nextStep(CPU cpu, void * param)
{
  if (m_menuCallback && Keyboard.isVKDown(VirtualKey::VK_PAUSE))
    m_menuCallback();
  return (cpu == i8080 ? i8080_instruction() : Z80Emulate((Z80_STATE*)param, 0, this));
}


IRAM_ATTR void Machine::run(CPU cpu, int address)
{
  Z80_STATE Z80State;

  if (cpu == i8080) {
    i8080_init(this);
    i8080_jump(address);
  } else {
    Z80Reset(&Z80State);
    Z80State.pc = address;
  }

  while (true) {
    if (m_realSpeed) {
      int64_t t = esp_timer_get_time();  // time in microseconds
      int cycles = nextStep(cpu, &Z80State);
      if (m_realSpeed) {
        t += cycles / 2;    // at 2MHz each cycle last 0.5us, so instruction time is cycles*0.5, that is cycles/2
        while (esp_timer_get_time() < t)
          ;
      }
    } else {
      nextStep(cpu, &Z80State);
    }
  }

}


int Machine::readByte(int address)
{
  return m_RAM[address];
}


void Machine::writeByte(int address, int value)
{
  m_RAM[address] = value;
}


int Machine::readIO(int address)
{
  for (Device * d = m_devices; d; d = d->next) {
    int value;
    if (d->read(address, &value))
      return value;
  }

  // not handlded!

  //Serial.printf("readIO 0x%x\n\r", address);

  return 0xff;
}


void Machine::writeIO(int address, int value)
{
  for (Device * d = m_devices; d; d = d->next)
    if (d->write(address, value))
      return;

  // not handlded!

  //Serial.printf("writeIO 0x%x %d %c\n\r", address, value, (char)value);
}


// Machine
////////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////////
// SIO


SIO::SIO(Machine * machine, int address)
  : Device(machine), m_address(address), m_getCharPreprocessor(nullptr), m_stream(nullptr)
{
  machine->attachDevice(this);
}


void SIO::attachStream(Stream * value, GetCharPreprocessor getCharPreprocessor)
{
  m_stream = value;
  m_getCharPreprocessor = getCharPreprocessor;
}


bool SIO::read(int address, int * result)
{
  if (address == m_address) {
    // CTRL
    bool available = false;
    if (m_stream)
      available = m_stream->available();
    *result = 0b10 | (available ? 1 : 0);
    return true;
  } else if (address == m_address + 1) {
    // DATA
    int ch = 0;
    if (m_stream && m_stream->available()) {
      ch = m_stream->read();
      if (m_getCharPreprocessor)
        ch = m_getCharPreprocessor(ch);
    }
    *result = ch;
    return true;
  }
  return false;
}


bool SIO::write(int address, int value)
{
  if (address == m_address) {
    // CTRL
    return true;
  } else if (address == m_address + 1) {
    // DATA
    if (m_stream)
      m_stream->write(value);
    return true;
  }
  return false;
}

// SIO
////////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////////
// Mits88Disk disk drive controller


Mits88Disk::Mits88Disk(Machine * machine, DiskFormat diskFormat)
  : Device(machine), m_diskFormat(diskFormat), m_drive(0)
{
  switch (diskFormat) {
    // 88-Disk 8'' inches has 77 tracks and 32 sectors
    case Disk_338K:
      m_trackSize   = 32;
      m_tracksCount = 77;
      break;
    // minidisk has 35 tracks and 16 sectors
    case MiniDisk_76K:
      m_trackSize   = 16;
      m_tracksCount = 35;
      break;
  };

  machine->attachDevice(this);

  for (int i = 0; i < DISKCOUNT; ++i) {
    m_readOnlyBuffer[i]   = nullptr;
    m_fileSectorBuffer[i] = nullptr;
    m_file[i]  = nullptr;
    m_track[i] = m_sector[i] = m_sectorPositioned[i] = m_pos[i] = 0;
    m_readByteReady[i] = 1;
  }
}


Mits88Disk::~Mits88Disk()
{
  detachAll();
}


void Mits88Disk::detachAll()
{
  for (int i = 0; i < DISKCOUNT; ++i)
    detach(i);
}


void Mits88Disk::detach(int drive)
{
  flush();
  if (m_readOnlyBuffer[drive]) {
    m_readOnlyBuffer[drive] = nullptr;
  } else if (m_file[drive]) {
    fclose(m_file[drive]);
    m_file[drive] = nullptr;
    delete [] m_fileSectorBuffer[drive];
    m_fileSectorBuffer[drive] = nullptr;
  }
}


void Mits88Disk::attachReadOnlyBuffer(int drive, uint8_t const * data)
{
  detach(drive);
  m_readOnlyBuffer[drive] = data;
}


void Mits88Disk::attachFile(int drive, char const * filename)
{
  detach(drive);

  m_fileSectorBuffer[drive] = new uint8_t[SECTOR_SIZE];

  // file exists?
  struct stat st;
  if (stat(filename, &st) != 0) {
    // file doesn't exist, create and fill with 0xE5
    m_file[drive] = fopen(filename, "w+");
    memset(m_fileSectorBuffer[drive], 0xE5, SECTOR_SIZE);
    for (int i = 0; i < (int) m_trackSize * m_tracksCount; ++i)
      diskWrite(i * SECTOR_SIZE, m_fileSectorBuffer[drive], SECTOR_SIZE, m_file[drive]);
  } else {
    // file already exists, just open for read/write
    m_file[drive] = fopen(filename, "r+");
  }
}


void Mits88Disk::flush()
{
  for (int i = 0; i < DISKCOUNT; ++i)
    if (m_file[i])
      diskFlush(m_file[i]);
}


int Mits88Disk::readByteFromDisk()
{
  int value;
  const int track  = m_track[m_drive];
  const int sector = m_sector[m_drive];
  const int pos    = m_pos[m_drive];

  if (m_readOnlyBuffer[m_drive]) {
    int position = (int) m_trackSize * SECTOR_SIZE * track + sector * SECTOR_SIZE + pos;
    value = m_readOnlyBuffer[m_drive][position];
  } else if (m_file[m_drive]) {
    if (pos == 0) {
      // read the entire sector into the buffer
      int position = (int) m_trackSize * SECTOR_SIZE * track + sector * SECTOR_SIZE;
      diskRead(position, m_fileSectorBuffer[m_drive], SECTOR_SIZE, m_file[m_drive]);
    }
    value = m_fileSectorBuffer[m_drive][pos];
  } else
    value = 0xFF;

  m_pos[m_drive] = (pos + 1) % SECTOR_SIZE;

  return value;
}


void Mits88Disk::writeByteToDisk(int value)
{
  const int track  = m_track[m_drive];
  const int sector = m_sector[m_drive];
  const int pos    = m_pos[m_drive];

  // writing 138th byte after the end of sector.
  bool endOfSector = (pos== SECTOR_SIZE);

  if (m_readOnlyBuffer[m_drive]) {
    // this is read only!
  } else if (m_file[m_drive]) {
    if (endOfSector) {
      int position = (int) m_trackSize * SECTOR_SIZE * track + sector * SECTOR_SIZE;
      diskWrite(position, m_fileSectorBuffer[m_drive], SECTOR_SIZE, m_file[m_drive]);
    } else {
      m_fileSectorBuffer[m_drive][pos] = value;
    }
  }

  m_pos[m_drive] = (endOfSector ? 0 : pos + 1);
}


bool Mits88Disk::read(int address, int * result)
{
  switch (address) {
    case 0x08:
    {
      // Drive Status
      int track0 = (m_track[m_drive] == 0 ? 0 : 1);
      *result = 0b00100000 | (track0 << 6) | (m_readByteReady[m_drive] << 7);
      return true;
    }

    case 0x09:
      // Sector Number
      if (m_sectorPositioned[m_drive] == sectorPositionedDelay) {
        m_sector[m_drive] = (m_sector[m_drive] + 1) % m_trackSize;
        m_sectorPositioned[m_drive] = 0;
        m_pos[m_drive] = 0;
      } else
        ++m_sectorPositioned[m_drive];
      m_readByteReady[m_drive] = (m_sectorPositioned[m_drive] == sectorPositionedDelay ? 0 : 1);
      *result = (m_sector[m_drive] << 1) | m_readByteReady[m_drive];
      return true;

    case 0x0A:
      // read data
      *result = readByteFromDisk();
      return true;

    default:
      return false;
  }
}


bool Mits88Disk::write(int address, int value)
{
  switch (address) {

    case 0x08:
      //  Drive Select
      if ((value & 0x80) == 0)
        setDrive(value & 0xF);
      return true;

    case 0x09:
      // Drive control
      if (value & 1)
        m_track[m_drive] = (m_track[m_drive] < m_tracksCount - 1 ? m_track[m_drive] + 1 : m_tracksCount - 1);
      else if (value & 2)
        m_track[m_drive] = (m_track[m_drive] > 0 ? m_track[m_drive] - 1 : 0);
      return true;

    case 0x0A:
      // write data
      writeByteToDisk(value);
      return true;

    default:
      return false;
  }
}


void Mits88Disk::setDrive(int value)
{
  m_drive = fabgl::iclamp(value, 0, DISKCOUNT - 1);
}


void Mits88Disk::sendDiskImageToStream(int drive, Stream * stream)
{
  const int prevDrive = m_drive;
  const int prevTrack = m_track[m_drive];

  setDrive(drive);

  for (int t = 0; t < m_tracksCount; ++t) {
    m_track[m_drive] = t;
    for (int s = 0; s < m_trackSize; ++s) {
      m_sector[m_drive] = s;
      m_pos[m_drive] = 0;
      for (int b = 0; b < SECTOR_SIZE; ++b) {
        int v = readByteFromDisk();
        stream->write(v);
      }
    }
  }

  setDrive(prevDrive);
  m_track[m_drive] = prevTrack;
}



void Mits88Disk::receiveDiskImageFromStream(int drive, Stream * stream)
{
  const int prevDrive = m_drive;
  const int prevTrack = m_track[m_drive];

  setDrive(drive);

  for (int t = 0; t < m_tracksCount; ++t) {
    m_track[m_drive] = t;
    for (int s = 0; s < m_trackSize; ++s) {
      m_sector[m_drive] = s;
      m_pos[m_drive] = 0;
      for (int b = 0; b < SECTOR_SIZE; ++b) {
        while (stream->available() == 0)
          ;
        int v = stream->read();
        writeByteToDisk(v);
      }
      writeByteToDisk(0);   // flush
      stream->write(0x06);  // ACK
    }
  }

  setDrive(prevDrive);
  m_track[m_drive] = prevTrack;
}


// Mits88Disk disk drive controller
////////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////////
// required by i8080.cpp

int i8080_hal_memory_read_word(void * context, int addr)
{
  Machine * m = (Machine*)context;
  return m->readByte(addr) | (m->readByte(addr + 1) << 8);
}


void i8080_hal_memory_write_word(void * context, int addr, int word)
{
  Machine * m = (Machine*)context;
  m->writeByte(addr, word & 0xff);
  m->writeByte(addr + 1, (word >> 8) & 0xff);
}


int i8080_hal_memory_read_byte(void * context, int addr)
{
  Machine * m = (Machine*)context;
  return m->readByte(addr);
}


void i8080_hal_memory_write_byte(void * context, int addr, int byte)
{
  Machine * m = (Machine*)context;
  m->writeByte(addr, byte);
}


int i8080_hal_io_input(void * context, int port)
{
  Machine * m = (Machine*)context;
  return m->readIO(port);
}


void i8080_hal_io_output(void * context, int port, int value)
{
  Machine * m = (Machine*)context;
  m->writeIO(port, value);
}


void i8080_hal_iff(void * context, int on)
{
  //printf("i8080_hal_iff %d\n", on);
}


////////////////////////////////////////////////////////////////////////////////////
