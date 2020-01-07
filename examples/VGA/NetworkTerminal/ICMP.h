/*
  Created by Fabrizio Di Vittorio (fdivitto2013@gmail.com) - www.fabgl.com
  Copyright (c) 2019-2020 Fabrizio Di Vittorio.
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


#ifndef ICMP_h
#define ICMP_h


#include "Client.h"

#include "freertos/FreeRTOS.h"

#include "lwip/netdb.h"
#include "lwip/raw.h"
#include "lwip/icmp.h"
#include "lwip/inet_chksum.h"


class ICMP {

public:

  ICMP();
  ~ICMP();

  // send Echo Request and wait for Echo Reply
  // returns "measured" echo time in microseconds. ret -1 on timeout or error
  int ping(IPAddress const &dest);
  int ping(char const * host);  // host can be IP or host name

  int receivedBytes() { return m_receivedBytes; }

  int receivedTTL() { return m_receivedTTL; }

  int receivedSeq() { return m_waitingSeq; }

  IPAddress const& hostIP() { return m_destIP; }

private:

  static uint8_t raw_recv_fn(void * arg, raw_pcb * pcb, pbuf * p, const ip_addr_t * addr);

  IPAddress     m_destIP;
  QueueHandle_t m_queue;
  int           m_waitingID;
  int           m_waitingSeq;
  int           m_receivedBytes;
  int           m_receivedTTL;
};


#endif /* ICMP_h */
