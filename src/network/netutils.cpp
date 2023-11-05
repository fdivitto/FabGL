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



#include <string.h>

#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"

#include "fabutils.h"

#include "netutils.h"



namespace fabgl {



////////////////////////////////////////////////////////////////////////////////////////////
// WiFiScanner


WiFiScanner::WiFiScanner()
  : m_items(nullptr), m_count(0)
{
}


WiFiScanner::~WiFiScanner()
{
  cleanUp();
}


// if justCount = true, maxItems is ignored
bool WiFiScanner::scan(int maxItems, bool justCount)
{
  esp_event_loop_create_default();

  // init with reduced memory footprint
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();  
  cfg.static_rx_buf_num = 2;
  cfg.static_tx_buf_num = 1;
  cfg.ampdu_rx_enable = cfg.ampdu_tx_enable = cfg.amsdu_tx_enable = 0;
  esp_wifi_init(&cfg);

  esp_wifi_set_mode(WIFI_MODE_STA);
  esp_wifi_start();
  
  auto r = esp_wifi_scan_start(nullptr, true);

  if (r == ESP_OK) {
    esp_wifi_scan_get_ap_num(&m_count);
    if (!justCount) {
      m_count = std::min<uint16_t>(m_count, maxItems); 
      m_items = new wifi_ap_record_t[m_count];
      uint16_t tempCount = m_count;
      esp_wifi_scan_get_ap_records(&tempCount, m_items);  // use tempCount because esp_wifi_scan_get_ap_records() changes it on exit (standing to the docs)
    }
  }

  esp_wifi_clear_ap_list(); // needed if esp_wifi_scan_get_ap_records() is not used
  esp_wifi_stop();
  esp_wifi_deinit();
  esp_event_loop_delete_default();

  return r == ESP_OK;
}


void WiFiScanner::cleanUp()
{
  if (m_items)
    delete [] m_items;
  m_items = nullptr;
}



////////////////////////////////////////////////////////////////////////////////////////////
// WiFiConnection


WiFiConnection::WiFiConnection()
  : m_netif(nullptr), m_state(WiFiConnectionState::Disconnected)
{
}


WiFiConnection::~WiFiConnection()
{
  disconnect();
}


WiFiConnectionState WiFiConnection::connect(char const * ssid, char const * password, int waitConnectionTimeOutMS)
{
  if (m_state == WiFiConnectionState::Disconnected) {

    esp_netif_init();
    esp_event_loop_create_default();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    m_state = WiFiConnectionState::ConnectingWiFi;

    esp_netif_config_t netif_config = ESP_NETIF_DEFAULT_WIFI_STA();
    m_netif = esp_netif_new(&netif_config);

    esp_netif_attach_wifi_station(m_netif);
    esp_wifi_set_default_wifi_sta_handlers();

    esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &wifiEventStaDisconnected, this);
    esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &wifiEventStaConnected, this);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ipEventStaGotIP, this);
    
    esp_wifi_set_storage(WIFI_STORAGE_RAM);

    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config));
    strncpy((char *)wifi_config.sta.ssid, ssid, strlen(ssid));
    if (password)
      strncpy((char *)wifi_config.sta.password, password, strlen(password));

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
    esp_wifi_connect();

  }

  TimeOut timeout;
  while (m_state != WiFiConnectionState::Connected && !timeout.expired(waitConnectionTimeOutMS))
    vTaskDelay(100 / portTICK_PERIOD_MS);

  return m_state;
}


void WiFiConnection::disconnect()
{
  if (m_state != WiFiConnectionState::Disconnected) {
    esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &wifiEventStaDisconnected);
    esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &wifiEventStaConnected);
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &ipEventStaGotIP);
    esp_wifi_stop();
    esp_wifi_clear_default_wifi_driver_and_handlers(m_netif);
    esp_netif_destroy(m_netif);
    m_netif = nullptr;
    esp_wifi_deinit();
    esp_netif_deinit();
    esp_event_loop_delete_default();
    m_state = WiFiConnectionState::Disconnected;
  }
}


void WiFiConnection::wifiEventStaConnected(void * arg, esp_event_base_t event_base, int32_t event_id, void * event_data)
{
  auto obj = (WiFiConnection *)arg;
  if (obj->m_state == WiFiConnectionState::ConnectingWiFi)
    obj->m_state = WiFiConnectionState::WaitingIP;
}


void WiFiConnection::wifiEventStaDisconnected(void * arg, esp_event_base_t event_base, int32_t event_id, void * event_data)
{
  auto obj = (WiFiConnection *)arg;
  if (obj->m_state == WiFiConnectionState::ConnectingWiFi || obj->m_state == WiFiConnectionState::Connected) {
      // tries to reconnect
      esp_wifi_connect();
  }
}


void WiFiConnection::ipEventStaGotIP(void * arg, esp_event_base_t event_base, int32_t event_id, void * event_data)
{
  auto obj = (WiFiConnection *)arg;
  if (obj->m_state == WiFiConnectionState::WaitingIP) {
    auto event = (ip_event_got_ip_t *)event_data;
    obj->m_IP      = event->ip_info.ip;
    obj->m_netmask = event->ip_info.netmask;
    obj->m_gateway = event->ip_info.gw;
    obj->m_state   = WiFiConnectionState::Connected;
  }
}



////////////////////////////////////////////////////////////////////////////////////////////
// HTTPRequest


HTTPRequest::HTTPRequest()
  : m_client(nullptr)
{
}


HTTPRequest::~HTTPRequest()
{
  close();
}


esp_err_t HTTPRequest::httpEventHandler(esp_http_client_event_t * evt)
{
  auto obj = (HTTPRequest *) evt->user_data;

  switch(evt->event_id) {
    case HTTP_EVENT_ERROR:
      break;
    case HTTP_EVENT_ON_CONNECTED:
      break;
    case HTTP_EVENT_ON_DATA:
      obj->onData(evt->data, evt->data_len);
      break;
    case HTTP_EVENT_ON_FINISH:
      break;
    case HTTP_EVENT_DISCONNECTED:
      break;
    default:
      break;
  }
  
  return ESP_OK;
}


int HTTPRequest::GET(char const * URL)
{
  if (m_client) {
    esp_http_client_set_url(m_client, URL);
  } else {
    esp_http_client_config_t config = {
      .url           = URL,
      .event_handler = httpEventHandler,
      .user_data     = this,
    };
    m_client = esp_http_client_init(&config);
  }  
  return esp_http_client_perform(m_client) == ESP_OK ? esp_http_client_get_status_code(m_client) : 0;
}


void HTTPRequest::close()
{
  if (m_client) {
    esp_http_client_close(m_client);
    esp_http_client_cleanup(m_client);
    m_client = nullptr;
  }
}


} // end of namespace
