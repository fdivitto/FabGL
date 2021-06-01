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


#include <string.h>

#include "MCP23S17.h"


namespace fabgl {





MCP23S17::MCP23S17()
  : m_SPIDevHandle(nullptr)
{
}


MCP23S17::~MCP23S17()
{
  end();
}


bool MCP23S17::begin(int MISO, int MOSI, int CLK, int CS, int CSActiveState, int host)
{
  // defaults
  int defMISO = 35;
  int defMOSI = 12;
  int defCLK  = 14;
  int defCS   = -1;
  switch (getChipPackage()) {
    case ChipPackage::ESP32PICOD4:
      // setup for TTGO VGA32
      defMISO = 2;
      defMOSI = 12;
      break;
    case ChipPackage::ESP32D0WDQ5:
      // setup for FabGL compatible board
      defCS = 13;
      if (CSActiveState == -1)
        CSActiveState = 1;
      break;
    default:
      break;
  }

  if (MISO == -1)
    MISO = defMISO;
  if (MOSI == -1)
    MOSI = defMOSI;
  if (CS == -1)
    CS = defCS;
  if (CLK == -1)
    CLK = defCLK;

  m_MISO    = int2gpio(MISO);
  m_MOSI    = int2gpio(MOSI);
  m_CLK     = int2gpio(CLK);
  m_CS      = int2gpio(CS);
  m_SPIHost = (spi_host_device_t) host;

  bool r = SPIBegin(CSActiveState);
  if (r) {
    // disable sequential mode
    // select bank 0
    writeReg(MCP_IOCON, MCP_IOCON_SEQOP);
  }
  return r;
}


// - disable sequential mode
// - select bank 0
// - enable hardware address
void MCP23S17::initDevice(uint8_t hwAddr)
{
  writeReg(MCP_IOCON, MCP_IOCON_SEQOP | MCP_IOCON_HAEN);
}


void MCP23S17::end()
{
  SPIEnd();
}


bool MCP23S17::SPIBegin(int CSActiveState)
{
  spi_bus_config_t busconf = { }; // zero init
  busconf.mosi_io_num     = m_MOSI;
  busconf.miso_io_num     = m_MISO;
  busconf.sclk_io_num     = m_CLK;
  busconf.quadwp_io_num   = -1;
  busconf.quadhd_io_num   = -1;
  busconf.flags           = SPICOMMON_BUSFLAG_MASTER;
  auto r = spi_bus_initialize(m_SPIHost, &busconf, MCP_DMACHANNEL);
  if (r == ESP_OK || r == ESP_ERR_INVALID_STATE) {  // ESP_ERR_INVALID_STATE, maybe spi_bus_initialize already called
    spi_device_interface_config_t devconf = { };  // zero init
    devconf.mode           = 0;
    devconf.clock_speed_hz = MCP_SPI_FREQ;
    devconf.spics_io_num   = m_CS;
    devconf.flags          = (CSActiveState == 1 ? SPI_DEVICE_POSITIVE_CS : 0);
    devconf.queue_size     = 1;
    return spi_bus_add_device(m_SPIHost, &devconf, &m_SPIDevHandle) == ESP_OK;
  }
  return false;
}


void MCP23S17::SPIEnd()
{
  if (m_SPIDevHandle) {
    spi_bus_remove_device(m_SPIDevHandle);
    m_SPIDevHandle = nullptr;
    spi_bus_free(m_SPIHost);  // this will not free bus if there is a device still connected (ie sdcard)
  }
}


void MCP23S17::writeReg(uint8_t addr, uint8_t value, uint8_t hwAddr)
{
  spi_device_acquire_bus(m_SPIDevHandle, portMAX_DELAY);

  uint8_t txdata[3] = { (uint8_t)(0b01000000 | (hwAddr << 1)), addr, value };
  spi_transaction_t ta;
  ta.flags      = 0;
  ta.length     = 24;
  ta.rxlength   = 0;
  ta.rx_buffer  = nullptr;
  ta.tx_buffer  = txdata;
  spi_device_transmit(m_SPIDevHandle, &ta);

  spi_device_release_bus(m_SPIDevHandle);
}


uint8_t MCP23S17::readReg(uint8_t addr, uint8_t hwAddr)
{
  spi_device_acquire_bus(m_SPIDevHandle, portMAX_DELAY);

  uint8_t txdata[3] = { (uint8_t)(0b01000001 | (hwAddr << 1)), addr };
  uint8_t rxdata[3] = { 0 };
  spi_transaction_t ta;
  ta.flags      = 0;
  ta.length     = 24;
  ta.rxlength   = 24;
  ta.rx_buffer  = rxdata;
  ta.tx_buffer  = txdata;
  uint8_t r = 0;
  if (spi_device_transmit(m_SPIDevHandle, &ta) == ESP_OK)
    r = rxdata[2];

  spi_device_release_bus(m_SPIDevHandle);

  return r;
}


void MCP23S17::writeReg16(uint8_t addr, uint16_t value, uint8_t hwAddr)
{
  spi_device_acquire_bus(m_SPIDevHandle, portMAX_DELAY);

  uint8_t txdata[4] = { (uint8_t)(0b01000000 | (hwAddr << 1)), addr, (uint8_t)(value & 0xff), (uint8_t)(value >> 8) };
  spi_transaction_t ta;
  ta.flags      = 0;
  ta.length     = 32;
  ta.rxlength   = 0;
  ta.rx_buffer  = nullptr;
  ta.tx_buffer  = txdata;
  spi_device_transmit(m_SPIDevHandle, &ta);

  spi_device_release_bus(m_SPIDevHandle);
}


uint16_t MCP23S17::readReg16(uint8_t addr, uint8_t hwAddr)
{
  spi_device_acquire_bus(m_SPIDevHandle, portMAX_DELAY);

  uint8_t txdata[4] = { (uint8_t)(0b01000001 | (hwAddr << 1)), addr };
  uint8_t rxdata[4] = { 0 };
  spi_transaction_t ta;
  ta.flags      = 0;
  ta.length     = 32;
  ta.rxlength   = 32;
  ta.rx_buffer  = rxdata;
  ta.tx_buffer  = txdata;
  uint16_t r = 0;
  if (spi_device_transmit(m_SPIDevHandle, &ta) == ESP_OK)
    r = rxdata[2] | (rxdata[3] << 8);

  spi_device_release_bus(m_SPIDevHandle);

  return r;
}


void MCP23S17::enableINTMirroring(bool value, uint8_t hwAddr)
{
  int iocon = readReg(MCP_IOCON, hwAddr);
  writeReg(MCP_IOCON, value ? iocon | MCP_IOCON_MIRROR : iocon & ~MCP_IOCON_MIRROR, hwAddr);
}


void MCP23S17::enableINTOpenDrain(bool value, uint8_t hwAddr)
{
  int iocon = readReg(MCP_IOCON, hwAddr);
  writeReg(MCP_IOCON, value ? iocon | MCP_IOCON_ODR : iocon & ~MCP_IOCON_ODR, hwAddr);
}


void MCP23S17::setINTActiveHigh(bool value, uint8_t hwAddr)
{
  int iocon = readReg(MCP_IOCON, hwAddr);
  writeReg(MCP_IOCON, value ? iocon | MCP_IOCON_INTPOL : iocon & ~MCP_IOCON_INTPOL, hwAddr);
}


void MCP23S17::configureGPIO(int gpio, MCPDir dir, bool pullup, uint8_t hwAddr)
{
  uint8_t mask = MCP_GPIO2MASK(gpio);
  // direction
  uint8_t reg = MCP_GPIO2REG(MCP_IODIR, gpio);
  if (dir == MCPDir::Input)
    writeReg(reg, readReg(reg, hwAddr) | mask, hwAddr);
  else
    writeReg(reg, readReg(reg, hwAddr) & ~mask, hwAddr);
  // pull-up
  reg = MCP_GPIO2REG(MCP_GPPU, gpio);
  writeReg(reg, (readReg(reg, hwAddr) & ~mask) | ((int)pullup * mask), hwAddr);
}


void MCP23S17::writeGPIO(int gpio, bool value, uint8_t hwAddr)
{
  uint8_t olat = readReg(MCP_GPIO2REG(MCP_OLAT, gpio), hwAddr);
  uint8_t mask = MCP_GPIO2MASK(gpio);
  uint8_t reg  = MCP_GPIO2REG(MCP_OLAT, gpio);
  writeReg(reg, value ? olat | mask : olat & ~mask, hwAddr);
}


bool MCP23S17::readGPIO(int gpio, uint8_t hwAddr)
{
  return readReg(MCP_GPIO2REG(MCP_GPIO, gpio), hwAddr) & MCP_GPIO2MASK(gpio);
}


void MCP23S17::enableInterrupt(int gpio, MCPIntTrigger trigger, bool defaultValue, uint8_t hwAddr)
{
  uint8_t mask = MCP_GPIO2MASK(gpio);
  // set interrupt trigger
  if (trigger == MCPIntTrigger::DefaultChange) {
    // interrupt triggered when value is different than "defaultValue)
    writeReg(MCP_GPIO2REG(MCP_INTCON, gpio), readReg(MCP_GPIO2REG(MCP_INTCON, gpio), hwAddr) | mask, hwAddr);
    writeReg(MCP_GPIO2REG(MCP_DEFVAL, gpio), (readReg(MCP_GPIO2REG(MCP_DEFVAL, gpio), hwAddr) & ~mask) | ((int)defaultValue * mask), hwAddr);
  } else {
    // interrupt triggered when value is different than previous value
    writeReg(MCP_GPIO2REG(MCP_INTCON, gpio), readReg(MCP_GPIO2REG(MCP_INTCON, gpio), hwAddr) & ~mask, hwAddr);
  }
  // enable interrupt
  writeReg(MCP_GPIO2REG(MCP_GPINTEN, gpio), readReg(MCP_GPIO2REG(MCP_GPINTEN, gpio), hwAddr) | mask, hwAddr);
}


void MCP23S17::disableInterrupt(int gpio, uint8_t hwAddr)
{
  uint8_t reg = MCP_GPIO2REG(MCP_GPINTEN, gpio);
  writeReg(reg, readReg(reg, hwAddr) & !MCP_GPIO2MASK(gpio), hwAddr);
}


void MCP23S17::writePort(int port, void const * buffer, size_t length, uint8_t hwAddr)
{
  // - disable sequential mode
  // - select bank 1 (to avoid switching between A and B registers)
  uint8_t iocon = readReg(MCP_IOCON, hwAddr);
  writeReg(MCP_IOCON, iocon | MCP_IOCON_SEQOP | MCP_IOCON_BANK);

  spi_device_acquire_bus(m_SPIDevHandle, portMAX_DELAY);

  spi_transaction_ext_t ta = { 0 };
  ta.command_bits    = 8;
  ta.address_bits    = 8;
  ta.base.cmd        = 0b01000000 | (hwAddr << 1);  // write
  ta.base.addr       = MCP_BNK1_OLAT + port * 0x10;
  ta.base.flags      = SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_ADDR;
  ta.base.length     = 16 + 8 * length;
  ta.base.rxlength   = 0;
  ta.base.rx_buffer  = nullptr;
  ta.base.tx_buffer  = buffer;
  spi_device_polling_transmit(m_SPIDevHandle, (spi_transaction_t*) &ta);

  spi_device_release_bus(m_SPIDevHandle);

  // restore IOCON
  writeReg(MCP_BNK1_IOCON, iocon);
}


void MCP23S17::readPort(int port, void * buffer, size_t length, uint8_t hwAddr)
{
  // - disable sequential mode
  // - select bank 1 (to avoid switching between A and B registers)
  uint8_t iocon = readReg(MCP_IOCON, hwAddr);
  writeReg(MCP_IOCON, iocon | MCP_IOCON_SEQOP | MCP_IOCON_BANK);

  spi_device_acquire_bus(m_SPIDevHandle, portMAX_DELAY);

  spi_transaction_ext_t ta = { 0 };
  ta.command_bits    = 8;
  ta.address_bits    = 8;
  ta.base.cmd        = 0b01000001 | (hwAddr << 1);  // read
  ta.base.addr       = MCP_BNK1_GPIO + port * 0x10;
  ta.base.flags      = SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_ADDR;
  ta.base.length     = 16 + 8 * length;
  ta.base.rxlength   = 8 * length;
  ta.base.rx_buffer  = buffer;
  ta.base.tx_buffer  = nullptr;
  spi_device_polling_transmit(m_SPIDevHandle, (spi_transaction_t*) &ta);

  spi_device_release_bus(m_SPIDevHandle);

  // restore IOCON
  writeReg(MCP_BNK1_IOCON, iocon);
}







} // fabgl namespace

