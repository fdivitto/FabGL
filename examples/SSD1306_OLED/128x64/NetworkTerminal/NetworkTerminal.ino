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


 /*
  * OLED - SDA => GPIO 4
  * OLED - SCL => GPIO 15
  */



#include "fabgl.h"

#include <WiFi.h>

#include "network/ICMP.h"


#define OLED_SDA       GPIO_NUM_4
#define OLED_SCL       GPIO_NUM_15
#define OLED_ADDR      0x3C

// if your display hasn't RESET set to GPIO_UNUSED
#define OLED_RESET     GPIO_UNUSED  // ie Heltec has GPIO_NUM_16 for reset



char const * AUTOEXEC = "info\r";


enum class State { Prompt, PromptInput, UnknownCommand, Help, Info, Wifi, TelnetInit, Telnet, Scan, Ping, Reset };


State        state = State::Prompt;
WiFiClient   client;
char const * currentScript = nullptr;
bool         error = false;

fabgl::I2C               I2C;
fabgl::SSD1306Controller DisplayController;
fabgl::PS2Controller     PS2Controller;
fabgl::Terminal          Terminal;
fabgl::LineEditor        LineEditor(&Terminal);



int clientWaitForChar()
{
  // not so good...:-)
  while (!client.available())
    ;
  return client.read();
}


int termWaitForChar()
{
  while (!Terminal.available())
    ;
  return Terminal.read();
}


void exe_info()
{
  Terminal.write("FabGL Terminal\r\n");
  Terminal.printf("Terminal Size: %dx%d\r\n", Terminal.getColumns(), Terminal.getRows());
  if (!PS2Controller.keyboard()->isKeyboardAvailable())
    Terminal.write("Keyboard Error!!\r\n");
  Terminal.printf("Free Mem: %d\r\n", heap_caps_get_free_size(0));
  if (WiFi.status() == WL_CONNECTED) {
    Terminal.printf("WiFi SSID: %s\r\n", WiFi.SSID().c_str());
    Terminal.printf("IP: %s\r\n", WiFi.localIP().toString().c_str());
  }
  Terminal.write("Type 'help' for info\r\n");
  error = false;
  state = State::Prompt;
}


void exe_help()
{
  Terminal.write("info\r\n");
  Terminal.write("  Shows system info.\r\n\r\n");
  termWaitForChar();
  Terminal.write("scan\r\n");
  Terminal.write("  Scan for WiFi networks.\r\n\r\n");
  termWaitForChar();
  Terminal.write("wifi SSID PASSWORD\r\n");
  Terminal.write("  Connect to SSID using PASSWORD.\r\n\r\n");
  termWaitForChar();
  Terminal.write("telnet HOST [PORT]\r\n");
  Terminal.write("  Open telnet session with HOST (IP or host name) using PORT.\r\n\r\n");
  termWaitForChar();
  Terminal.write("reboot\r\n");
  Terminal.write("  Restart the system.\r\n\r\n");
  error = false;
  state = State::Prompt;
}


void decode_command()
{
  auto inputLine = LineEditor.get();
  if (*inputLine == 0)
    state = State::Prompt;
  else if (strncmp(inputLine, "help", 4) == 0)
    state = State::Help;
  else if (strncmp(inputLine, "info", 4) == 0)
    state = State::Info;
  else if (strncmp(inputLine, "wifi", 4) == 0)
    state = State::Wifi;
  else if (strncmp(inputLine, "telnet", 6) == 0)
    state = State::TelnetInit;
  else if (strncmp(inputLine, "scan", 4) == 0)
    state = State::Scan;
  else if (strncmp(inputLine, "ping", 4) == 0)
    state = State::Ping;
  else if (strncmp(inputLine, "reboot", 4) == 0)
    state = State::Reset;
  else
    state = State::UnknownCommand;
}


void exe_prompt()
{
  if (currentScript) {
    // process commands from script
    if (*currentScript == 0 || error) {
      // end of script, return to prompt
      currentScript = nullptr;
      state = State::Prompt;
    } else {
      // execute current line and move to the next one
      int linelen = strchr(currentScript, '\r') - currentScript;
      LineEditor.setText(currentScript, linelen);
      currentScript += linelen + 1;
      decode_command();
    }
  } else {
    // process commands from keyboard
    Terminal.write(">");
    state = State::PromptInput;
  }
}


void exe_promptInput()
{
  LineEditor.setText("");
  LineEditor.edit();
  decode_command();
}


void exe_scan()
{
  static char const * ENC2STR[] = { "Open", "WEP", "WPA-PSK", "WPA2-PSK", "WPA/WPA2-PSK", "WPA-ENTERPRISE" };
  Terminal.write("Scanning...\r\n");
  Terminal.flush();
  int networksCount = WiFi.scanNetworks();
  Terminal.printf("%d network(s)\r\n", networksCount);
  if (networksCount) {
    for (int i = 0; i < networksCount; ++i)
      Terminal.printf("#%d %s %ddBm ch%d %s\r\n", i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.channel(i), ENC2STR[WiFi.encryptionType(i)]);
  }
  WiFi.scanDelete();
  error = false;
  state = State::Prompt;
}


void exe_wifi()
{
  static const int MAX_SSID_SIZE = 32;
  static const int MAX_PSW_SIZE  = 32;
  char ssid[MAX_SSID_SIZE + 1];
  char psw[MAX_PSW_SIZE + 1] = {0};
  error = true;
  auto inputLine = LineEditor.get();
  if (sscanf(inputLine, "wifi %32s %32s", ssid, psw) >= 1) {
    Terminal.write("Connecting WiFi...");
    Terminal.flush();
    WiFi.disconnect(true, true);
    for (int i = 0; i < 2; ++i) {
      WiFi.begin(ssid, psw);
      if (WiFi.waitForConnectResult() == WL_CONNECTED)
        break;
      WiFi.disconnect(true, true);
    }
    if (WiFi.status() == WL_CONNECTED) {
      Terminal.printf("SSID: %s\r\nIP: %s\r\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
      error = false;
    } else {
      Terminal.write("failed!\r\n");
    }
  }
  state = State::Prompt;
}



void exe_telnetInit()
{
  static const int MAX_HOST_SIZE = 32;
  char host[MAX_HOST_SIZE + 1];
  int port;
  error = true;
  auto inputLine = LineEditor.get();
  int pCount = sscanf(inputLine, "telnet %32s %d", host, &port);
  if (pCount > 0) {
    if (pCount == 1)
      port = 23;
    Terminal.printf("Trying %s...\r\n", host);
    if (client.connect(host, port)) {
      Terminal.printf("Connected to %s\r\n", host);
      error = false;
      state = State::Telnet;
    } else {
      Terminal.write("Unable to connect to remote host\r\n");
      state = State::Prompt;
    }
  } else {
    Terminal.write("Mistake\r\n");
    state = State::Prompt;
  }
}


void exe_telnet()
{
  // process data from remote host (up to 1024 codes at the time)
  for (int i = 0; client.available() && i < 1024; ++i) {
    int c = client.read();
    if (c == 0xFF) {
      // IAC (Interpret As Command)
      uint8_t cmd = clientWaitForChar();
      uint8_t opt = clientWaitForChar();
      if (cmd == 0xFD && opt == 0x1F) {
        // DO WINDOWSIZE
        client.write("\xFF\xFB\x1F", 3); // IAC WILL WINDOWSIZE
        client.write("\xFF\xFA\x1F" "\x00\x50\x00\x19" "\xFF\xF0", 9);  // IAC SB WINDOWSIZE 0 80 0 25 IAC SE
      } else if (cmd == 0xFD && opt == 0x18) {
        // DO TERMINALTYPE
        client.write("\xFF\xFB\x18", 3); // IAC WILL TERMINALTYPE
      } else if (cmd == 0xFA && opt == 0x18) {
        // SB TERMINALTYPE
        c = clientWaitForChar();  // bypass '1'
        c = clientWaitForChar();  // bypass IAC
        c = clientWaitForChar();  // bypass SE
        client.write("\xFF\xFA\x18\x00" "wsvt25" "\xFF\xF0", 12); // IAC SB TERMINALTYPE 0 "...." IAC SE
      } else {
        uint8_t pck[3] = {0xFF, 0, opt};
        if (cmd == 0xFD)  // DO -> WONT
          pck[1] = 0xFC;
        else if (cmd == 0xFB) // WILL -> DO
          pck[1] = 0xFD;
        client.write(pck, 3);
      }
    } else {
      Terminal.write(c);
    }
  }
  // process data from terminal (keyboard)
  while (Terminal.available()) {
    client.write( Terminal.read() );
  }
  // return to prompt?
  if (!client.connected()) {
    client.stop();
    state = State::Prompt;
  }
}


void exe_ping()
{
  char host[64];
  auto inputLine = LineEditor.get();
  int pcount = sscanf(inputLine, "ping %s", host);
  if (pcount > 0) {
    int sent = 0, recv = 0;
    fabgl::ICMP icmp;
    while (true) {

      // CTRL-C ?
      if (Terminal.available() && Terminal.read() == 0x03)
        break;

      int t = icmp.ping(host);
      if (t >= 0) {
        Terminal.printf("%s %d %dms\r\n", icmp.hostIP().toString().c_str(), icmp.receivedSeq(), (int)((double)t/1000.0));
        delay(1000);
        ++recv;
      } else if (t == -2) {
        Terminal.printf("Cannot resolve %s: Unknown host\r\n", host);
        break;
      } else {
        Terminal.printf("Timeout s=%d\r\n", icmp.receivedSeq());
      }
      ++sent;

    }
    if (sent > 0) {
      Terminal.printf("--- %s ping statistics ---\r\n", host);
      Terminal.printf("%d packets transmitted, %d packets received, %.1f%% packet loss\r\n", sent, recv, (double)(sent - recv) / sent * 100.0);
    }
  }
  state = State::Prompt;
}


void setup()
{
  //Serial.begin(115200); // DEBUG ONLY

  PS2Controller.begin(PS2Preset::KeyboardPort0);

  I2C.begin(OLED_SDA, OLED_SCL);

  DisplayController.begin(&I2C, OLED_ADDR, OLED_RESET);
  DisplayController.setResolution(OLED_128x64);

  Terminal.begin(&DisplayController);
  Terminal.connectLocally();      // to use Terminal.read(), available(), etc..
  //Terminal.setLogStream(Serial);  // DEBUG ONLY

  Terminal.setBackgroundColor(Color::Black);
  Terminal.setForegroundColor(Color::BrightGreen);
  Terminal.loadFont(&fabgl::FONT_6x8);
  Terminal.clear();

  Terminal.enableCursor(true);

  currentScript = AUTOEXEC;
}


void loop()
{
  switch (state) {

    case State::Prompt:
      exe_prompt();
      break;

    case State::PromptInput:
      exe_promptInput();
      break;

    case State::Help:
      exe_help();
      break;

    case State::Info:
      exe_info();
      break;

    case State::Wifi:
      exe_wifi();
      break;

    case State::TelnetInit:
      exe_telnetInit();
      break;

    case State::Telnet:
      exe_telnet();
      break;

    case State::Scan:
      exe_scan();
      break;

    case State::Ping:
      exe_ping();
      break;

    case State::Reset:
      ESP.restart();
      break;

    case State::UnknownCommand:
      Terminal.write("\r\nMistake\r\n");
      state = State::Prompt;
      break;

    default:
      Terminal.write("\r\nNot Implemeted\r\n");
      state = State::Prompt;
      break;

  }
}

