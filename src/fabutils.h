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


#include "freertos/FreeRTOS.h"


namespace fabgl {



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
  bool operator==(Rect const & r) { return X1 == r.X1 && Y1 == r.Y1 && X2 == r.X2 && Y2 == r.Y2; }
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



struct TimeOut {
  TimeOut();

  // -1 means "infinite", never times out
  bool expired(int valueMS);

private:
  int64_t m_start;
};



template <typename T>
struct StackItem {
  StackItem * next;
  T item;
  StackItem(StackItem * next_, T const & item_) : next(next_), item(item_) { }
};

template <typename T>
class Stack {
public:
  Stack() : m_items(NULL) { }
  bool isEmpty() { return m_items == NULL; }
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
private:
  StackItem<T> * m_items;
};


///////////////////////////////////////////////////////////////////////////////////


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


bool calcParity(uint8_t v);


inline Size rectSize(Rect const & rect)
{
  return Size(rect.X2 - rect.X1 + 1, rect.Y2 - rect.Y1 + 1);
}


inline Rect translate(Rect const & rect, int offsetX, int offsetY)
{
  return Rect(rect.X1 + offsetX, rect.Y1 + offsetY, rect.X2 + offsetX, rect.Y2 + offsetY);
}


inline Rect translate(Rect const & rect, Point const & offset)
{
  return Rect(rect.X1 + offset.X, rect.Y1 + offset.Y, rect.X2 + offset.X, rect.Y2 + offset.Y);
}


inline Point add(Point const & point1, Point const & point2)
{
  return Point(point1.X + point2.X, point1.Y + point2.Y);
}


inline Point sub(Point const & point1, Point const & point2)
{
  return Point(point1.X - point2.X, point1.Y - point2.Y);
}


inline Rect shrink(Rect const & rect, int value)
{
  return Rect(rect.X1 + value, rect.Y1 + value, rect.X2 - value, rect.Y2 - value);
}


inline Rect resize(Rect const & rect, int width, int height)
{
  return Rect(rect.X1, rect.Y1, rect.X1 + width - 1, rect.Y1 + height - 1);
}


inline Point neg(Point const & point)
{
  return Point(-point.X, -point.Y);
}


inline Rect intersection(Rect const & rect1, Rect const & rect2)
{
  return Rect(tmax(rect1.X1, rect2.X1), tmax(rect1.Y1, rect2.Y1), tmin(rect1.X2, rect2.X2), tmin(rect1.Y2, rect2.Y2));
}


inline bool intersect(Rect const & rect1, Rect const & rect2)
{
  return rect1.X1 <= rect2.X2 && rect1.X2 >= rect2.X1 && rect1.Y1 <= rect2.Y2 && rect1.Y2 >= rect2.Y1;
}


inline bool pointInRect(Point const & point, Rect const & rect)
{
  return point.X >= rect.X1 && point.Y >= rect.Y1 && point.X <= rect.X2 && point.Y <= rect.Y2;
}


inline bool pointInRect(int x, int y, Rect const & rect)
{
  return x >= rect.X1 && y >= rect.Y1 && x <= rect.X2 && y <= rect.Y2;
}


bool clipLine(int & x1, int & y1, int & x2, int & y2, Rect const & clipRect);


// Checks if the rect1 contains rect2
inline bool contains(Rect const & rect1, Rect const & rect2)
{
  return (rect2.X1 >= rect1.X1) && (rect2.Y1 >= rect1.Y1) && (rect2.X2 <= rect1.X2) && (rect2.Y2 <= rect1.Y2);
}


void removeRectangle(Stack<Rect> & rects, Rect const & mainRect, Rect const & rectToRemove);



} // end of namespace



