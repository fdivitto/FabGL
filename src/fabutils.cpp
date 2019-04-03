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


#include "fabutils.h"



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



}

