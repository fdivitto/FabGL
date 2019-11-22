#include "Arduino.h"  // TO REMOVE!!

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>

#include "altair.h"

#include "fabgl.h"

#include "i8080.h"





// Altair MITS standard BOOT EPROM
// Got from SIMH
// Copyright (c) 1997-2012, Charles E. Owen
const uint8_t AltairBootROM[] = {
    0041, 0000, 0114, 0021, 0030, 0377, 0016, 0346,
    0032, 0167, 0023, 0043, 0015, 0302, 0010, 0377,
    0303, 0000, 0114, 0000, 0000, 0000, 0000, 0000,
    0363, 0061, 0142, 0115, 0257, 0323, 0010, 0076,
    0004, 0323, 0011, 0303, 0031, 0114, 0333, 0010,
    0346, 0002, 0302, 0016, 0114, 0076, 0002, 0323,
    0011, 0333, 0010, 0346, 0100, 0302, 0016, 0114,
    0021, 0000, 0000, 0006, 0000, 0333, 0010, 0346,
    0004, 0302, 0045, 0114, 0076, 0020, 0365, 0325,
    0305, 0325, 0021, 0206, 0200, 0041, 0324, 0114,
    0333, 0011, 0037, 0332, 0070, 0114, 0346, 0037,
    0270, 0302, 0070, 0114, 0333, 0010, 0267, 0372,
    0104, 0114, 0333, 0012, 0167, 0043, 0035, 0312,
    0132, 0114, 0035, 0333, 0012, 0167, 0043, 0302,
    0104, 0114, 0341, 0021, 0327, 0114, 0001, 0200,
    0000, 0032, 0167, 0276, 0302, 0301, 0114, 0200,
    0107, 0023, 0043, 0015, 0302, 0141, 0114, 0032,
    0376, 0377, 0302, 0170, 0114, 0023, 0032, 0270,
    0301, 0353, 0302, 0265, 0114, 0361, 0361, 0052,
    0325, 0114, 0325, 0021, 0000, 0377, 0315, 0316,
    0114, 0321, 0332, 0276, 0114, 0315, 0316, 0114,
    0322, 0256, 0114, 0004, 0004, 0170, 0376, 0040,
    0332, 0054, 0114, 0006, 0001, 0312, 0054, 0114,
    0333, 0010, 0346, 0002, 0302, 0240, 0114, 0076,
    0001, 0323, 0011, 0303, 0043, 0114, 0076, 0200,
    0323, 0010, 0303, 0000, 0000, 0321, 0361, 0075,
    0302, 0056, 0114, 0076, 0103, 0001, 0076, 0117,
    0001, 0076, 0115, 0107, 0076, 0200, 0323, 0010,
    0170, 0323, 0001, 0303, 0311, 0114, 0172, 0274,
    0300, 0173, 0275, 0311, 0204, 0000, 0114, 0044,
    0026, 0126, 0026, 0000, 0000, 0000, 0000, 0000
};




////////////////////////////////////////////////////////////////////////////////////
// Machine


Machine::Machine(int RAMSize)
  : m_RAMSize(RAMSize),
    m_devices(nullptr),
    m_run(false)
{
  m_RAM = new uint8_t[m_RAMSize];
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


void Machine::run(int address)
{
  i8080_init(this);
  i8080_jump(address);

  m_run = true;

  while (m_run)
    i8080_instruction();
}


void Machine::stop()
{
  m_run = false;
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
  Serial.printf("readIO 0x%x\n\r", address);
  return 0xff;
}


void Machine::writeIO(int address, int value)
{
  for (Device * d = m_devices; d; d = d->next)
    if (d->write(address, value))
      return;

  // not handlded!
  Serial.printf("writeIO 0x%x %d %c\n\r", address, value, (char)value);
}

// Machine
////////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////////
// SIO
// TODO: at the moment always uses STDIO


SIO::SIO(Machine * machine, int address)
  : Device(machine), m_address(address), m_terminal(nullptr)
{
  machine->attachDevice(this);
}


void SIO::attachTerminal(TerminalClass * value)
{
  m_terminal = value;
}


bool SIO::read(int address, int * result)
{
  if (address == m_address) {
    // CTRL
    bool available = false;
    if (m_terminal)
      available = m_terminal->available();
    *result = 0b10 | (available ? 1 : 0);
    return true;
  } else if (address == m_address + 1) {
    // DATA
    int ch = 0;
    if (m_terminal)
      ch = m_terminal->read();
    //if (ch == 0x0A) ch = 0x0D;     // LF -> CR
    if (ch == 0x7F) ch = 0x08;     // DEL -> BACKSPACE
    //if (ch == 0x1B) machine()->stop(); // ESC -> stop machine
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
    if (m_terminal)
      m_terminal->write(value);
    return true;
  }
  return false;
}

// SIO
////////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////////
// Mits88Disk disk drive controller

Mits88Disk::Mits88Disk(Machine * machine)
  : Device(machine), m_drive(0)
{
  machine->attachDevice(this);
  for (int i = 0; i < DISKCOUNT; ++i) {
    m_readOnlyBuffer[i] = nullptr;
    m_fileSectorBuffer[i] = nullptr;
    m_file[i] = nullptr;
    m_track[i] = m_sector[i] = m_sectorPositioned[i] = m_pos[i] = 0;
    m_readByteReady[i] = 1;
  }
}


Mits88Disk::~Mits88Disk()
{
  for (int i = 0; i < DISKCOUNT; ++i)
    detach(i);
}


void Mits88Disk::detach(int drive)
{
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
    VGAController.suspendBackgroundPrimitiveExecution();
    for (int i = 0; i < TRACK_SIZE * TRACKS_COUNT; ++i)
      fwrite(m_fileSectorBuffer[drive], SECTOR_SIZE, 1, m_file[drive]);
    fflush(m_file[drive]);
    VGAController.resumeBackgroundPrimitiveExecution();
  } else {
    // file already exists, just open for read/write
    m_file[drive] = fopen(filename, "r+");
  }
}


int Mits88Disk::readByteFromDisk()
{
  int value;
  if (m_readOnlyBuffer[m_drive]) {
    int position = TRACK_SIZE * SECTOR_SIZE * m_track[m_drive] + m_sector[m_drive] * SECTOR_SIZE + m_pos[m_drive];
    value = m_readOnlyBuffer[m_drive][position];
  } else if (m_file[m_drive]) {
    if (m_pos[m_drive] == 0) {
      // read the entire sector into the buffer
      int position = TRACK_SIZE * SECTOR_SIZE * m_track[m_drive] + m_sector[m_drive] * SECTOR_SIZE;
      fseek(m_file[m_drive], position, SEEK_SET);
      fread(m_fileSectorBuffer[m_drive], SECTOR_SIZE, 1, m_file[m_drive]);
    }
    value = m_fileSectorBuffer[m_drive][m_pos[m_drive]];
  } else
    value = 0xFF;

  m_pos[m_drive] = (m_pos[m_drive] == SECTOR_SIZE ? 0 : m_pos[m_drive] + 1);

  return value;
}


void Mits88Disk::writeByteToDisk(int value)
{
  bool endOfSector = (m_pos[m_drive] == SECTOR_SIZE);  // writing 138th byte after the end of sector...ignore and flush

  if (m_readOnlyBuffer[m_drive]) {
    // this is read only!
  } else if (m_file[m_drive]) {
    if (endOfSector) {
      int position = TRACK_SIZE * SECTOR_SIZE * m_track[m_drive] + m_sector[m_drive] * SECTOR_SIZE;
      fseek(m_file[m_drive], position, SEEK_SET);
      VGAController.suspendBackgroundPrimitiveExecution();
      fwrite(m_fileSectorBuffer[m_drive], SECTOR_SIZE, 1, m_file[m_drive]);
      fflush(m_file[m_drive]);
      VGAController.resumeBackgroundPrimitiveExecution();
    } else {
      m_fileSectorBuffer[m_drive][m_pos[m_drive]] = value;
    }
  }

  m_pos[m_drive] = (endOfSector ? 0 : m_pos[m_drive] + 1);
}


bool Mits88Disk::read(int address, int * result)
{
  //printf("*** Mits88Disk::read 0x%x\n", address);
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
      if (m_sectorPositioned[m_drive] == sectorPositionedMax) {
        m_sector[m_drive] = (m_sector[m_drive] < 31 ? m_sector[m_drive] + 1 : 0);
        m_sectorPositioned[m_drive] = 0;
        m_pos[m_drive] = 0;
      } else
        ++m_sectorPositioned[m_drive];
      m_readByteReady[m_drive] = (m_sectorPositioned[m_drive] == sectorPositionedMax ? 0 : 1);
      *result = (m_sector[m_drive] << 1) | (m_sectorPositioned[m_drive] == sectorPositionedMax ? 0 : 1);
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
  //printf("*** Mits88Disk::write 0x%x <= 0x%x\n", address, value);
  switch (address) {

    case 0x08:
      //  Drive Select
      if ((value & 0x80) == 0) {
        m_drive = value & 0xF;
        if (m_drive >= DISKCOUNT)
          m_drive = DISKCOUNT - 1;
      }
      return true;

    case 0x09:
      // Drive control
      if (value & 1)
        m_track[m_drive] = (m_track[m_drive] < TRACKS_COUNT - 1 ? m_track[m_drive] + 1 : TRACKS_COUNT - 1);
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
