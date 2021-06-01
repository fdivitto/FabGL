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


#include "defs.h"
#include "HAL.h"
#include "BIOS.h"



// FCB fields
#define FCB_DR  0x00 // Drive. 0 for default, 1-16 for A-P
#define FCB_USR 0x00 // when stored to disk FCB_DR becomes User num
#define FCB_F1  0x01 // Filename, 7-bit ASCII
#define FCB_F2  0x02
#define FCB_F3  0x03
#define FCB_F4  0x04
#define FCB_F5  0x05
#define FCB_F6  0x06
#define FCB_F7  0x07
#define FCB_F8  0x08
#define FCB_T1  0x09 // Filetype, 7-bit ASCII
#define FCB_T2  0x0A
#define FCB_T3  0x0B
#define FCB_EX  0x0C // current extent (0-31)
#define FCB_S1  0x0D
#define FCB_S2  0x0E // extent high byte
#define FCB_RC  0x0F // record count (0-128)
#define FCB_AL  0x10 // allocation pointers (16 bytes)
#define FCB_TS1 0x18 // 4-byte creation time stamp field (directory label only)
#define FCB_TS2 0x1C // 4-byte update time stamp field (directory label only)
#define FCB_CR  0x20 // Current record within extent
#define FCB_R0  0x21 // Random access record number (0..7)
#define FCB_R1  0x22 // Random access record number (8..15)
#define FCB_R2  0x23 // Random access record number (16..17)


// COM (with RSX) header
#define COMHEAD_MAGIC      0x00
#define COMHEAD_LEN        0x01
#define COMHEAD_INIT       0x03
#define COMHEAD_LOADERFLAG 0x0D
#define COMHEAD_RSXCOUNT   0x0F
#define COMHEAD_RSXRECORDS 0x10


// RSX Record fields (part of "COM with RSX" header)
#define RSXRECORD_OFFSET  0x00
#define RSXRECORD_CODELEN 0x02
#define RSXRECORD_NONBANK 0x04
#define RSXRECORD_NAME    0x06


// RSX Prefix fields
#define RSXPREFIX_SERIAL  0x00
#define RSXPREFIX_START   0x06
#define RSXPREFIX_NEXT    0x0A
#define RSXPREFIX_PREV    0x0C
#define RSXPREFIX_REMOVE  0x0E
#define RSXPREFIX_NONBANK 0x0F
#define RSXPREFIX_NAME    0x10
#define RSXPREFIX_LOADER  0x18



// Directory label flags
#define DIRLABELFLAGS_EXISTS   0b00000001
#define DIRLABELFLAGS_CREATE   0b00010000
#define DIRLABELFLAGS_UPDATE   0b00100000
#define DIRLABELFLAGS_ACCESS   0b01000000
#define DIRLABELFLAGS_PASSWORD 0b10000000


#define CCP_HISTORY_LINEBUFFER_LEN 128  // number of bytes to allocate for each history buffer
#define CCP_HISTORY_DEPTH 4


#define DIRECTORY_EXT "[D]"


// temporary buffer used to copy files
#define COPYFILE_BUFFERSIZE 1024


struct FileSearchState {
  // current file FCB
  uint16_t  FCB;

  // where to store results (needs 32*4=128 bytes)
  uint16_t  DMA;

  // file index
  int16_t   index;

  // extent index
  int16_t   extIndex;

  // in case of "search all files" (DR = '?')
  bool      getAllFiles;

  // in case of "search all extents" (EX = '?')
  bool      getAllExtents;

  // remaining size of searching file, in bytes
  int32_t   size;

  bool      hasDirLabel;
  uint8_t   dirLabelFlags;

  // file datestamps (create/access, update)
  DateTime  createOrAccessDate;
  DateTime  updateDate;

  // 0 = ok, at least one file has been found
  // 1 = ok, but no file found / no more files
  // 2 = invalid drive
  int32_t   errCode;

  // which FCB point (0,1,2,3 where 3 can be used for SFCB)
  int32_t   retCode;

  // is true after first extent has been returned when
  // datestamp needs to be filled in DMA instead of FCB
  bool      returnSFCB;
};



struct OpenFileCache {
  FILE *   file;          // nullptr = free item
  uint32_t filenameHash;
#if MSGDEBUG & DEBUG_BDOS
  char     filename[12];
#endif
};



class BDOS {
public:

  BDOS(HAL * hal, BIOS * bios);

  ~BDOS();

  void runCommand(uint16_t cmdline);
  bool execProgram(int drive, char const * filename, uint16_t parameters);
  void execLoadedProgram(size_t size);
  void resetProgramEnv();

  void setSearchPath(char const * path);
  char const * getSearchPath();

  char const * getCurrentDir();
  char const * getCurrentDir(int drive);
  bool isDir(uint16_t FCBaddr);

  void setCurrentDrive(int drive);
  int getCurrentDrive();

  void setCurrentUser(int user);
  int getCurrentUser();

  bool strToDrive(char const * str, int * drive);
  bool strToDrive(uint16_t str, int * drive);

  bool checkDrive(int drive, int errfunc = -1);

  char * createAbsolutePath(uint16_t pathAddr, bool insertMountPath = true, int * drive = nullptr);

  static bool hasExt(char const * filename, char const * ext);
  bool hasExt(uint16_t FCBaddr, char const * ext);

  void setAuxStream(Stream * value)           { m_auxStream = value; }

  uint16_t getTPASize();
  uint16_t getTPATop();
  bool RSXInstalled();
  bool BDOSAddrChanged();
  bool BIOSAddrChanged();  

  int getOpenFilesCount();

  void closeAllFiles();

  static bool isFileDelimiter(char c);

  static bool fileMatchWithWildCards(char const * searchingName, char const * testingName);
  
  static void processPrevDirMarks(char * path);

  // SCB helpers
  void SCB_setBit(int field, int bitmask);
  void SCB_clearBit(int field, int bitmask);
  bool SCB_testBit(int field, int bitmask);
  void SCB_setByte(int field, uint8_t value);
  uint8_t SCB_getByte(int field);
  void SCB_setWord(int field, uint16_t value);
  uint16_t SCB_getWord(int field);

  // BDOS calls helpers from host (not CPU code)
  int BDOS_callConsoleIn();
  int BDOS_callConsoleStatus();
  int BDOS_callDirectConsoleIO(int mode);
  void BDOS_callConsoleOut(char c);
  void BDOS_callOutputString(char const * str, uint16_t workBufAddr, size_t workBufSize, size_t maxChars = 0);
  void BDOS_callOutputString(uint16_t str);
  void BDOS_callReadConsoleBuffer(uint16_t addr);
  void BDOS_callSystemReset();
  int BDOS_callParseFilename(uint16_t PFCBaddr);
  int BDOS_callSearchForFirst(uint16_t FCBaddr);
  int BDOS_callSearchForNext();
  int BDOS_callDeleteFile(uint16_t FCBaddr);
  int BDOS_callRenameFile(uint16_t FCBaddr);
  int BDOS_callOpenFile(uint16_t FCBaddr);
  int BDOS_callCloseFile(uint16_t FCBaddr);
  int BDOS_callReadSequential(uint16_t FCBaddr);
  void BDOS_callSetDMAAddress(uint16_t DMAaddr);
  int BDOS_callMakeFile(uint16_t FCBaddr);
  int BDOS_callCopyFile(uint16_t srcFullPathAddr, uint16_t dstPathAddr, bool overwrite, bool display);
  int BDOS_callChangeCurrentDirectory(uint16_t pathAddr);


private:

  // implements CPUExecutionHook
  void CPUstep();

  // set A, L, B, H CPU registers in one step
  void CPUsetALBH(uint8_t A, uint8_t L, uint8_t B, uint8_t H);

  // set A=L=AL, B=H=BH CPU registers in one step
  void CPUsetALBH(uint8_t AL, uint8_t BH);

  // set HL and BA  (H=hi byte,L=lo byte, B=hi byte, A=lo byte)
  void CPUsetHLBA(uint16_t HL, uint16_t BA);

  void processBDOS();

  void BDOS_systemReset();                  // 0 (0x00)
  void BDOS_consoleInput();                 // 1 (0x01)
  void BDOS_consoleOutput();                // 2 (0x02)
  void BDOS_auxInput();                     // 3 (0x03)
  void BDOS_auxOutput();                    // 4 (0x04)
  void BDOS_lstOutput();                    // 5 (0x05)
  void BDOS_directConsoleIO();              // 6 (0x06)
  void BDOS_auxInputStatus();               // 7 (0x07)
  void BDOS_auxOutputStatus();              // 8 (0x08)
  void BDOS_outputString();                 // 9 (0x09)
  void BDOS_readConsoleBuffer();            // 10 (0x0A)
  void BDOS_getConsoleStatus();             // 11 (0x0B)
  void BDOS_returnVersionNumber();          // 12 (0x0C)
  void BDOS_resetDiskSystem();              // 13 (0x0D)
  void BDOS_selectDisk();                   // 14 (0x0E)
  void BDOS_openFile();                     // 15 (0x0F)
  void BDOS_closeFile();                    // 16 (0x10)
  void BDOS_searchForFirst();               // 17 (0x11)
  void BDOS_searchForNext();                // 18 (0x12)
  void BDOS_deleteFile();                   // 19 (0x13)
  void BDOS_readSequential();               // 20 (0x14)
  void BDOS_writeSequential();              // 21 (0x15)
  void BDOS_makeFile();                     // 22 (0x16)
  void BDOS_renameFile();                   // 23 (0x17)
  void BDOS_returnLoginVector();            // 24 (0x18)
  void BDOS_returnCurrentDisk();            // 25 (0x19)
  void BDOS_setDMAAddress();                // 26 (0x1A)
  void BDOS_getAddr();                      // 27 (0x1B)
  void BDOS_writeProtectDisk();             // 28 (0x1C)
  void BDOS_getReadOnlyVector();            // 29 (0x1D)
  void BDOS_setFileAttributes();            // 30 (0x1E)
  void BDOS_getDPBAddress();                // 31 (0x1F)
  void BDOS_getSetUserCode();               // 32 (0x20)
  void BDOS_readRandom();                   // 33 (0x21)
  void BDOS_writeRandom();                  // 34 (0x22)
  void BDOS_computeFileSize();              // 35 (0x23)
  void BDOS_setRandomRecord();              // 36 (0x24)
  void BDOS_resetDrive();                   // 37 (0x25)
  void BDOS_accessDrive();                  // 38 (0x26)
  void BDOS_freeDrive();                    // 39 (0x27)
  void BDOS_writeRandomZeroFill();          // 40 (0x28)
  void BDOS_testAndWriteRecord();           // 41 (0x29)
  void BDOS_lockRecord();                   // 42 (0x2A)
  void BDOS_unlockRecord();                 // 43 (0x2B)
  void BDOS_setMultiSectorCount();          // 44 (0x2C)
  void BDOS_setErrorMode();                 // 45 (0x2D)
  void BDOS_getDiskFreeSpace();             // 46 (0x2E)
  void BDOS_chainToProgram();               // 47 (0x2F)
  void BDOS_flushBuffers();                 // 48 (0x30)
  void BDOS_getSetSystemControlBlock();     // 49 (0x31)
  void BDOS_directBIOSCall();               // 50 (0x32)
  void BDOS_loadOverlay();                  // 59 (0x3B)
  void BDOS_callResidentSystemExtension();  // 60 (0x3C)
  void BDOS_freeBlocks();                   // 98 (0x62)
  void BDOS_truncateFile();                 // 99 (0x63)
  void BDOS_setDirectoryLabel();            // 100 (0x64)
  void BDOS_returnDirLabelData();           // 101 (0x65)
  void BDOS_readFileDateStamps();           // 102 (0x66)
  void BDOS_setDateTime();                  // 104 (0x68)
  void BDOS_getDateTime();                  // 105 (0x69)
  void BDOS_getSetProgramReturnCode();      // 108 (0x6C)
  void BDOS_getSetConsoleMode();            // 109 (0x6D)
  void BDOS_getSetOutputDelimiter();        // 110 (0x6E)
  void BDOS_printBlock();                   // 111 (0x6F)
  void BDOS_listBlock();                    // 112 (0x70)
  void BDOS_parseFilename();                // 152 (0x98)
  void BDOS_copyFile();                     // 212 (0xD4)   ** specific of this BDOS implementation
  void BDOS_changeCurrentDirectory();       // 213 (0xD5)   ** specific of this BDOS implementation

  void BDOS_call(uint16_t * BC, uint16_t * DE, uint16_t * HL, uint16_t * AF);

  bool rawConsoleDirectAvailable();
  uint8_t rawConsoleDirectIn();

  bool rawConsoleAvailable();
  uint8_t rawConsoleIn();

  uint8_t consoleIn();

  // like BDOS_callXXX but avoid the overhead of BDOS entry. These interface directly to the BIOS
  void consoleOutChar(uint8_t c);
  void consoleOutStr(uint16_t addr, char delimiter = 0x00);
  void consoleOutStr(char const * str, char delimiter = 0x00);
  void consoleOutFmt(const char * format, ...);
  void lstOut(uint8_t c);
  void lstOut(char const * str);

  void switchPrinterEchoEnabled() { m_printerEchoEnabled = !m_printerEchoEnabled; }
  bool printerEchoEnabled()       { return m_printerEchoEnabled; }

  void parseParams();

  void setBrowserAtDrive(int drive);
  int getDriveFromFCB(uint16_t FCBaddr);

  void searchNextFillDMA_FCB(FileSearchState * state, bool isFirst, char const * filename11);
  void searchNextFillDMA_SFCB(FileSearchState * state);
  bool searchNextFillDMA_DirLabel(FileSearchState * state);
  void searchFirst(FileSearchState * state);
  void searchNext(FileSearchState * state);

  char const * expandFilename(char const * filename, char * expandedFilename, bool isDir);

  void getFilenameFromFCB(uint16_t FCBaddr, char * filename);

  uint16_t filenameToFCB(uint16_t filename, uint16_t FCB, uint16_t * passwordAddr, uint8_t * passwordLen);

  FILE * openFile(uint16_t FCBaddr, bool create, bool tempext, int errFunc, int * err);
  void closeFile(uint16_t FCBaddr);

  void createDir(uint16_t FCBaddr, int errFunc, int * err);

  void setAbsolutePosFCB(uint16_t FCBaddr, size_t pos);
  size_t getAbsolutePosFCB(uint16_t FCBaddr);
  void setPosFCB(uint16_t FCBaddr, size_t pos);
  size_t getPosFCB(uint16_t FCBaddr);

  // Console mode helpers
  bool isFUNC11CTRLCOnlyMode() { return SCB_getWord(SCB_CONSOLEMODE_W) & CONSOLEMODE_FUN11_CTRLC_ONLY; }
  bool isDisableStopScroll()   { return SCB_getWord(SCB_CONSOLEMODE_W) & CONSOLEMODE_DISABLE_STOPSCROLL; }
  bool isRawConsoleOutMode()   { return SCB_getWord(SCB_CONSOLEMODE_W) & CONSOLEMODE_RAWCONSOLE_OUTMODE; }
  bool isDisableCTRLCExit()    { return SCB_getWord(SCB_CONSOLEMODE_W) & CONSOLEMODE_DISABLE_CTRLC_EXIT; }

  // Error mode helpers
  bool isDefaultErrorMode()       { return SCB_getByte(SCB_ERRORMODE_B) < 0xFE; }
  bool isReturnErrorMode()        { return SCB_getByte(SCB_ERRORMODE_B) == 0xFF; }
  bool isDisplayReturnErrorMode() { return SCB_getByte(SCB_ERRORMODE_B) == 0xFE; }
  void doError(uint8_t A, uint8_t H, const char * format, ...);

  // RSX support
  void processRSXCOM();
  void loadRSX(uint16_t imageAddr, int imageLen);
  void removeRSX();

  // directory label
  void writeDirectoryLabel(int drive, uint16_t FCBaddr, int errfunc);
  uint8_t readDirectoryLabel(int drive, uint16_t FCBaddr, uint8_t * FCB);
  uint8_t getDirectoryLabelFlags(int drive);

  void saveIntoConsoleHistory(char const * str);
  char const * getPrevHistoryItem();
  char const * getNextHistoryItem();

  void copyFile(FILE * src, FILE * dst);

  uint32_t filenameHash(uint16_t FCBaddr);
  FILE * getFileFromCache(uint16_t FCBaddr);
  void addFileToCache(uint16_t FCBaddr, FILE * file);
  void removeFileFromCache(FILE * file);

  static void strToUpper(char * str);


  HAL *           m_HAL;

  BIOS *          m_BIOS;

  // internal state of BDOS_searchForFirst/BDOS_searchForNext
  // note: this is not internal state of searchFirst/searchNext
  FileSearchState m_fileSearchState;

  FileBrowser     m_fileBrowser;

  uint16_t        m_writeProtectWord;

  char *          m_currentDir[MAXDRIVERS];

  // TODO: refresh only new disk mount
  uint8_t         m_cachedDirLabelFlags[MAXDRIVERS];  // cached directory label FCB[FCB_EX], 0xff = to reload

  bool            m_printerEchoEnabled;

  Stream *        m_auxStream;

  uint8_t         m_consoleReadyChar; // 0 = no ready char, read directly from terminal

  // circular buffer for history of BDOS 10 function
  char *          m_history[CCP_HISTORY_DEPTH];
  int             m_readHistoryItem;
  int             m_writeHistoryItem;

  // if assigned replaces SCB_DRIVESEARCHCHAIN#_B
  char *          m_searchPath;

  // TODO: collisions exist, so it could be better to find another way to handle open files caching!
  OpenFileCache   m_openFileCache[CPMMAXFILES];

};


