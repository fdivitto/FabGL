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


#include <sys/stat.h>

#include "BDOS.h"



#pragma GCC optimize ("O2")



// Disc Parameter Block (DPB) - common to all disks
// configuration;
//     block size: 2K
//     disk space: 0x7fff * 2K = 67106816 bytes (TODO)
const DiscParameterBlock commonDiscParameterBlock = {
  .spt = 255,     // Number of 128-byte records per track
  .bsh = 4,       // Block shift. 3 => 1k, 4 => 2k, 5 => 4k....
  .blm = 0xF,     // Block mask. 7 => 1k, 0Fh => 2k, 1Fh => 4k...
  .exm = 0,       // Extent mask, see later
  .dsm = 0x7fff,  // (no. of blocks on the disc)-1. max 0x7fff : TODO may change by the total disk space
  .drm = 9998,    // (no. of directory entries)-1
  .al0 = 0,       // Directory allocation bitmap, first byte
  .al1 = 0,       // Directory allocation bitmap, second byte
  .cks = 0x8000,  // Checksum vector size, 0 or 8000h for a fixed disc. No. directory entries/4, rounded up.
  .off = 0,       // Offset, number of reserved tracks
  .psh = 0,       // Physical sector shift, 0 => 128-byte sectors  1 => 256-byte sectors  2 => 512-byte sectors...
  .phm = 0,       // Physical sector mask,  0 => 128-byte sectors  1 => 256-byte sectors  3 => 512-byte sectors...
};


// Disk Parameter Header (DPH)
const DiscParameterHeader discParameterHeader = {
  .XLT    = 0,
  .dummy  = { 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  .MF     = 0,
  .DPB    = DPB_ADDR,
  .CSV    = 0,
  .ALV    = 0,
  .DIRBCB = 0,
  .DTABCB = 0,
  .HASH   = 0xFFFF,
  .HBANK  = 0,
};


BDOS::BDOS(HAL * hal, BIOS * bios)
  : m_HAL(hal),
    m_BIOS(bios),
    m_printerEchoEnabled(false),
    m_auxStream(nullptr),
    m_consoleReadyChar(0),
    m_readHistoryItem(0),
    m_writeHistoryItem(0),
    m_searchPath(nullptr)
{
  #if MSGDEBUG & DEBUG_BDOS
  m_HAL->logf("BDOS started\r\n");
  #endif

  // initialize zero page to zero
  m_HAL->fillMem(0x0000, 0, 256);

  // reset open file cache
  for (int i = 0; i < CPMMAXFILES; ++i)
    m_openFileCache[i].file = nullptr;

  for (int i = 0; i < MAXDRIVERS; ++i)
    m_currentDir[i] = strdup("");

  // unknown SCB value
  SCB_setByte(SCB_UNKNOWN1_B, 0x07);

  // base address of BDOS
  SCB_setWord(SCB_BDOSBASE_W, BDOS_ENTRY);

  // SCB BDOS version
  SCB_setByte(SCB_BDOSVERSION_B, 0x31);

  // SCB address (undocumented)
  SCB_setWord(SCB_SCBADDR_W, SCB_ADDR);

  // console width and height
  SCB_setByte(SCB_CONSOLEWIDTH_B, m_HAL->getTerminalColumns() - 1);  // width - 1 (so 80 columns becomes 79)
  SCB_setByte(SCB_CONSOLEPAGELENGTH_B, m_HAL->getTerminalRows());

  // address of 128 byte buffer
  SCB_setWord(SCB_BNKBUF, BDOS_BUFADDR);

  // common base address (makes the system looked as "banked")
  SCB_setWord(SCB_COMMONBASEADDR_W, SYSTEM_ADDR);

  // SCB default logical to physical device assignments
  m_BIOS->assignPhysicalDevice(LOGICALDEV_CONIN,  PHYSICALDEV_KBD);   // console input  -- KBD
  m_BIOS->assignPhysicalDevice(LOGICALDEV_CONOUT, PHYSICALDEV_CRT);   // console output -- CRT
  m_BIOS->assignPhysicalDevice(LOGICALDEV_AUXIN,  PHYSICALDEV_UART1); // aux input      -- UART1
  m_BIOS->assignPhysicalDevice(LOGICALDEV_AUXOUT, PHYSICALDEV_UART1); // aux output     -- UART1
  m_BIOS->assignPhysicalDevice(LOGICALDEV_LIST,   PHYSICALDEV_LPT);   // list           -- LPT

  // current disk is A
  setCurrentDrive(0);

  // current user is 0
  setCurrentUser(0);

  // current user is 0
  SCB_setByte(SCB_CURRENTUSER_B, 0);

  // BDOS entry in SCB
  SCB_setWord(SCB_TOPOFUSERTPA_W, BDOS_ENTRY);

  // BDOS entry
  m_HAL->writeByte(PAGE0_BDOS, 0xC3);             // CPU opcode: JP
  m_HAL->writeWord(PAGE0_OSBASE, BDOS_ENTRY);

  // BDOS exit
  m_HAL->writeByte(BDOS_ENTRY, 0xC9);             // CPU opcode: RET

  // BIOS entry
  m_HAL->writeByte(PAGE0_WSTART, 0xC3);                 // CPU opcode: JP
  m_HAL->writeWord(PAGE0_WSTARTADDR, BIOS_ENTRY + 3);   // BIOS WBOOT function

  // BIOS jump table
  for (int i = 0; i < 33; ++i) {
    m_HAL->writeByte(BIOS_ENTRY + i * 3 + 0, 0xC3); // CPU opcode: JP
    m_HAL->writeWord(BIOS_ENTRY + i * 3 + 1, BIOS_RETS + i); // address of related BIOS exit (RET opcode)
  }

  // BIOS exits
  for (int i = 0; i < 33; ++i)
    m_HAL->writeByte(BIOS_RETS + i, 0xC9);  // CPU opcode: RET

  // Disc Parameter Block (DPB) - common to all disks
  m_HAL->copyMem(DPB_ADDR, &commonDiscParameterBlock, sizeof(DiscParameterBlock));

  // Disk Parameter Header (DPH)
  m_HAL->copyMem(DPH_ADDR, &discParameterHeader, sizeof(DiscParameterHeader));

  // invalidate cached directory label flags
  memset(m_cachedDirLabelFlags, 0xff, MAXDRIVERS);

  // default search drivers
  SCB_setByte(SCB_DRIVESEARCHCHAIN0_B, 0);  // first search on mount path of current drive
  SCB_setByte(SCB_DRIVESEARCHCHAIN1_B, 1);  // second search on mount path of drive A
  SCB_setByte(SCB_DRIVESEARCHCHAIN2_B, 0xFF);  // end marker

  // reset write protect disk word
  m_writeProtectWord = 0;

  // allocate console (BDOS 10) input buffer history
  for (int i = 0; i < CCP_HISTORY_DEPTH; ++i) {
    m_history[i] = (char*) malloc(CCP_HISTORY_LINEBUFFER_LEN);
    m_history[i][0] = 0;
  }

  // code to execute every CPU step
  m_HAL->onCPUStep = [&]() {
    // BIOS call?
    if (m_HAL->CPU_getPC() >= BIOS_RETS && m_HAL->CPU_getPC() <= BIOS_RETS + 33)
      m_BIOS->processBIOS(m_HAL->CPU_getPC() - BIOS_RETS);

    // BDOS call?
    if (m_HAL->CPU_getPC() == BDOS_ENTRY)
      processBDOS();
  };

  /*
  consoleOutFmt("STACK        : 0x%04X\r\n", STACK_ADDR);
  consoleOutFmt("BDOS         : 0x%04X\r\n", BDOS_ENTRY);
  consoleOutFmt("BIOS         : 0x%04X\r\n", BIOS_ENTRY);
  consoleOutFmt("DPB          : 0x%04X\r\n", DPB_ADDR);
  consoleOutFmt("DPH          : 0x%04X\r\n", DPH_ADDR);
  consoleOutFmt("SCB_PG       : 0x%04X\r\n", SCB_PAGEADDR);
  consoleOutFmt("SCB          : 0x%04X\r\n", SCB_ADDR);
  consoleOutFmt("BDOS_BUFADDR : 0x%04X\r\n", BDOS_BUFADDR);
  consoleOutFmt("CHRTBL_ADDR  : 0x%04X\r\n", CHRTBL_ADDR);
  //*/

  // get ready to exec CCP
  resetProgramEnv();
}


BDOS::~BDOS()
{
  for (int i = 0; i < MAXDRIVERS; ++i)
    free(m_currentDir[i]);

  for (int i = 0; i < CCP_HISTORY_DEPTH; ++i)
    free(m_history[i]);

  free(m_searchPath);

  m_HAL->releaseMem(0, 65535);
}


bool BDOS::isDir(uint16_t FCBaddr)
{
  return m_HAL->compareMem(FCBaddr + FCB_T1, DIRECTORY_EXT, 3) == 0;
}


// user: 0..15
void BDOS::setCurrentUser(int user)
{
  user = user & 0xF;
  SCB_setByte(SCB_CURRENTUSER_B, user);
  m_HAL->writeByte(PAGE0_CURDRVUSR, (m_HAL->readByte(PAGE0_CURDRVUSR) & 0x0F) | (user << 4));
}


// user: 0..15
int BDOS::getCurrentUser()
{
  return SCB_getByte(SCB_CURRENTUSER_B) & 0xF;
}


char const * BDOS::getCurrentDir()
{
  int drive = getCurrentDrive();  // default drive
  return m_currentDir[drive];
}


char const * BDOS::getCurrentDir(int drive)
{
  return m_currentDir[drive];
}


// drive: 0 = A, 15 = P
void BDOS::setCurrentDrive(int drive)
{
  drive = drive & 0xF;
  if (m_HAL->getDriveMountPath(drive)) {
    SCB_setByte(SCB_CURRENTDISK_B, drive);
    m_HAL->writeByte(PAGE0_CURDRVUSR, (m_HAL->readByte(PAGE0_CURDRVUSR) & 0xF0) | drive);
  }
}


// drive: 0 = A, 15 = P
int BDOS::getCurrentDrive()
{
  return SCB_getByte(SCB_CURRENTDISK_B) & 0xF;
}


// if "str" contains a drive specification return true ("A:", "B:"...) and set *drive
// otherwise return false and set *drive with the default drive
// str can be nullptr
bool BDOS::strToDrive(char const * str, int * drive)
{
  *drive = getCurrentDrive();
  auto slen = str ? strlen(str) : 0;
  if (slen >= 2 && isalpha(str[0]) && str[1] == ':') {
    // has drive specificator
    *drive = toupper(str[0]) - 'A';
    return true;
  }
  return false;
}


bool BDOS::strToDrive(uint16_t str, int * drive)
{
  *drive = getCurrentDrive();
  auto slen = str ? m_HAL->strLen(str) : 0;
  if (slen >= 2 && isalpha(m_HAL->readByte(str)) && m_HAL->readByte(str + 1) == ':') {
    // has drive specificator
    *drive = toupper(m_HAL->readByte(str)) - 'A';
    return true;
  }
  return false;
}


// if errfunc > -1 then call doError setting A=L=0xFF and H=B=0x04 registers (Invalid Drive)
bool BDOS::checkDrive(int drive, int errfunc)
{
  bool valid = drive >= 0 && drive < MAXDRIVERS && m_HAL->getDriveMountPath(drive) != nullptr;
  if (!valid) {
    if (errfunc > -1) {
      doError(0xFF, 0x04, "CP/M Error on %c: Invalid Drive\r\nFunction %d\r\n", 'A' + drive, errfunc);
    } else {
      consoleOutStr("Invalid Drive\r\n");
    }
  }
  return valid;
}


void BDOS::setSearchPath(char const * path)
{
  if (m_searchPath)
    free(m_searchPath);
  m_searchPath = strdup(path);
}


char const * BDOS::getSearchPath()
{
  return m_searchPath;
}


// load, process parameters and run the specified program (with parameters)
// cmdline can also be located at same position of PAGE0_DMA
void BDOS::runCommand(uint16_t cmdline)
{
  // find file name length
  auto filenameEnd = m_HAL->findChar(cmdline, ' ');
  if (!filenameEnd)
    filenameEnd = cmdline + m_HAL->strLen(cmdline);
  auto filenameLen = filenameEnd - cmdline;

  int drive;
  bool driveSpecified = strToDrive(cmdline, &drive);
  if (driveSpecified) {
    // has drive specificator
    cmdline += 2;
    filenameLen -= 2;
  }

  if (!checkDrive(drive, -1))
    return;

  int searchOrder = SCB_getByte(SCB_CCPFLAGS2_B) >> SCB_CCPFLAGS2_FILESEARCHORDER_BIT;
  int searchCount = (searchOrder == SCB_CCPFLAGS2_FILESEARCHORDER_COM ? 1 : 2);

  for (int i = 0; i < searchCount; ++i) {

    char afilename[filenameLen + 4];

    m_HAL->copyMem(afilename, cmdline, filenameLen);
    afilename[filenameLen] = 0;

    bool hasExtension = strrchr(afilename, '.');
    if (!hasExtension) {
      // hasn't extension, add ".COM" or ".SUB" depending on searchOrder
      bool trySUB = (i == 0 && searchOrder == SCB_CCPFLAGS2_FILESEARCHORDER_SUB_COM) || (i == 1 && searchOrder == SCB_CCPFLAGS2_FILESEARCHORDER_COM_SUB);
      strcat(afilename, trySUB ? ".SUB" : ".COM");
    }

    if (execProgram(drive, afilename, filenameEnd)) {
      // success
      return;
    }

    if (hasExtension)
      break;
  }

  // file not found, show error message only when is not in "cold start" mode (trying to execute PROFILE.SUB)
  if (SCB_testBit(SCB_CCPFLAGS3_B, SCB_CCPFLAGS3_COLDSTART)) {
    consoleOutStr(cmdline, m_HAL->readByte(filenameEnd));
    consoleOutChar('?');
  }
}


// load and exec specified program (.COM or .SUB)
// filename is relative path. execProgram tries to load it from following places:
//   - current directory of specified drive
//   - mount path (root dir) of drivers in SCB_DRIVESEARCHCHAIN#_B
bool BDOS::execProgram(int drive, char const * filename, uint16_t parameters)
{
  #if MSGDEBUG & DEBUG_BDOS
  char params[m_HAL->strLen(parameters) + 1];
  m_HAL->copyStr(params, parameters);
  m_HAL->logf("execProgram: drive=%d filename=\"%s\" params=\"%s\"\r\n", drive, filename, params);
  #endif

  bool isSUB = strstr(filename, ".SUB");

  if (isSUB) {
    // make sure the ".SUB" file exists
    setBrowserAtDrive(drive);
    if (!m_fileBrowser.exists(filename, false))
      return false; // sub not found
  }

  auto afilename = isSUB ? "SUBMIT.COM" : filename;

  FILE * fr = nullptr;

  // try current directory
  char fullpath[strlen(m_HAL->getDriveMountPath(drive)) + 1 + strlen(m_currentDir[drive]) + 1 + strlen(afilename) + 1];
  strcpy(fullpath, m_HAL->getDriveMountPath(drive));
  strcat(fullpath, "/");
  if (m_currentDir[drive] && strlen(m_currentDir[drive]) > 0) {
    strcat(fullpath, m_currentDir[drive]);
    strcat(fullpath, "/");
  }
  strcat(fullpath, afilename);
  fr = fopen(fullpath, "rb");

  if (fr == nullptr) {
    // not found, if m_searchPath is specified then search in specified paths, otherwise look into drivers specified by SCB_DRIVESEARCHCHAIN#_B

    if (m_searchPath) {

      // try paths in m_searchPath

      for (char const * p = m_searchPath, * next = nullptr; fr == nullptr && p != nullptr; p = next) {
        // bypass ';' and spaces
        while (isspace(*p) || *p == ';')
          ++p;
        // look head for start of next path
        next = strchr(p, ';');
        // get drive
        int drive = toupper(*p) - 'A';
        if (m_HAL->getDriveMountPath(drive)) {
          p += 2; // bypass drive specificator
          auto pathlen = next ? next - p : strlen(p); // this path length
          char fullpath[strlen(m_HAL->getDriveMountPath(drive)) + 1 + pathlen + 1 + strlen(afilename) + 1];
          strcpy(fullpath, m_HAL->getDriveMountPath(drive));
          strcat(fullpath, "/");
          strncat(fullpath, p, pathlen);
          strcat(fullpath, "/");
          strcat(fullpath, afilename);
          fr = fopen(fullpath, "rb");
        }
      }

    } else {

      // try mount paths of drivers in SCB_DRIVESEARCHCHAIN#_B
      for (int searchdrive = 0; fr == nullptr && searchdrive < 4; ++searchdrive) {
        int drv = SCB_getByte(SCB_DRIVESEARCHCHAIN0_B + searchdrive);
        if (drv == 0xFF)
          break;
        drive = (drv == 0 ? getCurrentDrive() : drv - 1);
        char fullpath[strlen(m_HAL->getDriveMountPath(drive)) + 1 + strlen(afilename) + 1];
        strcpy(fullpath, m_HAL->getDriveMountPath(drive));
        strcat(fullpath, "/");
        strcat(fullpath, afilename);
        fr = fopen(fullpath, "rb");
      }

    }

  }

  if (!fr) {
    // file not found
    return false;
  }

  fseek(fr, 0, SEEK_END);
  size_t size = ftell(fr);
  fseek(fr, 0, SEEK_SET);

  if (size > getTPASize()) {
    // out of memory
    fclose(fr);
    return false;
  }

  // Copy program parameters into default DMA
  // Must be done here, because parameters may lay on TPA!
  auto tail = parameters;
  auto ptr = PAGE0_DMA;
  m_HAL->writeByte(ptr, m_HAL->strLen(tail)); // first byte is the parameters length
  if (isSUB) {
    // is submit "filename" goes to the parameters side
    m_HAL->writeByte(ptr, m_HAL->readByte(ptr) + 1 + strlen(filename));
    ++ptr;
    m_HAL->writeByte(ptr++ , ' ');
    while (*filename)
      m_HAL->writeByte(ptr++, *filename++);
  } else
    ++ptr;
  m_HAL->copyStr(ptr, tail);

  auto dest = TPA_ADDR;
  while (size--)
    m_HAL->writeByte(dest++, fgetc(fr));
  fclose(fr);

  // set drive number where the program was loaded
  m_HAL->writeByte(PAGE0_LOADDRIVE, drive + 1);  // 1 = A

  execLoadedProgram(size);

  return true;
}


void BDOS::resetProgramEnv()
{
  // Reset Multi Sector Count
  SCB_setByte(SCB_MULTISECTORCOUNT_B, 1);

  // Output delimiter
  SCB_setByte(SCB_OUTPUTDELIMETER_B, '$');

  // DMA Address
  SCB_setWord(SCB_CURRENTDMAADDR_W, PAGE0_DMA);

  // Console mode
  SCB_setWord(SCB_CONSOLEMODE_W, 0);

  // Error mode
  SCB_setByte(SCB_ERRORMODE_B, 0);

  // Reset error drive
  SCB_setByte(SCB_ERRORDRIVE_B, 0);
}


// executes the program at TPA_ADDR
void BDOS::execLoadedProgram(size_t size)
{
  resetProgramEnv();

  // fills default FCBs
  parseParams();

  // does this COM contains RSXs? (0xC9 = RET)
  if (m_HAL->readByte(TPA_ADDR) == 0xC9 && size > 0xFF) {
    // yes, relocate them
    processRSXCOM();
  }

  // run
  m_HAL->CPU_pushStack(PAGE0_WSTART); // push PAGE0_WSTART as return address into the stack
  m_HAL->CPU_exec(TPA_ADDR, 0xFFFF);

  // remove RSXs that can be removed
  removeRSX();

  // this restores BDOS and BIOS entries (for BDOS also takes care of RSX)
  m_BIOS->BIOS_call_WBOOT();

  // release unused memory
  m_HAL->releaseMem(TPA_ADDR, getTPATop());
}


uint16_t BDOS::getTPASize()
{
  return getTPATop() - TPA_ADDR;
}


uint16_t BDOS::getTPATop()
{
  return m_HAL->readWord(PAGE0_OSBASE) - 6;
}


// the loaded COM contains zero or more RSXs
//  - execute pre-initialization code
//  - move and relocate RSXs
//  - move program from (TPA_ADDR + 0x100) to TPA_ADDR
void BDOS::processRSXCOM()
{
  // execute pre-initialization code
  m_HAL->CPU_pushStack(0xFFFF);  // RET will interrupt execution
  m_HAL->CPU_exec(TPA_ADDR + COMHEAD_INIT, 0xFFFF);

  // load RSXs

  int rsxRecords = m_HAL->readByte(TPA_ADDR + COMHEAD_RSXCOUNT);

  if (m_HAL->readByte(TPA_ADDR + 256) == 0xC9) {
    // the main program is just a RET. This file contains just RSXs.
    // (see special case at page 1-26, paragraph 2, of CP/M 3 Programmers's Guide).
    SCB_setBit(SCB_CCPFLAGS1_B, SCB_CCPFLAGS1_NULLRSX);
  }

  for (int i = 0; i < rsxRecords; ++i) {
    uint16_t rsxRecordAddr = TPA_ADDR + COMHEAD_RSXRECORDS + i * 16;
    if (m_HAL->readByte(rsxRecordAddr + RSXRECORD_NONBANK) == 0x00) {  // non banked flag: here we act as banked system (we already have some BDOS funcs)
      uint16_t codepos = m_HAL->readWord(rsxRecordAddr + RSXRECORD_OFFSET);
      uint16_t codelen = m_HAL->readWord(rsxRecordAddr + RSXRECORD_CODELEN);
      loadRSX(TPA_ADDR + codepos, codelen);
    }
  }

  // move main program
  uint16_t proglen = m_HAL->readWord(TPA_ADDR + COMHEAD_LEN);
  m_HAL->moveMem(TPA_ADDR, TPA_ADDR + 0x100, proglen);
}


void BDOS::loadRSX(uint16_t imageAddr, int imageLen)
{
  #if MSGDEBUG & DEBUG_BDOS
  char const * rsxName[8];
  m_HAL->copyMem(rsxName, imageAddr + RSXPREFIX_NAME, 8);
  m_HAL->logf("loadRSX \"%8s\"\r\n", rsxName);
  #endif

  // first RSX
  uint16_t firstRSXaddr = m_HAL->readWord(PAGE0_OSBASE) - RSXPREFIX_START;

  // calculate new RSX position
  uint16_t thisRSXaddr = (firstRSXaddr - imageLen) & 0xFF00; // page aligned

  // set next module address
  m_HAL->writeWord(imageAddr + RSXPREFIX_NEXT, m_HAL->readWord(PAGE0_OSBASE));

  // set prev module address
  m_HAL->writeWord(imageAddr + RSXPREFIX_PREV, PAGE0_BDOS);

  // redirect base entry
  m_HAL->writeWord(PAGE0_OSBASE, thisRSXaddr + RSXPREFIX_START);

  // update SCB MXTPA field
  SCB_setWord(SCB_TOPOFUSERTPA_W, m_HAL->readWord(PAGE0_OSBASE));

  // set next module "prev" field, if not original BDOS
  if (firstRSXaddr + RSXPREFIX_START != BDOS_ENTRY)
    m_HAL->writeWord(firstRSXaddr + RSXPREFIX_PREV, m_HAL->readWord(PAGE0_OSBASE));

  // copy and relocate RSX
  uint16_t relmapAddr = imageAddr + imageLen;
  int offset = (thisRSXaddr >> 8) - 1;
  for (int i = 0; i < imageLen; ++i) {
    if (m_HAL->readByte(relmapAddr + i / 8) & (1 << (7 - (i % 8))))
      m_HAL->writeByte(thisRSXaddr + i, m_HAL->readByte(imageAddr + i) + offset);
    else
      m_HAL->writeByte(thisRSXaddr + i, m_HAL->readByte(imageAddr + i));
  }
}


void BDOS::removeRSX()
{
  uint16_t rsxAddr = m_HAL->readWord(PAGE0_OSBASE) - RSXPREFIX_START;
  while (rsxAddr + RSXPREFIX_START != BDOS_ENTRY && rsxAddr != 0 && rsxAddr != 65535 - 6) {

    uint16_t rsxPrev = m_HAL->readWord(rsxAddr + RSXPREFIX_PREV);
    uint16_t rsxNext = m_HAL->readWord(rsxAddr + RSXPREFIX_NEXT);

    // can remove?
    if (m_HAL->readByte(rsxAddr + RSXPREFIX_REMOVE) == 0xFF && !SCB_testBit(SCB_CCPFLAGS1_B, SCB_CCPFLAGS1_NULLRSX)) {
      // yes, remove this RSX
      #if MSGDEBUG & DEBUG_BDOS
      char const * rsxName[8];
      m_HAL->copyMem(rsxName, rsxAddr + RSXPREFIX_NAME, 8);
      m_HAL->logf("removeRSX \"%8s\"\r\n", rsxName);
      #endif

      // update "next" field of previous RSX
      if (rsxPrev == PAGE0_BDOS) {
        // previous is PAGE0_BDOS (0x0005), update this address
        m_HAL->writeWord(PAGE0_OSBASE, rsxNext);
        // update SCB MXTPA field
        SCB_setWord(SCB_TOPOFUSERTPA_W, m_HAL->readWord(PAGE0_OSBASE));
      } else {
        // previous is another RSX, update its next field
        m_HAL->writeWord(rsxPrev - RSXPREFIX_START + RSXPREFIX_NEXT, rsxNext);
      }

      // update "prev" field of next RSX
      if (rsxNext != BDOS_ENTRY) {
        m_HAL->writeWord(rsxNext - RSXPREFIX_START + RSXPREFIX_PREV, rsxPrev);
      }
    }

    // just in case: remove RSX only flag, so it will be removed next time
    SCB_clearBit(SCB_CCPFLAGS1_B, SCB_CCPFLAGS1_NULLRSX);

    // next RSX
    rsxAddr = rsxNext - RSXPREFIX_START;
  }
}


// return true when there is at least one RSX in memory
bool BDOS::RSXInstalled()
{
  return SCB_getWord(SCB_TOPOFUSERTPA_W) != BDOS_ENTRY;
}


bool BDOS::BDOSAddrChanged()
{
  return m_HAL->readWord(PAGE0_OSBASE) != BDOS_ENTRY;
}


bool BDOS::BIOSAddrChanged()
{
  return m_HAL->readWord(PAGE0_WSTARTADDR) != (BIOS_ENTRY + 3);
}


// set A, L, B, H CPU registers in one step
void BDOS::CPUsetALBH(uint8_t A, uint8_t L, uint8_t B, uint8_t H)
{
  m_HAL->CPU_writeRegByte(Z80_A, A);
  m_HAL->CPU_writeRegByte(Z80_L, L);
  m_HAL->CPU_writeRegByte(Z80_B, B);
  m_HAL->CPU_writeRegByte(Z80_H, H);
}


void BDOS::CPUsetALBH(uint8_t AL, uint8_t BH)
{
  CPUsetALBH(AL, AL, BH, BH);
}


// set HL and BA  (H=hi byte,L=lo byte, B=hi byte, A=lo byte)
void BDOS::CPUsetHLBA(uint16_t HL, uint16_t BA)
{
  m_HAL->CPU_writeRegWord(Z80_HL, HL);
  m_HAL->CPU_writeRegByte(Z80_A, BA & 0xFF);
  m_HAL->CPU_writeRegByte(Z80_B, BA >> 8);
}


void BDOS::BDOS_call(uint16_t * BC, uint16_t * DE, uint16_t * HL, uint16_t * AF)
{
  m_HAL->CPU_writeRegWord(Z80_BC, *BC);
  m_HAL->CPU_writeRegWord(Z80_DE, *DE);
  m_HAL->CPU_writeRegWord(Z80_HL, *HL);
  m_HAL->CPU_writeRegWord(Z80_AF, *AF);

  // has the BDOS been changed?
  if (m_HAL->readWord(PAGE0_OSBASE) != BDOS_ENTRY) {

    // yes, you have to call BDOS using CPU execution

    // set return address (it is important that is it inside TPA, some RSX check it)
    uint16_t retAddr = 0x100;               // as if calling from 0x100 (like it is CPP)
    m_HAL->CPU_pushStack(retAddr);          // RET will interrupt execution

    m_HAL->CPU_exec(PAGE0_BDOS, retAddr);

  } else {

    // no, you can directly call BDOS function here

    processBDOS();
  }

  *BC = m_HAL->CPU_readRegWord(Z80_BC);
  *DE = m_HAL->CPU_readRegWord(Z80_DE);
  *HL = m_HAL->CPU_readRegWord(Z80_HL);
  *AF = m_HAL->CPU_readRegWord(Z80_AF);
}


int BDOS::BDOS_callConsoleIn()
{
  uint16_t BC = 0x0001, DE = 0, HL = 0, AF = 0;
  BDOS_call(&BC, &DE, &HL, &AF);
  return AF >> 8;
}


int BDOS::BDOS_callConsoleStatus()
{
  uint16_t BC = 0x000B, DE = 0, HL = 0, AF = 0;
  BDOS_call(&BC, &DE, &HL, &AF);
  return AF >> 8;
}


int BDOS::BDOS_callDirectConsoleIO(int mode)
{
  uint16_t BC = 0x0006, DE = mode, HL = 0, AF = 0;
  BDOS_call(&BC, &DE, &HL, &AF);
  return AF >> 8;
}


void BDOS::BDOS_callConsoleOut(char c)
{
  uint16_t BC = 0x0002, DE = c, HL = 0, AF = 0;
  BDOS_call(&BC, &DE, &HL, &AF);
}


// str must end with '0'
// maxChars can be 0 (entire length of "str")
void BDOS::BDOS_callOutputString(char const * str, uint16_t workBufAddr, size_t workBufSize, size_t maxChars)
{
  auto slen = strlen(str);
  if (maxChars > 0 && slen > maxChars)
    slen = maxChars;
  while (slen > 0) {
    auto len = fabgl::tmin<size_t>(workBufSize - 1, slen);
    m_HAL->copyMem(workBufAddr, str, len);
    m_HAL->writeByte(workBufAddr + len, SCB_getByte(SCB_OUTPUTDELIMETER_B));
    uint16_t BC = 0x0009, DE = workBufAddr, HL = 0, AF = 0;
    BDOS_call(&BC, &DE, &HL, &AF);
    slen -= len;
    str += len;
  }
}


void BDOS::BDOS_callOutputString(uint16_t str)
{
  uint16_t BC = 0x0009, DE = str, HL = 0, AF = 0;
  BDOS_call(&BC, &DE, &HL, &AF);
}


// if addr = 0x0000, then use PAGE0_DMA as input buffer
void BDOS::BDOS_callReadConsoleBuffer(uint16_t addr)
{
  uint16_t BC = 0x000A, DE = addr, HL = 0, AF = 0;
  BDOS_call(&BC, &DE, &HL, &AF);
}


void BDOS::BDOS_callSystemReset()
{
  uint16_t BC = 0x0000, DE = 0, HL = 0, AF = 0;
  BDOS_call(&BC, &DE, &HL, &AF);
}


// ret HL value
int BDOS::BDOS_callParseFilename(uint16_t PFCBaddr)
{
  uint16_t BC = 0x0098, DE = PFCBaddr, HL = 0, AF = 0;
  BDOS_call(&BC, &DE, &HL, &AF);
  return HL;
}


// ret HL value
int BDOS::BDOS_callSearchForFirst(uint16_t FCBaddr)
{
  uint16_t BC = 0x0011, DE = FCBaddr, HL = 0, AF = 0;
  BDOS_call(&BC, &DE, &HL, &AF);
  return HL;
}


// ret HL value
int BDOS::BDOS_callSearchForNext()
{
  uint16_t BC = 0x0012, DE = 0, HL = 0, AF = 0;
  BDOS_call(&BC, &DE, &HL, &AF);
  return HL;
}


// ret A value
int BDOS::BDOS_callDeleteFile(uint16_t FCBaddr)
{
  uint16_t BC = 0x0013, DE = FCBaddr, HL = 0, AF = 0;
  BDOS_call(&BC, &DE, &HL, &AF);
  return AF >> 8;
}


// ret A value
int BDOS::BDOS_callRenameFile(uint16_t FCBaddr)
{
  uint16_t BC = 0x0017, DE = FCBaddr, HL = 0, AF = 0;
  BDOS_call(&BC, &DE, &HL, &AF);
  return AF >> 8;
}


// ret HA value
int BDOS::BDOS_callOpenFile(uint16_t FCBaddr)
{
  uint16_t BC = 0x000F, DE = FCBaddr, HL = 0, AF = 0;
  BDOS_call(&BC, &DE, &HL, &AF);
  return (AF >> 8) | (HL & 0xFF00);
}


// ret HA value
int BDOS::BDOS_callMakeFile(uint16_t FCBaddr)
{
  uint16_t BC = 0x0016, DE = FCBaddr, HL = 0, AF = 0;
  BDOS_call(&BC, &DE, &HL, &AF);
  return (AF >> 8) | (HL & 0xFF00);
}


// ret A value
int BDOS::BDOS_callCloseFile(uint16_t FCBaddr)
{
  uint16_t BC = 0x0010, DE = FCBaddr, HL = 0, AF = 0;
  BDOS_call(&BC, &DE, &HL, &AF);
  return AF >> 8;
}


// ret A value
int BDOS::BDOS_callReadSequential(uint16_t FCBaddr)
{
  uint16_t BC = 0x0014, DE = FCBaddr, HL = 0, AF = 0;
  BDOS_call(&BC, &DE, &HL, &AF);
  return AF >> 8;
}


void BDOS::BDOS_callSetDMAAddress(uint16_t DMAaddr)
{
  uint16_t BC = 0x001A, DE = DMAaddr, HL = 0, AF = 0;
  BDOS_call(&BC, &DE, &HL, &AF);
}


int BDOS::BDOS_callCopyFile(uint16_t srcFullPathAddr, uint16_t dstPathAddr, bool overwrite, bool display)
{
  uint16_t BC = (overwrite ? 0x0100 : 0) | (display ? 0x0200 : 0) | 0x00D4;
  uint16_t DE = dstPathAddr;
  uint16_t HL = srcFullPathAddr;
  uint16_t AF = 0;
  BDOS_call(&BC, &DE, &HL, &AF);
  return AF >> 8;
}


int BDOS::BDOS_callChangeCurrentDirectory(uint16_t pathAddr)
{
  uint16_t BC = 0x00D5, DE = pathAddr, HL = 0, AF = 0;
  BDOS_call(&BC, &DE, &HL, &AF);
  return AF >> 8;
}


// ret = true, program end
void BDOS::processBDOS()
{
  // register C contains the BDOS function to execute
  int func = m_HAL->CPU_readRegByte(Z80_C);
  switch (func) {

    // 0 (0x00) : System Reset
    case 0x00:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: System Reset\r\n", func);
      #endif
      BDOS_systemReset();
      break;

    // 1 (0x01) : Console Input
    case 0x01:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: Console Input\r\n", func);
      #endif
      BDOS_consoleInput();
      break;

    // 2 (0x02) : Console output
    case 0x02:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: Console output\r\n", func);
      #endif
      BDOS_consoleOutput();
      break;

    // 3 (0x03) : Aux input
    case 0x03:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: Aux input\r\n", func);
      #endif
      BDOS_auxInput();
      break;

    // 4 (0x04) : Aux output
    case 0x04:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: Aux output\r\n", func);
      #endif
      BDOS_auxOutput();
      break;

    // 5 (0x05) : LST output
    case 0x05:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: LST output\r\n", func);
      #endif
      BDOS_lstOutput();
      break;

    // 6 (0x06) : Direct Console IO
    case 0x06:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: Direct Console IO\r\n", func);
      #endif
      BDOS_directConsoleIO();
      break;

    // 7 (0x07) : Aux input status
    case 0x07:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: Aux input status\r\n", func);
      #endif
      BDOS_auxInputStatus();
      break;

    // 8 (0x08) : Aux output status
    case 0x08:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: Aux output status\r\n", func);
      #endif
      BDOS_auxOutputStatus();
      break;

    // 9 (0x09) : Output string
    case 0x09:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: Output string\r\n", func);
      #endif
      BDOS_outputString();
      break;

    // 10 (0x0A) : Read console buffer
    case 0x0A:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: Read console buffer\r\n", func);
      #endif
      BDOS_readConsoleBuffer();
      break;

    // 11 (0x0B) : Console status
    case 0x0B:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: Console status\r\n", func);
      #endif
      BDOS_getConsoleStatus();
      break;

    // 12 (0x0C) : Return version number
    case 0x0C:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: Return version number\r\n", func);
      #endif
      BDOS_returnVersionNumber();
      break;

    // 13 (0x0D) : Reset disk system
    case 0x0D:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: Reset disk system\r\n", func);
      #endif
      BDOS_resetDiskSystem();
      break;

    // 14 (0x0E) : Select disk
    case 0x0E:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: Select disk\r\n", func);
      #endif
      BDOS_selectDisk();
      break;

    // 15 (0x0F) : Open file
    case 0x0F:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: Open file\r\n", func);
      #endif
      BDOS_openFile();
      break;

    // 16 (0x10) : Close file
    case 0x10:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: Close file\r\n", func);
      #endif
      BDOS_closeFile();
      break;

    // 17 (0x11) : Search for first
    case 0x11:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: Search for first\r\n", func);
      #endif
      BDOS_searchForFirst();
      break;

    // 18 (0x12) : Search for next
    case 0x12:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: Search for next\r\n", func);
      #endif
      BDOS_searchForNext();
      break;

    // 19 (0x13) : Delete file
    case 0x13:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: Delete file\r\n", func);
      #endif
      BDOS_deleteFile();
      break;

    // 20 (0x14) : Read sequential
    case 0x14:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: Read sequential\r\n", func);
      #endif
      BDOS_readSequential();
      break;

    // 21 (0x15) : Write sequential
    case 0x15:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: Write sequential\r\n", func);
      #endif
      BDOS_writeSequential();
      break;

    // 22 (0x16) : Create file / Create Directory
    case 0x16:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: Create file/dir\r\n", func);
      #endif
      BDOS_makeFile();
      break;

    // 23 (0x17) : Rename file
    case 0x17:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: Rename file\r\n", func);
      #endif
      BDOS_renameFile();
      break;

    // 24 (0x18) : Return login vector
    case 0x18:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: Return login vector\r\n", func);
      #endif
      BDOS_returnLoginVector();
      break;

    // 25 (0x19) : Return current disk
    case 0x19:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: Return current disk\r\n", func);
      #endif
      BDOS_returnCurrentDisk();
      break;

    // 26 (0x1A) : Set DMA address
    case 0x1A:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: Set DMA address\r\n", func);
      #endif
      BDOS_setDMAAddress();
      break;

    // 27 (0x1B) : Get Addr (Alloc)
    case 0x1B:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: Get Addr (Alloc)\r\n", func);
      #endif
      BDOS_getAddr();
      break;

    // 28 (0x1C) : Write protect disk
    case 0x1C:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: Write protect disk\r\n", func);
      #endif
      BDOS_writeProtectDisk();
      break;

    // 29 (0x1D) : Get read only vector
    case 0x1D:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: Get read only vector\r\n", func);
      #endif
      BDOS_getReadOnlyVector();
      break;

    // 30 (0x1E) : Set File Attributes
    case 0x1E:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: Set File Attributes\r\n", func);
      #endif
      BDOS_setFileAttributes();
      break;

    // 31 (0x1F) : Get DPB address (Disc Parameter Block)
    case 0x1F:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: Get DPB address\r\n", func);
      #endif
      BDOS_getDPBAddress();
      break;

    // 32 (0x20) : Get/set user number
    case 0x20:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: get/set user number\r\n", func);
      #endif
      BDOS_getSetUserCode();
      break;

    // 33 (0x21) : Random access read record
    case 0x21:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: Random access read record\r\n", func);
      #endif
      BDOS_readRandom();
      break;

    // 34 (0x22) : Random access write record
    case 0x22:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: Random access write record\r\n", func);
      #endif
      BDOS_writeRandom();
      break;

    // 35 (0x23) : Compute file size
    case 0x23:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: Compute file size\r\n", func);
      #endif
      BDOS_computeFileSize();
      break;

    // 36 (0x24) : Set random record
    case 0x24:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: Set random record\r\n", func);
      #endif
      BDOS_setRandomRecord();
      break;

    // 37 (0x25) : Reset drive
    case 0x25:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: Reset drive\r\n", func);
      #endif
      BDOS_resetDrive();
      break;

    // 38 (0x26) : Access drive
    case 0x26:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: Access drive\r\n", func);
      #endif
      BDOS_accessDrive();
      break;

    // 39 (0x27) : Free drive
    case 0x27:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: Free drive\r\n", func);
      #endif
      BDOS_freeDrive();
      break;

    // 40 (0x28) : Write random with zero fill
    case 0x28:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: Write random with zero fill\r\n", func);
      #endif
      BDOS_writeRandomZeroFill();
      break;

    // 41 (0x29) : Test and write record
    case 0x29:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: Test and write record\r\n", func);
      #endif
      BDOS_testAndWriteRecord();
      break;

    // 42 (0x2A) : Lock record
    case 0x2A:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: Lock record\r\n", func);
      #endif
      BDOS_lockRecord();
      break;

    // 43 (0x2B) : Unock record
    case 0x2B:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: Unock record\r\n", func);
      #endif
      BDOS_unlockRecord();
      break;

    // 44 (0x2C) : Set multi-sector count
    case 0x2C:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: Set multi-sector count\r\n", func);
      #endif
      BDOS_setMultiSectorCount();
      break;

    // 45 (0x2D) : Set error mode
    case 0x2D:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: Set error mode\r\n", func);
      #endif
      BDOS_setErrorMode();
      break;

    // 46 (0x2E) : Get disk free space
    case 0x2E:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: Get disk free space\r\n", func);
      #endif
      BDOS_getDiskFreeSpace();
      break;

    // 47 (0x2F) : Chain to program
    case 0x2F:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: Chain to program\r\n", func);
      #endif
      BDOS_chainToProgram();
      break;

    // 48 (0x30) : Flush buffers
    case 0x30:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: Flush buffers\r\n", func);
      #endif
      BDOS_flushBuffers();
      break;

    // 49 (0x31) : Get/set system control block
    case 0x31:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: Get/set system control block\r\n", func);
      #endif
      BDOS_getSetSystemControlBlock();
      break;

    // 50 (0x32) : Direct BIOS call
    case 0x32:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: Direct BIOS call\r\n", func);
      #endif
      BDOS_directBIOSCall();
      break;

    // 59 (0x3B) : Load overlay
    case 0x3B:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: Load overlay\r\n", func);
      #endif
      BDOS_loadOverlay();
      break;

    // 60 (0x3C) : Call System Resident Extension
    case 0x3C:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: Call System Resident Extension\r\n", func);
      #endif
      BDOS_callResidentSystemExtension();
      break;

    // 98 (0x62) : Free Blocks
    case 0x62:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: Free Blocks\r\n", func);
      #endif
      BDOS_freeBlocks();
      break;

    // 99 (0x63) : Truncate file
    case 0x63:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: Truncate file\r\n", func);
      #endif
      BDOS_truncateFile();
      break;

    // 100 (0x64) : Set Directory Label
    case 0x64:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: Set Directory Label\r\n", func);
      #endif
      BDOS_setDirectoryLabel();
      break;

    // 101 (0x65) : Return directory label data
    case 0x65:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: Return directory label data\r\n", func);
      #endif
      BDOS_returnDirLabelData();
      break;

    // 102 (0x66) : Read file date stamps and password mode
    case 0x66:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: Read file date stamps and password mode\r\n", func);
      #endif
      BDOS_readFileDateStamps();
      break;

    // 104 (0x68) : Set date and time
    case 0x68:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: Set date and time\r\n", func);
      #endif
      BDOS_setDateTime();
      break;

    // 105 (0x69) : Get date and time
    case 0x69:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: Get date and time\r\n", func);
      #endif
      BDOS_getDateTime();
      break;

    // 108 (0x6C) : Get/Set program return code
    case 0x6C:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: Get/Set program return code\r\n", func);
      #endif
      BDOS_getSetProgramReturnCode();
      break;

    // 109 (0x6D) : Set or get console mode
    case 0x6D:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: Set or get console mode\r\n", func);
      #endif
      BDOS_getSetConsoleMode();
      break;

    // 110 (0x6E) : Get set output delimiter
    case 0x6E:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: Get set output delimiter\r\n", func);
      #endif
      BDOS_getSetOutputDelimiter();
      break;

    // 111 (0x6F) : Print block
    case 0x6F:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: Print block\r\n", func);
      #endif
      BDOS_printBlock();
      break;

    // 112 (0x6F) : List block
    case 0x70:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: List block\r\n", func);
      #endif
      BDOS_listBlock();
      break;

    // 152 (0x98) : Parse filename
    case 0x98:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: Parse filename\r\n", func);
      #endif
      BDOS_parseFilename();
      break;

    // 212 (0xD4) : Copy file (specific of this BDOS implementation)
    case 0xD4:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: Copy file\r\n", func);
      #endif
      BDOS_copyFile();
      break;

    // 213 (0xD5) : Change current directory (specific of this BDOS implementation)
    case 0xD5:
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("BDOS %d: Change current directory\r\n", func);
      #endif
      BDOS_changeCurrentDirectory();
      break;

    default:
      #if MSGDEBUG & DEBUG_ERRORS
      m_HAL->logf("BDOS %d: Unsupported\r\n", func);
      #endif
      break;
  }
}


// this overload check Error Mode field and can display or abort application
void BDOS::doError(uint8_t A, uint8_t H, const char * format, ...)
{
  SCB_setByte(SCB_ERRORDRIVE_B, getCurrentDrive());
  if (isDefaultErrorMode()) {
    m_HAL->CPU_stop();
    SCB_setWord(SCB_PROGRAMRETCODE_W, 0xFFFD);
  }
  if (isDefaultErrorMode() || isDisplayReturnErrorMode()) {
    va_list ap;
    va_start(ap, format);
    int size = vsnprintf(nullptr, 0, format, ap) + 1;
    if (size > 0) {
      va_end(ap);
      va_start(ap, format);
      char buf[size + 1];
      vsnprintf(buf, size, format, ap);
      consoleOutStr(buf);
    }
    va_end(ap);
  }
  CPUsetALBH(A, H);
}


// BDOS 0 (0x00)
void BDOS::BDOS_systemReset()
{
  // jump to PAGE0_WSTART
  m_HAL->CPU_setPC(PAGE0_WSTART);
}


// BDOS 1 (0x01)
void BDOS::BDOS_consoleInput()
{
  uint8_t c = consoleIn();
  CPUsetALBH(c, 0);
}


// BDOS 2 (0x02)
void BDOS::BDOS_consoleOutput()
{
  char c = m_HAL->CPU_readRegByte(Z80_E);
  consoleOutChar(c);
}


// BDOS 3 (0x03)
void BDOS::BDOS_auxInput()
{
  int c = 0;
  if (m_auxStream) {
    while (!m_auxStream->available())
      ;
    c = m_auxStream->read();
  }
  CPUsetALBH(c, 0);
}


// BDOS 4 (0x04)
void BDOS::BDOS_auxOutput()
{
  if (m_auxStream) {
    m_auxStream->write(m_HAL->CPU_readRegByte(Z80_E));
  }
}


// BDOS 5 (0x05)
void BDOS::BDOS_lstOutput()
{
  lstOut(m_HAL->CPU_readRegByte(Z80_E));
}


// BDOS 6 (0x06)
void BDOS::BDOS_directConsoleIO()
{
  uint8_t v = m_HAL->CPU_readRegByte(Z80_E);
  if (v == 0xFF) {
    // return input char. If no char return 0
    uint8_t v = rawConsoleAvailable() ? rawConsoleIn() : 0;
    CPUsetALBH(v, 0);
  } else if (v == 0xFE) {
    // return status. 0 = no char avail, ff = char avail
    uint8_t v = rawConsoleAvailable() ? 0xFF : 0;
    CPUsetALBH(v, 0);
  } else if (v == 0xFD) {
    // return input char (suspend)
    uint8_t v = rawConsoleIn();
    CPUsetALBH(v, 0);
  } else {
    // send "v" con console
    m_BIOS->BIOS_call_CONOUT(v);
  }
}


// BDOS 7 (0x07)
void BDOS::BDOS_auxInputStatus()
{
  int v = 0;
  if (m_auxStream && m_auxStream->available())
    v = 0xFF;
  CPUsetALBH(v, 0);
}


// BDOS 8 (0x08)
void BDOS::BDOS_auxOutputStatus()
{
  // always ready!
  CPUsetALBH(0xFF, 0);
}


// BDOS 9 (0x09)
void BDOS::BDOS_outputString()
{
  uint16_t addr = m_HAL->CPU_readRegWord(Z80_DE);
  char delim = SCB_getByte(SCB_OUTPUTDELIMETER_B);
  consoleOutStr(addr, delim);
}


// BDOS 10 (0x0A)
void BDOS::BDOS_readConsoleBuffer()
{
  uint16_t bufAddrParam = m_HAL->CPU_readRegWord(Z80_DE);
  uint16_t bufAddr = (bufAddrParam ? bufAddrParam : PAGE0_DMA);
  int mx = fabgl::tmax<int>(1, m_HAL->readByte(bufAddr));
  LineEditor ed(nullptr);
  if (bufAddrParam == 0) {
    char str[m_HAL->strLen(bufAddr) + 1];
    m_HAL->copyStr(str, bufAddr + 2);
    ed.typeText(str);
  }

  ed.onRead = [&](int * c) {
    *c = m_BIOS->BIOS_call_CONIN();
  };

  ed.onWrite = [&](int c) {
    m_BIOS->BIOS_call_CONOUT(c);
  };

  ed.onChar = [&](int * c) {
    switch (*c) {
      case ASCII_CTRLC:
        if (isDisableCTRLCExit() == false) {
          *c = ASCII_CR;
          SCB_setWord(SCB_PROGRAMRETCODE_W, 0xFFFE);
          m_HAL->CPU_stop();
        }
        break;
      case ASCII_LF: // aka CTRL-J
        *c = ASCII_CR;
        break;
      case ASCII_CTRLP:
        switchPrinterEchoEnabled();
        break;
    }
  };

  ed.onSpecialChar = [&](LineEditorSpecialChar sc) {
    switch (sc) {
      case LineEditorSpecialChar::CursorUp:
      {
        char const * txt = getPrevHistoryItem();
        if (txt)
          ed.setText(txt);
        break;
      }
      case LineEditorSpecialChar::CursorDown:
      {
        char const * txt = getNextHistoryItem();
        if (txt)
          ed.setText(txt);
        break;
      }
    }
  };

  ed.onCarriageReturn = [&](int * op) {
    m_BIOS->BIOS_call_CONOUT(ASCII_CR);  // bdos 10 always echoes even CR
    *op = 1;  // on carriage return just perform end editing (without new line)
  };

  ed.edit(mx);

  auto len = strlen(ed.get());
  m_HAL->writeByte(bufAddr + 1, len);
  m_HAL->copyMem(bufAddr + 2, ed.get(), len);

  if (m_printerEchoEnabled)
    lstOut(ed.get());

  // save into history
  saveIntoConsoleHistory(ed.get());

  #if MSGDEBUG & DEBUG_BDOS
  m_HAL->logf("BDOS 10: Read console buffer - EXIT\r\n");
  #endif
}


// BDOS 11 (0x0B)
void BDOS::BDOS_getConsoleStatus()
{
  int ret = 0;
  if (isFUNC11CTRLCOnlyMode()) {
    if (rawConsoleDirectAvailable()) {
      // ret 0x01 only if CTRL-C has been typed
      m_consoleReadyChar = rawConsoleDirectIn();
      if (m_consoleReadyChar == ASCII_CTRLC)
        ret = 0x01;
    }
  } else if (rawConsoleAvailable()) {
    // ret 0x01 for any char
    ret = 0x01;
  }
  CPUsetALBH(ret, 0);
}


// BDOS 12 (0x0C)
void BDOS::BDOS_returnVersionNumber()
{
  // A=L=0x31 => 3.1 - CP/M Plus
  // B=H=0x00 => 8080, CP/M
  CPUsetALBH(0x31, 0);
}


// BDOS 13 (0x0D)
void BDOS::BDOS_resetDiskSystem()
{
  // reset DMA to 0x0080
  SCB_setWord(SCB_CURRENTDMAADDR_W, PAGE0_DMA);

  // current disk is A
  setCurrentDrive(0);

  // current user is 0
  setCurrentUser(0);

  // multi sector count
  SCB_setByte(SCB_MULTISECTORCOUNT_B, 1);

  // reset write protect disk word
  m_writeProtectWord = 0;

  CPUsetALBH(0x00, 0x00);
}


// BDOS 14 (0x0E)
void BDOS::BDOS_selectDisk()
{
  int drive = m_HAL->CPU_readRegByte(Z80_E);
  #if MSGDEBUG & DEBUG_BDOS
  m_HAL->logf("  drive=%c\r\n", 'A' + drive);
  #endif
  if (!checkDrive(drive, 14))
    return;
  setCurrentDrive(drive);
  CPUsetALBH(0x00, 0x00);
}


// generate a 32 bit hash for the filename and drive number
// djb2 hashing algorithm
// TODO: collisions exist, so it could be better to find another way to handle open files caching!
uint32_t BDOS::filenameHash(uint16_t FCBaddr)
{
  uint32_t hash = (5381 << 5) + 5381 + getDriveFromFCB(FCBaddr);
  for (int i = FCB_F1; i <= FCB_T3; ++i)
    hash = (hash << 5) + hash + (m_HAL->readByte(FCBaddr + i) & 0x7f);
  return hash;
}


// ret nullptr if not found
FILE * BDOS::getFileFromCache(uint16_t FCBaddr)
{
  const uint32_t hash = filenameHash(FCBaddr);
  for (int i = 0; i < CPMMAXFILES; ++i)
    if (hash == m_openFileCache[i].filenameHash && m_openFileCache[i].file) {

      #if MSGDEBUG & DEBUG_BDOS
      // check for hash failure
      for (int j = 0; j < 11; ++j) {
        if (m_openFileCache[i].filename[j] != (m_HAL->readByte(FCBaddr + FCB_F1 + j) & 0x7f)) {
          // fail!
          HAL::logf("Hash failure!!  \"");
          for (int k = 0; k < 11; ++k)
            HAL::logf("%c", m_HAL->readByte(FCBaddr + FCB_F1 + k) & 0x7f);
          HAL::logf("\" <> \"%s\"\r\n", m_openFileCache[i].filename);
          m_HAL->abort(AbortReason::GeneralFailure);
        }
      }
      #endif

      return m_openFileCache[i].file;
    }
  return nullptr;
}


void BDOS::addFileToCache(uint16_t FCBaddr, FILE * file)
{
  if (file) {
    const uint32_t hash = filenameHash(FCBaddr);
    int idx = -1;
    for (int i = 0; i < CPMMAXFILES; ++i)
      if (m_openFileCache[i].file == nullptr) {
        idx = i;
        break;
      }
    if (idx == -1) {
      // no more free items, close one (@TODO: chose a better algorithm!)
      idx = rand() % CPMMAXFILES;
      fclose(m_openFileCache[idx].file);
    }
    m_openFileCache[idx].file         = file;
    m_openFileCache[idx].filenameHash = hash;

    #if MSGDEBUG & DEBUG_BDOS
    for (int i = 0; i < 11; ++i)
      m_openFileCache[idx].filename[i] = m_HAL->readByte(FCBaddr + FCB_F1 + i) & 0x7f;
    m_openFileCache[idx].filename[11] = 0;
    HAL::logf("addFileToCache handle=%p name=\"%s\" idx=%d\r\n", file, m_openFileCache[idx].filename, idx);
    #endif
  }
}


void BDOS::removeFileFromCache(FILE * file)
{
  for (int i = 0; i < CPMMAXFILES; ++i)
    if (m_openFileCache[i].file == file) {
      m_openFileCache[i].file = nullptr;
      return;
    }
}


int BDOS::getOpenFilesCount()
{
  int ret = 0;
  for (int i = 0; i < CPMMAXFILES; ++i)
    if (m_openFileCache[i].file)
      ++ret;
  return ret;
}


void BDOS::closeAllFiles()
{
  for (int i = 0; i < CPMMAXFILES; ++i)
    if (m_openFileCache[i].file) {
      fclose(m_openFileCache[i].file);
      m_openFileCache[i].file = nullptr;
    }
}



// create:
//    false = file must exist otherwise returns nullptr
//    true  = file mustn't exist otherwise returns nullptr
// tempext:
//    false = let FCB specified extension
//    true = replace extension with $$$
// files are always open as read/write
//   err:
//     0 = no err
//     1 = invalid drive
//     2 = file not found or already exists
FILE * BDOS::openFile(uint16_t FCBaddr, bool create, bool tempext, int errFunc, int * err)
{
  *err = 0;

  // already open?
  if (!tempext) {
    auto f = getFileFromCache(FCBaddr);
    if (f) {
      if (create) {
        // create a file that is already open: close it before!
        removeFileFromCache(f);
        fclose(f);
      } else
        return f;
    }
  }

  int drive = getDriveFromFCB(FCBaddr);

  if (!checkDrive(drive, errFunc)) {
    *err = 1;
    return nullptr;
  }

  setBrowserAtDrive(drive);

  char filename[13];
  getFilenameFromFCB(FCBaddr, filename);
  strToUpper(filename);

  if (tempext) {
    // replace extension with '$$$'
    char * p = strchr(filename, '.');
    if (p)
      *p = 0;
    strcat(filename, ".$$$");
  }

  #if MSGDEBUG & DEBUG_BDOS
  m_HAL->logf("  openFile %s\r\n", filename);
  #endif

  int fullpathLen = m_fileBrowser.getFullPath(filename);
  char fullpath[fullpathLen];
  m_fileBrowser.getFullPath(filename, fullpath, fullpathLen);

  FILE * f = nullptr;

  if (create) {
    if (m_fileBrowser.exists(filename, false))
      *err = 2;
    else
      f = fopen(fullpath, "w+b");
  } else {
    if (!m_fileBrowser.exists(filename, false))
      *err = 2;
    else
      f = fopen(fullpath, "r+b");
  }

  if (f)
    addFileToCache(FCBaddr, f);

  #if MSGDEBUG & DEBUG_BDOS
  m_HAL->logf("  handle = %p\r\n", f);
  #endif

  return f;
}


void BDOS::closeFile(uint16_t FCBaddr)
{
  auto f = getFileFromCache(FCBaddr);
  if (f) {
    removeFileFromCache(f);
    fclose(f);
  }

  #if MSGDEBUG & DEBUG_BDOS
  m_HAL->logf("closeFile %p\r\n", f);
  #endif
}


// BDOS 15 (0x0F)
void BDOS::BDOS_openFile()
{
  uint16_t FCBaddr = m_HAL->CPU_readRegWord(Z80_DE);

  int err;
  FILE * f = openFile(FCBaddr, false, false, 15, &err);

  if (err == 1) {
    // invalid drive
    return; // doError already called
  }

  if (err == 2) {
    // file not found
    CPUsetALBH(0xFF, 0x00);
    return;
  }

  if (f) {
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);

    if (m_HAL->readByte(FCBaddr + FCB_CR) == 0xFF) {
      // last record byte count requested (put byte count into FCB_CR itself)
      m_HAL->writeByte(FCBaddr + FCB_CR, (size / 16384) % 128);
    }

    // set record count (of current extent)
    m_HAL->writeByte(FCBaddr + FCB_RC, fabgl::tmin<size_t>((size + 127) / 128, 128));

    // reset position to 0
    m_HAL->writeByte(FCBaddr + FCB_EX, 0);
    m_HAL->writeByte(FCBaddr + FCB_S2, 0);
    m_HAL->writeByte(FCBaddr + FCB_CR, 0);

    CPUsetALBH(0x00, 0x00);
  } else {
    doError(0xFF, 0x01, "CP/M Error opening file, I/O Error\r\nFunction 15\r\n");
  }
}


// BDOS 16 (0x10)
void BDOS::BDOS_closeFile()
{
  uint16_t FCBaddr = m_HAL->CPU_readRegWord(Z80_DE);
  closeFile(FCBaddr);
  CPUsetALBH(0x00, 0x00);
}


// BDOS 17 (0x11)
void BDOS::BDOS_searchForFirst()
{
  m_fileSearchState.FCB = m_HAL->CPU_readRegWord(Z80_DE);
  m_fileSearchState.DMA = SCB_getWord(SCB_CURRENTDMAADDR_W);
  searchFirst(&m_fileSearchState);

  // update SCB fields
  SCB_setWord(SCB_DCNT_W, m_fileSearchState.index << 2);
  SCB_setWord(SCB_SEARCHA_W, m_fileSearchState.FCB);
  SCB_setByte(SCB_SEARCHL_B, m_fileSearchState.getAllFiles ? 0x00 : 0x0F);

  if (m_fileSearchState.errCode == 0) {
    // 0: file found
    CPUsetALBH(m_fileSearchState.retCode, 0x00);
  } else if (m_fileSearchState.errCode == 1) {
    // 1: no files
    CPUsetALBH(0xFF, 0x00);
  } else {
    // 2: invalid drive
    doError(0xFF, 0x04, "CP/M Error, Invalid Drive\r\nFunction 17\r\n");
  }
}


// BDOS 18 (0x12)
void BDOS::BDOS_searchForNext()
{
  m_fileSearchState.DMA = SCB_getWord(SCB_CURRENTDMAADDR_W);  // necessary? May it be changed between BDOS 17 and 18?
  searchNext(&m_fileSearchState);

  // update SCB fields
  SCB_setWord(SCB_DCNT_W, m_fileSearchState.index << 2);
  SCB_setWord(SCB_SEARCHA_W, m_fileSearchState.FCB);

  if (m_fileSearchState.errCode == 0) {
    // 0: file found
    CPUsetALBH(m_fileSearchState.retCode, 0x00);
  } else {
    // 1: no more files
    CPUsetALBH(0xFF, 0x00);
  }
}


// BDOS 19 (0x13)
void BDOS::BDOS_deleteFile()
{
  FileSearchState state;
  state.DMA = BDOS_BUFADDR;
  state.FCB = m_HAL->CPU_readRegWord(Z80_DE);
  searchFirst(&state);
  if (state.errCode == 1) {
    // no items
    #if MSGDEBUG & DEBUG_BDOS
    m_HAL->logf("  no items\r\n");
    #endif
    CPUsetALBH(0xFF, 0x00);
  } else if (state.errCode == 2) {
    // invalid drive
    #if MSGDEBUG & DEBUG_BDOS
    m_HAL->logf("  invalid drive\r\n");
    #endif
    doError(0xFF, 0x04, "CP/M Error, Invalid Drive\r\nFunction 19\r\n");
  } else {  // r == 0, file found
    while (state.errCode == 0) {
      uint16_t FCBaddr = state.DMA + 32 * state.retCode;
      bool isDirFlag = isDir(FCBaddr);
      if (isDirFlag) {
        // replace "[D]" with "   "
        m_HAL->fillMem(FCBaddr + 9, ' ', 3);
      }
      char filename[13];
      getFilenameFromFCB(FCBaddr, filename);
      #if MSGDEBUG & DEBUG_BDOS
      m_HAL->logf("  filename=%s\r\n", filename);
      #endif
      // m_fileBrowser already positioned by searchFirst()
      m_fileBrowser.remove(filename);
      searchNext(&state);
    }
    CPUsetALBH(0x00, 0x00);
  }
}


// BDOS 20 (0x14)
void BDOS::BDOS_readSequential()
{
  uint16_t DMAaddr = SCB_getWord(SCB_CURRENTDMAADDR_W);
  uint16_t FCBaddr = m_HAL->CPU_readRegWord(Z80_DE);

  int err;
  FILE * f = openFile(FCBaddr, false, false, 20, &err);

  #if MSGDEBUG & DEBUG_BDOS
  m_HAL->logf("  FCB_EX=%d FCB_S2=%d FCB_CR=%d handle=%p\r\n", m_HAL->readByte(FCBaddr + FCB_EX), m_HAL->readByte(FCBaddr + FCB_S2), m_HAL->readByte(FCBaddr + FCB_CR), f);
  #endif

  if (err == 1) {
    // invalid drive
    return; // doError already called
  }

  if (err == 2) {
    // file not found
    CPUsetALBH(0xFF, 0x00);
    return;
  }

  if (f) {

    // go to position identified by FCB_EX, FCB_S2 and FCB_CR
    size_t pos = getPosFCB(FCBaddr);
    fseek(f, pos, SEEK_SET);

    int recCount = SCB_getByte(SCB_MULTISECTORCOUNT_B);
    int bytesCount = recCount * 128;
    int bytesRead = 0;
    for (; bytesRead < bytesCount; ++bytesRead) {
      int c = fgetc(f);
      if (c == EOF)
        break;
      m_HAL->writeByte(DMAaddr + bytesRead, c);
    }

    // fill missing bytes
    for (int i = bytesRead; i < bytesCount; ++i)
      m_HAL->writeByte(DMAaddr + i, 0x1A);

    auto r = (bytesRead + 127) / 128;  // r = number of records read

    #if MSGDEBUG & DEBUG_BDOS
    m_HAL->logf("  pos=%d reccount=%d read=%d\r\n", (int)pos, recCount, (int)r);
    #endif

    // update position
    setPosFCB(FCBaddr, pos + r * 128);

    if (r < recCount) {
      // EOF (H=B=number of records read)
      CPUsetALBH(0x01, r);
    } else {
      // all ok
      CPUsetALBH(0x00, 0x00);
    }
  } else {
    doError(0xFF, 0x01, "CP/M Error reading file, I/O Error\r\nFunction 20\r\n");
  }
}


// BDOS 21 (0x15)
void BDOS::BDOS_writeSequential()
{
  uint16_t DMAaddr = SCB_getWord(SCB_CURRENTDMAADDR_W);
  uint16_t FCBaddr = m_HAL->CPU_readRegWord(Z80_DE);

  int err;
  FILE * f = openFile(FCBaddr, false, false, 21, &err);

  #if MSGDEBUG & DEBUG_BDOS
  m_HAL->logf("  FCB_EX=%d FCB_S2=%d FCB_CR=%d handle=%p\r\n", m_HAL->readByte(FCBaddr + FCB_EX), m_HAL->readByte(FCBaddr + FCB_S2), m_HAL->readByte(FCBaddr + FCB_CR), f);
  #endif

  if (err == 1) {
    // invalid drive
    return; // doError already called
  }

  if (err == 2) {
    // file not found
    CPUsetALBH(0xFF, 0x00);
    return;
  }

  if (f) {

    // go to position identified by FCB_EX, FCB_S2 and FCB_CR
    size_t pos = getPosFCB(FCBaddr);
    fseek(f, pos, SEEK_SET);

    int recCount = SCB_getByte(SCB_MULTISECTORCOUNT_B);
    int bytesCount = recCount * 128;
    int bytesWritten = 0;
    for (; bytesWritten < bytesCount; ++bytesWritten) {
      int e = fputc(m_HAL->readByte(DMAaddr + bytesWritten), f);
      if (e == EOF)
        break;
    }
    auto r = (bytesWritten + 127) / 128;

    #if MSGDEBUG & DEBUG_BDOS
    m_HAL->logf("  pos=%d reccount=%d wrote=%d\r\n", (int)pos, recCount, (int)r);
    #endif

    // update position
    setPosFCB(FCBaddr, pos + r * 128);

    if (r < recCount) {
      // no available data block (H=B=number of records written)
      CPUsetALBH(0x02, r);
    } else {
      // all ok
      CPUsetALBH(0x00, 0x00);
    }

  } else {
    doError(0xFF, 0x01, "CP/M Error writing file, I/O Error\r\nFunction 21\r\n");
  }
}


//   err:
//     0 = no err
//     1 = invalid drive
//     2 = dir already exists
void BDOS::createDir(uint16_t FCBaddr, int errFunc, int * err)
{
  *err = 0;

  int drive = getDriveFromFCB(FCBaddr);

  if (!checkDrive(drive, errFunc)) {
    *err = 1;
    return;
  }

  setBrowserAtDrive(drive);

  char dirname[13];
  getFilenameFromFCB(FCBaddr, dirname);

  if (m_fileBrowser.exists(dirname, false)) {
    *err = 2;
    return;
  }

  m_fileBrowser.makeDirectory(dirname);
}


// BDOS 22 (0x16)
// if bit 7 of FCB drive is set then create a directory instead of a file (CP/M 86 v4)
void BDOS::BDOS_makeFile()
{
  uint16_t FCBaddr = m_HAL->CPU_readRegWord(Z80_DE);

  bool createDirFlag = m_HAL->readByte(FCBaddr) & 0x80;

  int err;
  FILE * f = nullptr;

  if (createDirFlag) {

    // create directory
    createDir(FCBaddr, 22, &err);

  } else {

    f = openFile(FCBaddr, true, false, 22, &err);

  }

  if (err == 1) {
    // invalid drive
    return; // doError already called
  }

  if (err == 2) {
    // file already exists
    doError(0xFF, 0x08, "CP/M Error, File/Dir already exists\r\nFunction 22\r\n");
    return;
  }

  if (f) {

    // all ok

    // reset position to 0
    m_HAL->writeByte(FCBaddr + FCB_EX, 0);
    m_HAL->writeByte(FCBaddr + FCB_S2, 0);
    m_HAL->writeByte(FCBaddr + FCB_CR, 0);

    CPUsetALBH(0x00, 0x00);

  } else if (createDirFlag){
    // creatre directory ok
    CPUsetALBH(0x00, 0x00);
  } else {
    // other non hardware error
    CPUsetALBH(0xFF, 0x00);
  }
}


// BDOS 23 (0x17)
void BDOS::BDOS_renameFile()
{
  uint16_t FCBaddr_old = m_HAL->CPU_readRegWord(Z80_DE);
  uint16_t FCBaddr_new = FCBaddr_old + 16;
  char filename_old[13];
  getFilenameFromFCB(FCBaddr_old, filename_old);
  char filename_new[13];
  getFilenameFromFCB(FCBaddr_new, filename_new);

  int drive = getDriveFromFCB(FCBaddr_old);

  if (!checkDrive(drive, 23))
    return;

  setBrowserAtDrive(drive);

  if (m_fileBrowser.exists(filename_new, false)) {
    // new file already exists
    doError(0xFF, 0x08, "CP/M Error, File already exists\r\nFunction 23\r\n");
  } else if (!m_fileBrowser.exists(filename_old, false)) {
    // orig file not found
    CPUsetALBH(0xFF, 0x00);  // non fatal error
  } else {
    // all ok
    m_fileBrowser.rename(filename_old, filename_new);
    CPUsetALBH(0x00, 0x00);
  }
}


// BDOS 24 (0x18)
void BDOS::BDOS_returnLoginVector()
{
  uint16_t loginVector = 0;
  for (int i = 0; i < MAXDRIVERS; ++i)
    if (m_HAL->getDriveMountPath(i))
      loginVector |= 1 << i;
  m_HAL->CPU_writeRegWord(Z80_HL, loginVector);
}


// BDOS 25 (0x19)
void BDOS::BDOS_returnCurrentDisk()
{
  uint8_t drive = getCurrentDrive();
  CPUsetALBH(drive, 0);
}


// BDOS 26 (0x1A)
void BDOS::BDOS_setDMAAddress()
{
  SCB_setWord(SCB_CURRENTDMAADDR_W, m_HAL->CPU_readRegWord(Z80_DE));
}


// BDOS 27 (0x1B)
void BDOS::BDOS_getAddr()
{
  // not implemented, returns always error
  m_HAL->CPU_writeRegWord(Z80_HL, 0xFFFF);
}


// BDOS 28 (0x1C)
void BDOS::BDOS_writeProtectDisk()
{
  m_writeProtectWord |= 1 << getCurrentDrive();
}


// BDOS 29 (0x1D)
void BDOS::BDOS_getReadOnlyVector()
{
  m_HAL->CPU_writeRegWord(Z80_HL, m_writeProtectWord);
}


// BDOS 30 (0x1E)
void BDOS::BDOS_setFileAttributes()
{
  uint16_t FCBaddr = m_HAL->CPU_readRegWord(Z80_DE);

  int drive = getDriveFromFCB(FCBaddr);

  if (!checkDrive(drive, 30))
    return;

  char filename[13];
  getFilenameFromFCB(FCBaddr, filename);
  setBrowserAtDrive(drive);

  if (m_fileBrowser.exists(filename, false)) {

    // sets last record byte count?
    if ((m_HAL->readByte(FCBaddr + FCB_F6) & 0x80) != 0 && m_HAL->readByte(FCBaddr + FCB_CR) > 0) {

      // yes, set the exact file size
      int byteCount = m_HAL->readByte(FCBaddr + FCB_CR);  // last record byte count (0 = 128 => don't truncate!)

      // get file size
      auto fileSize = m_fileBrowser.fileSize(filename);

      // adjust file size
      fileSize = ((fileSize + 127) / 128 - 1) * 128 + byteCount;

      m_fileBrowser.truncate(filename, fileSize);
    }

    CPUsetALBH(0x00, 0x00);

  } else {
    // file not found
    CPUsetALBH(0xFF, 0x00);
  }
}


// BDOS 31 (0x1F)
void BDOS::BDOS_getDPBAddress()
{
  CPUsetHLBA(DPB_ADDR, DPB_ADDR);
}


// BDOS 32 (0x20)
void BDOS::BDOS_getSetUserCode()
{
  uint8_t user = m_HAL->CPU_readRegByte(Z80_E);
  if (user == 0xFF) {
    uint8_t user = SCB_getByte(SCB_CURRENTUSER_B);
    CPUsetALBH(user, 0);
  } else {
    SCB_setByte(SCB_CURRENTUSER_B, user & 0xF);
  }
}


// BDOS 33 (0x21)
void BDOS::BDOS_readRandom()
{
  uint16_t DMAaddr = SCB_getWord(SCB_CURRENTDMAADDR_W);
  uint16_t FCBaddr = m_HAL->CPU_readRegWord(Z80_DE);

  int err;
  FILE * f = openFile(FCBaddr, false, false, 33, &err);

  #if MSGDEBUG & DEBUG_BDOS
  m_HAL->logf("  FCB_R0=%d FCB_R1=%d FCB_R2=%d handle=%p\r\n", m_HAL->readByte(FCBaddr + FCB_R0), m_HAL->readByte(FCBaddr + FCB_R1), m_HAL->readByte(FCBaddr + FCB_R2), f);
  #endif

  if (err == 1) {
    // invalid drive
    return; // doError already called
  }

  if (err == 2) {
    // file not found
    CPUsetALBH(0xFF, 0x00);
    return;
  }

  if (f) {

    // go to position identified by R0, R1, R2
    size_t pos = getAbsolutePosFCB(FCBaddr);
    fseek(f, pos, SEEK_SET);

    int recCount = SCB_getByte(SCB_MULTISECTORCOUNT_B);
    int bytesCount = recCount * 128;
    int bytesRead = 0;
    for (; bytesRead < bytesCount; ++bytesRead) {
      int c = fgetc(f);
      if (c == EOF)
        break;
      m_HAL->writeByte(DMAaddr + bytesRead, c);
    }

    // fill missing bytes
    for (int i = bytesRead; i < bytesCount; ++i)
      m_HAL->writeByte(DMAaddr + i, 0x1A);

    auto r = (bytesRead + 127) / 128;  // r = number of records read

    // reposition at the start of this record
    fseek(f, pos, SEEK_SET);
    setPosFCB(FCBaddr, pos);

    if (r == 0) {
      // EOF
      CPUsetALBH(0x01, 0x00);
    } else {
      // ok
      CPUsetALBH(0x00, 0x00);
    }

  } else {
    doError(0xFF, 0x01, "CP/M Error reading file, I/O Error\r\nFunction 33\r\n");
  }
}


// BDOS 34 (0x22)
void BDOS::BDOS_writeRandom()
{
  uint16_t DMAaddr = SCB_getWord(SCB_CURRENTDMAADDR_W);
  uint16_t FCBaddr = m_HAL->CPU_readRegWord(Z80_DE);

  int err;
  FILE * f = openFile(FCBaddr, false, false, 34, &err);

  #if MSGDEBUG & DEBUG_BDOS
  m_HAL->logf("  FCB_R0=%d FCB_R1=%d FCB_R2=%d handle=%p\r\n", m_HAL->readByte(FCBaddr + FCB_R0), m_HAL->readByte(FCBaddr + FCB_R1), m_HAL->readByte(FCBaddr + FCB_R2), f);
  #endif

  if (err == 1) {
    // invalid drive
    return; // doError already called
  }

  if (err == 2) {
    // file not found
    CPUsetALBH(0xFF, 0x00);
    return;
  }

  if (f) {

    // go to position identified by R0, R1, R2
    size_t pos = getAbsolutePosFCB(FCBaddr);
    fseek(f, pos, SEEK_SET);

    int recCount = SCB_getByte(SCB_MULTISECTORCOUNT_B);
    int bytesCount = recCount * 128;
    int bytesWritten = 0;
    for (; bytesWritten < bytesCount; ++bytesWritten) {
      int e = fputc(m_HAL->readByte(DMAaddr + bytesWritten), f);
      if (e == EOF)
        break;
    }

    // reposition at the start of this record
    fseek(f, pos, SEEK_SET);
    setPosFCB(FCBaddr, pos);

    CPUsetALBH(0x00, 0x00);

  } else {
    doError(0xFF, 0x01, "CP/M Error writing file, I/O Error\r\nFunction 34\r\n");
  }

}


// BDOS 35 (0x23)
void BDOS::BDOS_computeFileSize()
{
  uint16_t FCBaddr = m_HAL->CPU_readRegWord(Z80_DE);

  int err;
  FILE * f = openFile(FCBaddr, false, false, 35, &err);

  if (err == 1) {
    // invalid drive
    return; // doError already called
  }

  if (err == 2) {
    // file not found
    CPUsetALBH(0xFF, 0x00);
    return;
  }

  if (f) {
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    closeFile(FCBaddr);

    setAbsolutePosFCB(FCBaddr, size);
    CPUsetALBH(0x00, 0x00);

  } else {
    doError(0xFF, 0x01, "CP/M Error reading file, I/O Error\r\nFunction 35\r\n");
  }
}


// BDOS 36 (0x24)
void BDOS::BDOS_setRandomRecord()
{
  uint16_t FCBaddr = m_HAL->CPU_readRegWord(Z80_DE);

  // TODO: is it really necessary to open file?

  int err;
  FILE * f = openFile(FCBaddr, false, false, 36, &err);

  if (err == 1) {
    // invalid drive
    return; // doError already called
  }

  if (err == 2) {
    // file not found
    CPUsetALBH(0xFF, 0x00);
    return;
  }

  if (f) {

    // go to position identified by FCB_EX, FCB_S2 and FCB_CR
    size_t pos = getPosFCB(FCBaddr);
    setAbsolutePosFCB(FCBaddr, pos);  // set r0, r1 and r2

  } else {
    doError(0xFF, 0x01, "CP/M Error reading file, I/O Error\r\nFunction 36\r\n");
  }
}


// BDOS 37 (0x25)
void BDOS::BDOS_resetDrive()
{
  uint16_t driveVector = m_HAL->CPU_readRegWord(Z80_DE);
  for (int i = 0; i < MAXDRIVERS; ++i) {
    if (driveVector & (1 << i)) {
      // reset drive "i"
      m_writeProtectWord &= ~(1 << i);  // restore read/only flag
    }
  }
}


// BDOS 38 (0x26)
void BDOS::BDOS_accessDrive()
{
  // not implemented (MP/M only)
  CPUsetALBH(0x00, 0x00);
}


// BDOS 39 (0x27)
void BDOS::BDOS_freeDrive()
{
  // not implemented (MP/M only)
  CPUsetALBH(0x00, 0x00);
}


// BDOS 40 (0x28)
void BDOS::BDOS_writeRandomZeroFill()
{
  // because we don't allocate "blocks", this has just the same behaviour of write random
  BDOS_writeRandom();
}


// BDOS 41 (0x29)
void BDOS::BDOS_testAndWriteRecord()
{
  // not implemented (MP/M II only)
  CPUsetALBH(0xFF, 0x00);
}


// BDOS 42 (0x2A)
void BDOS::BDOS_lockRecord()
{
  // not implemented (MP/M II only)
  CPUsetALBH(0x00, 0x00);
}


// BDOS 43 (0x2B)
void BDOS::BDOS_unlockRecord()
{
  // not implemented (MP/M II only)
  CPUsetALBH(0x00, 0x00);
}


// BDOS 44 (0x2C)
void BDOS::BDOS_setMultiSectorCount()
{
  uint8_t v = m_HAL->CPU_readRegByte(Z80_E);
  SCB_setByte(SCB_MULTISECTORCOUNT_B, v);
  if (v >= 1 && v <= 128)
    CPUsetALBH(0x00, 0x00);
  else
    CPUsetALBH(0xFF, 0x00);
}


// BDOS 45 (0x2D)
void BDOS::BDOS_setErrorMode()
{
  SCB_setByte(SCB_ERRORMODE_B, m_HAL->CPU_readRegByte(Z80_E));
}


// BDOS 46 (0x2E)
void BDOS::BDOS_getDiskFreeSpace()
{
  int drive = m_HAL->CPU_readRegByte(Z80_E);
  if (!checkDrive(drive, 46))
    return;

  setBrowserAtDrive(drive);

  int64_t total = 0, used = 0;
  m_fileBrowser.getFSInfo(m_fileBrowser.getCurrentDriveType(), 0, &total, &used);
  uint32_t free = (uint32_t) fabgl::tmin<int64_t>((total - used) / 128, 2147483647);  // 2147483648 = 2^24 * 128 - 1 = 2^31 - 1

  uint16_t DMAaddr = SCB_getWord(SCB_CURRENTDMAADDR_W);
  m_HAL->writeByte(DMAaddr + 0, free & 0xFF);
  m_HAL->writeByte(DMAaddr + 1, (free >> 8) & 0xFF);
  m_HAL->writeByte(DMAaddr + 2, (free >> 16) & 0xFF);

  CPUsetALBH(0x00, 0x00);
}


// BDOS 47 (0x2F)
void BDOS::BDOS_chainToProgram()
{
  int chainFlag = m_HAL->CPU_readRegByte(Z80_E);
  if (chainFlag == 0xFF)
    SCB_setBit(SCB_CCPFLAGS1_B, SCB_CCPFLAGS1_CHAINCHANGEDU); // initializes the default drive and user number to the current program values
  else
    SCB_clearBit(SCB_CCPFLAGS1_B, SCB_CCPFLAGS1_CHAINCHANGEDU);  // set default values (previous)

  SCB_setBit(SCB_CCPFLAGS1_B, SCB_CCOFLAGS1_CHAININING);

  m_HAL->CPU_stop();
}


// BDOS 48 (0x30)
void BDOS::BDOS_flushBuffers()
{
  // nothing to do, files are implicitely flushed
}


// BDOS 49 (0x31)
void BDOS::BDOS_getSetSystemControlBlock()
{
  uint8_t  q_offset  = m_HAL->readByte(m_HAL->CPU_readRegWord(Z80_DE) + 0);
  uint8_t  q_set     = m_HAL->readByte(m_HAL->CPU_readRegWord(Z80_DE) + 1);
  uint8_t  q_value_b = m_HAL->readByte(m_HAL->CPU_readRegWord(Z80_DE) + 2);
  uint16_t q_value_w = m_HAL->readWord(m_HAL->CPU_readRegWord(Z80_DE) + 2);

  // dynamic fields
  switch (q_offset) {
    // Console Column Position
    case SCB_CONSOLECOLPOS_B:
    {
      int row, col;
      m_HAL->getTerminalCursorPos(&col, &row);
      SCB_setByte(SCB_CONSOLECOLPOS_B, col - 1);
      break;
    }
  }

  if (q_set == 0) {
    // Read byte at offset into A, and word at offset into HL.
    CPUsetHLBA(SCB_getWord(q_offset), SCB_getWord(q_offset));
  } else if (q_set == 0x0FF) {
    // Write byte
    SCB_setByte(q_offset, q_value_b);
  } else if (q_set == 0x0FE) {
    // Write word
    SCB_setWord(q_offset, q_value_w);
  }
}


// BDOS 50 (0x32)
void BDOS::BDOS_directBIOSCall()
{
  uint16_t PBaddr = m_HAL->CPU_readRegWord(Z80_DE);
  m_HAL->CPU_writeRegByte(Z80_A, m_HAL->readByte(PBaddr + 1));
  m_HAL->CPU_writeRegByte(Z80_C, m_HAL->readByte(PBaddr + 2));
  m_HAL->CPU_writeRegByte(Z80_B, m_HAL->readByte(PBaddr + 3));
  m_HAL->CPU_writeRegByte(Z80_E, m_HAL->readByte(PBaddr + 4));
  m_HAL->CPU_writeRegByte(Z80_D, m_HAL->readByte(PBaddr + 5));
  m_HAL->CPU_writeRegByte(Z80_L, m_HAL->readByte(PBaddr + 6));
  m_HAL->CPU_writeRegByte(Z80_H, m_HAL->readByte(PBaddr + 7));
  m_BIOS->processBIOS(m_HAL->readByte(PBaddr + 0));
  m_HAL->writeByte(PBaddr + 1, m_HAL->CPU_readRegByte(Z80_A));
  m_HAL->writeByte(PBaddr + 2, m_HAL->CPU_readRegByte(Z80_C));
  m_HAL->writeByte(PBaddr + 3, m_HAL->CPU_readRegByte(Z80_B));
  m_HAL->writeByte(PBaddr + 4, m_HAL->CPU_readRegByte(Z80_E));
  m_HAL->writeByte(PBaddr + 5, m_HAL->CPU_readRegByte(Z80_D));
  m_HAL->writeByte(PBaddr + 6, m_HAL->CPU_readRegByte(Z80_L));
  m_HAL->writeByte(PBaddr + 7, m_HAL->CPU_readRegByte(Z80_H));
}


// BDOS 59 (0x3B)
// Always present, even when LOADER is not loaded
void BDOS::BDOS_loadOverlay()
{
  uint16_t FCBaddr = m_HAL->CPU_readRegWord(Z80_DE);

  removeRSX();

  if (FCBaddr == 0x0000) {
    // invalid FCB
    CPUsetALBH(0xFE, 0x00);
    return;
  }

  uint16_t loadAddr = m_HAL->readByte(FCBaddr + FCB_R0) | (m_HAL->readByte(FCBaddr + FCB_R1) << 8);

  int err;
  FILE * f = openFile(FCBaddr, false, false, 59, &err);

  if (err == 1) {
    // invalid drive
    return; // doError already called
  }

  if (err == 2) {
    // file not found
    CPUsetALBH(0xFF, 0x00);
    return;
  }

  if (!f) {
    doError(0xFF, 0x09, "CP/M Invalid FCB\r\nFunction 59\r\n");
    return;
  }

  // is PRL?
  if (hasExt(FCBaddr, "PRL")) {
    // TODO
    #if MSGDEBUG & DEBUG_ERRORS
    m_HAL->logf("Unsupported load PRL in BDS 59\r\n");
    #endif
    CPUsetALBH(0xFF, 0x00);
  } else {
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size > getTPASize()) {
      // no enough memory
      CPUsetALBH(0xFE, 0x00);
      closeFile(FCBaddr);
      return;
    }
    int c;
    while ((c = fgetc(f)) != EOF)
      m_HAL->writeByte(loadAddr++, c);
    CPUsetALBH(0x00, 0x00);
  }

  closeFile(FCBaddr);
}


// BDOS 60 (0x3C)
void BDOS::BDOS_callResidentSystemExtension()
{
  CPUsetALBH(0xFF, 0x00);
}


// BDOS 98 (0x62)
void BDOS::BDOS_freeBlocks()
{
  CPUsetALBH(0x00, 0x00);
}


// BDOS 99 (0x63)
void BDOS::BDOS_truncateFile()
{
  uint16_t FCBaddr = m_HAL->CPU_readRegWord(Z80_DE);

  int drive = getDriveFromFCB(FCBaddr);
  if (!checkDrive(drive, 99))
    return;

  char filename[13];
  getFilenameFromFCB(FCBaddr, filename);
  setBrowserAtDrive(drive);

  if (m_fileBrowser.exists(filename, false)) {

    // copy "absolute pos"+128 bytes
    // round to blocks (newlen should be already 128 bytes aligned)
    // +1 because r0,r1,r2 indicates "last" block, not the required size
    size_t newlen = 128 * ((getAbsolutePosFCB(FCBaddr) + 127) / 128 + 1);

    if (m_fileBrowser.truncate(filename, newlen))
      CPUsetALBH(0x00, 0x00);
    else
      doError(0xFF, 0x01, "CP/M I/O Error\r\nFunction 99\r\n");

  } else {
    doError(0xFF, 0x00, "CP/M Error, File Not Found\r\nFunction 99\r\n");
  }
}


// Directory label is implemented using a (hidden) file named DIRLABEL_FILENAME
// located at the root of mount point. That file contains the FCB (32 bytes) specified by func 100.
// drive: 0 = A, 1 = B...
void BDOS::writeDirectoryLabel(int drive, uint16_t FCBaddr, int errfunc)
{
  // we need update datetime in SCB
  m_BIOS->updateSCBFromHALDateTime();

  uint8_t wFCB[32] = {0};

  // read creation date if any
  if (readDirectoryLabel(drive, 0, wFCB) == 0) {
    // there isn't an existing label
    m_HAL->copyMem(wFCB + FCB_TS1, SCB_ADDR + SCB_DATEDAYS_W, 4); // copy creation date from SCB
  }

  // copy label name and flags from FCB to wFCB
  m_HAL->copyMem(wFCB + FCB_F1, FCBaddr + FCB_F1, 12);

  // adjust some FCB fields
  wFCB[FCB_DR]  = 0x20;  // directory label flag
  wFCB[FCB_EX] |= 1;     // directory label exists

  m_cachedDirLabelFlags[drive] = wFCB[FCB_EX];

  // set update date
  m_HAL->copyMem(wFCB + FCB_TS2, SCB_ADDR + SCB_DATEDAYS_W, 4); // copy update date from SCB

  char fullpath[strlen(m_HAL->getDriveMountPath(drive)) + 1 + strlen(DIRLABEL_FILENAME) + 1];
  strcpy(fullpath, m_HAL->getDriveMountPath(drive));
  strcat(fullpath, "/");
  strcat(fullpath, DIRLABEL_FILENAME);
  auto f = fopen(fullpath, "wb");
  fwrite(wFCB, 1, 32, f);
  fclose(f);
}


// FCB buffer musts have space for at least 32 bytes
// drive: 0 = A, 1 = B...
// ret: directory label flags or 0
// notes:
//   - if FCBaddr is 0, it is not used.
//   - if FCB is nullptr, it is not used.
uint8_t BDOS::readDirectoryLabel(int drive, uint16_t FCBaddr, uint8_t * FCB)
{
  m_cachedDirLabelFlags[drive] = 0;
  char fullpath[strlen(m_HAL->getDriveMountPath(drive)) + 1 + strlen(DIRLABEL_FILENAME) + 1];
  strcpy(fullpath, m_HAL->getDriveMountPath(drive));
  strcat(fullpath, "/");
  strcat(fullpath, DIRLABEL_FILENAME);
  auto f = fopen(fullpath, "rb");
  if (f) {
    for (int i = 0; i < 32; ++i) {
      int c = fgetc(f);
      if (FCBaddr)
        m_HAL->writeByte(FCBaddr + i, c);
      if (FCB)
        FCB[i] = c;
      if (i == FCB_EX)
        m_cachedDirLabelFlags[drive] = c;
    }
    fclose(f);
  }
  return m_cachedDirLabelFlags[drive];
}


// use readDirectoryLabel() if not cached, otherwise returns cached value
uint8_t BDOS::getDirectoryLabelFlags(int drive)
{
  if (m_cachedDirLabelFlags[drive] == 0xFF) {
    // no cached flags, read directory label
    return readDirectoryLabel(drive, 0, nullptr);
  }
  return m_cachedDirLabelFlags[drive];
}


// BDOS 100 (0x64)
void BDOS::BDOS_setDirectoryLabel()
{
  uint16_t FCBaddr = m_HAL->CPU_readRegWord(Z80_DE);

  int drive = getDriveFromFCB(FCBaddr) & 0x0F;
  if (!checkDrive(drive, 100))
    return;

  writeDirectoryLabel(drive, FCBaddr, 100);

  CPUsetALBH(0x00, 0x00);
}


// BDOS 101 (0x65)
void BDOS::BDOS_returnDirLabelData()
{
  uint16_t FCBaddr = BDOS_BUFADDR;
  int drive = m_HAL->CPU_readRegByte(Z80_E);
  uint8_t flags = readDirectoryLabel(drive, FCBaddr, nullptr);
  CPUsetALBH(flags, 0x00);
}


// BDOS 102 (0x66)
void BDOS::BDOS_readFileDateStamps()
{
  uint16_t FCBaddr = m_HAL->CPU_readRegWord(Z80_DE);
  int drive = getDriveFromFCB(FCBaddr);
  if (!checkDrive(drive, 102))
    return;

  setBrowserAtDrive(drive);

  char filename[13];
  getFilenameFromFCB(FCBaddr, filename);

  if (!m_fileBrowser.exists(filename, false)) {
    CPUsetALBH(0xFF, 0x00);
    return;
  }

  auto dirLabelFlags = getDirectoryLabelFlags(drive);

  m_HAL->writeByte(FCBaddr + 12, 0);  // TODO: no password
  m_HAL->fillMem(FCBaddr + 24, 0, 4);
  m_HAL->fillMem(FCBaddr + 28, 0, 4);

  if (dirLabelFlags & DIRLABELFLAGS_EXISTS) {
    int year, month, day, hour, minutes, seconds;
    DateTime dt;
    if (dirLabelFlags & DIRLABELFLAGS_CREATE) {
      m_fileBrowser.fileCreationDate(filename, &year, &month, &day, &hour, &minutes, &seconds);
      dt.set(year, month, day, hour, minutes, seconds);
      m_HAL->copyMem(FCBaddr + 24, &dt, 4);
    } else if (dirLabelFlags & DIRLABELFLAGS_ACCESS) {
      m_fileBrowser.fileAccessDate(filename, &year, &month, &day, &hour, &minutes, &seconds);
      dt.set(year, month, day, hour, minutes, seconds);
      m_HAL->copyMem(FCBaddr + 24, &dt, 4);
    }
    if (dirLabelFlags & DIRLABELFLAGS_UPDATE) {
      m_fileBrowser.fileUpdateDate(filename, &year, &month, &day, &hour, &minutes, &seconds);
      dt.set(year, month, day, hour, minutes, seconds);
      m_HAL->copyMem(FCBaddr + 28, &dt, 4);
    }
  }

  CPUsetALBH(0x00, 0x00);
}


// BDOS 104 (0x68)
void BDOS::BDOS_setDateTime()
{
  // put into SCB the date from [DE]
  uint16_t DATaddr = m_HAL->CPU_readRegWord(Z80_DE);
  SCB_setWord(SCB_DATEDAYS_W, m_HAL->readWord(DATaddr + 0));
  SCB_setByte(SCB_HOUR_B, m_HAL->readByte(DATaddr + 2));
  SCB_setByte(SCB_MINUTES_B, m_HAL->readByte(DATaddr + 3));
  SCB_setByte(SCB_SECONDS_B, 0);
  // ask bios to update datetime from SCB
  m_BIOS->updateHALDateTimeFromSCB();
}


// BDOS 105 (0x69)
void BDOS::BDOS_getDateTime()
{
  // ask bios to update SCB
  m_BIOS->updateSCBFromHALDateTime();
  // put into [DE] and A the content of datetime SCB fields
  uint16_t DATaddr = m_HAL->CPU_readRegWord(Z80_DE);
  m_HAL->writeWord(DATaddr + 0, SCB_getWord(SCB_DATEDAYS_W));
  m_HAL->writeByte(DATaddr + 2, SCB_getByte(SCB_HOUR_B));
  m_HAL->writeByte(DATaddr + 3, SCB_getByte(SCB_MINUTES_B));
  CPUsetALBH(SCB_getByte(SCB_SECONDS_B), 0);
}


// 108 (0x6C)
void BDOS::BDOS_getSetProgramReturnCode()
{
  uint16_t code = m_HAL->CPU_readRegWord(Z80_DE);
  if (code == 0xFFFF) {
    // get
    m_HAL->CPU_writeRegWord(Z80_HL, SCB_getWord(SCB_PROGRAMRETCODE_W));
  } else {
    // set
    SCB_setWord(SCB_PROGRAMRETCODE_W, code);
  }
}


// BDOS 109 (0x6D)
void BDOS::BDOS_getSetConsoleMode()
{
  uint16_t newval = m_HAL->CPU_readRegWord(Z80_DE);
  if (newval == 0xFFFF) {
    // get
    m_HAL->CPU_writeRegWord(Z80_HL, SCB_getWord(SCB_CONSOLEMODE_W));
  } else {
    // set
    SCB_setWord(SCB_CONSOLEMODE_W, newval);
  }
}


// BDOS 110 (0x6E)
void BDOS::BDOS_getSetOutputDelimiter()
{
  if (m_HAL->CPU_readRegWord(Z80_DE) == 0xFFFF) {
    // get
    uint8_t delim = SCB_getByte(SCB_OUTPUTDELIMETER_B);
    CPUsetALBH(delim, 0);
  } else {
    // set
    SCB_setByte(SCB_OUTPUTDELIMETER_B, m_HAL->CPU_readRegByte(Z80_E));
  }
}


// 111 (0x6F)
void BDOS::BDOS_printBlock()
{
  uint16_t CCB = m_HAL->CPU_readRegWord(Z80_DE);  // CCB = Character Control Block
  uint16_t str = m_HAL->readWord(CCB);
  auto len = m_HAL->readWord(CCB + 2);
  while (len--)
    consoleOutChar(m_HAL->readByte(str++));
}


// 112 (0x70)
void BDOS::BDOS_listBlock()
{
  uint16_t CCB = m_HAL->CPU_readRegWord(Z80_DE);  // CCB = Character Control Block
  uint16_t str = m_HAL->readWord(CCB);
  auto len = m_HAL->readWord(CCB + 2);
  while (len--)
    lstOut(m_HAL->readByte(str++));
}


// 152 (0x98)
void BDOS::BDOS_parseFilename()
{
  uint16_t PFCB    = m_HAL->CPU_readRegWord(Z80_DE);  // PFCB = Parse Filename Control Block
  uint16_t strAddr = m_HAL->readWord(PFCB);
  uint16_t FCBaddr = m_HAL->readWord(PFCB + 2);

  m_HAL->fillMem(FCBaddr, 0, 36);
  m_HAL->fillMem(FCBaddr + 16, ' ', 8); // blanks on password field

  auto next = filenameToFCB(strAddr, FCBaddr, nullptr, nullptr);

  // skip trailing blanks
  while (m_HAL->readByte(next) != 0 && isspace(m_HAL->readByte(next)))
    ++next;

  // return value (HL) is 0 to indicate "end of input", otherwise it is the address of next delimiter
  uint16_t ret = 0x0000;
  char nextChar = m_HAL->readByte(next);
  if (nextChar != 0 && nextChar != ASCII_CR)
    ret = strAddr + (next - strAddr);

  m_HAL->CPU_writeRegWord(Z80_HL, ret);
}


// only searchingName can have wildcards
bool BDOS::fileMatchWithWildCards(char const * searchingName, char const * testingName)
{
  auto searchingNameLen = strlen(searchingName);
  for (int i = 0; i < searchingNameLen; ++i) {
    if (searchingName[i] == '*') {
      // wildcard '*', bypass all chars until "." or end of testing name
      for (++testingName; *testingName && *testingName != '.'; ++testingName)
        ;
    } else if (searchingName[i] == '?' || toupper(searchingName[i]) == toupper(*testingName)) {
      // matching char
      ++testingName;
    } else
      return false; // fail
  }
  return true;
}


void BDOS::copyFile(FILE * src, FILE * dst)
{
  void * buffer = malloc(COPYFILE_BUFFERSIZE);
  while (true) {
    auto r = fread(buffer, 1, COPYFILE_BUFFERSIZE, src);
    if (r == 0)
      break;
    fwrite(buffer, 1, r, dst);
  }
  free(buffer);
}


// return true if filename has specified extension (without ".")
bool BDOS::hasExt(char const * filename, char const * ext)
{
  auto p = strchr(filename, '.');
  if (!p)
    return strlen(ext) == 0;
  ++p;
  return strcasecmp(p, ext) == 0;
}


bool BDOS::hasExt(uint16_t FCBaddr, char const * ext)
{
  return m_HAL->compareMem(FCBaddr + FCB_T1, ext, 3) == 0;
}


void BDOS::strToUpper(char * str)
{
  for (; *str; ++str)
    *str = toupper(*str);
}



// Creates an absolute path (including mounting path) from a relative or disk-relative path.
//
// path can be:
//    nullptr or empty => absolute path point to current directory of current drive
//    \                => absolute path point to root directory of current drive
//    drive:           => absolute path point to current directory of specified drive
//    drive:\          => absolute path point to root directory of specified drive
//    something        => absolute path point to current directory of current drive + '/' + something
//    \something       => absolute path point to root directory of current drive + '/' + something
//    drive:something  => absolute path point to current directory of specified drive + '/' + something
//    drive:\something => absolute path point to root directory of specifie drive + '/' + something
// notes:
//    - "something" can be a single name or multiple names separated by '/' or '\'
//    - separators can be '/' or '\'
//    - result buffer must be deallocated using free()
// returns:
//   nullptr => invalid drive
char * BDOS::createAbsolutePath(uint16_t pathAddr, bool insertMountPath, int * drive)
{
  char * path = nullptr;

  auto pathLen = pathAddr ? m_HAL->strLen(pathAddr) : 0;
  char pathStorage[pathLen + 1];
  if (pathLen > 0) {
    m_HAL->copyStr(pathStorage, pathAddr);
    path = pathStorage;
    strToUpper(path);
  }

  // bypass spaces
  while (path && isspace(*path))
    ++path;

  // source drive (if any, otherwise strToDrive() uses default drive)
  int srcDrive;
  if (strToDrive(path, &srcDrive))
    path += 2;
  if (!checkDrive(srcDrive)) {
    // fail, invalid drive (message already shown by checkDrive())
    return nullptr;
  }

  if (drive)
    *drive = srcDrive;

  // source is relative or absolute path?
  bool srcIsAbsolute = path && strlen(path) > 0 && (path[0] == '\\' || path[0] == '/');
  if (srcIsAbsolute)
    ++path;  // bypass separator

  // build source absolute path
  char * srcAbsPath;
  if (srcIsAbsolute) {
    srcAbsPath = strdup(path);
  } else {
    // concatenate current directory and specified path
    auto specPathLen = path ? strlen(path) : 0;
    auto curdir = getCurrentDir(srcDrive);
    srcAbsPath = (char*) malloc(strlen(curdir) + 1 + specPathLen + 1);
    strcpy(srcAbsPath, curdir);
    if (path && specPathLen > 0) {
      if (curdir && strlen(curdir) > 0)
        strcat(srcAbsPath, "/");
      strcat(srcAbsPath, path);
    }
  }

  fabgl::replacePathSep(srcAbsPath, '/');

  // handle ".." (previous directory)
  processPrevDirMarks(srcAbsPath);

  if (!insertMountPath) {
    // do not insert mounting path
    return srcAbsPath;
  }

  // build source actual path (mount path + absolute path)
  auto srcMountPath = m_HAL->getDriveMountPath(srcDrive);
  char * srcActualPath = (char*) malloc(strlen(srcMountPath) + 1 + strlen(srcAbsPath) + 1);
  strcpy(srcActualPath, srcMountPath);
  if (strlen(srcAbsPath) > 0) {
    strcat(srcActualPath, "/");
    strcat(srcActualPath, srcAbsPath);
  }

  free(srcAbsPath);

  return srcActualPath;
}


// process ".." marks, removing previous directory from the path
//   "AAA/../BBB"     => "BBB"
//   "AAA/.."         => ""
//   "AAA/BBB/.."     => "AAA"
//   "AAA/BBB/../CCC" => "AAA/CCC"
//   "AAA/BBB/../.."  => ""
//   "AAA/BBB/../CCC/../DDD" => "AAA/DDD"
//   "AAA/BBB/CCC/../.." => "AAA"
//   "AAA/BBB/CCC/../../DDD" => "AAA/DDD"
//    012345678901234567890
void BDOS::processPrevDirMarks(char * path)
{
  int p = 0;
  for (; path[p]; ++p) {
    if (path[p] == '/' && path[p + 1] == '.' && path[p + 2] == '.') {
      // found "/.." pattern
      // search beginning of previous dir
      int b = p - 1;
      while (b >= 0 && path[b] != '/')
        --b;
      ++b;
      // removes from "b" to "p"
      p += 3;
      if (path[p] == '/')
        ++p;
      memmove(path + b, path + p, strlen(path + p) + 1);
      p = b - 2;
      if (p < 0)
        p = -1;
    } else if (path[p] == '.' && path[p + 1] == '.') {
      // found ".." pattern
      // just remove (eventually even additional '/')
      int b = p;
      while (path[b] == '.' || path[b] == '/')
        ++b;
      memmove(path + p, path + b, strlen(path + b) + 1);
      p = -1;
    }
  }
  if (path[p - 1] == '/')
    path[p - 1] = 0;
}


// 212 (0xD4) : Copy file (specific of this BDOS implementation)
//   HL : zero terminated string which specify "path + filename" of source
//   DE : zero terminated string which specify "path + [filename]" of destination
//   B  : mode flags:
//         bit 0 : 0 = fail if destination exists  1 = overwrite if destination exists
//         bit 1 : 0 = don't show copied files     1 = show copied files
// ret:
//   A = 0  -> success
//   A = 1  -> fail, source doesn't exist
//   A = 2  -> fail, destination path doesn't exist
//   A = 3  -> fail, destination already exist
//   A = 4  -> fail, source and dest are the same file
// notes:
//   * supports '?' and '*' wildcards
void BDOS::BDOS_copyFile()
{
  auto srcFullPathAddr = m_HAL->CPU_readRegWord(Z80_HL);
  auto dstPathAddr     = m_HAL->CPU_readRegWord(Z80_DE);
  bool overwrite       = (m_HAL->CPU_readRegByte(Z80_B) & 1);
  bool display         = (m_HAL->CPU_readRegByte(Z80_B) & 2);

  auto srcActualPath = createAbsolutePath(srcFullPathAddr);
  if (!srcActualPath) {
    // fail, source doesn't exist
    free(srcActualPath);
    CPUsetALBH(1, 0);
    return;
  }

  // break source path and filename path
  char * srcFilename = strrchr(srcActualPath, '/');
  *srcFilename = 0;
  ++srcFilename;

  auto dstActualPath = createAbsolutePath(dstPathAddr);
  if (!dstActualPath) {
    // fail, dest desn't exist
    free(srcActualPath);
    free(dstActualPath);
    CPUsetALBH(2, 0);
    return;
  }

  // is dest path a directory?
  // @TODO: what about SPIFFS?
  struct stat statbuf;
  bool destIsDir = stat(dstActualPath, &statbuf) == 0 && S_ISDIR(statbuf.st_mode);

  #if MSGDEBUG & DEBUG_BDOS
  m_HAL->logf("overwrite = %s \r\n", overwrite ? "YES" : "NO");
  m_HAL->logf("srcActualPath = \"%s\" \r\n", srcActualPath);
  m_HAL->logf("dstActualPath = \"%s\" \r\n", dstActualPath);
  #endif

  int copied = 0;

  if (m_fileBrowser.setDirectory(srcActualPath)) {

    for (int i = 0; i < m_fileBrowser.count(); ++i) {

      auto diritem = m_fileBrowser.get(i);
      if (!diritem->isDir && fileMatchWithWildCards(srcFilename, diritem->name)) {
        // name match

        // compose dest path
        char * dstActualFullPath;
        if (destIsDir) {
          // destination is a directory, append source filename
          dstActualFullPath = (char *) malloc(strlen(dstActualPath) + 1 + strlen(diritem->name) + 1);
          strcpy(dstActualFullPath, dstActualPath);
          strcat(dstActualFullPath, "/");
          strcat(dstActualFullPath, diritem->name);
        } else {
          // destination is a file
          dstActualFullPath = strdup(dstActualPath);
        }

        // compose source path
        auto srcActualFullPath = (char *) malloc(strlen(srcActualPath) + 1 + strlen(diritem->name) + 1);
        strcpy(srcActualFullPath, srcActualPath);
        strcat(srcActualFullPath, "/");
        strcat(srcActualFullPath, diritem->name);

        // source and dest are the same file?
        if (strcasecmp(dstActualFullPath, srcActualFullPath) == 0) {
          // yes...
          free(dstActualFullPath);
          free(srcActualFullPath);
          copied = -3;
          #if MSGDEBUG & DEBUG_BDOS
          m_HAL->logf("  source and dest are the same file\r\n");
          #endif
          break;
        }

        if (!overwrite) {
          // fail if dest already exists
          FILE * f = fopen(dstActualFullPath, "rb");
          if (f) {
            fclose(f);
            free(dstActualFullPath);
            free(srcActualFullPath);
            copied = -1;
            #if MSGDEBUG & DEBUG_BDOS
            m_HAL->logf("  file already exists\r\n");
            #endif
            break;
          }
        }

        // try to open dest file
        FILE * dstFile = fopen(dstActualFullPath, "wb");

        if (!dstFile) {
          // fail, dest path doesn't exist
          free(dstActualFullPath);
          free(srcActualFullPath);
          copied = -2;
          #if MSGDEBUG & DEBUG_BDOS
          m_HAL->logf("  dest path doesn't exist\r\n");
          #endif
          break;
        }

        #if MSGDEBUG & DEBUG_BDOS
        m_HAL->logf("copying \"%s\" to \"%s\" \r\n", srcActualFullPath, dstActualFullPath);
        #endif

        // open source file (always exists)
        FILE * srcFile = fopen(srcActualFullPath, "rb");

        // copy files
        copyFile(srcFile, dstFile);

        if (display) {
          consoleOutStr(diritem->name);
          consoleOutStr("\r\n");
        }

        // cleanup
        fclose(srcFile);
        fclose(dstFile);
        free(srcActualFullPath);
        free(dstActualFullPath);

        ++copied;
      }

    }

  }

  // cleanup buffers
  free(srcActualPath);
  free(dstActualPath);

  if (copied == 0) {
    // no source file
    m_HAL->CPU_writeRegByte(Z80_A, 1);
  } else if (copied > 0) {
    // at least one copied
    m_HAL->CPU_writeRegByte(Z80_A, 0);
  } else if (copied == -1) {
    // overwrite = false and at least one file already exists
    m_HAL->CPU_writeRegByte(Z80_A, 3);
  } else if (copied == -2) {
    // destination path doesn't exist
    m_HAL->CPU_writeRegByte(Z80_A, 2);
  } else if (copied == -3) {
    // source and dest are the same
    m_HAL->CPU_writeRegByte(Z80_A, 4);
  }

}


// 213 (0xD5) : Change current directory (specific of this BDOS implementation)
//   DE : zero terminated string which specify "path" of new current directory
// ret:
//   A = 0  -> success
//   A = 1  -> fail, dir doesn't exist
// path can be:
//    empty (first byte = 0) => set root as current directory of current disk
//    \                      => set root as current directory of current disk
//    disk:\                 => set root as current directory of specified disk
//    something              => set current dir + something as current directory of current disk
//    ...and so on...
void BDOS::BDOS_changeCurrentDirectory()
{
  uint16_t pathAddr = m_HAL->CPU_readRegWord(Z80_DE);

  int drive;
  auto actualPath = createAbsolutePath(pathAddr, true, &drive);
  if (!actualPath) {
    // fail, dir doesn't exist
    CPUsetALBH(1, 0);
    return;
  }

  if (m_fileBrowser.setDirectory(actualPath)) {
    // success, remove mount path
    auto mountPathLen = strlen(m_HAL->getDriveMountPath(drive));
    if (mountPathLen == strlen(actualPath)) {
      // root dir optimization
      m_currentDir[drive][0] = 0;
      free(actualPath);
    } else {
      memmove(actualPath, actualPath + mountPathLen + 1, strlen(actualPath) - mountPathLen + 1);
      free(m_currentDir[drive]);
      m_currentDir[drive] = actualPath;
    }
    CPUsetALBH(0, 0);
  } else {
    // fail, dir doesn't exist
    free(actualPath);
    CPUsetALBH(1, 0);
  }
}


// fills R0, R1 and R2 of FCB with pos
// pos is in bytes (up aligned to 128 bytes)
void BDOS::setAbsolutePosFCB(uint16_t FCBaddr, size_t pos)
{
  auto blk = (pos + 127) / 128; // number of blocks
  m_HAL->writeByte(FCBaddr + FCB_R0, blk & 0xff);
  m_HAL->writeByte(FCBaddr + FCB_R1, (blk >> 8) & 0xff);
  m_HAL->writeByte(FCBaddr + FCB_R2, (blk >> 16) & 0xff);
}


// return pos (in bytes) from R0, R1 and R2 of FCB
size_t BDOS::getAbsolutePosFCB(uint16_t FCBaddr)
{
  return 128 * (m_HAL->readByte(FCBaddr + FCB_R0) | (m_HAL->readByte(FCBaddr + FCB_R1) << 8) | (m_HAL->readByte(FCBaddr + FCB_R2) << 16));
}


// fills EX, S2 and CR of FPC with pos
// pos is in bytes (down aligned to 128 bytes)
void BDOS::setPosFCB(uint16_t FCBaddr, size_t pos)
{
  m_HAL->writeByte(FCBaddr + FCB_EX, (pos % 524288) / 16384); // extent, low byte
  m_HAL->writeByte(FCBaddr + FCB_S2, (pos / 524288));         // extent, high byte
  m_HAL->writeByte(FCBaddr + FCB_CR, (pos % 16384) / 128);    // cr (current record)
}


// return pos (in bytes) from EX, S2 and CR
size_t BDOS::getPosFCB(uint16_t FCBaddr)
{
  return m_HAL->readByte(FCBaddr + FCB_EX) * 16384 + m_HAL->readByte(FCBaddr + FCB_S2) * 524288 + m_HAL->readByte(FCBaddr + FCB_CR) * 128;
}


// convert 11 bytes filename (8+3) of FCB to normal string (filename + '.' + ext)
void BDOS::getFilenameFromFCB(uint16_t FCBaddr, char * filename)
{
  for (int i = 0; i < 11; ++i) {
    char c = m_HAL->readByte(FCBaddr + 1 + i) & 0x7f;
    if (i < 8) {
      if (c == ' ') {
        i = 7;
        continue;
      }
    } else if (i == 8) {
      if (c != ' ') {
        *filename++ = '.';
      } else
        break;
    } else {  // i > 8
      if (c == ' ')
        break;
    }
    *filename++ = c;
  }
  *filename = 0;
}


// general form is:
//   {d:}filename{.typ}{;password}
// extract drive ("A:", "B:"...) and filename and fill the FCB
// expand jolly '*' if present
// eliminates leading spaces
// FCB must be initialized with blanks and zeros (where necessary) before call filenameToFCB() because only specified fields are actually filled
// passwordAddr:
//   nullptr => copy password into FCB
//   pointer => put address of password into *passwordAddr
// passwordLen:
//   set only when passwordAddr is not nullptr
// ret. next char after the filename (zero or a delimiter)
uint16_t BDOS::filenameToFCB(uint16_t filename, uint16_t FCB, uint16_t * passwordAddr, uint8_t * passwordLen)
{
  // bypass leading spaces
  while (m_HAL->readByte(filename) != 0 && isspace(m_HAL->readByte(filename)))
    ++filename;

  // drive specified?
  int drive;
  if (strToDrive(filename, &drive)) {
    m_HAL->writeByte(FCB + FCB_DR, drive + 1); // 1 = A
    filename += 2;
  } else
    m_HAL->writeByte(FCB + FCB_DR, 0);         // 0 = default drive

  // calc filename size
  int filenameSize = 0;
  while (m_HAL->readByte(filename + filenameSize) != 0 && !isspace(m_HAL->readByte(filename + filenameSize)))
    ++filenameSize;

  char filenameStr[filenameSize + 1];
  m_HAL->copyMem(filenameStr, filename, filenameSize);
  filenameStr[filenameSize] = 0;

  char filename11[11];
  auto sep = expandFilename(filenameStr, filename11, false);
  m_HAL->copyMem(FCB + 1, filename11, 11);

  auto sepAddr = filename + (sep - filenameStr);

  if (sep[0] == ';' && isalnum(sep[1])) {
    // extract password
    ++sep; ++sepAddr;
    if (passwordAddr) {
      *passwordAddr = sepAddr;
      for (*passwordLen = 0; *passwordLen < 8 && !isFileDelimiter(*sep); ++*passwordLen) {
        ++sep; ++sepAddr;
      }
    } else {
      for (int i = 0; i < 8; ++i) {
        m_HAL->writeByte(FCB + 16 + i, isFileDelimiter(*sep) ? ' ' : toupper(*sep));
        ++sep; ++sepAddr;
      }
    }
  } else if (passwordAddr) {
    *passwordAddr = 0;
    *passwordLen  = 0;
  }

  return sepAddr;
}


// convert filename to 11 bytes (8+3)
// expands '*' with '?' on filename and file type
// ret. next char after the filename (zero or a delimiter)
char const * BDOS::expandFilename(char const * filename, char * expandedFilename, bool isDir)
{
  memset(expandedFilename, ' ', 11);
  if (strcmp(filename, "..") == 0) {
    expandedFilename[0] = '.';
    expandedFilename[1] = '.';
    filename += 2;
  } else if (strcmp(filename, ".") == 0) {
    expandedFilename[0] = '.';
    ++filename;
  } else {
    for (int i = 0; i < 11;) {
      if (i <= 8 && *filename == '.') {
        i = 8;
      } else if (*filename == '*') {
        // expand jolly '*'..
        if (i < 8) {
          // ..on filename
          for (; i < 8; ++i)
            expandedFilename[i] = '?';
        } else {
          // ..on file type
          for (; i < 11; ++i)
            expandedFilename[i] = '?';
        }
      } else if (isFileDelimiter(*filename) || *filename < 32) {
        break;
      } else {
        expandedFilename[i] = toupper(*filename);
        ++i;
      }
      ++filename;
    }
  }
  if (isDir)
    memcpy(expandedFilename + 8, DIRECTORY_EXT, 3);
  return filename;
}


int BDOS::getDriveFromFCB(uint16_t FCBaddr)
{
  int rawdrive = m_HAL->readByte(FCBaddr + FCB_DR) & 0x1F;  // 0x1F allows 0..16 range
  if (rawdrive == 0)
    return getCurrentDrive();
  else
    return rawdrive - 1;
}


bool BDOS::isFileDelimiter(char c)
{
  return (c == 0x00 ||  // null
          c == 0x20 ||  // space
          c == 0x0D ||  // return
          c == 0x09 ||  // tab
          c == 0x3A ||  // :
          c == 0x2E ||  // .
          c == 0x3B ||  // ;
          c == 0x3D ||  // =
          c == 0x2C ||  // ,
          c == 0x5B ||  // [
          c == 0x5D ||  // ]
          c == 0x3C ||  // <
          c == 0x3E ||  // >
          c == 0x7C);   // |
}


// parse parameters in PAGE0_DMA (0x0080) and fills FCB1 and FCB2
void BDOS::parseParams()
{
  m_HAL->fillMem(PAGE0_FCB1, 0, 36);           // reset default FCB1 (this will reset up to 0x80, not included)
  m_HAL->fillMem(PAGE0_FCB1 + FCB_F1, 32, 11); // blanks on filename of FCB1
  m_HAL->fillMem(PAGE0_FCB2 + FCB_F1, 32, 11); // blanks on filename of FCB2
  int len = m_HAL->readByte(PAGE0_DMA);
  if (len > 1) {
    uint16_t tailAddr = PAGE0_DMA + 2;  // +2 to bypass params size and first space
    // file 1
    uint16_t passAddr;
    uint8_t passLen;
    auto next = filenameToFCB(tailAddr, PAGE0_FCB1, &passAddr, &passLen);
    if (passAddr && passLen > 0) {
      m_HAL->writeWord(PAGE0_FCB1PASSADDR_W, passAddr);
      m_HAL->writeByte(PAGE0_FCB1PASSLEN, passLen);
    }
    // file 2
    if (next) {
      filenameToFCB(next, PAGE0_FCB2, &passAddr, &passLen);
      if (passAddr && passLen > 0) {
        m_HAL->writeWord(PAGE0_FCB2PASSADDR_W, passAddr);
        m_HAL->writeByte(PAGE0_FCB2PASSLEN, passLen);
      }
    }
  }
}


void BDOS::setBrowserAtDrive(int drive)
{
  char fullpath[strlen(m_HAL->getDriveMountPath(drive)) + 1 + strlen(m_currentDir[drive]) + 1];
  strcpy(fullpath, m_HAL->getDriveMountPath(drive));
  if (m_currentDir[drive] && strlen(m_currentDir[drive]) > 0) {
    strcat(fullpath, "/");
    strcat(fullpath, m_currentDir[drive]);
  }
  m_fileBrowser.setDirectory(fullpath);
}


// state->FCB and state->DMA must be initialized before searchFirst() call
void BDOS::searchFirst(FileSearchState * state)
{
  state->index         = -1;
  state->extIndex      = -1;
  state->size          = 0;
  state->returnSFCB    = false;

  state->getAllFiles   = (m_HAL->readByte(state->FCB + FCB_DR) == '?');
  state->getAllExtents = (m_HAL->readByte(state->FCB + FCB_EX) == '?');

  int drive = state->getAllFiles ? getCurrentDrive() : getDriveFromFCB(state->FCB);
  if (m_HAL->getDriveMountPath(drive)) {
    state->dirLabelFlags = getDirectoryLabelFlags(drive);
    state->hasDirLabel   = (state->dirLabelFlags != 0xFF && (state->dirLabelFlags & DIRLABELFLAGS_EXISTS));
    setBrowserAtDrive(drive);
    searchNext(state);
  } else {
    // invalid drive
    state->errCode = 2;
  }
}


void BDOS::searchNextFillDMA_FCB(FileSearchState * state, bool isFirst, char const * filename11)
{
  // write file info into DMA

  uint16_t DMA = state->DMA;

  if (isFirst) {
    m_HAL->fillMem(DMA, 0, 32);
    m_HAL->writeByte(DMA + FCB_USR, 0); // user number 0 TODO

    m_HAL->copyMem(DMA + FCB_F1, filename11, 11);

    // reset extent number
    m_HAL->writeByte(DMA + FCB_EX, 0);  // extent lo [EX]
    m_HAL->writeByte(DMA + FCB_S2, 0);  // extent hi [S2]
  } else {
    // increment extent number
    m_HAL->writeByte(DMA + FCB_EX, m_HAL->readByte(DMA + FCB_EX) + 1);  // extent lo [EX]
    if (m_HAL->readByte(DMA + FCB_EX) == 32) {
      m_HAL->writeByte(DMA + FCB_EX, 0);
      m_HAL->writeByte(DMA + FCB_S2, m_HAL->readByte(DMA + FCB_S2) + 1);  // extent hi [S2]
    }
  }

  // RC
  m_HAL->writeByte(DMA + FCB_RC, fabgl::tmin((state->size + 127) / 128, 128));

  int extentSize = m_HAL->readByte(DMA + FCB_RC) * 128;

  // S1 (Last Record Byte Count)
  if (extentSize < 16383 && state->size != extentSize) {
    // not using the full extent space, so this is the last extent
    m_HAL->writeByte(DMA + FCB_S1, state->size % 128);
  } else // extentSize >= 16383
    m_HAL->writeByte(DMA + FCB_S1, 0);

  // D0-D15
  // because each allocation block is 2K and we have a lot of blocks
  // then each block pointer needs 16 bits (two bytes)
  m_HAL->fillMem(DMA + FCB_AL, 0, 16);
  int requiredBlocks = (extentSize + 2047) / 2048;
  for (int i = 0; i < requiredBlocks; ++i) {
    // set dummy 0xffff block pointer
    m_HAL->writeByte(DMA + FCB_AL + i * 2 + 0, 0xff);
    m_HAL->writeByte(DMA + FCB_AL + i * 2 + 1, 0xff);
  }

  // other extents of this reply appair as deleted
  m_HAL->fillMem(DMA + 32, 0xE5, 96);

  state->size -= extentSize;
}


void BDOS::searchNextFillDMA_SFCB(FileSearchState * state)
{
  uint16_t DMA = state->DMA;
  m_HAL->fillMem(DMA + 96, 0, 32);

  m_HAL->writeByte(DMA + 96 + 0, 0x21);
  m_HAL->copyMem(DMA + 96 + 1, &state->createOrAccessDate, 4);
  m_HAL->copyMem(DMA + 96 + 5, &state->updateDate, 4);
  m_HAL->writeByte(DMA + 96 +  9, 0);
  m_HAL->writeByte(DMA + 96 + 10, 0);
}


bool BDOS::searchNextFillDMA_DirLabel(FileSearchState * state)
{
  uint16_t DMA = state->DMA;
  if (readDirectoryLabel(getCurrentDrive(), DMA, nullptr) & 0x01) {
    // directory label exists
    m_HAL->fillMem(DMA + 32, 0xE5, 96);
    searchNextFillDMA_SFCB(state);  // SFCB of directory label!
    return true;
  }
  return false;
}


void BDOS::searchNext(FileSearchState * state)
{
  while (true) {

    if (state->getAllFiles && state->returnSFCB) {

      // just return SFCB of the matched file

      searchNextFillDMA_SFCB(state);
      state->returnSFCB = false;
      state->errCode = 0;
      state->retCode = 3; // get fourth FCB
      return;

    } else if ((state->getAllFiles || state->getAllExtents) && state->size > 0) {

      // still returning previous file, more extents need to be returned

      searchNextFillDMA_FCB(state, false, nullptr);
      if (state->hasDirLabel)
        searchNextFillDMA_SFCB(state);
      state->errCode = 0;
      state->retCode = 0;
      return;

    } else {

      // return a new file

      if (state->hasDirLabel && state->extIndex == -1 && state->getAllFiles && searchNextFillDMA_DirLabel(state)) {
        // get directory label as first extent
        state->extIndex += 1;
        state->returnSFCB = true; // directory label has its own datestamp (unused?)
        state->retCode = 0;
        state->errCode = 0;
        return;
      }

      state->index    += 1;
      state->extIndex += 1;
      if (state->index >= m_fileBrowser.count()) {
        // no more files
        state->errCode = 1;
        return;
      }

      char const * filename = m_fileBrowser.get(state->index)->name;

      char filename11[11];
      expandFilename(filename, filename11, m_fileBrowser.get(state->index)->isDir);

      bool match = true;

      if (!state->getAllFiles) {

        // does the search match?

        char searchingStrg[11];
        m_HAL->copyMem(searchingStrg, state->FCB + 1, 11);
        char const * searching = searchingStrg;

        char const * filename11ptr = filename11;
        for (int i = 0; i < 11; ++i, ++searching, ++filename11ptr) {
          if (*searching != '?' && toupper(*searching) != *filename11ptr) {
            match = false;
            break;
          }
        }
      }

      if (match) {
        // file found

        // get file size
        state->size = (int) m_fileBrowser.fileSize(filename);

        searchNextFillDMA_FCB(state, true, filename11);

        if (state->hasDirLabel) {
          // get file date stamps
          int year = 0, month = 0, day = 0, hour = 0, minutes = 0, seconds = 0;
          if (state->dirLabelFlags & DIRLABELFLAGS_CREATE)
            m_fileBrowser.fileCreationDate(filename, &year, &month, &day, &hour, &minutes, &seconds);
          if (state->dirLabelFlags & DIRLABELFLAGS_ACCESS)
            m_fileBrowser.fileAccessDate(filename, &year, &month, &day, &hour, &minutes, &seconds);
          state->createOrAccessDate.set(year, month, day, hour, minutes, seconds);
          year = month = day = hour = minutes = seconds = 0;
          if (state->dirLabelFlags & DIRLABELFLAGS_UPDATE)
            m_fileBrowser.fileUpdateDate(filename, &year, &month, &day, &hour, &minutes, &seconds);
          state->updateDate.set(year, month, day, hour, minutes, seconds);
          searchNextFillDMA_SFCB(state);
          state->returnSFCB = true;
        }

        state->retCode = 0;
        state->errCode = 0;
        return;
      }

    }

  }
}


// don't care about m_consoleReadyChar
uint8_t BDOS::rawConsoleDirectIn()
{
  return m_BIOS->BIOS_call_CONIN();
}


// don't care about m_consoleReadyChar
bool BDOS::rawConsoleDirectAvailable()
{
  return m_BIOS->BIOS_call_CONST() != 0;
}


// always blocking
uint8_t BDOS::rawConsoleIn()
{
  uint8_t r;
  if (m_consoleReadyChar != 0) {
    r = m_consoleReadyChar;
    m_consoleReadyChar = 0;
  } else {
    r = rawConsoleDirectIn();
  }
  return r;
}


bool BDOS::rawConsoleAvailable()
{
  return m_consoleReadyChar != 0 || rawConsoleDirectAvailable();
}


// output "c" to console
// - check for CTRL-P (if enabled)
// - check (and stop) for CTRL-S / CTRL-Q (if enabled)
// - echoes to LST (if enabled)
// - check for CTRL-C (if enabled)
// - expand TAB (if enabled)
void BDOS::consoleOutChar(uint8_t c)
{
  bool rawConsole      = isRawConsoleOutMode();
  bool checkCTRLP      = !rawConsole;
  bool checkCTRLC      = !isDisableCTRLCExit();
  bool checkStopScroll = !isDisableStopScroll();

  // user typed ctrl chars?
  if ((checkCTRLP || checkCTRLC || checkStopScroll) && rawConsoleAvailable()) {
    uint8_t tc = rawConsoleIn();
    // CTRL-P?
    if (checkCTRLP && tc == ASCII_CTRLP) {
      switchPrinterEchoEnabled();
    }
    // CTRL-C?
    else if (checkCTRLC && tc == ASCII_CTRLC) {
      m_HAL->CPU_stop();
      SCB_setWord(SCB_PROGRAMRETCODE_W, 0xFFFE);
    }
    // CTRL-S?
    else if (checkStopScroll && tc == ASCII_CTRLS) {
      // wait for CTRL-Q
      while ((tc = rawConsoleIn()) != ASCII_CTRLQ) {
        // CTRL-P is also checked here
        if (checkCTRLP && tc == ASCII_CTRLP)
          switchPrinterEchoEnabled();
        // CTRL-C is also checked here
        if (checkCTRLC && tc == ASCII_CTRLC) {
          m_HAL->CPU_stop();
          SCB_setWord(SCB_PROGRAMRETCODE_W, 0xFFFE);
          break;
        }
      }
    } else
      m_consoleReadyChar = tc;  // reinject character
  }

  if (c == ASCII_TAB && rawConsole == false) {
    // expand TAB to 8 spaces
    for (int i = 0; i < 8; ++i) {
      m_BIOS->BIOS_call_CONOUT(' ');
      if (m_printerEchoEnabled)
        lstOut(' ');
    }
  } else {
    m_BIOS->BIOS_call_CONOUT(c);
    if (rawConsole == false && m_printerEchoEnabled)
      lstOut(c);
  }
}


uint8_t BDOS::consoleIn()
{
  uint8_t c = 0;
  while (true) {
    c = rawConsoleIn();
    switch (c) {

      // TAB (expand to 8 spaces)
      case ASCII_TAB:
        for (int i = 0; i < 8; ++i) {
          m_BIOS->BIOS_call_CONOUT(' ');
          if (m_printerEchoEnabled)
            lstOut(' ');
        }
        break;

      // CTRL-P, switch on/off printer echo
      case ASCII_CTRLP:
        if (isDisableStopScroll() == false) { // isDisableStopScroll() disables also CTRL-P
          switchPrinterEchoEnabled();
          continue; // reloop
        }
        break;

      // CTRL-C, terminate program
      case ASCII_CTRLC:
        if (isDisableCTRLCExit() == false) {
          m_HAL->CPU_stop();
          SCB_setWord(SCB_PROGRAMRETCODE_W, 0xFFFE);
        }
        break;

      // CTRL-S/CTRL-Q (stop scroll and wait for CTRL-Q)
      case ASCII_CTRLS:
        if (!isDisableStopScroll()) {
          while ((c = rawConsoleIn()) != ASCII_CTRLQ) {
            // CTRL-P is also checked here
            if (c == ASCII_CTRLP)
              switchPrinterEchoEnabled();
            // CTRL-C is also checked here
            if (c == ASCII_CTRLC && isDisableCTRLCExit() == false) {
              m_HAL->CPU_stop();
              SCB_setWord(SCB_PROGRAMRETCODE_W, 0xFFFE);
              return c;
            }
            m_BIOS->BIOS_call_CONOUT(ASCII_BEL);
          }
          continue; // reloop
        }
        break;

      // others, echo
      default:
        m_BIOS->BIOS_call_CONOUT(c);
        if (m_printerEchoEnabled)
          lstOut(c);
        break;

    }
    break;  // exit loop
  }
  return c;
}


void BDOS::lstOut(uint8_t c)
{
  m_BIOS->BIOS_call_LIST(c);
}


void BDOS::lstOut(char const * str)
{
  while (*str)
    lstOut(*str++);
}


// print until "delimeter"
void BDOS::consoleOutStr(char const * str, char delimiter)
{
  while (*str != delimiter)
    consoleOutChar(*str++);
}


// print until "delimiter"
void BDOS::consoleOutStr(uint16_t addr, char delimiter)
{
  char c;
  while ((c = m_HAL->readByte(addr++)) != delimiter)
    consoleOutChar(c);
}


void BDOS::consoleOutFmt(const char * format, ...)
{
  va_list ap;
  va_start(ap, format);
  int size = vsnprintf(nullptr, 0, format, ap) + 1;
  if (size > 0) {
    va_end(ap);
    va_start(ap, format);
    char buf[size + 1];
    vsnprintf(buf, size, format, ap);
    consoleOutStr(buf);
  }
  va_end(ap);
}


void BDOS::saveIntoConsoleHistory(char const * str)
{
  if (str && strlen(str) > 0) {
    int prevIndex = m_writeHistoryItem > 0 ? m_writeHistoryItem - 1 : CCP_HISTORY_DEPTH - 1;
    if (strcmp(m_history[prevIndex], str) != 0) {
      strcpy(m_history[m_writeHistoryItem], str);
      ++m_writeHistoryItem;
      if (m_writeHistoryItem == CCP_HISTORY_DEPTH)
        m_writeHistoryItem = 0;
    }
    m_readHistoryItem = m_writeHistoryItem;
  }
  /*
  for (int i = 0; i < CCP_HISTORY_DEPTH; ++i)
    m_HAL->logf("%d: \"%s\"\r\n", i, m_history[i]);
  //*/
}


char const * BDOS::getPrevHistoryItem()
{
  --m_readHistoryItem;
  if (m_readHistoryItem < 0)
    m_readHistoryItem = CCP_HISTORY_DEPTH - 1;
  return m_history[m_readHistoryItem];
}


char const * BDOS::getNextHistoryItem()
{
  ++m_readHistoryItem;
  if (m_readHistoryItem == CCP_HISTORY_DEPTH)
    m_readHistoryItem = 0;
  return m_history[m_readHistoryItem];
}



void BDOS::SCB_setBit(int field, int bitmask)
{
  SCB_setByte(field, SCB_getByte(field) | bitmask);
}


void BDOS::SCB_clearBit(int field, int bitmask)
{
  SCB_setByte(field, SCB_getByte(field) & ~bitmask);
}


bool BDOS::SCB_testBit(int field, int bitmask)
{
  return (SCB_getByte(field) & bitmask) != 0;
}


void BDOS::SCB_setByte(int field, uint8_t value)
{
  m_HAL->writeByte(SCB_ADDR + field, value);
}


uint8_t BDOS::SCB_getByte(int field)
{
  return m_HAL->readByte(SCB_ADDR + field);
}


void BDOS::SCB_setWord(int field, uint16_t value)
{
  m_HAL->writeWord(SCB_ADDR + field, value);
}


uint16_t BDOS::SCB_getWord(int field)
{
  return m_HAL->readWord(SCB_ADDR + field);
}
