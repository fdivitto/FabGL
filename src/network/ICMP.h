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
 * @brief This file contains ICMP (ping) class
 */


#ifdef ARDUINO


#include "Client.h"

#include "freertos/FreeRTOS.h"

#include "lwip/netdb.h"
#include "lwip/raw.h"
#include "lwip/icmp.h"
#include "lwip/inet_chksum.h"


#include "fabglconf.h"
#include "fabutils.h"



namespace fabgl {



/**
 * @brief ICMP Implementation
 *
 * Used to ping a network device using its IP or its host name.
 *
 * Example:
 *
 *     fabgl::ICMP icmp;
 *     while (true) {
 *
 *       // CTRL-C ?
 *       if (Terminal.available() && Terminal.read() == 0x03)
 *         break;
 *
 *       int t = icmp.ping("www.fabgl.com");
 *       if (t >= 0) {
 *         Terminal.printf("%d bytes from %s: icmp_seq=%d ttl=%d time=%.3f ms\r\n", icmp.receivedBytes(), icmp.hostIP().toString().c_str(), icmp.receivedSeq(), icmp.receivedTTL(), (double)t/1000.0);
 *         delay(1000);
 *       } else if (t == -2) {
 *         Terminal.printf("Cannot resolve %s: Unknown host\r\n", host);
 *         break;
 *       } else {
 *         Terminal.printf("Request timeout for icmp_seq %d\r\n", icmp.receivedSeq());
 *       }
 *
 *     }
 *
 */
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




}


#endif // #ifdef ARDUINO
