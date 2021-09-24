/*
  Created by Fabrizio Di Vittorio (fdivitto2013@gmail.com) - <http://www.fabgl.com>
  Copyright (c) 2019-2021 Fabrizio Di Vittorio.
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
"fd0" : Floppy drive 0 (A) filename or URL
"fd1" : Floppy drive 1 (B) filename or URL
"hd0" : Hard disk drive 0 filename or URL
"hd1" : Hard disk drive 1 filename or URL

Example:

desc "FreeDOS (A:) + DOS Programming Tools (C:)"  fd0 "http://www.fabglib.org/downloads/A_freedos.img"  hd0 "http://www.fabglib.org/downloads/C_dosdev.img"
desc "My Own MSDOS"  fd0 BOOT.IMG  hd0 HDD.IMG
desc "Floppy Only"  fd0 TESTBOOT.IMG

*/


#pragma once


#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "bios.h"



#define MACHINE_CONF_FILENAME "mconfs.txt"

#define NL "\r\n"


static const char DefaultConfFile[] =
  "desc \"FreeDOS (A:)\"                               fd0 http://www.fabglib.org/downloads/A_freedos.img" NL
  "desc \"FreeDOS (A:) + DOS Programming Tools (C:)\"  fd0 http://www.fabglib.org/downloads/A_freedos.img hd0 http://www.fabglib.org/downloads/C_dosdev.img" NL
  "desc \"FreeDOS (A:) + Windows 3.0 Hercules (C:)\"   fd0 http://www.fabglib.org/downloads/A_freedos.img hd0 http://www.fabglib.org/downloads/C_winherc.img" NL
  "desc \"FreeDOS (A:) + DOS Programs and Games (C:)\" fd0 http://www.fabglib.org/downloads/A_freedos.img hd0 http://www.fabglib.org/downloads/C_dosprog.img" NL
  "desc \"MS-DOS 3.31 (A:)\"                           fd0 http://www.fabglib.org/downloads/A_MSDOS331.img" NL
  "desc \"Linux ELKS 0.4.0\"                           fd0 http://www.fabglib.org/downloads/A_ELK040.img" NL
  "desc \"CP/M 86 + Turbo Pascal 3\"                   fd0 http://www.fabglib.org/downloads/A_CPM86.img" NL;



////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MachineConf


struct MachineConfItem {
  MachineConfItem * next;
  char            * desc;
  char            * disk[DISKCOUNT];

  MachineConfItem()
    : next(nullptr),
      desc(nullptr),
      disk()  // this zeroes disk[] items
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
  void saveTag(FILE * file, char const * tag, char const * value);

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
 
