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



#include "mconf.h"


#define MAXTAGLENGTH     6
#define MAXVALUELENGTH 128


MachineConf::MachineConf()
  : m_itemsList(nullptr)
{
}


MachineConf::~MachineConf()
{
  cleanUp();
}


void MachineConf::cleanUp()
{
  while (deleteItem(0))
    ;
}


bool MachineConf::deleteItem(int index)
{
  MachineConfItem * item = m_itemsList;
  MachineConfItem * prev = nullptr;
  while (item && --index > -1) {
    prev = item;
    item = item->next;
  }
  if (item) {
    if (prev)
      prev->next = item->next;
    else
      m_itemsList = item->next;
    delete item;
  }
  return item != nullptr;
}


MachineConfItem * MachineConf::getItem(int index)
{
  auto item = m_itemsList;
  while (item && --index > -1)
    item = item->next;
  return item;
}


void MachineConf::addItem(MachineConfItem * item)
{
  if (item->isValid()) {
    if (m_itemsList) {
      auto prev = m_itemsList;
      while (prev->next)
        prev = prev->next;
      prev->next = item;
    } else
      m_itemsList = item;
  } else
    delete item;
}


void MachineConf::loadFromFile(FILE * file)
{
  cleanUp();
  auto item = new MachineConfItem;
  char tag[MAXTAGLENGTH + 1];
  char * value = (char*) malloc(MAXVALUELENGTH + 1);
  int ipos;
  char c = fgetc(file);
  while (!feof(file)) {
    while (!feof(file) && (isspace(c) || iscntrl(c))) {
      if (c == 0x0a) {
        // new line
        addItem(item);
        item = new MachineConfItem;
      }
      c = fgetc(file);
    }
    // get tag
    ipos = 0;
    while (!feof(file) && !isspace(c)) {
      if (ipos < MAXTAGLENGTH)
        tag[ipos++] = c;
      c = fgetc(file);
    }
    tag[ipos] = 0;
    // bypass spaces
    while (!feof(file) && isspace(c))
      c = fgetc(file);
    // get tag value
    ipos = 0;
    bool quote = false;
    while (!feof(file) && (quote || !isspace(c))) {
      if (c == '"')
        quote = !quote;
      else if (ipos < MAXVALUELENGTH)
        value[ipos++] = c;
      c = fgetc(file);
    }
    value[ipos] = 0;
    // assign tag
    if (!item->desc && strcmp("desc", tag) == 0)
      item->desc = strdup(value);
    else if (!item->dska && strcmp("dska", tag) == 0)
      item->dska = strdup(value);
    else if (!item->dskb && strcmp("dskb", tag) == 0)
      item->dskb = strdup(value);
    else if (!item->dskc && strcmp("dskc", tag) == 0)
      item->dskc = strdup(value);
  }
  addItem(item);
  free(value);
}


void MachineConf::saveTag(FILE * file, char const * tag, char const * value)
{
  fputs(tag, file);
  fputs(" \"", file);
  fputs(value, file);
  fputs("\" ", file);
}


void MachineConf::saveToFile(FILE * file)
{
  for (auto item = m_itemsList; item; item = item->next) {
    saveTag(file, "desc", item->desc);
    if (item->dska)
      saveTag(file, "dska", item->dska);
    if (item->dskb)
      saveTag(file, "dskb", item->dskb);
    if (item->dskc)
      saveTag(file, "dskc", item->dskc);
    fputs("\r\n", file);
  }
}
