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


/*
   This is a basic implementation of IEC and Commodore DOS, used just to LOAD and SAVE programs, and to list directory content.
   Future improvements may add command channel and other file types (SEQ, REL...).
*/


#pragma once


#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>


#define DEBUGIEC 0


class Machine;
class PRGCreator;



class IECDrive {

  static constexpr int COMMANDBUFFERMAXLEN = 256;
  static constexpr int CHANNELSCOUNT       = 16;

  // link layer state (layer 1-2)
  enum class LinkState {
    Idle,
    SenderLookingForDevices,
    ReadyToReceive,
    WaitRecvDataValid,
    WaitRecvDataInvalid,
    DataAccepted,
    GoToIdle,
    TurnAround,
    TurnAroundReady,
    ReadyToSend,
    WaitReceiverReady,
    SendDataInvalid,
    SendDataValid,
    WaitDataAccepted,
    WaitEndOfEOIAck,
  };

  // arbitration layer state (layer 3)
  enum class ArbState {
    Idle,
    Listen,
    Open,
    Close,
    Talk,
  };

  enum class ChannelType {
    Closed,
    ReadDirectory,
    ReadPRG,
    WritePRG,
  };

public:

  IECDrive(Machine * machine, int deviceNum);
  ~IECDrive();

  void reset();

  void tick(int cycles);

  bool isActive()               { return m_inputATN || m_arbState != ArbState::Idle; }

  void setInputDATA(bool value) { m_inputDATA = value; }
  void setInputCLK(bool value)  { m_inputCLK  = value; }
  void setInputATN(bool value)  { m_inputATN  = value; }


private:

  void setDATA(bool value);
  void setCLK(bool value);

  void changeLinkState(LinkState newState);

  void changeArbState(ArbState newState);

  void processByte(uint8_t value);

  void processOPEN();
  void processCLOSE();
  void processWRITE();

  void fetchNextByteToSend();

  void prepareDirectoryRead();
  void finalizeDirectoryRead();


  Machine *    m_machine;

  uint8_t      m_deviceNum;     // this device number (8, 9...)

  // input signals - true = pulled-down, false = released (pulled-up)
  bool         m_inputATN;
  bool         m_inputDATA;
  bool         m_inputCLK;

  // previous state of ATN
  bool         m_prevATN;

  // link layer state
  LinkState    m_linkState;
  bool         m_isLastByte;
  bool         m_isEmpty;
  int          m_linkStateCycles;
  int          m_dataBit;
  uint8_t      m_curByte;

  // arbitration layer state
  ArbState     m_arbState;
  int          m_channel;                        // current listen/talk channel (-1 = not assigned)
  FILE *       m_files[CHANNELSCOUNT];           // associated file for each channel
  size_t       m_channelCounter[CHANNELSCOUNT];  // bytes counter used on talking mode
  ChannelType  m_channelType[CHANNELSCOUNT];
  uint8_t      m_commandBuffer[COMMANDBUFFERMAXLEN + 1];
  int          m_commandBufferLen;

  // directory support
  PRGCreator * m_PRGCreator;
  bool         m_shortDir;

};
