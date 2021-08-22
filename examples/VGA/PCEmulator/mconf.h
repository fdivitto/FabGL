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


 #pragma once


/*

** Disks configuration text format **

Each line contains a configuration, which includes description and disks location. Location can be
an URL or a local path.

Allowed tags:

"desc" : Textual description of the configuration
"dska" : Floppy drive A location
"dskb" : Floppy drive B location
"dskc" : Hard disk drive C location

Example:

desc "FreeDOS (A:) + DOS Programming Tools (C:)"  -dska "http://www.fabglib.org/downloads/A_freedos.img"  dskc "http://www.fabglib.org/downloads/C_dosdev.img"
desc "My Own MSDOS" dska BOOT.IMG -dskc HDD.IMG
desc "Floppy Only" dska TESTBOOT.IMG

*/


#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>



struct MachineConfItem {
  MachineConfItem * next;
  char            * desc;
  char            * dska;
  char            * dskb;
  char            * dskc;

  MachineConfItem()
    : MachineConfItem(nullptr, nullptr, nullptr, nullptr)
  {
  }

  MachineConfItem(char * desc_, char * dska_, char * dskb_, char * dskc_)
    : next(nullptr),
      desc(desc_),
      dska(dska_),
      dskb(dskb_),
      dskc(dskc_)
  {
  }

  ~MachineConfItem()
  {
    free(desc);
    free(dska);
    free(dskb);
    free(dskc);
  }

  bool isValid()                                      { return desc != nullptr; }

};


class MachineConf {
public:
  MachineConf();
  ~MachineConf();

  void loadFromFile(FILE * file);
  void saveToFile(FILE * file);

  bool deleteItem(int index);

  MachineConfItem * getFirstItem()     { return m_itemsList; }
  MachineConfItem * getItem(int index);

private:

  void cleanUp();
  void addItem(MachineConfItem * item);
  void saveTag(FILE * file, char const * tag, char const * value);

  MachineConfItem * m_itemsList;

};
 
