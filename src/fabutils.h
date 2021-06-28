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


/**
 * @file
 *
 * @brief This file contains some utility classes and functions
 *
 */


#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <driver/adc.h>
#include <esp_system.h>


namespace fabgl {


// manage IDF versioning
#ifdef ESP_IDF_VERSION
  #define FABGL_ESP_IDF_VERSION_VAL                      ESP_IDF_VERSION_VAL
  #define FABGL_ESP_IDF_VERSION                          ESP_IDF_VERSION
#else
  #define FABGL_ESP_IDF_VERSION_VAL(major, minor, patch) ((major << 16) | (minor << 8) | (patch))
  #define FABGL_ESP_IDF_VERSION                          FABGL_ESP_IDF_VERSION_VAL(0, 0, 0)
#endif



#define GPIO_UNUSED GPIO_NUM_MAX


//////////////////////////////////////////////////////////////////////////////////////////////
// PSRAM_HACK
// ESP32 Revision 1 has following bug: "When the CPU accesses external SRAM through cache, under certain conditions read and write errors occur"
// A workaround is done by the compiler, so whenever PSRAM is enabled the workaround is automatically applied (-mfix-esp32-psram-cache-issue compiler option).
// Unfortunately this workaround reduces performance, even when SRAM is not access, like in VGAXController interrupt handler. This is unacceptable for the interrupt routine.
// In order to confuse the compiler and prevent the workaround from being applied, a "nop" is added between load and store instructions (PSRAM_HACK).

#ifdef ARDUINO
  #ifdef BOARD_HAS_PSRAM
    #define FABGL_NEED_PSRAM_DISABLE_HACK
  #endif
#else
  #ifdef CONFIG_SPIRAM_SUPPORT
    #define FABGL_NEED_PSRAM_DISABLE_HACK
  #endif
#endif

#ifdef FABGL_NEED_PSRAM_DISABLE_HACK
  #define PSRAM_HACK asm(" nop")
#else
  #define PSRAM_HACK
#endif

// ESP32 PSRAM bug workaround (use when the library is NOT compiled with PSRAM hack enabled)
// Plase between a write and a read PSRAM operation (write->ASM_MEMW->read), not viceversa
#define ASM_MEMW asm(" MEMW");




//////////////////////////////////////////////////////////////////////////////////////////////


// Integer square root by Halleck's method, with Legalize's speedup
int isqrt (int x);


template <typename T>
const T & tmax(const T & a, const T & b)
{
  return (a < b) ? b : a;
}


constexpr auto imax = tmax<int>;


template <typename T>
const T & tmin(const T & a, const T & b)
{
  return !(b < a) ? a : b;
}


constexpr auto imin = tmin<int>;



template <typename T>
const T & tclamp(const T & v, const T & lo, const T & hi)
{
  return (v < lo ? lo : (v > hi ? hi : v));
}


constexpr auto iclamp = tclamp<int>;


template <typename T>
const T & twrap(const T & v, const T & lo, const T & hi)
{
  return (v < lo ? hi : (v > hi ? lo : v));
}


template <typename T>
void tswap(T & v1, T & v2)
{
  T t = v1;
  v1 = v2;
  v2 = t;
}


constexpr auto iswap = tswap<int>;


template <typename T>
T moveItems(T dest, T src, size_t n)
{
  T pd = dest;
  T ps = src;
  if (pd != ps) {
    if (ps < pd)
      for (pd += n, ps += n; n--;)
        *--pd = *--ps;
    else
      while (n--)
        *pd++ = *ps++;
  }
  return dest;
}


void rgb222_to_hsv(int R, int G, int B, double * h, double * s, double * v);


//////////////////////////////////////////////////////////////////////////////////////////////


/**
 * @brief Represents the coordinate of a point.
 *
 * Coordinates start from 0.
 */
struct Point {
  int16_t X;    /**< Horizontal coordinate */
  int16_t Y;    /**< Vertical coordinate */

  Point() : X(0), Y(0) { }
  Point(int X_, int Y_) : X(X_), Y(Y_) { }
  
  Point add(Point const & p) const { return Point(X + p.X, Y + p.Y); }
  Point sub(Point const & p) const { return Point(X - p.X, Y - p.Y); }
  Point neg() const                { return Point(-X, -Y); }
  bool operator==(Point const & r) { return X == r.X && Y == r.Y; }
  bool operator!=(Point const & r) { return X != r.X || Y != r.Y; }
} __attribute__ ((packed));


/**
 * @brief Represents a bidimensional size.
 */
struct Size {
  int16_t width;   /**< Horizontal size */
  int16_t height;  /**< Vertical size */

  Size() : width(0), height(0) { }
  Size(int width_, int height_) : width(width_), height(height_) { }
  bool operator==(Size const & r) { return width == r.width && height == r.height; }
  bool operator!=(Size const & r) { return width != r.width || height != r.height; }
} __attribute__ ((packed));



/**
 * @brief Represents a rectangle.
 *
 * Top and Left coordinates start from 0.
 */
struct Rect {
  int16_t X1;   /**< Horizontal top-left coordinate */
  int16_t Y1;   /**< Vertical top-left coordinate */
  int16_t X2;   /**< Horizontal bottom-right coordinate */
  int16_t Y2;   /**< Vertical bottom-right coordinate */

  Rect() : X1(0), Y1(0), X2(0), Y2(0) { }
  Rect(int X1_, int Y1_, int X2_, int Y2_) : X1(X1_), Y1(Y1_), X2(X2_), Y2(Y2_) { }
  Rect(Rect const & r) { X1 = r.X1; Y1 = r.Y1; X2 = r.X2; Y2 = r.Y2; }

  bool operator==(Rect const & r)                { return X1 == r.X1 && Y1 == r.Y1 && X2 == r.X2 && Y2 == r.Y2; }
  bool operator!=(Rect const & r)                { return X1 != r.X1 || Y1 != r.Y1 || X2 != r.X2 || Y2 != r.Y2; }
  Point pos() const                              { return Point(X1, Y1); }
  Size size() const                              { return Size(X2 - X1 + 1, Y2 - Y1 + 1); }
  int width() const                              { return X2 - X1 + 1; }
  int height() const                             { return Y2 - Y1 + 1; }
  Rect translate(int offsetX, int offsetY) const { return Rect(X1 + offsetX, Y1 + offsetY, X2 + offsetX, Y2 + offsetY); }
  Rect translate(Point const & offset) const     { return Rect(X1 + offset.X, Y1 + offset.Y, X2 + offset.X, Y2 + offset.Y); }
  Rect move(Point const & position) const        { return Rect(position.X, position.Y, position.X + width() - 1, position.Y + height() - 1); }
  Rect move(int x, int y) const                  { return Rect(x, y, x + width() - 1, y + height() - 1); }
  Rect shrink(int value) const                   { return Rect(X1 + value, Y1 + value, X2 - value, Y2 - value); }
  Rect hShrink(int value) const                  { return Rect(X1 + value, Y1, X2 - value, Y2); }
  Rect vShrink(int value) const                  { return Rect(X1, Y1 + value, X2, Y2 - value); }
  Rect resize(int width, int height) const       { return Rect(X1, Y1, X1 + width - 1, Y1 + height - 1); }
  Rect resize(Size size) const                   { return Rect(X1, Y1, X1 + size.width - 1, Y1 + size.height - 1); }
  Rect intersection(Rect const & rect) const;
  bool intersects(Rect const & rect) const       { return X1 <= rect.X2 && X2 >= rect.X1 && Y1 <= rect.Y2 && Y2 >= rect.Y1; }
  bool contains(Rect const & rect) const         { return (rect.X1 >= X1) && (rect.Y1 >= Y1) && (rect.X2 <= X2) && (rect.Y2 <= Y2); }
  bool contains(Point const & point) const       { return point.X >= X1 && point.Y >= Y1 && point.X <= X2 && point.Y <= Y2; }
  bool contains(int x, int y) const              { return x >= X1 && y >= Y1 && x <= X2 && y <= Y2; }
  Rect merge(Rect const & rect) const;
} __attribute__ ((packed));



/**
 * @brief Describes mouse buttons status.
 */
struct MouseButtons {
  uint8_t left   : 1;   /**< Contains 1 when left button is pressed. */
  uint8_t middle : 1;   /**< Contains 1 when middle button is pressed. */
  uint8_t right  : 1;   /**< Contains 1 when right button is pressed. */

  MouseButtons() : left(0), middle(0), right(0) { }
};



/**
 * @brief Describes mouse absolute position, scroll wheel delta and buttons status.
 */
struct MouseStatus {
  int16_t      X;           /**< Absolute horizontal mouse position. */
  int16_t      Y;           /**< Absolute vertical mouse position. */
  int8_t       wheelDelta;  /**< Scroll wheel delta. */
  MouseButtons buttons;     /**< Mouse buttons status. */

  MouseStatus() : X(0), Y(0), wheelDelta(0) { }
};



#define FONTINFOFLAGS_ITALIC    1
#define FONTINFOFLAGS_UNDERLINE 2
#define FONTINFODLAFS_STRIKEOUT 4
#define FONTINFOFLAGS_VARWIDTH  8


struct FontInfo {
  uint8_t  pointSize;
  uint8_t  width;   // used only for fixed width fonts (FONTINFOFLAGS_VARWIDTH = 0)
  uint8_t  height;
  uint8_t  ascent;
  uint8_t  inleading;
  uint8_t  exleading;
  uint8_t  flags;
  uint16_t weight;
  uint16_t charset;
  // when FONTINFOFLAGS_VARWIDTH = 0:
  //   data[] contains 256 items each one representing a single character
  // when FONTINFOFLAGS_VARWIDTH = 1:
  //   data[] contains 256 items each one representing a single character. First byte contains the
  //   character width. "chptr" is filled with an array of pointers to the single characters.
  uint8_t const *  data;
  uint32_t const * chptr;  // used only for variable width fonts (FONTINFOFLAGS_VARWIDTH = 1)
  uint16_t codepage;
};




///////////////////////////////////////////////////////////////////////////////////
// TimeOut


struct TimeOut {
  TimeOut();

  // -1 means "infinite", never times out
  bool expired(int valueMS);

private:
  int64_t m_start;
};



///////////////////////////////////////////////////////////////////////////////////
// Stack


template <typename T>
struct StackItem {
  StackItem * next;
  T           item;
  StackItem(StackItem * next_, T const & item_) : next(next_), item(item_) { }
};

template <typename T>
class Stack {
public:
  Stack() : m_items(nullptr) { }
  bool isEmpty() { return m_items == nullptr; }
  void push(T const & value) {
    m_items = new StackItem<T>(m_items, value);
  }
  T pop() {
    if (m_items) {
      StackItem<T> * iptr = m_items;
      m_items = iptr->next;
      T r = iptr->item;
      delete iptr;
      return r;
    } else
      return T();
  }
  int count() {
    int r = 0;
    for (auto i = m_items; i; i = i->next)
      ++r;
    return r;
  }
private:
  StackItem<T> * m_items;
};



///////////////////////////////////////////////////////////////////////////////////
// Delegate

template <typename ...Params>
struct Delegate {

  // empty constructor
  Delegate() : m_func(nullptr) {
  }

  // denied copy
  Delegate(const Delegate & c) = delete;

  // construct from lambda
  template <typename Func>
  Delegate(Func f) : Delegate() {
    *this = f;
  }

  ~Delegate() {
    cleanUp();
  }

  // assignment operator from Func
  template <typename Func>
  void operator=(Func f) {
    cleanUp();
    m_closure  = [] (void * func, const Params & ...params) -> void { (*(Func *)func)(params...); };
    m_func     = heap_caps_malloc(sizeof(Func), MALLOC_CAP_32BIT | MALLOC_CAP_INTERNAL);
    moveItems<uint32_t*>((uint32_t*)m_func, (uint32_t*)&f, sizeof(Func) / sizeof(uint32_t));
  }

  // denied assignment from Delegate
  void operator=(const Delegate&) = delete;

  void operator()(const Params & ...params) {
    if (m_func)
      m_closure(m_func, params...);
  }

private:

  void (*m_closure)(void * func, const Params & ...params);
  void * m_func;

  void cleanUp() {
    if (m_func)
      heap_caps_free(m_func);
  }
};



///////////////////////////////////////////////////////////////////////////////////
// StringList

class StringList {

public:
  StringList();
  ~StringList();
  int append(char const * str);
  int appendFmt(const char *format, ...);
  void append(char const * strlist[], int count);
  void appendSepList(char const * strlist, char separator);
  void insert(int index, char const * str);
  void set(int index, char const * str);
  void remove(int index);
  int count()                 { return m_count; }
  char const * get(int index) { return m_items[index]; }
  void clear();
  void takeStrings();
  void select(int index, bool value);
  void deselectAll();
  bool selected(int index);
  int getFirstSelected();
  void copyFrom(StringList const & src);
  void copySelectionMapFrom(StringList const & src);

private:
  void checkAllocatedSpace(int requiredItems);

  char const * * m_items;

  // each 32 bit word can select up to 32 items, one bit per item
  uint32_t *     m_selMap;

  // If true (default is false) all strings added (append/insert/set) are copied.
  // Strings will be released when no more used (destructor, clear(), etc...).
  // This flag is permanently switched to True by takeStrings() call.
  bool           m_ownStrings;

  uint16_t       m_count;     // actual items
  uint16_t       m_allocated; // allocated items

};


///////////////////////////////////////////////////////////////////////////////////
// LightMemoryPool
// Each allocated block starts with a two bytes header (int16_t). Bit 15 is allocation flag (0=free, 1=allocated).
// Bits 14..0 represent the block size.
// The maximum size of a block is 32767 bytes.
// free() just marks the block header as free.

class LightMemoryPool {
public:
  LightMemoryPool(int poolSize);
  ~LightMemoryPool();
  void * alloc(int size);
  void free(void * mem) { if (mem) markFree((uint8_t*)mem - m_mem - 2); }

  bool memCheck();
  int totFree();      // get total free memory
  int totAllocated(); // get total allocated memory
  int largestFree();

private:

  void mark(int pos, int16_t size, bool allocated);
  void markFree(int pos) { m_mem[pos + 1] &= 0x7f; }
  int16_t getSize(int pos);
  bool isFree(int pos);

  uint8_t * m_mem;
  int       m_poolSize;
};



///////////////////////////////////////////////////////////////////////////////////
// FileBrowser


/**
 * @brief FileBrowser item specificator
 */
struct DirItem {
  bool isDir;          /**< True if this is a directory, false if this is an ordinary file */
  char const * name;   /**< File or directory name */
};


/** \ingroup Enumerations
* @brief This enum defines drive types (SPIFFS or SD Card)
*/
enum class DriveType {
  None,    /**< Unspecified */
  SPIFFS,  /**< SPIFFS (Flash) */
  SDCard,  /**< SD Card */
};


/**
 * @brief FileBrowser allows basic file system operations (dir, mkdir, remove and rename)
 *
 * Note: SPIFFS filenames (including fullpath) cannot exceed SPIFFS_OBJ_NAME_LEN (32 bytes) size.
 */
class FileBrowser {
public:

  FileBrowser();
  ~FileBrowser();

  /**
   * @brief Sets absolute directory path
   *
   * @param path Absolute directory path (ie "/spiffs")
   *
   * @return True on success
   */
  bool setDirectory(const char * path);

  /**
   * @brief Sets relative directory path
   *
   * @param subdir Relative directory path (ie "subdir")
   */
  void changeDirectory(const char * subdir);

  /**
   * @brief Reloads directory content
   *
   * @return True on successful reload.
   */
  bool reload();

  /**
   * @brief Determines absolute path of current directory
   *
   * @return Absolute path of current directory
   */
  char const * directory() { return m_dir; }

  /**
   * @brief Determines number of files in current directory
   *
   * @return Number of directory files, included parent ".."
   */
  int count() { return m_count; }

  /**
   * @brief Gets file/directory at index
   *
   * @param index File or directory index. 0 = is always parent directory
   *
   * @return File or directory specificator
   */
  DirItem const * get(int index) { return m_items + index; }

  /**
   * @brief Determines if a file or directory exists
   *
   * @param name Relative file or directory name
   * @param caseSensitive If true (default) comparison is case sensitive
   *
   * @return True if the file exists
   */
  bool exists(char const * name, bool caseSensitive = true);

  /**
   * @brief Determines file size
   *
   * @param name Relative file name
   *
   * @return File size in bytes
   */
  size_t fileSize(char const * name);

  /**
   * @brief Gets file creation date and time
   *
   * @param name Relative file name
   * @param year Pointer to year
   * @param month Pointer to month (1..12)
   * @param day Pointer to day (1..31)
   * @param hour Pointer to hour (0..23)
   * @param minutes Pointer to minutes (0..59)
   * @param seconds Pointer to seconds (0..59, or 60)
   *
   * @return True on success
   */
  bool fileCreationDate(char const * name, int * year, int * month, int * day, int * hour, int * minutes, int * seconds);

  /**
   * @brief Gets file update date and time
   *
   * @param name Relative file name
   * @param year Pointer to year
   * @param month Pointer to month (1..12)
   * @param day Pointer to day (1..31)
   * @param hour Pointer to hour (0..23)
   * @param minutes Pointer to minutes (0..59)
   * @param seconds Pointer to seconds (0..59, or 60)
   *
   * @return True on success
   */
  bool fileUpdateDate(char const * name, int * year, int * month, int * day, int * hour, int * minutes, int * seconds);

  /**
   * @brief Gets file access date and time
   *
   * @param name Relative file name
   * @param year Pointer to year
   * @param month Pointer to month (1..12)
   * @param day Pointer to day (1..31)
   * @param hour Pointer to hour (0..23)
   * @param minutes Pointer to minutes (0..59)
   * @param seconds Pointer to seconds (0..59, or 60)
   *
   * @return True on success
   */
  bool fileAccessDate(char const * name, int * year, int * month, int * day, int * hour, int * minutes, int * seconds);

  /**
   * @brief Determines if the items are sorted
   *
   * @param value If true items will be sorted in ascending order (directories first)
   */
  void setSorted(bool value);

  void setIncludeHiddenFiles(bool value) { m_includeHiddenFiles = value; }

  /**
   * @brief Creates a directory
   *
   * @param dirname Relative directory name
   */
  void makeDirectory(char const * dirname);

  /**
   * @brief Removes a file or directory
   *
   * If filesystem is SPIFFS then this method can also remove a non empty directory
   *
   * @param name Relative file or directory name
   */
  void remove(char const * name);

  /**
   * @brief Renames a file
   *
   * @param oldName Relative old file name
   * @param newName Relative new file name
   */
  void rename(char const * oldName, char const * newName);

  /**
   * @brief Truncates a file to the specified size
   *
   * @param name Relative file name
   * @param size New size in bytes
   *
   * @return Returns true on success.
   */
  bool truncate(char const * name, size_t size);

  /**
   * @brief Creates a random temporary filename, with absolute path
   *
   * @return Pointer to temporary string. It must be released using free().
   */
  char * createTempFilename();

  /**
   * @brief Composes a full file path given a relative name
   *
   * @param name Relative file name
   * @param outPath Where to place the full path. This can be NULL (used to calculate required buffer size).
   * @param maxlen Maximum size of outPath string. This can be 0 when outPath is NULL.
   *
   * @return Required outPath size.
   */
  int getFullPath(char const * name, char * outPath = nullptr, int maxlen = 0);

  /**
   * @brief Opens a file from current directory
   *
   * @param filename Name of file to open
   * @param mode Open mode (like the fopen mode)
   *
   * @return Same result of fopen C function
   */
  FILE * openFile(char const * filename, char const * mode);

  /**
   * @brief Returns the drive type of current directory
   *
   * @return Drive type.
   */
  DriveType getCurrentDriveType();

  /**
   * @brief Returns the drive type of specified path
   *
   * @param path Path
   *
   * @return Drive type.
   */
  static DriveType getDriveType(char const * path);

  /**
   * @brief Formats SPIFFS or SD Card
   *
   * The filesystem must be already mounted before calling Format().
   * The formatted filesystem results unmounted at the end of formatting, so remount is necessary.
   *
   * @param driveType Type of support (SPI-Flash or SD Card).
   * @param drive Drive number (Can be 0 when only one filesystem is mounted at the time)
   *
   * @return Returns True on success.
   *
   * Example:
   *
   *     // Format SPIFFS and remount it
   *     FileBrowser::format(fabgl::DriveType::SPIFFS, 0);
   *     FileBrowser::mountSPIFFS(false, "/spiffs");
   */
  static bool format(DriveType driveType, int drive);

  /**
   * @brief Mounts filesystem on SD Card
   *
   * @param formatOnFail Formats SD Card when it cannot be mounted.
   * @param mountPath Mount directory (ex. "/sdcard").
   * @param maxFiles Number of files that can be open at the time (default 4).
   * @param allocationUnitSize Allocation unit size (default 16K).
   * @param MISO Pin for MISO signal (default 16 for WROOM-32, 2 for PICO-D4).
   * @param MOSI Pin for MOSI signal (default 17 for WROOM-32, 12 for PICO-D4).
   * @param CLK Pin for CLK signal (default 14).
   * @param CS Pin for CS signal (default 13).
   *
   * @return Returns True on success.
   *
   * Example:
   *
   *     // Mount SD Card
   *     FileBrowser::mountSDCard(false, "/sdcard");
   */
  static bool mountSDCard(bool formatOnFail, char const * mountPath, size_t maxFiles = 4, int allocationUnitSize = 16 * 1024, int MISO = 16, int MOSI = 17, int CLK = 14, int CS = 13);

  /**
   * @brief Remounts SDCard filesystem, using the same parameters
   *
   * @return Returns True on success.
   */
  static bool remountSDCard();

  /**
   * @brief Unmounts filesystem on SD Card
   */
  static void unmountSDCard();

  /**
   * @brief Mounts filesystem on SPIFFS (Flash)
   *
   * @param formatOnFail Formats SD Card when it cannot be mounted.
   * @param mountPath Mount directory (ex. "/spiffs").
   * @param maxFiles Number of files that can be open at the time (default 4).
   *
   * @return Returns True on success.
   *
   * Example:
   *
   *     // Mount SD Card
   *     FileBrowser::mountSPIFFS(false, "/spiffs");
   */
  static bool mountSPIFFS(bool formatOnFail, char const * mountPath, size_t maxFiles = 4);

  /**
   * @brief Remounts SPIFFS filesystem, using the same parameters
   *
   * @return Returns True on success.
   */
  static bool remountSPIFFS();

  /**
   * @brief Unmounts filesystem on SPIFFS (Flash)
   */
  static void unmountSPIFFS();

  /**
   * @brief Gets total and free space on a filesystem
   *
   * @param driveType Type of support (SPI-Flash or SD Card).
   * @param drive Drive number (Can be 0 when only one filesystem is mounted at the time).
   * @param total Total space on the filesystem in bytes.
   * @param used Used space on the filesystem in bytes.
   *
   * @return Returns True on success.
   *
   * Example:
   *
   *     // print used and free space on SPIFFS (Flash)
   *     int64_t total, used;
   *     FileBrowser::getFSInfo(fabgl::DriveType::SPIFFS, 0, &total, &used);
   *     Serial.printf("%lld KiB used, %lld KiB free", used / 1024, (total - used) / 1024);
   *
   *     // print used and free space on SD Card
   *     int64_t total, used;
   *     FileBrowser::getFSInfo(fabgl::DriveType::SDCard, 0, &total, &used);
   *     Serial.printf("%lld KiB used, %lld KiB free", used / 1024, (total - used) / 1024);
   */
  static bool getFSInfo(DriveType driveType, int drive, int64_t * total, int64_t * used);

private:

  void clear();
  int countDirEntries(int * namesLength);

  // SPIFFS static infos
  static bool         s_SPIFFSMounted;
  static char const * s_SPIFFSMountPath;
  static size_t       s_SPIFFSMaxFiles;

  // SD Card static infos
  static bool         s_SDCardMounted;
  static char const * s_SDCardMountPath;
  static size_t       s_SDCardMaxFiles;
  static int          s_SDCardAllocationUnitSize;
  static int8_t       s_SDCardMISO;
  static int8_t       s_SDCardMOSI;
  static int8_t       s_SDCardCLK;
  static int8_t       s_SDCardCS;

  char *    m_dir;
  int       m_count;
  DirItem * m_items;
  bool      m_sorted;
  bool      m_includeHiddenFiles;
  char *    m_namesStorage;
};




///////////////////////////////////////////////////////////////////////////////////



bool clipLine(int & x1, int & y1, int & x2, int & y2, Rect const & clipRect, bool checkOnly);


void removeRectangle(Stack<Rect> & rects, Rect const & mainRect, Rect const & rectToRemove);


bool calcParity(uint8_t v);

// why these? this is like heap_caps_malloc with MALLOC_CAP_32BIT. Unfortunately
// heap_caps_malloc crashes, so we need this workaround.
void * realloc32(void * ptr, size_t size);
void free32(void * ptr);


inline gpio_num_t int2gpio(int gpio)
{
  return gpio == -1 ? GPIO_UNUSED : (gpio_num_t)gpio;
}


// converts 0..9 -> '0'..'9', 10..15 -> 'a'..'f'
inline char digit2hex(int digit)
{
  return digit < 10 ? '0' + digit : 'a' + digit - 10;
}


// converts '0'..'9' -> 0..9, 'a'..'f' -> 10..15
inline int hex2digit(char hex)
{
  return hex < 'a' ? hex - '0' : hex - 'a' + 10;
}


// milliseconds to FreeRTOS ticks.
// ms = -1 => maximum delay (portMAX_DELAY)
uint32_t msToTicks(int ms);


/** \ingroup Enumerations
* @brief This enum defines ESP32 module types (packages)
*/
enum class ChipPackage {
  Unknown,
  ESP32D0WDQ6,  /**< Unknown */
  ESP32D0WDQ5,  /**< ESP32D0WDQ5 */
  ESP32D2WDQ5,  /**< ESP32D2WDQ5 */
  ESP32PICOD4,  /**< ESP32PICOD4 */
};


ChipPackage getChipPackage();

inline __attribute__((always_inline)) uint32_t getCycleCount() {
  uint32_t ccount;
  __asm__ __volatile__(
    "esync \n\t"
    "rsr %0, ccount \n\t"
    : "=a" (ccount)
  );
  return ccount;
}

/**
 * @brief Replaces path separators
 *
 * @param path Path with separators to replace
 * @param newSep New separator character
 */
void replacePathSep(char * path, char newSep);


/**
 * @brief Composes UART configuration word
 *
 * @param parity Parity. 0 = none, 1 = even, 2 = odd
 * @param dataLength Data word length. 0 = 5 bits, 1 = 6 bits, 2 = 7 bits, 3 = 8 bits
 * @param stopBits Number of stop bits. 1 = 1 bit, 2 = 1.5 bits, 3 = 2 bits
 */
inline uint32_t UARTConf(int parity, int dataLength, int stopBits)
{
  uint32_t w = 0x8000000 | (dataLength << 2) | (stopBits << 4);
  if (parity)
    w |= (parity == 1 ? 0b10 : 0b11);
  return w;
}


adc1_channel_t ADC1_GPIO2Channel(gpio_num_t gpio);


void esp_intr_alloc_pinnedToCore(int source, int flags, intr_handler_t handler, void * arg, intr_handle_t * ret_handle, int core);


// mode: GPIO_MODE_DISABLE,
//       GPIO_MODE_INPUT,
//       GPIO_MODE_OUTPUT,
//       GPIO_MODE_OUTPUT_OD (open drain),
//       GPIO_MODE_INPUT_OUTPUT_OD (open drain),
//       GPIO_MODE_INPUT_OUTPUT
void configureGPIO(gpio_num_t gpio, gpio_mode_t mode);


uint32_t getApbFrequency();

uint32_t getCPUFrequencyMHz();


///////////////////////////////////////////////////////////////////////////////////
// AutoSemaphore

struct AutoSemaphore {
  AutoSemaphore(SemaphoreHandle_t mutex) : m_mutex(mutex) { xSemaphoreTake(m_mutex, portMAX_DELAY); }
  ~AutoSemaphore()                                        { xSemaphoreGive(m_mutex); }
private:
  SemaphoreHandle_t m_mutex;
};



///////////////////////////////////////////////////////////////////////////////////
// CoreUsage

/**
 * @brief This class helps to choice a core for intensive processing tasks
 */
struct CoreUsage {

  static int busiestCore()                 { return s_busiestCore; }
  static int quietCore()                   { return s_busiestCore != -1 ? s_busiestCore ^ 1 : -1; }
  static void setBusiestCore(int core)     { s_busiestCore = core; }

  private:
    static int s_busiestCore;  // 0 = core 0, 1 = core 1 (default is FABGLIB_VIDEO_CPUINTENSIVE_TASKS_CORE)
};


///////////////////////////////////////////////////////////////////////////////////


/** \ingroup Enumerations
 * @brief Represents each possible real or derived (SHIFT + real) key.
 */
enum VirtualKey {
  VK_NONE,            /**< No character (marks the first virtual key) */

  VK_SPACE,           /**< Space */
  
  VK_0,               /**< Number 0 */
  VK_1,               /**< Number 1 */
  VK_2,               /**< Number 2 */
  VK_3,               /**< Number 3 */
  VK_4,               /**< Number 4 */
  VK_5,               /**< Number 5 */
  VK_6,               /**< Number 6 */
  VK_7,               /**< Number 7 */
  VK_8,               /**< Number 8 */
  VK_9,               /**< Number 9 */
  VK_KP_0,            /**< Keypad number 0 */
  VK_KP_1,            /**< Keypad number 1 */
  VK_KP_2,            /**< Keypad number 2 */
  VK_KP_3,            /**< Keypad number 3 */
  VK_KP_4,            /**< Keypad number 4 */
  VK_KP_5,            /**< Keypad number 5 */
  VK_KP_6,            /**< Keypad number 6 */
  VK_KP_7,            /**< Keypad number 7 */
  VK_KP_8,            /**< Keypad number 8 */
  VK_KP_9,            /**< Keypad number 9 */

  VK_a,               /**< Lower case letter 'a' */
  VK_b,               /**< Lower case letter 'b' */
  VK_c,               /**< Lower case letter 'c' */
  VK_d,               /**< Lower case letter 'd' */
  VK_e,               /**< Lower case letter 'e' */
  VK_f,               /**< Lower case letter 'f' */
  VK_g,               /**< Lower case letter 'g' */
  VK_h,               /**< Lower case letter 'h' */
  VK_i,               /**< Lower case letter 'i' */
  VK_j,               /**< Lower case letter 'j' */
  VK_k,               /**< Lower case letter 'k' */
  VK_l,               /**< Lower case letter 'l' */
  VK_m,               /**< Lower case letter 'm' */
  VK_n,               /**< Lower case letter 'n' */
  VK_o,               /**< Lower case letter 'o' */
  VK_p,               /**< Lower case letter 'p' */
  VK_q,               /**< Lower case letter 'q' */
  VK_r,               /**< Lower case letter 'r' */
  VK_s,               /**< Lower case letter 's' */
  VK_t,               /**< Lower case letter 't' */
  VK_u,               /**< Lower case letter 'u' */
  VK_v,               /**< Lower case letter 'v' */
  VK_w,               /**< Lower case letter 'w' */
  VK_x,               /**< Lower case letter 'x' */
  VK_y,               /**< Lower case letter 'y' */
  VK_z,               /**< Lower case letter 'z' */
  VK_A,               /**< Upper case letter 'A' */
  VK_B,               /**< Upper case letter 'B' */
  VK_C,               /**< Upper case letter 'C' */
  VK_D,               /**< Upper case letter 'D' */
  VK_E,               /**< Upper case letter 'E' */
  VK_F,               /**< Upper case letter 'F' */
  VK_G,               /**< Upper case letter 'G' */
  VK_H,               /**< Upper case letter 'H' */
  VK_I,               /**< Upper case letter 'I' */
  VK_J,               /**< Upper case letter 'J' */
  VK_K,               /**< Upper case letter 'K' */
  VK_L,               /**< Upper case letter 'L' */
  VK_M,               /**< Upper case letter 'M' */
  VK_N,               /**< Upper case letter 'N' */
  VK_O,               /**< Upper case letter 'O' */
  VK_P,               /**< Upper case letter 'P' */
  VK_Q,               /**< Upper case letter 'Q' */
  VK_R,               /**< Upper case letter 'R' */
  VK_S,               /**< Upper case letter 'S' */
  VK_T,               /**< Upper case letter 'T' */
  VK_U,               /**< Upper case letter 'U' */
  VK_V,               /**< Upper case letter 'V' */
  VK_W,               /**< Upper case letter 'W' */
  VK_X,               /**< Upper case letter 'X' */
  VK_Y,               /**< Upper case letter 'Y' */
  VK_Z,               /**< Upper case letter 'Z' */

  VK_GRAVEACCENT,     /**< Grave accent: ` */
  VK_ACUTEACCENT,     /**< Acute accent: ´ */
  VK_QUOTE,           /**< Quote: ' */
  VK_QUOTEDBL,        /**< Double quote: " */
  VK_EQUALS,          /**< Equals: = */
  VK_MINUS,           /**< Minus: - */
  VK_KP_MINUS,        /**< Keypad minus: - */
  VK_PLUS,            /**< Plus: + */
  VK_KP_PLUS,         /**< Keypad plus: + */
  VK_KP_MULTIPLY,     /**< Keypad multiply: * */
  VK_ASTERISK,        /**< Asterisk: * */
  VK_BACKSLASH,       /**< Backslash: \ */
  VK_KP_DIVIDE,       /**< Keypad divide: / */
  VK_SLASH,           /**< Slash: / */
  VK_KP_PERIOD,       /**< Keypad period: . */
  VK_PERIOD,          /**< Period: . */
  VK_COLON,           /**< Colon: : */
  VK_COMMA,           /**< Comma: , */
  VK_SEMICOLON,       /**< Semicolon: ; */
  VK_AMPERSAND,       /**< Ampersand: & */
  VK_VERTICALBAR,     /**< Vertical bar: | */
  VK_HASH,            /**< Hash: # */
  VK_AT,              /**< At: @ */
  VK_CARET,           /**< Caret: ^ */
  VK_DOLLAR,          /**< Dollar: $ */
  VK_POUND,           /**< Pound: £ */
  VK_EURO,            /**< Euro: € */
  VK_PERCENT,         /**< Percent: % */
  VK_EXCLAIM,         /**< Exclamation mark: ! */
  VK_QUESTION,        /**< Question mark: ? */
  VK_LEFTBRACE,       /**< Left brace: { */
  VK_RIGHTBRACE,      /**< Right brace: } */
  VK_LEFTBRACKET,     /**< Left bracket: [ */
  VK_RIGHTBRACKET,    /**< Right bracket: ] */
  VK_LEFTPAREN,       /**< Left parenthesis: ( */
  VK_RIGHTPAREN,      /**< Right parenthesis: ) */
  VK_LESS,            /**< Less: < */
  VK_GREATER,         /**< Greater: > */
  VK_UNDERSCORE,      /**< Underscore: _ */
  VK_DEGREE,          /**< Degree: ° */
  VK_SECTION,         /**< Section: § */
  VK_TILDE,           /**< Tilde: ~ */
  VK_NEGATION,        /**< Negation: ¬ */

  VK_LSHIFT,          /**< Left SHIFT */
  VK_RSHIFT,          /**< Right SHIFT */
  VK_LALT,            /**< Left ALT */
  VK_RALT,            /**< Right ALT */
  VK_LCTRL,           /**< Left CTRL */
  VK_RCTRL,           /**< Right CTRL */
  VK_LGUI,            /**< Left GUI */
  VK_RGUI,            /**< Right GUI */

  VK_ESCAPE,          /**< ESC */

  VK_PRINTSCREEN,     /**< PRINTSCREEN */
  VK_SYSREQ,          /**< SYSREQ */

  VK_INSERT,          /**< INS */
  VK_KP_INSERT,       /**< Keypad INS */
  VK_DELETE,          /**< DEL */
  VK_KP_DELETE,       /**< Keypad DEL */
  VK_BACKSPACE,       /**< Backspace */
  VK_HOME,            /**< HOME */
  VK_KP_HOME,         /**< Keypad HOME */
  VK_END,             /**< END */
  VK_KP_END,          /**< Keypad END */
  VK_PAUSE,           /**< PAUSE */
  VK_BREAK,           /**< CTRL + PAUSE */
  VK_SCROLLLOCK,      /**< SCROLLLOCK */
  VK_NUMLOCK,         /**< NUMLOCK */
  VK_CAPSLOCK,        /**< CAPSLOCK */
  VK_TAB,             /**< TAB */
  VK_RETURN,          /**< RETURN */
  VK_KP_ENTER,        /**< Keypad ENTER */
  VK_APPLICATION,     /**< APPLICATION / MENU key */
  VK_PAGEUP,          /**< PAGEUP */
  VK_KP_PAGEUP,       /**< Keypad PAGEUP */
  VK_PAGEDOWN,        /**< PAGEDOWN */
  VK_KP_PAGEDOWN,     /**< Keypad PAGEDOWN */
  VK_UP,              /**< Cursor UP */
  VK_KP_UP,           /**< Keypad cursor UP  */
  VK_DOWN,            /**< Cursor DOWN */
  VK_KP_DOWN,         /**< Keypad cursor DOWN */
  VK_LEFT,            /**< Cursor LEFT */
  VK_KP_LEFT,         /**< Keypad cursor LEFT */
  VK_RIGHT,           /**< Cursor RIGHT */
  VK_KP_RIGHT,        /**< Keypad cursor RIGHT */
  VK_KP_CENTER,       /**< Keypad CENTER key */

  VK_F1,              /**< F1 function key */
  VK_F2,              /**< F2 function key */
  VK_F3,              /**< F3 function key */
  VK_F4,              /**< F4 function key */
  VK_F5,              /**< F5 function key */
  VK_F6,              /**< F6 function key */
  VK_F7,              /**< F7 function key */
  VK_F8,              /**< F8 function key */
  VK_F9,              /**< F9 function key */
  VK_F10,             /**< F10 function key */
  VK_F11,             /**< F11 function key */
  VK_F12,             /**< F12 function key */
  
  VK_GRAVE_a,         /**< Grave a: à */
  VK_GRAVE_e,         /**< Grave e: è */
  VK_GRAVE_i,         /**< Grave i: ì */
  VK_GRAVE_o,         /**< Grave o: ò */
  VK_GRAVE_u,         /**< Grave u: ù */
  VK_GRAVE_y,         /**< Grave y: ỳ */

  VK_ACUTE_a,         /**< Acute a: á */
  VK_ACUTE_e,         /**< Acute e: é */
  VK_ACUTE_i,         /**< Acute i: í */
  VK_ACUTE_o,         /**< Acute o: ó */
  VK_ACUTE_u,         /**< Acute u: ú */
  VK_ACUTE_y,         /**< Acute y: ý */

  VK_GRAVE_A,		      /**< Grave A: À */
  VK_GRAVE_E,		      /**< Grave E: È */
  VK_GRAVE_I,		      /**< Grave I: Ì */
  VK_GRAVE_O,		      /**< Grave O: Ò */
  VK_GRAVE_U,		      /**< Grave U: Ù */
  VK_GRAVE_Y,         /**< Grave Y: Ỳ */

  VK_ACUTE_A,		      /**< Acute A: Á */
  VK_ACUTE_E,		      /**< Acute E: É */
  VK_ACUTE_I,		      /**< Acute I: Í */
  VK_ACUTE_O,		      /**< Acute O: Ó */
  VK_ACUTE_U,		      /**< Acute U: Ú */
  VK_ACUTE_Y,         /**< Acute Y: Ý */

  VK_UMLAUT_a,        /**< Diaeresis a: ä */
  VK_UMLAUT_e,        /**< Diaeresis e: ë */
  VK_UMLAUT_i,        /**< Diaeresis i: ï */
  VK_UMLAUT_o,        /**< Diaeresis o: ö */
  VK_UMLAUT_u,        /**< Diaeresis u: ü */
  VK_UMLAUT_y,        /**< Diaeresis y: ÿ */

  VK_UMLAUT_A,        /**< Diaeresis A: Ä */
  VK_UMLAUT_E,        /**< Diaeresis E: Ë */
  VK_UMLAUT_I,        /**< Diaeresis I: Ï */
  VK_UMLAUT_O,        /**< Diaeresis O: Ö */
  VK_UMLAUT_U,        /**< Diaeresis U: Ü */
  VK_UMLAUT_Y,        /**< Diaeresis Y: Ÿ */

  VK_CARET_a,		      /**< Caret a: â */
  VK_CARET_e,		      /**< Caret e: ê */
  VK_CARET_i,		      /**< Caret i: î */
  VK_CARET_o,		      /**< Caret o: ô */
  VK_CARET_u,		      /**< Caret u: û */
  VK_CARET_y,         /**< Caret y: ŷ */

  VK_CARET_A,		      /**< Caret A: Â */
  VK_CARET_E,		      /**< Caret E: Ê */
  VK_CARET_I,		      /**< Caret I: Î */
  VK_CARET_O,		      /**< Caret O: Ô */
  VK_CARET_U,		      /**< Caret U: Û */
  VK_CARET_Y,         /**< Caret Y: Ŷ */

  VK_CEDILLA_c,       /**< Cedilla c: ç */
  VK_CEDILLA_C,       /**< Cedilla C: Ç */
  
  VK_TILDE_a,         /**< Lower case tilde a: ã */
  VK_TILDE_o,         /**< Lower case tilde o: õ */
  VK_TILDE_n,		      /**< Lower case tilde n: ñ */

  VK_TILDE_A,         /**< Upper case tilde A: Ã */
  VK_TILDE_O,         /**< Upper case tilde O: Õ */
  VK_TILDE_N,		      /**< Upper case tilde N: Ñ */

  VK_UPPER_a,		      /**< primera: a */
  VK_ESZETT,          /**< Eszett: ß */
  VK_EXCLAIM_INV,     /**< Inverted exclamation mark: ! */
  VK_QUESTION_INV,    /**< Inverted question mark : ? */
  VK_INTERPUNCT,	    /**< Interpunct : · */
  VK_DIAERESIS,	  	  /**< Diaeresis  : ¨ */
  VK_SQUARE,          /**< Square     : ² */
  VK_CURRENCY,        /**< Currency   : ¤ */
  VK_MU,              /**< Mu         : µ */

  VK_ASCII,           /**< Specifies an ASCII code - used when virtual key is embedded in VirtualKeyItem structure and VirtualKeyItem.ASCII is valid */
  VK_LAST,            // marks the last virtual key

};


/**
 * @brief A struct which contains a virtual key, key state and associated scan code
 */
struct VirtualKeyItem {
  VirtualKey vk;              /**< Virtual key */
  uint8_t    down;            /**< 0 = up, 1 = down */
  uint8_t    scancode[8];     /**< Keyboard scancode. Ends with zero if length is <8, otherwise gets the entire length (like PAUSE, which is 8 bytes) */
  uint8_t    ASCII;           /**< ASCII value (0 = if it isn't possible to translate from virtual key) */
  uint8_t    CTRL       : 1;  /**< CTRL key state at the time of this virtual key event */
  uint8_t    LALT       : 1;  /**< LEFT ALT key state at the time of this virtual key event */
  uint8_t    RALT       : 1;  /**< RIGHT ALT key state at the time of this virtual key event */
  uint8_t    SHIFT      : 1;  /**< SHIFT key state at the time of this virtual key event */
  uint8_t    GUI        : 1;  /**< GUI key state at the time of this virtual key event */
  uint8_t    CAPSLOCK   : 1;  /**< CAPSLOCK key state at the time of this virtual key event */
  uint8_t    NUMLOCK    : 1;  /**< NUMLOCK key state at the time of this virtual key event */
  uint8_t    SCROLLLOCK : 1;  /**< SCROLLLOCK key state at the time of this virtual key event */
};



///////////////////////////////////////////////////////////////////////////////////
// Virtual keys helpers

inline bool isSHIFT(VirtualKey value)
{
  return value == VK_LSHIFT || value == VK_RSHIFT;
}


inline bool isALT(VirtualKey value)
{
  return value == VK_LALT || value == VK_RALT;
}


inline bool isCTRL(VirtualKey value)
{
  return value == VK_LCTRL || value == VK_RCTRL;
}


inline bool isGUI(VirtualKey value)
{
  return value == VK_LGUI || value == VK_RGUI;  
}



///////////////////////////////////////////////////////////////////////////////////
// ASCII control characters

#define ASCII_NUL   0x00   // Null
#define ASCII_SOH   0x01   // Start of Heading
#define ASCII_CTRLA 0x01   // CTRL-A
#define ASCII_STX   0x02   // Start of Text
#define ASCII_CTRLB 0x02   // CTRL-B
#define ASCII_ETX   0x03   // End Of Text
#define ASCII_CTRLC 0x03   // CTRL-C
#define ASCII_EOT   0x04   // End Of Transmission
#define ASCII_CTRLD 0x04   // CTRL-D
#define ASCII_ENQ   0x05   // Enquiry
#define ASCII_CTRLE 0x05   // CTRL-E
#define ASCII_ACK   0x06   // Acknowledge
#define ASCII_CTRLF 0x06   // CTRL-F
#define ASCII_BEL   0x07   // Bell
#define ASCII_CTRLG 0x07   // CTRL-G
#define ASCII_BS    0x08   // Backspace
#define ASCII_CTRLH 0x08   // CTRL-H
#define ASCII_HT    0x09   // Horizontal Tab
#define ASCII_TAB   0x09   // Horizontal Tab
#define ASCII_CTRLI 0x09   // CTRL-I
#define ASCII_LF    0x0A   // Line Feed
#define ASCII_CTRLJ 0x0A   // CTRL-J
#define ASCII_VT    0x0B   // Vertical Tab
#define ASCII_CTRLK 0x0B   // CTRL-K
#define ASCII_FF    0x0C   // Form Feed
#define ASCII_CTRLL 0x0C   // CTRL-L
#define ASCII_CR    0x0D   // Carriage Return
#define ASCII_CTRLM 0x0D   // CTRL-M
#define ASCII_SO    0x0E   // Shift Out
#define ASCII_CTRLN 0x0E   // CTRL-N
#define ASCII_SI    0x0F   // Shift In
#define ASCII_CTRLO 0x0F   // CTRL-O
#define ASCII_DLE   0x10   // Data Link Escape
#define ASCII_CTRLP 0x10   // CTRL-P
#define ASCII_DC1   0x11   // Device Control 1
#define ASCII_CTRLQ 0x11   // CTRL-Q
#define ASCII_XON   0x11   // Transmission On
#define ASCII_DC2   0x12   // Device Control 2
#define ASCII_CTRLR 0x12   // CTRL-R
#define ASCII_DC3   0x13   // Device Control 3
#define ASCII_XOFF  0x13   // Transmission Off
#define ASCII_CTRLS 0x13   // CTRL-S
#define ASCII_DC4   0x14   // Device Control 4
#define ASCII_CTRLT 0x14   // CTRL-T
#define ASCII_NAK   0x15   // Negative Acknowledge
#define ASCII_CTRLU 0x15   // CTRL-U
#define ASCII_SYN   0x16   // Synchronous Idle
#define ASCII_CTRLV 0x16   // CTRL-V
#define ASCII_ETB   0x17   // End-of-Transmission-Block
#define ASCII_CTRLW 0x17   // CTRL-W
#define ASCII_CAN   0x18   // Cancel
#define ASCII_CTRLX 0x18   // CTRL-X
#define ASCII_EM    0x19   // End of Medium
#define ASCII_CTRLY 0x19   // CTRL-Y
#define ASCII_SUB   0x1A   // Substitute
#define ASCII_CTRLZ 0x1A   // CTRL-Z
#define ASCII_ESC   0x1B   // Escape
#define ASCII_FS    0x1C   // File Separator
#define ASCII_GS    0x1D   // Group Separator
#define ASCII_RS    0x1E   // Record Separator
#define ASCII_US    0x1F   // Unit Separator
#define ASCII_SPC   0x20   // Space
#define ASCII_DEL   0x7F   // Delete


} // end of namespace



