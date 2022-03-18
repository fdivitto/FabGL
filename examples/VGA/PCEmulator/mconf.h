/*
  Created by Fabrizio Di Vittorio (fdivitto2013@gmail.com) - <http://www.fabgl.com>
  Copyright (c) 2019-2022 Fabrizio Di Vittorio.
  All rights reserved.


* Please contact fdivitto2013@gmail.com if you need a commercial license.


* This library and related software is available under GPL v3.

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

** Disks configuration text format **

Each line contains a machine configuration, which includes description and disks images location. Location can be a URL or a local path.

Allowed tags:

"desc" : Textual description of the configuration
"fd0"  : Floppy drive 0 (A) filename or URL
"fd1"  : Floppy drive 1 (B) filename or URL
"hd0"  : Hard disk drive 0 filename or URL
"hd1"  : Hard disk drive 1 filename or URL
"chs0" : Hard disk 0 geometry (Cylinders,Heads,Sectors)
"chs1" : Hard disk 1 geometry (Cylinders,Heads,Sectors)
"boot" : Boot drive. Values: fd0, fd1, hd0, hd1. Default is fd0

A filename may contain a path, relative to SD_MOUNT_PATH mounting path. For example, if "disks/disk1.img" is specified and SD_MOUNT_PATH="/SD", then it loads "/SD/disks/disk1.img".


Examples:

Download first floppy image from "http://www.fabglib.org/downloads/A_freedos.img" and first hard disk image from "http://www.fabglib.org/downloads/C_dosdev.img". Boot from floppy:
    desc "FreeDOS (A:) + DOS Programming Tools (C:)"  fd0 "http://www.fabglib.org/downloads/A_freedos.img"  hd0 "http://www.fabglib.org/downloads/C_dosdev.img"

First hard disk is "HDD_10M.IMG", having 306 cylinders, 4 heads and 17 sectors. Boot from hard disk:
    desc "My Own MSDOS" hd0 HDD_10M.IMG chs0 306,4,17 boot hd0

First floppy drive is "TESTBOOT.IMG". Boot from first floppy:
    desc "Floppy Only"  fd0 TESTBOOT.IMG boot fd0

Boot from disk1.img located inside "msdos3" folder:
    desc "Machine1" fd0 msdos3/disk1.img

*/


#pragma once


#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "bios.h"


#define SD_MOUNT_PATH         "/SD"

#define MACHINE_CONF_FILENAME "mconfs.txt"

#define NL                    "\r\n"


static const char DefaultConfFile[] =
  "desc \"FreeDOS (floppy 1.44MB)\"                    fd0 http://www.fabglib.org/downloads/floppy_FREEDOS.img"           NL
  "desc \"MS-DOS 3.31 (floppy 1.44MB)\"                fd0 http://www.fabglib.org/downloads/floppy_MSDOS331.img"          NL
  "desc \"CP/M 86 + Turbo Pascal 3 (floppy 1.44MB)\"   fd0 http://www.fabglib.org/downloads/floppy_CPM86.img"             NL
  "desc \"Linux ELKS 0.4.0 (floppy 1.44MB)\"           fd0 http://www.fabglib.org/downloads/floppy_ELK040.img"            NL
  "desc \"FreeDOS (HDD 8MB)\"                          hd0 http://www.fabglib.org/downloads/hd8_FREEDOS.img     boot hd0" NL
  "desc \"DOS Programs and Games (HDD 20MB)\"          hd0 http://www.fabglib.org/downloads/hd20_DOSPROG.img    boot hd0" NL
  "desc \"DOS Programming Tools (HDD 20MB)\"           hd0 http://www.fabglib.org/downloads/hd20_DOSDEV.img     boot hd0" NL
  "desc \"Windows 3.0 (HDD 20MB)\"                     hd0 http://www.fabglib.org/downloads/hd20_WINHERC.img    boot hd0" NL
  "desc \"GEM 3.13 (HDD 8MB)\"                         hd0 http://www.fabglib.org/downloads/hd8_GEM31.img       boot hd0" NL
  "desc \"GEOS 2.0 (HDD 20MB)\"                        hd0 http://www.fabglib.org/downloads/hd20_GEOS20.img      boot hd0" NL
  "desc \"MS-DOS 5.00 (HDD 8MB)\"                      hd0 http://www.fabglib.org/downloads/hd8_MSDOS500.img    boot hd0" NL
  "desc \"MS-DOS 6.22 (HDD 8MB)\"                      hd0 http://www.fabglib.org/downloads/hd8_MSDOS622.img    boot hd0" NL
  "desc \"SvarDOS (HDD (hDD 10MB)\"                    hd0 http://www.fabglib.org/downloads/hd10_SVARDOS.img    boot hd0" NL;


////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MachineConf


struct MachineConfItem {
  MachineConfItem * next;
  char            * desc;
  char            * disk[DISKCOUNT];        // path relative to mounting path
  uint16_t          cylinders[DISKCOUNT];
  uint16_t          heads[DISKCOUNT];
  uint16_t          sectors[DISKCOUNT];
  uint8_t           bootDrive;

  MachineConfItem()
    : next(nullptr),
      desc(nullptr),
      disk(),
      cylinders(),
      heads(),
      sectors(),
      bootDrive(0)
  {
  }

  ~MachineConfItem()
  {
    free(desc);
    for (int i = 0; i < DISKCOUNT; ++i)
      free(disk[i]);
  }

  bool isValid()                                { return desc != nullptr; }

  void setDesc(char const * value)              { free(desc); desc = strdup(value); }
  void setDisk(int index, char const * value)   { free(disk[index]); disk[index] = strdup(value); }
  void setCHS(int index, char const * value)    { sscanf(value, "%hu,%hu,%hu", &cylinders[index], &heads[index], &sectors[index]); }

  void setBootDrive(char const * value)         {
    for (int i = 0; i < DISKCOUNT; ++i)
      if (strcmp(value, driveIndexToStr(i)) == 0) {
        bootDrive = i;
        return;
      }
  }

  static char const * driveIndexToStr(int index) {
    static char const * DRVNAME[DISKCOUNT] = { "fd0", "fd1", "hd0", "hd1" };
    return DRVNAME[index];
  }
};


class MachineConf {
public:
  MachineConf();
  ~MachineConf();

  void loadFromFile(FILE * file);
  void saveToFile(FILE * file);

  bool deleteItem(int index);

  void addItem(MachineConfItem * item);
  void insertItem(int insertingPoint, MachineConfItem * item);

  MachineConfItem * getFirstItem()     { return m_itemsList; }
  MachineConfItem * getItem(int index);

private:

  void cleanUp();

  MachineConfItem * m_itemsList;

};



////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////
// helpers


void loadMachineConfiguration(MachineConf * mconf);

void saveMachineConfiguration(MachineConf * mconf);

void editConfigDialog(InputBox * ibox, MachineConf * mconf, int idx);

void newConfigDialog(InputBox * ibox, MachineConf * mconf, int idx);

void delConfigDialog(InputBox * ibox, MachineConf * mconf, int idx);

void drawInfo(Canvas * canvas);
 
bool createFATFloppyImage(InputBox * ibox, int diskType, char const * directory, char const * filename);
