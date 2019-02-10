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



#ifndef _UTILS_H_INCLUDED
#define _UTILS_H_INCLUDED




namespace fabgl {



template <typename T>
const T & max(const T & a, const T & b)
{
  return (a < b) ? b : a;
}


template <typename T>
const T & min(const T & a, const T & b)
{
  return !(b < a) ? a : b;
}


template <typename T>
const T & clamp(const T & v, const T & lo, const T & hi)
{
  return (v < lo ? lo : (v > hi ? hi : v));
}


template <typename T>
const T & wrap(const T & v, const T & lo, const T & hi)
{
  return (v < lo ? hi : (v > hi ? lo : v));
}


template <typename T>
void swap(T & v1, T & v2)
{
  T t = v1;
  v1 = v2;
  v2 = t;
}


inline bool calcParity(uint8_t v)
{
  v ^= v >> 4;
  v &= 0xf;
  return (0x6996 >> v) & 1;
}




} // end of namespace






#endif  // _UTILS_H_INCLUDED
