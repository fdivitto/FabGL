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



#pragma once


/**
 * @file
 *
 * @brief This file contains some utility classes and functions
 *
 */


#include "freertos/FreeRTOS.h"


namespace fabgl {



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
};


/**
 * @brief Represents a bidimensional size.
 */
struct Size {
  int16_t width;   /**< Horizontal size */
  int16_t height;  /**< Vertical size */

  Size() : width(0), height(0) { }
  Size(int width_, int height_) : width(width_), height(height_) { }
};



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
  Rect intersection(Rect const & rect) const     { return Rect(tmax(X1, rect.X1), tmax(Y1, rect.Y1), tmin(X2, rect.X2), tmin(Y2, rect.Y2)); }
  bool intersects(Rect const & rect) const       { return X1 <= rect.X2 && X2 >= rect.X1 && Y1 <= rect.Y2 && Y2 >= rect.Y1; }
  bool contains(Rect const & rect) const         { return (rect.X1 >= X1) && (rect.Y1 >= Y1) && (rect.X2 <= X2) && (rect.Y2 <= Y2); }
  bool contains(Point const & point) const       { return point.X >= X1 && point.Y >= Y1 && point.X <= X2 && point.Y <= Y2; }
  bool contains(int x, int y) const              { return x >= X1 && y >= Y1 && x <= X2 && y <= Y2; }
};



/**
 * @brief Describes mouse buttons status.
 */
struct MouseButtons {
  uint8_t left   : 1;   /**< Contains 1 when left button is pressed. */
  uint8_t middle : 1;   /**< Contains 1 when middle button is pressed. */
  uint8_t right  : 1;   /**< Contains 1 when right button is pressed. */
};



/**
 * @brief Describes mouse absolute position, scroll wheel delta and buttons status.
 */
struct MouseStatus {
  int16_t      X;           /**< Absolute horizontal mouse position. */
  int16_t      Y;           /**< Absolute vertical mouse position. */
  int8_t       wheelDelta;  /**< Scroll wheel delta. */
  MouseButtons buttons;     /**< Mouse buttons status. */
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

  template <typename Func>
  void operator=(Func f) {
    m_closure = [] (void * func, const Params & ...params) -> void { (*(Func *)func)(params...); };
    m_func = heap_caps_malloc(sizeof(Func), MALLOC_CAP_32BIT);
    moveItems<uint32_t*>((uint32_t*)m_func, (uint32_t*)&f, sizeof(Func) / sizeof(uint32_t));
  }

  ~Delegate() {
    heap_caps_free(m_func);
  }

  void operator()(const Params & ...params) {
    if (m_func)
      m_closure(m_func, params...);
  }

private:
  void (*m_closure)(void * func, const Params & ...params);
  void * m_func = nullptr;
};



///////////////////////////////////////////////////////////////////////////////////
// StringList

class StringList {

public:
  StringList();
  ~StringList();
  int append(char const * str);
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
  void copyFrom(StringList const & src);

private:
  void checkAllocatedSpace(int requiredItems);

  char const * * m_items;

  // each 32 bit word can select up to 32 items, one bit per item
  uint32_t *     m_selMap;

  // If true (default is false) all strings added (append/insert/set) are copied.
  // Strings will be freed when no more used (destructor, clear(), etc...).
  // This flag is permanently switched to True by takeStrings() call.
  bool           m_ownStrings;

  uint16_t       m_count;     // actual items
  uint16_t       m_allocated; // allocated items

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


/**
 * @brief FileBrowser allows basic file system operations (dir, mkdir, remove and rename)
 */
class FileBrowser {
public:

  FileBrowser();
  ~FileBrowser();

  /**
   * @brief Sets absolute directory path
   *
   * @param path Absolute directory path (ie "/spiffs")
   */
  void setDirectory(const char * path);    // set absolute path

  /**
   * @brief Sets relative directory path
   *
   * @param subdir Relative directory path (ie "subdir")
   */
  void changeDirectory(const char * subdir); // set relative path

  /**
   * @brief Reloads directory content
   */
  void reload();

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
   * @brief Determines if a file exists
   *
   * @param name Relative file or directory name
   *
   * @return True if the file exists
   */
  bool exists(char const * name);

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
   * @brief Compose a full file path given a relative name
   *
   * @param name Relative file name
   * @param outPath Where to place the full path. This can be NULL (used to calculate required buffer size).
   * @param maxlen Maximum size of outPath string. This can be 0 when outPath is NULL.
   *
   * @return Required outPath size
   */
  int getFullPath(char const * name, char * outPath = nullptr, int maxlen = 0);

private:

  void clear();
  int countDirEntries(int * namesLength);

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


void suspendInterrupts();
void resumeInterrupts();


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
  VK_PRINTSCREEN1,    /**< PRINTSCREEN is translated as separated VK_PRINTSCREEN1 and VK_PRINTSCREEN2. VK_PRINTSCREEN2 is also generated by CTRL or SHIFT + PRINTSCREEN. So pressing PRINTSCREEN both VK_PRINTSCREEN1 and VK_PRINTSCREEN2 are generated, while pressing CTRL+PRINTSCREEN or SHIFT+PRINTSCREEN only VK_PRINTSCREEN2 is generated. */
  VK_PRINTSCREEN2,    /**< PRINTSCREEN. See VK_PRINTSCREEN1 */
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
  VK_GRAVE_a,         /**< Grave 'a': à */
  VK_GRAVE_e,         /**< Grave 'e': è */
  VK_ACUTE_e,         /**< Acute 'e': é */
  VK_GRAVE_i,         /**< Grave 'i': ì */
  VK_GRAVE_o,         /**< Grave 'o': ò */
  VK_GRAVE_u,         /**< Grave 'u': ù */
  VK_CEDILLA_c,       /**< Cedilla 'c': ç */
  VK_ESZETT,          /**< Eszett: ß */
  VK_UMLAUT_u,        /**< Umlaut 'u': ü */
  VK_UMLAUT_o,        /**< Umlaut 'o': ö */
  VK_UMLAUT_a,        /**< Umlaut 'a': ä */

  VK_LAST,            // marks the last virtual key
};


} // end of namespace



