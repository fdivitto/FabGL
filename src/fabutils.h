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



#ifndef _FABUTILS_H_INCLUDED
#define _FABUTILS_H_INCLUDED


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



struct TimeOut {
  TimeOut();

  // -1 means "infinite", never times out
  bool expired(int valueMS);

private:
  int64_t m_start;
};



///////////////////////////////////////////////////////////////////////////////////


// Integer square root by Halleck's method, with Legalize's speedup
int isqrt (int x);


template <typename T>
const T & tmax(const T & a, const T & b)
{
  return (a < b) ? b : a;
}


template <typename T>
const T & tmin(const T & a, const T & b)
{
  return !(b < a) ? a : b;
}


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


inline Rect translate(Rect const & rect, int offsetX, int offsetY)
{
  return Rect(rect.X1 + offsetX, rect.Y1 + offsetY, rect.X2 + offsetX, rect.Y2 + offsetY);
}


inline Rect intersection(Rect const & rect1, Rect const & rect2)
{
  return Rect(tmax(rect1.X1, rect2.X1), tmax(rect1.Y1, rect2.Y1), tmin(rect1.X2, rect2.X2), tmin(rect1.Y2, rect2.Y2));
}


inline bool intersect(Rect const & rect1, Rect const & rect2)
{
  return rect1.X1 <= rect2.X2 && rect1.X2 >= rect2.X1 && rect1.Y1 <= rect2.Y2 && rect1.Y2 >= rect2.Y1;
}


bool clipLine(int & x1, int & y1, int & x2, int & y2, Rect const & clipRect);



} // end of namespace






#endif  // _FABUTILS_H_INCLUDED
