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


#ifdef ARDUINO


#include "ICMP.h"

#include "Arduino.h"
#include "WiFiGeneric.h"


namespace fabgl {


ICMP::ICMP()
  : m_queue(xQueueCreate(1, sizeof(uint8_t))), m_waitingID(rand() & 0xFFFF), m_waitingSeq(-1)
{
}


ICMP::~ICMP()
{
  vQueueDelete(m_queue);
}


// -1 = timeout
// -2 = cannot resolve name
int ICMP::ping(char const * host)
{
  IPAddress hostIP((uint32_t)0);
  if (!WiFiGenericClass::hostByName(host, hostIP))
    return -2;
  return ping(hostIP);
}


// -1 = timeout
int ICMP::ping(IPAddress const &dest)
{
  static int32_t const TIMEOUT        = 1000;
  static int32_t const TIMEOUT_RESULT = -1;

  m_destIP = dest;

  int result = TIMEOUT_RESULT;

  // generate seq
  ++m_waitingSeq;

  // prepare packet to send
  pbuf * hdrbuf = pbuf_alloc(PBUF_IP, sizeof(icmp_echo_hdr), PBUF_RAM);
  icmp_echo_hdr * hdr = (icmp_echo_hdr *) hdrbuf->payload;
  hdr->type   = ICMP_ECHO;
  hdr->code   = 0;
  hdr->chksum = 0;
  hdr->id     = htons(m_waitingID);
  hdr->seqno  = htons(m_waitingSeq);
  hdr->chksum = inet_chksum((uint16_t *) hdr, sizeof(icmp_echo_hdr));

  // send Echo request
  raw_pcb * pcb = raw_new(IP_PROTO_ICMP);
  raw_recv(pcb, ICMP::raw_recv_fn, this);
  raw_bind(pcb, IP_ADDR_ANY);

  ip_addr_t addr;
  addr.type = IPADDR_TYPE_V4;
  addr.u_addr.ip4.addr = dest;
  raw_sendto(pcb, hdrbuf, &addr);
  pbuf_free(hdrbuf);

  result = -1;
  int32_t t1 = micros();
  uint8_t c;
  if (xQueueReceive(m_queue, &c, pdMS_TO_TICKS(TIMEOUT)))
    result = micros() - t1;
  raw_remove(pcb);

  return result;
}


uint8_t ICMP::raw_recv_fn(void * arg, raw_pcb * pcb, pbuf * p, const ip_addr_t * addr)
{
  ICMP * this_ = (ICMP *)arg;

  ip_hdr * iphdr = (ip_hdr *)p->payload;

  int ttl = IPH_TTL(iphdr);

  if (p->tot_len >= sizeof(ip_hdr) + sizeof(icmp_echo_hdr) && pbuf_header(p, -(s16_t)sizeof(ip_hdr)) == 0) {
    icmp_echo_hdr * hdr = (icmp_echo_hdr *) p->payload;
    if (ntohs(hdr->id) == this_->m_waitingID && ntohs(hdr->seqno) == this_->m_waitingSeq) {
      this_->m_receivedBytes = p->tot_len;
      this_->m_receivedTTL = ttl;
      uint8_t c = 0;
      xQueueSendToBack(this_->m_queue, &c, portMAX_DELAY);
    }
    pbuf_free(p);
    return 1;
  }

  return 0;
}



}



#endif // #ifdef ARDUINO
