/*
  Created by Fabrizio Di Vittorio (fdivitto2013@gmail.com) - <http://www.fabgl.com>
  Copyright (c) 2019-2023 Fabrizio Di Vittorio.
  All rights reserved.


* Please contact fdivitto2013@gmail.com if you need a commercial license.


* This library and related software is available under GPL v3.

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
 * @brief This file contains fabgl::WiFiScanner, fabgl::WiFiConnection, fabgl::HTTPRequest definitions.
 */


#include <stdint.h>
#include <stddef.h>

#include "esp_system.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_client.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "fabglconf.h"
#include "fabutils.h"



namespace fabgl {



///////////////////////////////////////////////////////////////////////////////////
// WiFiScanner

/**
* @brief WiFi scanner helper class
*
* WiFi scanner allows to scan for nearby WiFi networks
*
* Example:
*
*     fabgl::WiFiScanner scanner;
*     if (scanner.scan())
*       for (int i = 0; i < scanner.count(); ++i) {
*         auto item = scanner.get(i);
*         Term.printf("#%d %s %d dBm\r\n", i, item->ssid, item->rssi);
*       }
*/
class WiFiScanner {

public:

  WiFiScanner();
  ~WiFiScanner();

  /**
   * @brief Performs WiFi scan
   *
   * @param maxItems Maximum number of networks to return
   * @param justCount If true just count number of networks (WiFiScanner::get() will returns nullptr and maxItems is ignored)
   *
   * @return True on success
   */
  bool scan(int maxItems = 8, bool justCount = false);

  /**
   * @brief Returns number of found or maximum requested networks
   *
   * @return Number of found or maximum requested networks
   */
  int count() { return m_count; }

  /**
   * @brief Returns specified network info
   *
   * @param index Index of network to get (0 = first network
   *
   * @return WiFi network info or nullptr in case justCount parameter of scan() is True
   */
  wifi_ap_record_t * get(int index) { return m_items ? m_items + index : nullptr; }

  /**
   * @brief Free resources used to scan WiFi networks
   */
  void cleanUp();

private:

  wifi_ap_record_t * m_items;
  uint16_t           m_count;
};



///////////////////////////////////////////////////////////////////////////////////
// WiFiConnection


/** \ingroup Enumerations
* @brief This enum defines fabgl::WiFiConnection state
*/
enum class WiFiConnectionState {
  Disconnected,     /**< Disconnected or unable to connect */
  ConnectingWiFi,   /**< Connecting in progress */
  WaitingIP,        /**< Connecting success, waiting for IP */
  Connected,        /**< Successfully connected */
};


/**
* @brief WiFi connection helper class
*
* WiFiConnection allows to establish and maintain a connection with a WiFi network
*
* Example:
*
*     auto nc = fabgl::WiFiConnection();
*     if (nc.connect("MySSID", "MyPassword")) {
*       Term.printf("Connected. IP:%d.%d.%d.%d NMASK:%d.%d.%d.%d GATEW:%d.%d.%d.%d\r\n", IP2STR(&nc.IP()), IP2STR(&nc.netmask()), IP2STR(&nc.gateway()));
*
*       ...to some stuff with the network (ie using fabgl::HTTPRequest)
*
*       // now disconnect
*       nc.disconnect();
*     }
*/
class WiFiConnection {

public:

  WiFiConnection();
  ~WiFiConnection();

  /**
   * @brief Tries to connect to a WiFi network.
   *
   * @param ssid SSID of required WiFi network
   * @param password Password of the WiFi network
   * @param waitConnectionTimeOutMS Timeout in milliseconds
   *
   * @return Returns WiFiConnectionState::Connected on success
   */
  WiFiConnectionState connect(char const * ssid, char const * password, int waitConnectionTimeOutMS = 10000);

  /**
   * @brief Disconnects from the WiFi
   */
  void disconnect();

  /**
   * @brief Returns connection state
   *
   * @return Returns current connection state
   */
  WiFiConnectionState state() { return m_state; }

  /**
   * @brief Returns IP address
   *
   * @return Returns acquired IP address
   */
  esp_ip4_addr_t const & IP() { return m_IP; }

  /**
   * @brief Returns netmask
   *
   * @return Returns acquired netmask
   */
  esp_ip4_addr_t const & netmask() { return m_netmask; }

  /**
   * @brief Returns gateway address
   *
   * @return Returns gateway address
   */
  esp_ip4_addr_t const & gateway() { return m_gateway; }


private:

  static void wifiEventStaConnected(void * arg, esp_event_base_t event_base, int32_t event_id, void * event_data);
  static void wifiEventStaDisconnected(void * arg, esp_event_base_t event_base, int32_t event_id, void * event_data);
  static void ipEventStaGotIP(void * arg, esp_event_base_t event_base, int32_t event_id, void * event_data);

  esp_netif_t *        m_netif;
  WiFiConnectionState  m_state;
  esp_ip4_addr_t       m_IP;
  esp_ip4_addr_t       m_netmask;
  esp_ip4_addr_t       m_gateway;
};



///////////////////////////////////////////////////////////////////////////////////
// HTTPRequest


/**
* @brief HTTP connection and request helper
*
* HTTPRequest allows to connect to a website and get a page or a file
*
* Example:
*
*     auto nc = fabgl::WiFiConnection();
*     if (nc.connect("MySSID", "MyPassword")) {
*       fabgl::HTTPRequest req;
*       // just count how many bytes we have got
*       int received = 0;
*       req.onData = [&](void const * data, int len) {
*         received += len;
*       };
*       // start GET method
*       if (req.GET("http://www.fabglib.org/schema_audio.png") == 200)
*         Term.printf("Received %d of %d bytes\r\n", received, req.contentLength());
*     }
*/
class HTTPRequest {

public:

  HTTPRequest();
  ~HTTPRequest();

  /**
   * @brief Performs the GET method
   *
   * Multiple consecutive requestes can be issued by the same fabgl::HTTPRequest object.
   *
   * @param URL The page or file to retrieve
   *
   * @return Returns the HTTP error or success code (0 = connection failure)
   */
  int GET(char const * URL);

  /**
   * @brief Gets page content length
   *
   * @return Returns page content length as returned by the web server
   */
  int contentLength() { return esp_http_client_get_content_length(m_client); }

  /**
   * @brief Closes HTTP connection
   */
  void close();


  // delegates
  
  /**
   * @brief Data received delegate
   *
   * This delegate is called whenever a block of data has been received.
   * First parameter specifies a pointer to the received buffer.
   * Second parameter specifies received buffer length in bytes.
   */
  Delegate<void const *, int> onData; // (data, len)


private:

  static esp_err_t httpEventHandler(esp_http_client_event_t * evt);

  esp_http_client_handle_t m_client;

};



} // end of namespace

