/*
  Created by Fabrizio Di Vittorio (fdivitto2013@gmail.com) - <http://www.fabgl.com>
  Copyright (c) 2019 Fabrizio Di Vittorio.
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


#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>

#include "fabutils.h"
#include "vgacontroller.h"
#include "ps2controller.h"



namespace fabgl {



////////////////////////////////////////////////////////////////////////////////////////////
// TimeOut


TimeOut::TimeOut()
  : m_start(esp_timer_get_time())
{
}


bool TimeOut::expired(int valueMS)
{
  return valueMS > -1 && ((esp_timer_get_time() - m_start) / 1000) > valueMS;
}



////////////////////////////////////////////////////////////////////////////////////////////
// isqrt

// Integer square root by Halleck's method, with Legalize's speedup
int isqrt (int x)
{
  if (x < 1)
    return 0;
  int squaredbit = 0x40000000;
  int remainder = x;
  int root = 0;
  while (squaredbit > 0) {
    if (remainder >= (squaredbit | root)) {
      remainder -= (squaredbit | root);
      root >>= 1;
      root |= squaredbit;
    } else
      root >>= 1;
    squaredbit >>= 2;
  }
  return root;
}



////////////////////////////////////////////////////////////////////////////////////////////
// calcParity

bool calcParity(uint8_t v)
{
  v ^= v >> 4;
  v &= 0xf;
  return (0x6996 >> v) & 1;
}


////////////////////////////////////////////////////////////////////////////////////////////
// realloc32
// free32

// size must be a multiple of uint32_t (32 bit)
void * realloc32(void * ptr, size_t size)
{
  uint32_t * newBuffer = (uint32_t*) heap_caps_malloc(size, MALLOC_CAP_32BIT);
  if (ptr) {
    moveItems(newBuffer, (uint32_t*)ptr, size / sizeof(uint32_t));
    heap_caps_free(ptr);
  }
  return newBuffer;
}


void free32(void * ptr)
{
  heap_caps_free(ptr);
}



////////////////////////////////////////////////////////////////////////////////////////////
// suspendInterrupts
// resumeInterrupts

void suspendInterrupts()
{
  VGAController.suspendBackgroundPrimitiveExecution();
  PS2Controller.suspend();
}


void resumeInterrupts()
{
  PS2Controller.resume();
  VGAController.resumeBackgroundPrimitiveExecution();
}


////////////////////////////////////////////////////////////////////////////////////////////
// Sutherland-Cohen line clipping algorithm

static int clipLine_code(int x, int y, Rect const & clipRect)
{
  int code = 0;
  if (x < clipRect.X1)
    code = 1;
  else if (x > clipRect.X2)
    code = 2;
  if (y < clipRect.Y1)
    code |= 4;
  else if (y > clipRect.Y2)
    code |= 8;
  return code;
}

// false = line is out of clipping rect
// true = line intersects or is inside the clipping rect (x1, y1, x2, y2 are changed if checkOnly=false)
bool clipLine(int & x1, int & y1, int & x2, int & y2, Rect const & clipRect, bool checkOnly)
{
  int newX1 = x1;
  int newY1 = y1;
  int newX2 = x2;
  int newY2 = y2;
  int topLeftCode     = clipLine_code(newX1, newY1, clipRect);
  int bottomRightCode = clipLine_code(newX2, newY2, clipRect);
  while (true) {
    if ((topLeftCode == 0) && (bottomRightCode == 0)) {
      if (!checkOnly) {
        x1 = newX1;
        y1 = newY1;
        x2 = newX2;
        y2 = newY2;
      }
      return true;
    } else if (topLeftCode & bottomRightCode) {
      break;
    } else {
      int x = 0, y = 0;
      int ncode = topLeftCode != 0 ? topLeftCode : bottomRightCode;
      if (ncode & 8) {
        x = newX1 + (newX2 - newX1) * (clipRect.Y2 - newY1) / (newY2 - newY1);
        y = clipRect.Y2;
      } else if (ncode & 4) {
        x = newX1 + (newX2 - newX1) * (clipRect.Y1 - newY1) / (newY2 - newY1);
        y = clipRect.Y1;
      } else if (ncode & 2) {
        y = newY1 + (newY2 - newY1) * (clipRect.X2 - newX1) / (newX2 - newX1);
        x = clipRect.X2;
      } else if (ncode & 1) {
        y = newY1 + (newY2 - newY1) * (clipRect.X1 - newX1) / (newX2 - newX1);
        x = clipRect.X1;
      }
      if (ncode == topLeftCode) {
        newX1 = x;
        newY1 = y;
        topLeftCode = clipLine_code(newX1, newY1, clipRect);
      } else {
        newX2 = x;
        newY2 = y;
        bottomRightCode = clipLine_code(newX2, newY2, clipRect);
      }
    }
  }
  return false;
}


////////////////////////////////////////////////////////////////////////////////////////////
// removeRectangle
// remove "rectToRemove" from "mainRect", pushing remaining rectangles to "rects" stack

void removeRectangle(Stack<Rect> & rects, Rect const & mainRect, Rect const & rectToRemove)
{
  if (!mainRect.intersects(rectToRemove) || rectToRemove.contains(mainRect))
    return;

  // top rectangle
  if (mainRect.Y1 < rectToRemove.Y1)
    rects.push(Rect(mainRect.X1, mainRect.Y1, mainRect.X2, rectToRemove.Y1 - 1));

  // bottom rectangle
  if (mainRect.Y2 > rectToRemove.Y2)
    rects.push(Rect(mainRect.X1, rectToRemove.Y2 + 1, mainRect.X2, mainRect.Y2));

  // left rectangle
  if (mainRect.X1 < rectToRemove.X1)
    rects.push(Rect(mainRect.X1, tmax(rectToRemove.Y1, mainRect.Y1), rectToRemove.X1 - 1, tmin(rectToRemove.Y2, mainRect.Y2)));

  // right rectangle
  if (mainRect.X2 > rectToRemove.X2)
    rects.push(Rect(rectToRemove.X2 + 1, tmax(rectToRemove.Y1, mainRect.Y1), mainRect.X2, tmin(rectToRemove.Y2, mainRect.Y2)));
}



///////////////////////////////////////////////////////////////////////////////////
// StringList


StringList::StringList()
  : m_items(nullptr),
    m_selMap(nullptr),
    m_ownStrings(false),
    m_count(0),
    m_allocated(0)
{
}


StringList::~StringList()
{
  clear();
}


void StringList::clear()
{
  if (m_ownStrings) {
    for (int i = 0; i < m_count; ++i)
      free((void*) m_items[i]);
  }
  free32(m_items);
  free32(m_selMap);
  m_items  = nullptr;
  m_selMap = nullptr;
  m_count  = m_allocated = 0;
}


void StringList::copyFrom(StringList const & src)
{
  clear();
  m_count = src.m_count;
  checkAllocatedSpace(m_count);
  for (int i = 0; i < m_count; ++i) {
    m_items[i] = nullptr;
    set(i, src.m_items[i]);
  }
  deselectAll();
}


void StringList::checkAllocatedSpace(int requiredItems)
{
  if (m_allocated < requiredItems) {
    if (m_allocated == 0) {
      // first time allocates exact space
      m_allocated = requiredItems;
    } else {
      // next times allocate double
      while (m_allocated < requiredItems)
        m_allocated *= 2;
    }
    m_items  = (char const**) realloc32(m_items, m_allocated * sizeof(char const *));
    m_selMap = (uint32_t*) realloc32(m_selMap, (31 + m_allocated) / 32 * sizeof(uint32_t));
  }
}


void StringList::insert(int index, char const * str)
{
  ++m_count;
  checkAllocatedSpace(m_count);
  moveItems(m_items + index + 1, m_items + index, m_count - index - 1);
  m_items[index] = nullptr;
  set(index, str);
  deselectAll();
}


int StringList::append(char const * str)
{
  insert(m_count, str);
  return m_count - 1;
}


void StringList::set(int index, char const * str)
{
  if (m_ownStrings) {
    free((void*)m_items[index]);
    m_items[index] = (char const*) malloc(strlen(str) + 1);
    strcpy((char*)m_items[index], str);
  } else {
    m_items[index] = str;
  }
}


void StringList::remove(int index)
{
  if (m_ownStrings)
    free((void*)m_items[index]);
  moveItems(m_items + index, m_items + index + 1, m_count - index - 1);
  --m_count;
  deselectAll();
}


void StringList::takeStrings()
{
  if (!m_ownStrings) {
    m_ownStrings = true;
    // take existing strings
    for (int i = 0; i < m_count; ++i) {
      char const * str = m_items[i];
      m_items[i] = nullptr;
      set(i, str);
    }
  }
}


void StringList::deselectAll()
{
  for (int i = 0; i < (31 + m_count) / 32; ++i)
    m_selMap[i] = 0;
}


bool StringList::selected(int index)
{
  return m_selMap[index / 32] & (1 << (index % 32));
}


void StringList::select(int index, bool value)
{
  if (value)
    m_selMap[index / 32] |= 1 << (index % 32);
  else
    m_selMap[index / 32] &= ~(1 << (index % 32));
}



// StringList
///////////////////////////////////////////////////////////////////////////////////



///////////////////////////////////////////////////////////////////////////////////
// FileBrowser

FileBrowser::FileBrowser()
  : m_dir(nullptr),
    m_count(0),
    m_items(nullptr),
    m_sorted(true),
    m_includeHiddenFiles(false),
    m_namesStorage(nullptr)
{
}


FileBrowser::~FileBrowser()
{
  clear();
}


void FileBrowser::clear()
{
  free(m_items);
  m_items = nullptr;

  free(m_namesStorage);
  m_namesStorage = nullptr;

  m_count = 0;
}


// set absolute directory (full path must be specified)
void FileBrowser::setDirectory(const char * path)
{
  free(m_dir);
  m_dir = strdup(path);
  reload();
}


// set relative directory:
//   ".." : go to the parent directory
//   "dirname": go inside the specified sub directory
void FileBrowser::changeDirectory(const char * subdir)
{
  if (!m_dir)
    return;
  if (strcmp(subdir, "..") == 0) {
    // go to parent directory
    auto lastSlash = strrchr(m_dir, '/');
    if (lastSlash && lastSlash != m_dir) {
      *lastSlash = 0;
      reload();
    }
  } else {
    // go to sub directory
    int oldLen = strlen(m_dir);
    char * newDir = (char*) malloc(oldLen + 1 + strlen(subdir) + 1);  // m_dir + '/' + subdir + 0
    strcpy(newDir, m_dir);
    newDir[oldLen] = '/';
    strcpy(newDir + oldLen + 1, subdir);
    free(m_dir);
    m_dir = newDir;
    reload();
  }
}


int FileBrowser::countDirEntries(int * namesLength)
{
  int c = 0;
  *namesLength = 0;
  if (m_dir) {
    suspendInterrupts();
    auto dirp = opendir(m_dir);
    while (dirp) {
      auto dp = readdir(dirp);
      if (dp == NULL)
        break;
      if (strcmp(".", dp->d_name) && strcmp("..", dp->d_name) && dp->d_type != DT_UNKNOWN) {
        *namesLength += strlen(dp->d_name) + 1;
        ++c;
      }
    }
    closedir(dirp);
    resumeInterrupts();
  }
  return c;
}


bool FileBrowser::exists(char const * name)
{
  for (int i = 0; i < m_count; ++i)
    if (strcmp(name, m_items[i].name) == 0)
      return true;
  return false;
}


int DirComp(const void * i1, const void * i2)
{
  DirItem * d1 = (DirItem*)i1;
  DirItem * d2 = (DirItem*)i2;
  if (d1->isDir != d2->isDir) // directories first
    return d1->isDir ? -1 : +1;
  else
    return strcmp(d1->name, d2->name);
}


void FileBrowser::reload()
{
  clear();
  int namesAlloc;
  int c = countDirEntries(&namesAlloc);
  m_items = (DirItem*) malloc(sizeof(DirItem) * (c + 1));
  m_namesStorage = (char*) malloc(namesAlloc);
  char * sname = m_namesStorage;

  // first item is always ".."
  m_items[0].name  = "..";
  m_items[0].isDir = true;
  ++m_count;

  suspendInterrupts();
  auto dirp = opendir(m_dir);
  for (int i = 0; i < c; ++i) {
    auto dp = readdir(dirp);
    if (strcmp(".", dp->d_name) && strcmp("..", dp->d_name) && dp->d_type != DT_UNKNOWN) {
      DirItem * di = m_items + m_count;
      // check if this is a simulated directory (like in SPIFFS)
      auto slashPos = strchr(dp->d_name, '/');
      if (slashPos) {
        // yes, this is a simulated dir. Trunc and avoid to insert it twice
        int len = slashPos - dp->d_name;
        strncpy(sname, dp->d_name, len);
        sname[len] = 0;
        if (!exists(sname)) {
          di->name  = sname;
          di->isDir = true;
          sname += len + 1;
          ++m_count;
        }
      } else if (m_includeHiddenFiles || dp->d_name[0] != '.') {
        strcpy(sname, dp->d_name);
        di->name  = sname;
        di->isDir = (dp->d_type == DT_DIR);
        sname += strlen(sname) + 1;
        ++m_count;
      }
    }
  }
  closedir(dirp);
  resumeInterrupts();
  if (m_sorted)
    qsort(m_items, m_count, sizeof(DirItem), DirComp);
}


// note: for SPIFFS this creates an empty ".dirname" file. The SPIFFS path is detected when path starts with "/spiffs"
// "dirname" is not a path, just a directory name created inside "m_dir"
void FileBrowser::makeDirectory(char const * dirname)
{
  int dirnameLen = strlen(dirname);
  if (dirnameLen > 0) {
    suspendInterrupts();
    if (strncmp(m_dir, "/spiffs", 7) == 0) {
      // simulated directory, puts an hidden placeholder
      char fullpath[strlen(m_dir) + 3 + 2 * dirnameLen + 1];
      sprintf(fullpath, "%s/%s/.%s", m_dir, dirname, dirname);
      FILE * f = fopen(fullpath, "wb");
      fclose(f);
    } else {
      char fullpath[strlen(m_dir) + 1 + dirnameLen + 1];
      sprintf(fullpath, "%s/%s", m_dir, dirname);
      mkdir(fullpath, ACCESSPERMS);
    }
    resumeInterrupts();
  }
}


// removes a file or a directory (and all files inside it)
// The SPIFFS path is detected when path starts with "/spiffs"
// "name" is not a path, just a file or directory name inside "m_dir"
void FileBrowser::remove(char const * name)
{
  suspendInterrupts();

  char fullpath[strlen(m_dir) + 1 + strlen(name) + 1];
  sprintf(fullpath, "%s/%s", m_dir, name);
  int r = unlink(fullpath);

  if (r != 0) {
    // failed
    if (strncmp(m_dir, "/spiffs", 7) == 0) {
      // simulated directory
      // maybe this is a directory, remove ".dir" file
      char hidpath[strlen(m_dir) + 3 + 2 * strlen(name) + 1];
      sprintf(hidpath, "%s/%s/.%s", m_dir, name, name);
      unlink(hidpath);
      // maybe the directory contains files, remove all
      auto dirp = opendir(fullpath);
      while (dirp) {
        auto dp = readdir(dirp);
        if (dp == NULL)
          break;
        if (strcmp(".", dp->d_name) && strcmp("..", dp->d_name) && dp->d_type != DT_UNKNOWN) {
          char sfullpath[strlen(fullpath) + 1 + strlen(dp->d_name) + 1];
          sprintf(sfullpath, "%s/%s", fullpath, dp->d_name);
          unlink(sfullpath);
        }
      }
      closedir(dirp);
    }
  }

  resumeInterrupts();
}


// works only for files
void FileBrowser::rename(char const * oldName, char const * newName)
{
  suspendInterrupts();

  char oldfullpath[strlen(m_dir) + 1 + strlen(oldName) + 1];
  sprintf(oldfullpath, "%s/%s", m_dir, oldName);

  char newfullpath[strlen(m_dir) + 1 + strlen(newName) + 1];
  sprintf(newfullpath, "%s/%s", m_dir, newName);

  ::rename(oldfullpath, newfullpath);

  resumeInterrupts();
}


// concatenates current directory and specified name and store result into fullpath
// Specifying outPath=nullptr returns required length
int FileBrowser::getFullPath(char const * name, char * outPath, int maxlen)
{
  return outPath ? snprintf(outPath, maxlen, "%s/%s", m_dir, name) : snprintf(nullptr, 0, "%s/%s", m_dir, name) + 1;
}


// FileBrowser
///////////////////////////////////////////////////////////////////////////////////



}

