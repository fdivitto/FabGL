/*
  Created by Fabrizio Di Vittorio (fdivitto2013@gmail.com) - www.fabgl.com
  Copyright (c) 2019-2021 Fabrizio Di Vittorio.
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



#include "CCP.h"
#include "supervisor.h"


#ifdef HAS_WIFI
  #include "network/ICMP.h"
#endif


#pragma GCC optimize ("O2")


// statically allocated variables

#define CCP_STATIC_VARS_ADDR   TPA_ADDR
#define CCP_STATIC_VARS_GAP    128

#define CCP_CONSOLEBUFFER_ADDR CCP_STATIC_VARS_ADDR + CCP_STATIC_VARS_GAP
#define CCP_CONSOLEBUFFER_SIZE 128

#define CCP_PFCB_ADDR          CCP_CONSOLEBUFFER_ADDR + CCP_CONSOLEBUFFER_SIZE
#define CCP_PFCB_SIZE          4

#define CCP_FCB1_ADDR          CCP_PFCB_ADDR + CCP_PFCB_SIZE
#define CCP_FCB1_SIZE          36

#define CCP_FCB2_ADDR          CCP_FCB1_ADDR + CCP_FCB1_SIZE
#define CCP_FCB2_SIZE          36

#define CCP_OUTSTRBUF_ADDR     CCP_FCB2_ADDR + CCP_FCB2_SIZE
#define CCP_OUTSTRBUF_SIZE     128

#define CCP_DMA1_ADDR          CCP_OUTSTRBUF_ADDR + CCP_OUTSTRBUF_SIZE
#define CCP_DMA1_SIZE          128

#define CCP_DMA2_ADDR          CCP_DMA1_ADDR + CCP_DMA1_SIZE
#define CCP_DMA2_SIZE          128



constexpr int COMMANDSCOUNT = 24;

static const struct {
  char const * name;
  char const * desc;
} CMDS[COMMANDSCOUNT] = {
  { "<DIR     >", "Directory view." },
  { "<LS      >", "Colored directory view." },
  { "<CD      >", "Display/changes the current directory." },
  { "<ERA     >", "Removes one or more files (aliases: \"ERASE\", \"DELETE\", \"DEL\", \"RM\")." },
  { "<HELP    >", "Show help (alias: \"?\")." },
  { "<RENAME  >", "Renames on or more files (alias: \"REN\")." },
  { "<TYPE    >", "Display the contents of a text file (alias: \"cat\")." },
  { "<PATH    >", "Get/set locations where to look for programs." },
  { "<MKDIR   >", "Make directory (alias: \"md\")." },
  { "<RMDIR   >", "Remove directory." },
  { "<COPY    >", "Improved file copy (alias: \"cp\")." },
  { "<INFO    >", "Show system info." },
  { "<DINFO   >", "Show debug info." },
  { "<REBOOT  >", "Restart system." },
  { "<TERM    >", "Select a terminal session." },
  { "<EXIT    >", "Exit current or specified session." },
  { "<EMU     >", "Select terminal emulation type." },
  { "<KEYB    >", "Change keyboard layout." },
  { "<WIFISCAN>", "Scan for WiFi networks." },
  { "<WIFI    >", "Connect to WiFi network." },
  { "<PING    >", "Ping an host." },
  { "<TELNET  >", "Open a Telnet session to a host." },
  { "<FORMAT  >", "Erase SPIFFS or SD Card and restore programs." },

  { "<F1...F12>", "Use function keys to create or switch sessions." },
};




CCP::CCP(HAL * hal, BDOS * bdos)
  : m_HAL(hal),
    m_BDOS(bdos),
    m_defaultTerminalType(DEFAULT_TERMINAL_TYPE),
    m_termCtrl(hal->terminal())
{
}


CCP::~CCP()
{
}


void CCP::run()
{
  m_exitSystem = false;

  // support for multiple commands in a line (separated by "!", example "dir *.com!dir*.sub")
  char * multicmd = nullptr;

  while (!m_exitSystem) {

    if (m_HAL->aborting())
      break;

    // setup stack
    m_HAL->CPU_setStackPointer(m_BDOS->getTPATop());

    // CCP wants BDOS errors displayed
    m_BDOS->SCB_setByte(SCB_ERRORMODE_B, 0xFE);

    // flag to signal "CCP running"
    m_BDOS->SCB_setBit(SCB_CCPFLAGS2_B, SCB_CCPFLAGS2_CCPPRESENT);

    uint16_t cmdlineAddr = CCP_CONSOLEBUFFER_ADDR;
    size_t len;

    if (!m_BDOS->SCB_testBit(SCB_CCPFLAGS3_B, SCB_CCPFLAGS3_COLDSTART)) {

      // cold start, try to execute PROFILE.SUB

      m_HAL->copyStr(cmdlineAddr, "PROFILE.SUB");
      len = strlen("PROFILE.SUB");

    } else if (m_BDOS->SCB_testBit(SCB_CCPFLAGS1_B, SCB_CCOFLAGS1_CHAININING)) {

      // chaining

      m_BDOS->SCB_clearBit(SCB_CCPFLAGS1_B, SCB_CCOFLAGS1_CHAININING);
      m_HAL->copyStr(cmdlineAddr, PAGE0_DMA);
      len = m_HAL->strLen(cmdlineAddr);

    } else {

      consoleOut("\r\n");

      // submitting (RSX - GET active)?
      if (!m_BDOS->SCB_testBit(SCB_CCPFLAGS2_B, SCB_CCPFLAGS2_SUBMIT)) {
        //// NOT submitting

        // reset program environment
        m_BDOS->resetProgramEnv();

        // reset page mode to its default value
        m_BDOS->SCB_setByte(SCB_PAGEMODE_B, m_BDOS->SCB_getByte(SCB_DEFAULTPAGEMODE_B));

        // reset error code
        m_BDOS->SCB_setWord(SCB_PROGRAMRETCODE_W, 0x0000);

        // close all zombie files
        m_BDOS->closeAllFiles();

        // release unused memory
        m_HAL->releaseMem(TPA_ADDR, m_BDOS->getTPATop());

        // reset terminal type
        m_HAL->setTerminalType(m_defaultTerminalType);

        // set default char style
        m_termCtrl.setForegroundColor(Color::BrightGreen);
        m_termCtrl.setBackgroundColor(Color::Black);
        m_termCtrl.setCharStyle(CharStyle::Bold, false);
      }

      // prompt
      consoleOutChar('A' + m_BDOS->getCurrentDrive());
      auto curdir = m_BDOS->getCurrentDir();
      if (strlen(curdir) > 0) {
        consoleOutChar(':');
        consoleOut(curdir);
      }
      consoleOutChar('>');

      m_HAL->writeByte(cmdlineAddr + 0, CCP_CONSOLEBUFFER_SIZE - 1); // max len
      m_HAL->writeByte(cmdlineAddr + 1, 0);
      m_HAL->writeByte(cmdlineAddr + 2, 0);

      if (multicmd) {

        // get input from multiple commands (multicmd)
        m_HAL->copyStr(cmdlineAddr + 2, multicmd);
        m_HAL->writeByte(cmdlineAddr + 1, strlen(multicmd));
        free(multicmd);
        multicmd = nullptr;
        consoleOut(cmdlineAddr + 2);

      } else {

        // get input from console
        m_BDOS->BDOS_callReadConsoleBuffer(CCP_CONSOLEBUFFER_ADDR);

      }

      len = m_HAL->readByte(cmdlineAddr + 1);
      cmdlineAddr += 2;      // bypass maxlen and string length
      m_HAL->writeByte(cmdlineAddr + len, 0);  // set ending zero

      // bypass heading spaces
      while (isspace(m_HAL->readByte(cmdlineAddr))) {
        ++cmdlineAddr;
        --len;
      }

      // is this a comment (starts with semicolon)?
      if (m_HAL->readByte(cmdlineAddr) == ';')
        continue; // yes, get another line

      // is this a conditional execution command (starts with colon)?
      if (m_HAL->readByte(cmdlineAddr) == ':') {
        // don't execute if last return code is not zero
        if (m_BDOS->SCB_getWord(SCB_PROGRAMRETCODE_W) != 0)
          continue;
        // bypass ':'
        ++cmdlineAddr;
        --len;
      }

      // uppercase command (up to first space), detect and separate multiple commands (avoid double '!!')
      bool spcFound = false;
      for (int i = 0; i < len; ++i) {
        auto c = m_HAL->readByte(cmdlineAddr + i);
        if (c == '!') {
          if (m_HAL->readByte(cmdlineAddr + i + 1) != '!') {
            // found multiple commands separator, split first command from the others
            multicmd = (char *) malloc(len - i + 1);
            m_HAL->copyStr(multicmd, cmdlineAddr + i + 1);
            len = i;
            m_HAL->writeByte(cmdlineAddr + len, 0);
            break;
          } else {
            // found double "!!", convert to single
            m_HAL->moveMem(cmdlineAddr + i, cmdlineAddr + i + 1, len - i);
            --len;
          }
        } else if (c == ' ') {
          spcFound = true;
        } else if (!spcFound) {
          m_HAL->writeByte(cmdlineAddr + i, toupper(c));
        }
      }

    }

    if (len > 0) {
      consoleOut("\r\n");

      // flag to signal "CCP not running". This must be done also for built-in commands!
      m_BDOS->SCB_clearBit(SCB_CCPFLAGS2_B, SCB_CCPFLAGS2_CCPPRESENT);

      uint16_t tailAddr = m_HAL->findChar(cmdlineAddr, ' ');
      auto cmdlen = tailAddr ? tailAddr - cmdlineAddr : len;
      if (!internalCommand(cmdlineAddr, cmdlen, tailAddr)) {

        // this is a transient command

        // save default drive/user
        m_BDOS->SCB_setByte(SCB_CCPDISK_B, m_BDOS->getCurrentDrive());
        m_BDOS->SCB_setByte(SCB_CCPUSER_B, m_BDOS->getCurrentUser());

        m_BDOS->runCommand(cmdlineAddr);

        // restore default drive/user?
        if (!m_BDOS->SCB_testBit(SCB_CCPFLAGS1_B, SCB_CCOFLAGS1_CHAININING) || !m_BDOS->SCB_testBit(SCB_CCPFLAGS1_B, SCB_CCPFLAGS1_CHAINCHANGEDU)) {
          // yes, restore previous drive/user
          m_BDOS->setCurrentDrive(m_BDOS->SCB_getByte(SCB_CCPDISK_B));
          m_BDOS->setCurrentUser(m_BDOS->SCB_getByte(SCB_CCPUSER_B));
        } else {
          // no, set CCP current drive the current one
          m_BDOS->SCB_setByte(SCB_CCPDISK_B, m_BDOS->getCurrentDrive());
          m_BDOS->SCB_setByte(SCB_CCPUSER_B, m_BDOS->getCurrentUser());
        }

      }
    }

    m_BDOS->SCB_setBit(SCB_CCPFLAGS3_B, SCB_CCPFLAGS3_COLDSTART);

  }
}


void CCP::consoleOutChar(char c)
{
  m_BDOS->BDOS_callConsoleOut(c);
}


void CCP::consoleOut(char const * str, size_t maxChars)
{
  int prevOutDelim = m_BDOS->SCB_getByte(SCB_OUTPUTDELIMETER_B);
  m_BDOS->SCB_setByte(SCB_OUTPUTDELIMETER_B, 0);
  m_BDOS->BDOS_callOutputString(str, CCP_OUTSTRBUF_ADDR, CCP_OUTSTRBUF_SIZE, maxChars);
  m_BDOS->SCB_setByte(SCB_OUTPUTDELIMETER_B, prevOutDelim);
}


// string must end with "0"
void CCP::consoleOut(uint16_t str, size_t maxChars)
{
  if (maxChars) {
    uint8_t c;
    while (maxChars-- && (c = m_HAL->readByte(str++)))
      m_BDOS->BDOS_callConsoleOut(c);
  } else {
    int prevOutDelim = m_BDOS->SCB_getByte(SCB_OUTPUTDELIMETER_B);
    m_BDOS->SCB_setByte(SCB_OUTPUTDELIMETER_B, 0);
    m_BDOS->BDOS_callOutputString(str);
    m_BDOS->SCB_setByte(SCB_OUTPUTDELIMETER_B, prevOutDelim);
  }
}


void CCP::consoleOutFmt(const char * format, ...)
{
  va_list ap;
  va_start(ap, format);
  int size = vsnprintf(nullptr, 0, format, ap) + 1;
  if (size > 0) {
    va_end(ap);
    va_start(ap, format);
    char buf[size + 1];
    vsnprintf(buf, size, format, ap);
    consoleOut(buf);
  }
  va_end(ap);
}


bool CCP::iscmd(char const * cmd, size_t cmdlen, uint16_t cmdlineAddr)
{
  while (cmdlen > 0 && *cmd && m_HAL->readByte(cmdlineAddr) && toupper(*cmd) == toupper(m_HAL->readByte(cmdlineAddr))) {
    --cmdlen;
    ++cmd;
    ++cmdlineAddr;
  }
  return *cmd == 0 && cmdlen == 0;
}


bool CCP::internalCommand(uint16_t cmdlineAddr, size_t cmdlen, uint16_t tailAddr)
{
  int drive;
  bool hasDriveSpec = m_BDOS->strToDrive(cmdlineAddr, &drive);

  if (hasDriveSpec) {
    // contains drive specificator
    if (cmdlen == 2) {
      // change current drive
      if (m_HAL->getDriveMountPath(drive) == nullptr) {
        consoleOut("Invalid Drive\r\n");
      } else {
        m_BDOS->setCurrentDrive(drive);
      }
      return true;
    }
  } else if (iscmd("cd", cmdlen, cmdlineAddr)) {
    return cmd_CD(tailAddr);
  } else if (iscmd("exit", cmdlen, cmdlineAddr)) {
    return cmd_EXIT(tailAddr);
  } else if (iscmd("dir", cmdlen, cmdlineAddr)) {
    return cmd_DIR(tailAddr);
  } else if (iscmd("ls", cmdlen, cmdlineAddr)) {
    return cmd_LS(tailAddr);
  } else if (iscmd("era", cmdlen, cmdlineAddr) ||
             iscmd("erase", cmdlen, cmdlineAddr) ||
             iscmd("delete", cmdlen, cmdlineAddr) ||
             iscmd("del", cmdlen, cmdlineAddr) ||
             iscmd("rm", cmdlen, cmdlineAddr)) {
    return cmd_ERASE(tailAddr);
  } else if (iscmd("help", cmdlen, cmdlineAddr) || iscmd("?", cmdlen, cmdlineAddr)) {
    return cmd_HELP(tailAddr);
  } else if (iscmd("rename", cmdlen, cmdlineAddr) || iscmd("ren", cmdlen, cmdlineAddr)) {
    return cmd_RENAME(tailAddr);
  } else if (iscmd("type", cmdlen, cmdlineAddr) || iscmd("cat", cmdlen, cmdlineAddr)) {
    return cmd_TYPE(tailAddr);
  } else if (iscmd("path", cmdlen, cmdlineAddr)) {
    return cmd_PATH(tailAddr);
  } else if (iscmd("mkdir", cmdlen, cmdlineAddr) || iscmd("md", cmdlen, cmdlineAddr)) {
    return cmd_MKDIR(tailAddr);
  } else if (iscmd("rmdir", cmdlen, cmdlineAddr)) {
    return cmd_RMDIR(tailAddr);
  } else if (iscmd("copy", cmdlen, cmdlineAddr) || iscmd("cp", cmdlen, cmdlineAddr)) {
    return cmd_COPY(tailAddr);
  } else if (iscmd("term", cmdlen, cmdlineAddr)) {
    return cmd_TERM(tailAddr);
  } else if (iscmd("info", cmdlen, cmdlineAddr)) {
    return cmd_INFO(tailAddr);
  } else if (iscmd("dinfo", cmdlen, cmdlineAddr)) {
    return cmd_DINFO(tailAddr);
  } else if (iscmd("reboot", cmdlen, cmdlineAddr)) {
    return cmd_REBOOT(tailAddr);
  } else if (iscmd("emu", cmdlen, cmdlineAddr)) {
    return cmd_EMU(tailAddr);
  } else if (iscmd("keyb", cmdlen, cmdlineAddr)) {
    return cmd_KEYB(tailAddr);
  } else if (iscmd("wifiscan", cmdlen, cmdlineAddr)) {
    return cmd_WIFISCAN(tailAddr);
  } else if (iscmd("wifi", cmdlen, cmdlineAddr)) {
    return cmd_WIFI(tailAddr);
  } else if (iscmd("ping", cmdlen, cmdlineAddr)) {
    return cmd_PING(tailAddr);
  } else if (iscmd("telnet", cmdlen, cmdlineAddr)) {
    return cmd_TELNET(tailAddr);
  } else if (iscmd("format", cmdlen, cmdlineAddr)) {
    return cmd_FORMAT(tailAddr);
  }

  return false;
}


bool CCP::cmd_HELP(uint16_t paramsAddr)
{
  if (paramsAddr && m_HAL->strLen(paramsAddr) > 1)
    return false;

  consoleOut("\nBuilt-in commands:\r\n");

  int conHeight = m_BDOS->SCB_getByte(SCB_PAGEMODE_B) == 0 ? m_BDOS->SCB_getByte(SCB_CONSOLEPAGELENGTH_B) : 0;  // 0 = unpaged

  for (int i = 0, row = 1; i < COMMANDSCOUNT; ++i, ++row) {
    m_termCtrl.setForegroundColor(Color::BrightWhite);
    consoleOut(CMDS[i].name);
    consoleOutChar(' ');
    m_termCtrl.setForegroundColor(Color::BrightYellow);
    consoleOut(CMDS[i].desc);
    consoleOut("\r\n");

    if (conHeight && conHeight == row + 3) {
      consoleOut("\r\nPress RETURN to Continue ");
      m_BDOS->BDOS_callConsoleIn();
      consoleOut("\r\n");
      row = 1;
    }

  }

  return true;
}


// DIR [ambiguous_filespec]
bool CCP::cmd_DIR(uint16_t paramsAddr)
{
  // are there options?
  if (paramsAddr && m_HAL->findChar(paramsAddr, '['))
    return false; // yes, search for transient program

  uint16_t FCBaddr = CCP_FCB1_ADDR;

  int r = 0;

  if (paramsAddr && m_HAL->strLen(paramsAddr) > 1) {

    // parse filename
    uint16_t PFCB = CCP_PFCB_ADDR;
    m_HAL->writeWord(PFCB + 0, paramsAddr);
    m_HAL->writeWord(PFCB + 2, CCP_FCB1_ADDR);
    r = m_BDOS->BDOS_callParseFilename(PFCB);

  } else {

    // no params
    m_HAL->writeByte(FCBaddr + 0, 0);
    m_HAL->writeByte(FCBaddr + 1, ' ');

  }

  if (r != 0xFFFF) {

    int driveRaw = m_HAL->readByte(FCBaddr + 0);
    int drive = driveRaw == 0 ? m_BDOS->getCurrentDrive() : driveRaw - 1;

    if (m_HAL->readByte(FCBaddr + 1) == ' ') {
      // no file specified, fill with all '?'
      for (int i = 0; i < 11; ++i)
        m_HAL->writeByte(FCBaddr + 1 + i, '?');
    }

    uint16_t DMA  = m_BDOS->SCB_getWord(SCB_CURRENTDMAADDR_W);
    int conWidth  = m_BDOS->SCB_getByte(SCB_CONSOLEWIDTH_B) + 1;
    int conHeight = m_BDOS->SCB_getByte(SCB_PAGEMODE_B) == 0 ? m_BDOS->SCB_getByte(SCB_CONSOLEPAGELENGTH_B) : 0;  // 0 = unpaged

    int col = 1;
    int row = 1;

    int dirCount   = 0;
    int filesCount = 0;

    r = m_BDOS->BDOS_callSearchForFirst(CCP_FCB1_ADDR);
    while (r < 3) {
      uint16_t foundFCB = DMA + r * 32;

      if (m_BDOS->isDir(foundFCB))
        ++dirCount;
      else
        ++filesCount;

      consoleOutChar(col == 1 ? 'A' + drive : ' ');
      consoleOut(": ");
      consoleOut(foundFCB + 1, 8);
      consoleOutChar(' ');
      consoleOut(foundFCB + 9, 3);

      col += 15;
      if (col + 15 >= conWidth) {
        col = 1;
        consoleOut("\r\n");
        ++row;

        if (conHeight && conHeight == row + 1) {
          consoleOut("\r\nPress RETURN to Continue ");
          m_BDOS->BDOS_callConsoleIn();
          consoleOut("\r\n");
          row = 1;
        }
      }

      r = m_BDOS->BDOS_callSearchForNext();
    }

    if (dirCount == 0 && filesCount == 0 && paramsAddr) {
      consoleOut("File");
      consoleOut(paramsAddr);
      consoleOut(" not found.\r\n");
    }

  }

  return true;
}


// LS [path]
// notes:
//   - path can contain a directory and/or filename with wildcards
//   - this command doesn't use searchfirst/searchnext bdos calls
//   - uses ANSI escape codes
// examples:
//    ls
//    ls *.com
//    ls bin
//    ls bin\*.com
//    ls a:              <= current directory of A:
//    ls a:bin           <= current directory + "BIN" of A:
//    ls a:\             <= root directory of A:
//    ls a:\*.com
//    ls a:\bin          <= root directory + "BIN" of A:
//    ls a:\bin\*.com
//    ls ..\*.com
bool CCP::cmd_LS(uint16_t paramsAddr)
{
  auto srcActualPath = m_BDOS->createAbsolutePath(paramsAddr);
  if (!srcActualPath)
    return true;  // fail, invalid path

  char * srcFilename = nullptr;

  FileBrowser fb;
  if (!fb.setDirectory(srcActualPath)) {
    // failed, maybe the last part is a filename
    // break source path and filename path
    srcFilename = strrchr(srcActualPath, '/');
    if (!srcFilename) {
      // fail
      consoleOut("Invalid path\r\n");
      return true;
    }
    *srcFilename = 0;
    ++srcFilename;
    if (!fb.setDirectory(srcActualPath)) {
      // fail
      consoleOut("Invalid path\r\n");
      return true;
    }
  }

  int conWidth = m_BDOS->SCB_getByte(SCB_CONSOLEWIDTH_B) + 1;
  int col = 1;

  int dirCount   = 0;
  int filesCount = 0;

  for (int i = 0; i < fb.count(); ++i) {
    auto item = fb.get(i);
    if (srcFilename == nullptr || BDOS::fileMatchWithWildCards(srcFilename, item->name)) {

      if (item->isDir) {
        m_termCtrl.setCharStyle(CharStyle::Bold, true);
        m_termCtrl.setForegroundColor(Color::BrightYellow);
        ++dirCount;
      } else {
        m_termCtrl.setCharStyle(CharStyle::Bold, false);
        if (BDOS::hasExt(item->name, "com") || BDOS::hasExt(item->name, "sub"))
          m_termCtrl.setForegroundColor(Color::BrightBlue);
        else
          m_termCtrl.setForegroundColor(Color::BrightWhite);
        ++filesCount;
      }

      consoleOut(item->name);

      // prevent an additional new line in particular cases
      if (i == fb.count() - 1)
        break;

      auto nameLen = strlen(item->name);

      if (nameLen <= 12) {
        // short filename

        for (int j = 15 - (int)nameLen; j >= 0; --j)
          consoleOutChar(' ');

        col += 15;
        if (col + 15 >= conWidth) {
          col = 1;
          consoleOut("\r\n");
        }
      } else {
        // long filename
        consoleOut("\r\n");
      }

    }
  }

  m_termCtrl.setForegroundColor(Color::BrightGreen);

  if (dirCount == 0 && filesCount == 0 && paramsAddr) {
    consoleOut("File");
    consoleOut(paramsAddr);
    consoleOut(" not found.\r\n");
  } else {
    consoleOutFmt("\r\n    %d File(s)    %d Dir(s)\r\n", filesCount, dirCount);
  }


  // cleanup
  free(srcActualPath);

  return true;
}


// ERASE [ambiguous_filespec]
bool CCP::cmd_ERASE(uint16_t paramsAddr)
{
  // are there options?
  if (paramsAddr && m_HAL->findChar(paramsAddr, '['))
    return false; // yes, search for transient program

  if (paramsAddr == 0 || m_HAL->strLen(paramsAddr) <= 1) {
    // no file specified, ask file name
    consoleOut("Enter filename: ");
    // get input from console
    uint16_t cmdline = CCP_CONSOLEBUFFER_ADDR;
    m_HAL->writeByte(cmdline + 0, CCP_CONSOLEBUFFER_SIZE - 1); // max len
    m_HAL->writeByte(cmdline + 1, 0);
    m_HAL->writeByte(cmdline + 2, 0);
    m_BDOS->BDOS_callReadConsoleBuffer(CCP_CONSOLEBUFFER_ADDR);
    consoleOut("\r\n");
    int len = m_HAL->readByte(cmdline + 1);
    cmdline += 2;      // bypass maxlen and string length
    m_HAL->writeByte(cmdline + len, 0);  // set ending zero
    paramsAddr = cmdline;
  } else
    ++paramsAddr; // bypass initial space

  // parse filename
  uint16_t PFCB = CCP_PFCB_ADDR;
  m_HAL->writeWord(PFCB + 0, paramsAddr);
  m_HAL->writeWord(PFCB + 2, CCP_FCB1_ADDR);
  int r = m_BDOS->BDOS_callParseFilename(PFCB);

  if (r != 0xFFFF) {
    uint16_t FCB = CCP_FCB1_ADDR;

    // require confirm if a wildcard is present
    bool requireConfirm = false;
    for (int i = 1; i < 12; ++i) {
      char c = m_HAL->readByte(FCB + i);
      if (c == '*' || c == '?') {
        requireConfirm = true;
        break;
      }
    }

    if (requireConfirm) {
      consoleOut("Erase ");
      consoleOut(paramsAddr);
      consoleOut(" (Y/N)? ");
      int c = m_BDOS->BDOS_callConsoleIn();
      if (c != 'y' && c != 'Y')
        return true;
    }

    int r = m_BDOS->BDOS_callDeleteFile(CCP_FCB1_ADDR);
    if (r == 0xFF) {
      consoleOut("No File\r\n");
    }

    return true;
  }

  // something failed, call erase.com
  return false;
}


// CD [directory]
// examples:
//    cd bin
//    cd mysoft\bin
//    cd ..\soft
bool CCP::cmd_CD(uint16_t paramsAddr)
{
  if (paramsAddr && m_HAL->strLen(paramsAddr) > 1) {
    auto r = m_BDOS->BDOS_callChangeCurrentDirectory(paramsAddr);
    if (r)
      consoleOut("Path not found\r\n");
  } else {
    // no params, show current dir
    consoleOutChar('A' + m_BDOS->getCurrentDrive());
    consoleOut(":\\");
    consoleOut(m_BDOS->getCurrentDir());
    consoleOut("\r\n");
  }
  return true;
}


// RENAME unambiguous_filespec=unambiguous_filespec
//   RENAME.COM invoked if:
//      - no parameters
//      - wildcards in file names
//      - target file already exist
//   '=' is not mandatory, it is just a separator (like a "space")
bool CCP::cmd_RENAME(uint16_t paramsAddr)
{
  // are there parameters?
  if (paramsAddr == 0 || m_HAL->strLen(paramsAddr) <= 1)
    return false; // no, use RENAME.COM

  // are there wildcards in file names?
  uint8_t c;
  uint16_t addr = paramsAddr;
  while ((c = m_HAL->readByte(addr++)))
    if (c == '?' || c == '*')
      return false; // yes, use RENAME.COM

  // parse first filename
  uint16_t PFCB = CCP_PFCB_ADDR;
  m_HAL->writeWord(PFCB + 0, paramsAddr);
  m_HAL->writeWord(PFCB + 2, CCP_FCB1_ADDR);
  uint16_t next = m_BDOS->BDOS_callParseFilename(PFCB);

  if (next != 0xFFFF && next > CCP_CONSOLEBUFFER_ADDR) {

    // parse second filename
    ++next; // bypass delimeter
    m_HAL->writeWord(PFCB + 0, next);
    m_HAL->writeWord(PFCB + 2, CCP_FCB2_ADDR);
    next = m_BDOS->BDOS_callParseFilename(PFCB);

    if (next != 0xFFFF) {

      // check drivers
      if (m_HAL->readByte(CCP_FCB1_ADDR + 0) != m_HAL->readByte(CCP_FCB2_ADDR + 0))
        return false;

      // disable display of error, so rename will silently fail if dest already exist or source doesn't exist and rename.com is then called
      m_BDOS->SCB_setByte(SCB_ERRORMODE_B, 0xFF);

      // prepare second FCB for rename (copying dest filename in seconds 16 bytes)
      m_HAL->copyMem(CCP_FCB2_ADDR + 16, CCP_FCB1_ADDR, 16);

      int r = m_BDOS->BDOS_callRenameFile(CCP_FCB2_ADDR);

      return r == 0;

    }
  }

  // something failed, call rename.com
  return false;
}



// TYPE unambiguous_filespec
bool CCP::cmd_TYPE(uint16_t paramsAddr)
{
  // are there parameters?
  if (paramsAddr == 0 || m_HAL->strLen(paramsAddr) <= 1)
    return false;    // no, use TYPE.COM

  // are there options or wildcards?
  uint8_t c;
  uint16_t addr = paramsAddr;
  while ((c = m_HAL->readByte(addr++)))
    if (c == '[' || c == '*' || c == '?')
      return false;  // yes, use TYPE.COM

  // parse filename
  uint16_t PFCB = CCP_PFCB_ADDR;
  m_HAL->writeWord(PFCB + 0,paramsAddr);
  m_HAL->writeWord(PFCB + 2, CCP_FCB1_ADDR);
  int r = m_BDOS->BDOS_callParseFilename(PFCB);

  if (r != 0xFFFF) {

    // try to open file
    r = m_BDOS->BDOS_callOpenFile(CCP_FCB1_ADDR);

    if (r == 0) {

      // setup DMA address
      m_BDOS->BDOS_callSetDMAAddress(CCP_DMA1_ADDR);

      uint16_t dma = CCP_DMA1_ADDR;

      int conHeight = m_BDOS->SCB_getByte(SCB_PAGEMODE_B) == 0 ? m_BDOS->SCB_getByte(SCB_CONSOLEPAGELENGTH_B) : 0;  // 0 = unpaged
      int row = 1;

      for (bool loop = true; loop; ) {
        if (m_BDOS->BDOS_callReadSequential(CCP_FCB1_ADDR))
          break;

        for (int i = 0; i < 128; ++i) {
          char c = m_HAL->readByte(dma + i);

          if (c == 0x1A)
            break;

          consoleOutChar(c);

          if (m_BDOS->SCB_getWord(SCB_PROGRAMRETCODE_W) == 0xFFFE) {
            // CTRL-C in consoleOut
            loop = false;
            break;
          }

          if (c == '\n') {
            if (conHeight && conHeight == row + 1) {
              consoleOut("\r\nPress RETURN to Continue ");
              int c = m_BDOS->BDOS_callConsoleIn();
              if (c == ASCII_CTRLC) {
                loop = false;
                break;
              }
              consoleOut("\r\n");
              row = 1;
            } else
              ++row;
          }
        }
      }

      m_BDOS->BDOS_callCloseFile(CCP_FCB1_ADDR);

    }

    if (r == 0x00FF) {
      consoleOut("No File");
    }

    return true;

  }

  return false;
}


// PATH [dir1;dir2;...]
//   a dir must always specify the drive (ie "A:BIN;B:MYSOFT/BIN")
bool CCP::cmd_PATH(uint16_t paramsAddr)
{
  // are there parameters?
  if (paramsAddr == 0 || m_HAL->strLen(paramsAddr) <= 1) {
    // no, show current path

    auto spath = m_BDOS->getSearchPath();
    consoleOut(spath ? spath : "No Path");
    consoleOut("\r\n");

  } else {
    // yes, set new path

    char searchPathStorage[m_HAL->strLen(paramsAddr) + 1];
    m_HAL->copyStr(searchPathStorage, paramsAddr);
    char const * searchPath = searchPathStorage;

    // bypass spaces
    while (isspace(*searchPath))
      ++searchPath;

    // check every path is complete
    for (char const * p = searchPath, * next = nullptr; p != nullptr; p = next) {
      // bypass ';' and spaces
      while (isspace(*p) || *p == ';')
        ++p;
      // look head for start of next path
      next = strchr(p, ';');
      // get drive
      if (*(p + 1) != ':') {
        consoleOut("Drive not specified in path\r\n");
        return true;
      }
      int drive = toupper(*p) - 'A';
      if (drive < 0 || drive >= MAXDRIVERS) {
        consoleOutFmt("Invalid Drive %c: in path\r\n", 'A' + drive);
        return true;
      }
      p += 2; // bypass drive specificator
    }

    m_BDOS->setSearchPath(searchPath);

  }

  return true;
}


// MKDIR unambiguous_filespec
bool CCP::cmd_MKDIR(uint16_t paramsAddr)
{
  // are there parameters?
  if (paramsAddr == 0 || m_HAL->strLen(paramsAddr) <= 1) {
    // no, fail
    consoleOut("No directory name specified\r\n");
    return true;
  }

  // parse dirname
  uint16_t PFCB = CCP_PFCB_ADDR;
  m_HAL->writeWord(PFCB + 0, paramsAddr);
  m_HAL->writeWord(PFCB + 2, CCP_FCB1_ADDR);
  int r = m_BDOS->BDOS_callParseFilename(PFCB);

  if (r != 0xFFFF) {
    // set flag for create directory
    m_HAL->writeByte(CCP_FCB1_ADDR, 0x80 | m_HAL->readByte(CCP_FCB1_ADDR));

    m_BDOS->BDOS_callMakeFile(CCP_FCB1_ADDR);
  }

  return true;
}


// rmdir unambiguous_dirspec
bool CCP::cmd_RMDIR(uint16_t paramsAddr)
{
  // are there parameters?
  if (paramsAddr == 0 || m_HAL->strLen(paramsAddr) <= 1) {
    // no, fail
    consoleOut("No directory name specified\r\n");
    return true;
  }

  // parse dirname
  uint16_t PFCB = CCP_PFCB_ADDR;
  m_HAL->writeWord(PFCB + 0, paramsAddr);
  m_HAL->writeWord(PFCB + 2, CCP_FCB1_ADDR);
  int r = m_BDOS->BDOS_callParseFilename(PFCB);

  if (r != 0xFFFF) {
    consoleOut("Remove ");
    consoleOut(paramsAddr);
    consoleOut(" (Y/N)? ");
    int c = m_BDOS->BDOS_callConsoleIn();
    if (c != 'y' && c != 'Y')
      return true;

    // add directory extension
    m_HAL->copyMem(CCP_FCB1_ADDR + 9, DIRECTORY_EXT, 3);

    int r = m_BDOS->BDOS_callDeleteFile(CCP_FCB1_ADDR);
    if (r == 0xFF) {
      consoleOut("No Directory\r\n");
    }
  }

  return true;
}


// copy fullsrcpath/filename fulldstpath[/filename]
//   examples:
//      copy source.dat b:                      <= source from current disk, current directory
//      copy b:source.dat a:/                   <= source from current dir of B, dest current disk current dir
//      copy b:source.dat c:                    <= source and dest in current dir of specified disks
//      copy a:/dir/source.dat b:/dir           <= source and dest fully specified by absolute paths
//      copy a:/dir/*.com b:/subdir
//      copy *.com ../
//      copy source.dat dest.dat                <= copy source.dat into dest.dat (changes the name)
//      copy source.dat BIN/dest.dat            <= copy source.dat into BIN, naming it dest.dat
// Destination must be always a directory or a disk plus optionally a filename:
//    disk:      <= use current directory of specified disk
//    disk:/     <= use root directory of specified disk
//    disk:/dir  <= use "dir" from root directory of specified disk
//    disk:dir   <= use subdirectory "dir" starting from current directory of disk:
//    dir        <= use subdirectory "dir" of current disk from current directory
//    /          <= use root "dir" of current disk
//    file       <= name destination as file
//    dir/file   <= destination is inside "dir", and has name "file"
// Wildcards accepted
bool CCP::cmd_COPY(uint16_t paramsAddr)
{
  // are there parameters?
  if (paramsAddr == 0 || m_HAL->strLen(paramsAddr) <= 1) {
    // no, fail
    consoleOut("No source or destination specified\r\n");
    return true;
  }

  // bypass spaces
  char c;
  while ((c = m_HAL->readByte(paramsAddr)) && isspace(c))
    ++paramsAddr;

  //// get source
  auto spc = m_HAL->findChar(paramsAddr, ' ');
  if (!spc) {
    // no spaces after source, no destenation!
    consoleOut("No destination specified\r\n");
    return true;
  }
  auto len = spc - paramsAddr;
  m_HAL->copyMem(CCP_DMA1_ADDR, paramsAddr, len);
  m_HAL->writeByte(CCP_DMA1_ADDR + len, 0);

  //// get dest
  paramsAddr += len;

  // bypass spaces
  while ((c = m_HAL->readByte(paramsAddr)) && isspace(c))
    ++paramsAddr;

  m_HAL->copyStr(CCP_DMA2_ADDR, paramsAddr);

  // first call doesn't overwrite
  auto r = m_BDOS->BDOS_callCopyFile(CCP_DMA1_ADDR, CCP_DMA2_ADDR, false, true);

  if (r == 1) {
    consoleOut("Error, source doesn't exist\r\n");
  } else if (r == 2) {
    consoleOut("Error, destination path doesn't exist\r\n");
  } else if (r == 3) {
    consoleOut("Overwrite ");
    consoleOut(paramsAddr);
    consoleOut(" (Y/N)? ");
    int c = m_BDOS->BDOS_callConsoleIn();
    if (c != 'y' && c != 'Y')
      return true;
    consoleOut("\r\n");
    m_BDOS->BDOS_callCopyFile(CCP_DMA1_ADDR, CCP_DMA2_ADDR, true, true);
  } else if (r == 4) {
    consoleOut("Error, source and dest match\r\n");
  }

  return true;
}


// TERM id    <= activate session (id 0..11)
bool CCP::cmd_TERM(uint16_t paramsAddr)
{
  // are there parameters?
  if (paramsAddr == 0 || m_HAL->strLen(paramsAddr) <= 1) {
    // no, fail
    consoleOut("Usage:\r\n");
    consoleOut("  TERM 0-11 : Activate specified session. Example: TERM 1\r\n");
    consoleOut("  TERM AUX  : Connect a new session to the serial port\r\n");
    return true;
  }

  char paramStore[m_HAL->strLen(paramsAddr) + 1];
  m_HAL->copyStr(paramStore, paramsAddr);
  char * param = paramStore;

  while (*param && isspace(*param))
    ++param;

  if (strcasecmp(param, "AUX") == 0) {

    // start serial port session
    m_HAL->abort(AbortReason::AuxTerm);
    return true;

  } else {

    int id = atoi(param);
    if (id >= 0 && id < 12) {
      Supervisor::instance()->activateSession(id);
      return true;
    }

  }

  consoleOut("Invalid parameters\r\n");
  return true;
}


bool CCP::cmd_INFO(uint16_t paramsAddr)
{
  consoleOut("\r\n");
  m_termCtrl.setBackgroundColor(Color::Blue);
  m_termCtrl.setForegroundColor(Color::BrightYellow);
  consoleOut("Multisession/Multitasking CP/M 3 (Plus) Compatible System");
  m_termCtrl.setBackgroundColor(Color::Black);  // required in case of scrolling
  consoleOut("\r\n");

  m_termCtrl.setBackgroundColor(Color::Blue);
  m_termCtrl.setForegroundColor(Color::BrightWhite);
  consoleOut("www.fabgl.com - ESP32 Graphics Library                   ");
  m_termCtrl.setBackgroundColor(Color::Black);  // required in case of scrolling
  consoleOut("\r\n");

  m_termCtrl.setBackgroundColor(Color::Blue);
  m_termCtrl.setForegroundColor(Color::BrightCyan);
  consoleOut("(c) 2021 by Fabrizio Di Vittorio - fdivitto2013@gmail.com");

  m_termCtrl.setForegroundColor(Color::BrightYellow);
  m_termCtrl.setBackgroundColor(Color::Black);

  consoleOut("\r\n\nMounts:\r\n");
  for (int i = 0; i < MAXDRIVERS; ++i) {
    if (m_HAL->getDriveMountPath(i))
      consoleOutFmt("  %c:  %s\r\n", 'A' + i, m_HAL->getDriveMountPath(i));
  }

  consoleOutFmt("\n%d Bytes TPA  (System free %d Bytes)\r\n", m_BDOS->getTPASize(), HAL::systemFree());

  int sessionID = Supervisor::instance()->getSessionIDByTaskHandle(xTaskGetCurrentTaskHandle());
  consoleOutFmt("Terminal #%d (%s)\r\n", sessionID + 1, SupportedTerminals::names()[m_defaultTerminalType]);

  #ifdef HAS_WIFI
  if (HAL::wifiConnected()) {
    consoleOutFmt("WiFi SSID  : %s\r\n", WiFi.SSID().c_str());
    consoleOutFmt("Current IP : %s\r\n", WiFi.localIP().toString().c_str());
  }
  #endif

  m_termCtrl.setForegroundColor(Color::BrightWhite);
  consoleOut("\r\nPress F1...F12 to change session. Type \"help\" to get command list\r\n");

  return true;
}


bool CCP::cmd_REBOOT(uint16_t paramsAddr)
{
  FileBrowser::unmountSDCard();
  FileBrowser::unmountSPIFFS();
  ESP.restart();

  return true;
}


// EMU terminal_index
// example:
//   EMU 2
bool CCP::cmd_EMU(uint16_t paramsAddr)
{
  // are there parameters?
  if (paramsAddr == 0 || m_HAL->strLen(paramsAddr) <= 1) {
    // no, fail
    consoleOut("Usage:\r\n");
    consoleOutFmt("  EMU 0-%d : Set terminal emulation. Example: EMU 3\r\n\n", SupportedTerminals::count() - 1);
    consoleOut("Supported terminal emulations:\r\n");
    for (int i = 0; i < SupportedTerminals::count(); ++i)
      consoleOutFmt("  %d = %s\r\n", i, SupportedTerminals::names()[i]);
    return true;
  }

  char paramStore[m_HAL->strLen(paramsAddr) + 1];
  m_HAL->copyStr(paramStore, paramsAddr);
  char * param = paramStore;

  while (*param && isspace(*param))
    ++param;

  int idx = atoi(param);

  if (idx >= 0 && idx < SupportedTerminals::count()) {

    m_defaultTerminalType = SupportedTerminals::types()[idx];
    m_HAL->setTerminalType(m_defaultTerminalType);
    consoleOutFmt("Terminal type is: %s\r\n", SupportedTerminals::names()[m_defaultTerminalType]);

  } else {
    consoleOut("Invalid index number\r\n");
  }

  return true;
}


// keyb layout
// example:
//   KEYB DE
bool CCP::cmd_KEYB(uint16_t paramsAddr)
{
  // are there parameters?
  if (paramsAddr == 0 || m_HAL->strLen(paramsAddr) <= 1) {
    // no, fail
    consoleOut("Usage:\r\n");
    consoleOutFmt("  KEYB US, UK, DE, IT, ES : Set keyboard layout. Example: KEYB DE\r\n");
    return true;
  }

  char paramStore[m_HAL->strLen(paramsAddr) + 1];
  m_HAL->copyStr(paramStore, paramsAddr);
  char * param = paramStore;

  while (*param && isspace(*param))
    ++param;

  fabgl::KeyboardLayout const * layout;

  if (strcasecmp(param, "US") == 0)
    layout = &fabgl::USLayout;
  else if (strcasecmp(param, "UK") == 0)
    layout = &fabgl::UKLayout;
  else if (strcasecmp(param, "DE") == 0)
    layout = &fabgl::GermanLayout;
  else if (strcasecmp(param, "IT") == 0)
    layout = &fabgl::ItalianLayout;
  else if (strcasecmp(param, "ES") == 0)
    layout = &fabgl::SpanishLayout;
  else {
    consoleOut("Invalid keyboard layout\r\n");
    return true;
  }

  m_HAL->terminal()->keyboard()->setLayout(layout);

  return true;
}


// exit     <= exit current session
// exit id  <= exit specified session (id = 0..11)
bool CCP::cmd_EXIT(uint16_t paramsAddr)
{
  // are there parameters?
  if (paramsAddr == 0 || m_HAL->strLen(paramsAddr) <= 1) {
    // no, exit this session
    m_exitSystem = true;
    return true;
  }

  char paramStore[m_HAL->strLen(paramsAddr) + 1];
  m_HAL->copyStr(paramStore, paramsAddr);
  char * param = paramStore;

  while (*param && isspace(*param))
    ++param;

  int idx = atoi(param);

  if (idx >= 0 && idx < SupportedTerminals::count())
    Supervisor::instance()->abortSession(idx, AbortReason::SessionClosed);

  return true;
}


// show some debug info
bool CCP::cmd_DINFO(uint16_t paramsAddr)
{
  consoleOutFmt("Open sessions        : %d\r\n", Supervisor::instance()->getOpenSessions());
  consoleOutFmt("Allocated blocks     : %d (1 block = 1024 bytes)\r\n", m_HAL->allocatedBlocks());
  consoleOutFmt("Zombie files         : %d\r\n", m_BDOS->getOpenFilesCount());
  consoleOutFmt("RSX installed        : %c\r\n", m_BDOS->RSXInstalled() ? 'Y' : 'N');
  consoleOutFmt("BDOS Address changed : %c\r\n", m_BDOS->BDOSAddrChanged() ? 'Y' : 'N');
  consoleOutFmt("BIOS Address changed : %c\r\n", m_BDOS->BIOSAddrChanged() ? 'Y' : 'N');
  consoleOutFmt("TPA Size             : %d Bytes\r\n", m_BDOS->getTPASize());
  return true;
}


bool CCP::wifiErrorMsg()
{
  consoleOut("WiFi not compiled. Enable HAS_WIFI in defs.h\r\n");
  return true;
}


bool CCP::cmd_WIFISCAN(uint16_t paramsAddr)
{
  #ifdef HAS_WIFI

  static char const * ENC2STR[] = { "Open", "WEP", "WPA-PSK", "WPA2-PSK", "WPA/WPA2-PSK", "WPA-ENTERPRISE" };
  m_HAL->setTerminalType(TermType::ANSILegacy);
  consoleOut("Scanning...");
  delay(100); // give time to display last terminal msg, because we will suspend interrupts...
  int networksCount = WiFi.scanNetworks();
  consoleOutFmt("%d network(s) found\r\n", networksCount);
  if (networksCount) {
    consoleOut("\e[90m #\e[4GSSID\e[45GRSSI\e[55GCh\e[60GEncryption\e[32m\r\n");
    for (int i = 0; i < networksCount; ++i)
      consoleOutFmt("\e[33m %d\e[4G%s\e[33m\e[45G%d dBm\e[55G%d\e[60G%s\e[32m\r\n", i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.channel(i), ENC2STR[WiFi.encryptionType(i)]);
  }
  WiFi.scanDelete();
  m_HAL->setTerminalType(m_defaultTerminalType);
  return true;

  #else

  return wifiErrorMsg();

  #endif
}


bool CCP::cmd_WIFI(uint16_t paramsAddr)
{
  #ifdef HAS_WIFI

  // are there parameters?
  if (paramsAddr == 0 || m_HAL->strLen(paramsAddr) <= 1) {
    // no, fail
    consoleOut("Usage:\r\n");
    consoleOutFmt("  WIFI ssid password : Connect to WiFi network. Example: WIFI mynet mypass\r\n");
    return true;
  }

  char paramStore[m_HAL->strLen(paramsAddr) + 1];
  m_HAL->copyStr(paramStore, paramsAddr);
  char * param = paramStore;

  while (*param && isspace(*param))
    ++param;

  static const int MAX_SSID_SIZE = 32;
  static const int MAX_PSW_SIZE  = 32;
  char ssid[MAX_SSID_SIZE + 1];
  char psw[MAX_PSW_SIZE + 1] = {0};
  if (sscanf(param, "%32s %32s", ssid, psw) >= 1) {
    consoleOut("Connecting WiFi...");
    WiFi.disconnect(true, true);
    for (int i = 0; i < 2; ++i) {
      WiFi.begin(ssid, psw);
      if (WiFi.waitForConnectResult() == WL_CONNECTED)
        break;
      WiFi.disconnect(true, true);
    }
    if (WiFi.status() == WL_CONNECTED) {
      consoleOutFmt("connected to %s, IP is %s\r\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
    } else {
      consoleOutFmt("failed!\r\n");
    }
  }
  return true;

  #else

  return wifiErrorMsg();

  #endif
}


bool CCP::cmd_PING(uint16_t paramsAddr)
{
  #ifdef HAS_WIFI

  // are there parameters?
  if (paramsAddr == 0 || m_HAL->strLen(paramsAddr) <= 1) {
    // no, fail
    consoleOut("Usage:\r\n");
    consoleOutFmt("  PING host : Pings an host or IP. Example: PING www.fabgl.com\r\n");
    return true;
  }

  char paramStore[m_HAL->strLen(paramsAddr) + 1];
  m_HAL->copyStr(paramStore, paramsAddr);
  char * param = paramStore;

  while (*param && isspace(*param))
    ++param;

  int sent = 0, recv = 0;
  fabgl::ICMP icmp;
  while (true) {

    // CTRL-C ?
    if (m_BDOS->SCB_getWord(SCB_PROGRAMRETCODE_W) == 0xFFFE)
      break;

    int t = icmp.ping(param);
    if (t >= 0) {
      consoleOutFmt("%d bytes from %s: icmp_seq=%d ttl=%d time=%.3f ms\r\n", icmp.receivedBytes(), icmp.hostIP().toString().c_str(), icmp.receivedSeq(), icmp.receivedTTL(), (double)t/1000.0);
      delay(1000);
      ++recv;
    } else if (t == -2) {
      consoleOutFmt("Cannot resolve %s: Unknown host\r\n", param);
      break;
    } else {
      consoleOutFmt("Request timeout for icmp_seq %d\r\n", icmp.receivedSeq());
    }
    ++sent;

  }
  if (sent > 0) {
    consoleOutFmt("--- %s ping statistics ---\r\n", param);
    consoleOutFmt("%d packets transmitted, %d packets received, %.1f%% packet loss\r\n", sent, recv, (double)(sent - recv) / sent * 100.0);
  }

  return true;

  #else

  return wifiErrorMsg();

  #endif

}


#ifdef HAS_WIFI
static int clientWaitForChar(WiFiClient & client)
{
  // not so good...:-)
  while (!client.available())
    ;
  return client.read();
}
#endif


bool CCP::cmd_TELNET(uint16_t paramsAddr)
{
  #ifdef HAS_WIFI

  // are there parameters?
  if (paramsAddr == 0 || m_HAL->strLen(paramsAddr) <= 1) {
    // no, fail
    consoleOut("Usage:\r\n");
    consoleOutFmt("  TELNET host : Telnet to host or IP. Example: TELNET towel.blinkenlights.nl\r\n");
    return true;
  }

  char paramStore[m_HAL->strLen(paramsAddr) + 1];
  m_HAL->copyStr(paramStore, paramsAddr);
  char * param = paramStore;

  while (*param && isspace(*param))
    ++param;

  auto host = param;

  // find port number
  while (*param && !isspace(*param))
    ++param;
  *param++ = 0;
  auto port = atoi(param);
  if (port == 0)
    port = 23;

  WiFiClient client;

  consoleOutFmt("Trying %s, port %d...\r\n", host, port);
  if (client.connect(host, port)) {
    consoleOutFmt("Connected to %s\r\n", host);
  } else {
    consoleOut("Unable to connect to remote host\r\n");
    return true;
  }

  while (true) {

    // CTRL-C ?
    if (m_BDOS->SCB_getWord(SCB_PROGRAMRETCODE_W) == 0xFFFE)
      break;

    // process data from remote host (up to 1024 codes at the time)
    //for (int i = 0; client.available() && i < 1024; ++i) {
    if (client.available()) {
      int c = client.read();
      if (c == 0xFF) {
        // IAC (Interpret As Command)
        uint8_t cmd = clientWaitForChar(client);
        uint8_t opt = clientWaitForChar(client);
        if (cmd == 0xFD && opt == 0x1F) {
          // DO WINDOWSIZE
          client.write("\xFF\xFB\x1F", 3); // IAC WILL WINDOWSIZE
          client.write("\xFF\xFA\x1F" "\x00\x50\x00\x19" "\xFF\xF0", 9);  // IAC SB WINDOWSIZE 0 80 0 25 IAC SE
        } else if (cmd == 0xFD && opt == 0x18) {
          // DO TERMINALTYPE
          client.write("\xFF\xFB\x18", 3); // IAC WILL TERMINALTYPE
        } else if (cmd == 0xFA && opt == 0x18) {
          // SB TERMINALTYPE
          c = clientWaitForChar(client);  // bypass '1'
          c = clientWaitForChar(client);  // bypass IAC
          c = clientWaitForChar(client);  // bypass SE
          client.write("\xFF\xFA\x18\x00" "wsvt25" "\xFF\xF0", 12); // IAC SB TERMINALTYPE 0 "...." IAC SE
        } else {
          uint8_t pck[3] = {0xFF, 0, opt};
          if (cmd == 0xFD)  // DO -> WONT
            pck[1] = 0xFC;
          else if (cmd == 0xFB) // WILL -> DO
            pck[1] = 0xFD;
          client.write(pck, 3);
        }
      } else {
        consoleOutChar(c);
      }
    }
    // process data from terminal (keyboard)
    while (m_BDOS->BDOS_callConsoleStatus()) {
      client.write( m_BDOS->BDOS_callDirectConsoleIO(0xFF) );
    }
    // return to prompt?
    if (!client.connected()) {
      client.stop();
      break;
    }

  }

  return true;

  #else

  return wifiErrorMsg();

  #endif

}


bool CCP::cmd_FORMAT(uint16_t paramsAddr)
{
  auto basePath = m_BDOS->createAbsolutePath(0);
  auto driveType = FileBrowser::getDriveType(basePath);
  free(basePath);
  consoleOutFmt("WARNING: ALL DATA ON %s WILL BE LOST!\r\n", driveType == fabgl::DriveType::SPIFFS ? "SPIFFS" : "SD Card");
  consoleOut("Proceed with Format (Y/N)? ");
  int c = m_BDOS->BDOS_callConsoleIn();
  if (c != 'y' && c != 'Y')
    return true;
  consoleOut("\r\nFormatting...");
  delay(100);
  FileBrowser::format(driveType, 0);
  ESP.restart();
  return true;
}
