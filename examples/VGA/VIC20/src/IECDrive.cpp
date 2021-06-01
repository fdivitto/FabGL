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


#include "IECDrive.h"
#include "machine.h"

#include <string.h>
#include <stdio.h>


#pragma GCC optimize ("O2")


using fabgl::imin;
using fabgl::imax;


#if DEBUGIEC
extern int testTiming;
#endif


IECDrive::IECDrive(Machine * machine, int deviceNum)
  : m_machine(machine),
    m_deviceNum(deviceNum),
    m_PRGCreator(nullptr)
{
  for (int i = 0; i < CHANNELSCOUNT; ++i) {
    m_files[i]       = nullptr;
    m_channelType[i] = ChannelType::Closed;
  }
}


IECDrive::~IECDrive()
{
  finalizeDirectoryRead();
}


void IECDrive::reset()
{
  setDATA(false);
  setCLK(false);

  m_inputATN  = false;
  m_inputDATA = false;
  m_inputCLK  = false;

  m_prevATN   = false;

  changeLinkState(LinkState::Idle);
  changeArbState(ArbState::Idle);

  m_channel = -1;
  m_commandBufferLen = 0;

  finalizeDirectoryRead();

  m_shortDir = true;
}


// value: true = pull-down,  false = release (pull-up)
void IECDrive::setDATA(bool value)
{
  m_machine->VIA1().setBitPA(1, !value);
}


// value: true = pull-down,  false = release (pull-up)
void IECDrive::setCLK(bool value)
{
  m_machine->VIA1().setBitPA(0, !value);
}


void IECDrive::changeLinkState(LinkState newState)
{
  m_linkState = newState;
  m_linkStateCycles = 0;
}


void IECDrive::tick(int cycles)
{

  if (!m_prevATN && m_inputATN == true) {
    m_prevATN = true;
    setDATA(true);
    setCLK(false);
    changeLinkState(LinkState::SenderLookingForDevices);
    //printf("ATN true\n");
  } else if (m_prevATN && m_inputATN == false) {
    m_prevATN = false;
    //printf("ATN false\n");
  }

  if (!m_inputATN && m_arbState == ArbState::Idle)
    return;

  m_linkStateCycles += cycles;

  switch (m_linkState) {

    case LinkState::Idle:
      if (m_inputCLK == true) {
        // sender is checking devices presence
        setDATA(true);    // signal I'm here!
        changeLinkState(LinkState::SenderLookingForDevices);
      }
      break;

    case LinkState::SenderLookingForDevices:
      if (m_inputCLK == false) {
        // sender is ready to send, signal we are ready to receive
        setDATA(false);
        m_isLastByte = false;
        changeLinkState(LinkState::ReadyToReceive);
      }
      break;

    case LinkState::ReadyToReceive:
      if (m_linkStateCycles > 60 && m_isLastByte) {
        // after 60us, end of EOI ack
        setDATA(false);
        changeLinkState(LinkState::ReadyToReceive);
      } else if (m_linkStateCycles > 256 && !m_isLastByte) {
        // after 256us, start of EOI ack
        setDATA(true);
        m_isLastByte = true;
        changeLinkState(LinkState::ReadyToReceive); // just to reset cycles counter
      } else if (m_inputCLK == true) {
        // sender holds CLK
        changeLinkState(LinkState::WaitRecvDataValid);
        m_dataBit = 0;
        m_curByte = 0;
      } else if (m_linkStateCycles > 512) {
        // after 512us, timeout
        changeLinkState(LinkState::GoToIdle);
      }
      break;

    case LinkState::WaitRecvDataValid:
      if (m_inputCLK == false) {
        // CLK release, data valid
        m_curByte |= (int)!m_inputDATA << m_dataBit;
        ++m_dataBit;
        changeLinkState(LinkState::WaitRecvDataInvalid);
      } else if (m_linkStateCycles > 1024) {
        // timeout
        changeLinkState(LinkState::GoToIdle);
      }
      break;

    case LinkState::WaitRecvDataInvalid:
      if (m_inputCLK == true) {
        // CLK hold, data invalid
        changeLinkState(m_dataBit == 8 ? LinkState::DataAccepted : LinkState::WaitRecvDataValid);
      } else if (m_linkStateCycles > 1024) {
        // timeout
        changeLinkState(LinkState::GoToIdle);
      }
      break;

    case LinkState::DataAccepted:
      if (m_inputDATA == false) {
        setDATA(true);                  // frame handshake
        changeLinkState(m_isLastByte ? LinkState::GoToIdle : LinkState::SenderLookingForDevices);
        processByte(m_curByte);
      }
      break;

    case LinkState::GoToIdle:
      if (m_linkStateCycles >= 60) {
        changeLinkState(LinkState::Idle);
        setDATA(false);
        setCLK(false);
      }
      break;

    case LinkState::TurnAround:
      if (m_inputCLK == false) {
        setDATA(false);
        setCLK(true);
        changeLinkState(LinkState::TurnAroundReady);
      }
      break;

    case LinkState::TurnAroundReady:
      if (m_linkStateCycles >= 100) {
        setCLK(false);  // signal we are ready to send
        changeLinkState(LinkState::WaitReceiverReady);
      }
      break;

    case LinkState::WaitReceiverReady:
      if (m_inputDATA == false) {
        // receiver is ready to receive
        changeLinkState(LinkState::ReadyToSend);
        fetchNextByteToSend();
        m_dataBit = 0;
      }
      break;

    case LinkState::ReadyToSend:
      if (m_isEmpty) {
        if (m_linkStateCycles > 512) {
          changeLinkState(LinkState::GoToIdle);
        }
      } else if (m_isLastByte) {
        if (m_linkStateCycles > 200 && m_inputDATA == true) {
          // wait EOI ack release
          changeLinkState(LinkState::WaitEndOfEOIAck);
        }
      } else {
        if (m_linkStateCycles > 60) {
          setCLK(true);
          changeLinkState(LinkState::SendDataInvalid);
        }
      }
      break;

    case LinkState::WaitEndOfEOIAck:
      if (m_inputDATA == false) {
        setCLK(true);
        changeLinkState(LinkState::SendDataInvalid);
      }
      break;

    case LinkState::SendDataInvalid:
      if (m_linkStateCycles > 60) {
        setCLK(false);
        setDATA(!((m_curByte >> m_dataBit) & 1));
        ++m_dataBit;
        changeLinkState(LinkState::SendDataValid);
      }
      break;

    case LinkState::SendDataValid:
      if (m_linkStateCycles > 60) {
        setCLK(true);
        if (m_dataBit == 8) {
          setDATA(false);
          changeLinkState(LinkState::WaitDataAccepted);
        } else {
          changeLinkState(LinkState::SendDataInvalid);
        }
      }
      break;

    case LinkState::WaitDataAccepted:
      if (m_inputDATA == true) {
        if (m_isLastByte)
          changeLinkState(LinkState::GoToIdle);
        else
          changeLinkState(LinkState::TurnAroundReady);
      }
      break;

  }
}


void IECDrive::changeArbState(ArbState newState)
{
  m_arbState = newState;
}


/*

  aaaaa : device address (0..31). Valid address range from 0 to 30. Device 31 (0b11111) means UNTALK or UNLISTEN.
  sssss : secondary address - channel (0..31)

  001aaaaa : LISTEN or UNLISTEN
  010aaaaa : TALK or UNTALK

  011sssss : SECONDARY ADDRESS - CHANNEL. Can follow LISTEN/TALK/UNTALK (with ATN true)

  Associate channel (secondary address) with a name:
    1)     LISTEN
    2)     1111ssss : OPEN NAMED CHANNEL (secondary address restricted to 0..15)
    3...n) Name as string of chars
    last)  UNLISTEN

  Dissociate channel (secondary address) from a name:
    1)     LISTEN
    2)     1110ssss : CLOSE NAMED CHANNEL (secondary address restricted to 0..15)
    3)     UNLISTEN

  Channels meaning:
    0     : named PRG read
    1     : named PRG write
    2..14 : named
    15    : command / status

*/
void IECDrive::processByte(uint8_t value)
{
  //printf("%02X %s\n", m_curByte, m_isLastByte ? "(last byte)" : "");

  if (m_inputATN) {

    // Attention mode
    int cmd  = value & 0xe0;  // higher 3 bits
    int addr = value & 0x1f;  // lower 5 bits
    switch (cmd) {

      // LISTEN / UNLISTEN
      case 0x20:
        if (addr == 31) {
          // UNLISTEN
          //printf("  UNLISTEN\n");
          if (m_arbState == ArbState::Open && m_commandBufferLen > 0)
            processOPEN();
          else if (m_arbState == ArbState::Close)
            processCLOSE();
          else
            processWRITE();
          changeArbState(ArbState::Idle);
        } else if (addr == m_deviceNum) {
          // LISTEN
          //printf("  LISTEN\n");
          changeArbState(ArbState::Listen);
        }
        m_channel = -1;
        m_commandBufferLen = 0;
        break;

      // TALK / UNTALK
      case 0x40:
        m_channel = -1;
        if (addr == m_deviceNum) {
          // TALK
          //printf("  TALK\n");
          changeArbState(ArbState::Talk);
        } else {
          // UNTALK
          //printf("  UNTALK\n");
          changeArbState(ArbState::Idle);
        }
        break;

      // unnamed channel (SECONDARY ADDRESS)
      case 0x60:
        if (m_arbState != ArbState::Idle) {
          // unnamed channel specified
          m_channel = addr;
          //printf("  CHANNEL %d\n", m_channel);
          if (m_arbState == ArbState::Talk)
            changeLinkState(LinkState::TurnAround);
        }
        break;

      // named channel (OPEN/CLOSE)
      case 0xe0:
        m_channel = addr & 0xf;
        if (addr & 0x10) {
          // OPEN
          changeArbState(ArbState::Open); // wait for name
          //printf("  OPEN %d\n", m_channel);
        } else {
          // CLOSE
          //printf("  CLOSE %d\n", m_channel);
          changeArbState(ArbState::Close);
        }

    }

    return;
  }

  // receiving characters on non-attention mode

  if (m_arbState == ArbState::Open || m_arbState == ArbState::Listen) {
    // receive data or channel name (command, filename, etc...)
    if (m_commandBufferLen < COMMANDBUFFERMAXLEN) {
      m_commandBuffer[m_commandBufferLen++] = value;
      m_commandBuffer[m_commandBufferLen] = 0;
    }
  }
}


void IECDrive::processOPEN()
{
  //printf("OPEN %d: \"%s\"\n", m_channel, m_commandBuffer);
  auto filename = (char *)m_commandBuffer;
  if (m_channel == 0 && filename[0] == '$') {
    // directory
    m_channelType[m_channel] = ChannelType::ReadDirectory;
    prepareDirectoryRead();
    m_isEmpty = false;
  } else if (m_channel <= 1) {
    // PRG read or write
    m_channelType[m_channel] = (m_channel == 0 ? ChannelType::ReadPRG : ChannelType::WritePRG);
    strcat(filename, ".PRG");
    m_files[m_channel] = m_machine->fileBrowser()->openFile(filename, m_channelType[m_channel] == ChannelType::ReadPRG ? "rb" : "wb");
    m_isEmpty = (m_files[m_channel] == nullptr);
    if (!m_isEmpty) {
      // get file size (bytes to send)
      fseek(m_files[m_channel], 0, SEEK_END);
      m_channelCounter[m_channel] = ftell(m_files[m_channel]);
      fseek(m_files[m_channel], 0, SEEK_SET);
    }
  } else {
    // other open cases
    // @TODO
  }
}


void IECDrive::processCLOSE()
{
  //printf("CLOSE %d\n", m_channel);

  switch (m_channelType[m_channel]) {

    case ChannelType::ReadDirectory:
      finalizeDirectoryRead();
      break;

    case ChannelType::ReadPRG:
    case ChannelType::WritePRG:
      fclose(m_files[m_channel]);
      m_files[m_channel] = nullptr;
      break;

    case ChannelType::Closed:
      // Should not happen
      break;
  }
  m_channelType[m_channel] = ChannelType::Closed;
}


void IECDrive::processWRITE()
{
  /*
  printf("WRITE %d: ", m_channel);
  for (int i = 0; i < m_commandBufferLen; ++i)
    printf("%02X ", m_commandBuffer[i]);
  printf("\n");
  */

  switch (m_channelType[m_channel]) {

    case ChannelType::WritePRG:
      fwrite(m_commandBuffer, 1, m_commandBufferLen, m_files[m_channel]);
      break;

    case ChannelType::Closed:
    case ChannelType::ReadPRG:
    case ChannelType::ReadDirectory:
      // should not happen
      break;
  }
}


// setup m_curByte and m_isLastByte
void IECDrive::fetchNextByteToSend()
{
  switch (m_channelType[m_channel]) {

    case ChannelType::ReadDirectory:
      m_curByte = m_PRGCreator->get()[ m_channelCounter[m_channel] ];
      m_channelCounter[m_channel] += 1;
      m_isLastByte = (m_channelCounter[m_channel] == m_PRGCreator->len());
      break;

    case ChannelType::ReadPRG:
      if (m_files[m_channel]) {
        m_curByte = fgetc(m_files[m_channel]);
        m_channelCounter[m_channel] -= 1;
        m_isLastByte = (m_channelCounter[m_channel] == 0);
      }
      break;

    case ChannelType::WritePRG:
    case ChannelType::Closed:
      // should not happen
      break;
  }
  //printf(" fetchNextByteToSend, m_curByte=%02X, m_isLastByte=%d\n", m_curByte, m_isLastByte);
}


void ASCII2PET(char * str)
{
  for (; *str; ++str) {
    *str = toupper(*str);
  }
}


void IECDrive::prepareDirectoryRead()
{
  finalizeDirectoryRead();

  auto fileBrowser = m_machine->fileBrowser();
  fileBrowser->reload();
  m_PRGCreator = new PRGCreator(0x0401);
  char linebuf[32];

  // header
  memset(linebuf, ' ', 25);
  linebuf[0] = 0x12;  // string reverse tag
  linebuf[1] = '"';   // initial quote
  auto cdir = fileBrowser->directory();
  cdir = strrchr(cdir, '/') + 1;
  if (!cdir)
    cdir = fileBrowser->directory();
  memcpy(linebuf + 2, cdir, imin(16, (int)strlen(cdir)));
  linebuf[18] = '"';  // ending quote
  if (m_shortDir) {
    linebuf[19] = 0;
  } else {
    linebuf[23] = '2';  // ver
    linebuf[24] = 'A';  // ver
    linebuf[25] = 0;
  }
  ASCII2PET(linebuf);
  m_PRGCreator->addline(0, linebuf);

  // files
  for (int i = 0; i < fileBrowser->count(); ++i) {
    memset(linebuf, ' ', 27);
    auto filename = fileBrowser->get(i)->name;
    auto len = strlen(filename);
    int blocks = 0;
    char const * ext = nullptr;
    if (fileBrowser->get(i)->isDir) {
      // directory
      if (filename[0] == '.')
        continue;
      ext = ".DIR";
    } else {
      // file
      blocks = imax(1, (int) fileBrowser->fileSize(filename) / 256);
      ext = strchr(filename, '.');
      if (ext)
        len -= strlen(ext);
    }
    len = imin((m_shortDir ? 10 : 16), (int)len);
    int bs = blocks > 9 ? (blocks > 99 ? 1 : 2) : 3;
    int p = bs;
    linebuf[p++] = '"';
    memcpy(linebuf + p, filename, len);
    p += len;
    linebuf[p++] = '"';
    if (ext)
      strcpy(linebuf + (m_shortDir ? 13 : 19) + bs, ext + 1);
    else
      linebuf[p] = 0;

    ASCII2PET(linebuf);
    m_PRGCreator->addline(blocks, linebuf);
  }

  m_channelCounter[m_channel] = 0;
}


void IECDrive::finalizeDirectoryRead()
{
  if (m_PRGCreator)
    delete(m_PRGCreator);
  m_PRGCreator = nullptr;
}

