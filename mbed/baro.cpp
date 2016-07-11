#include "baro.h"

MS5607::MS5607(I2C &bus, byte subAddress) : _bus(bus) {
  _address = (0x76 | (subAddress & 1)) << 1;
}
    
bool MS5607::reset() {
  char cmd = kCmdReset;
  return 0 == _bus.write(_address, &cmd, 1);
}

bool MS5607::readPROM(byte address, word &result) {
  byte cmd[1];
  cmd[0] = kCmdReadPROM | ((address & 0x07) << 1);
  if (0 != _bus.write(_address, (char *)cmd, 1)) {
    return false;
  }
  
  byte reply[2];
  if (0 != _bus.read(_address, (char *)reply, 2)) {
    return false;
  }

  result = ((word)reply[0] << 8) | reply[1];
  return true;
}

bool MS5607::startConversion(ConversionType type, OversamplingRate osr) {
  byte cmd[1];
  cmd[0] = kCmdConvert | ((byte)type << 4) | ((byte)osr << 1);
  return 0 == _bus.write(_address, (char *)cmd, 1);
}

bool MS5607::readResult24(long &result) {
  byte cmd[1];
  cmd[0] = kCmdReadADC;
  if (0 != _bus.write(_address, (char *)cmd, 1)) {
    return false;
  }
  
  byte reply[3];
  if (0 != _bus.read(_address, (char *)reply, 3)) {
    return false;
  }
 
  result = ((long)reply[0] << 16) | ((word)reply[1] << 8) | reply[2];
  return true;
}

bool MS5607::readResult16(word &result) {
  byte cmd[1];
  cmd[0] = kCmdReadADC;
  if (0 != _bus.write(_address, (char *)cmd, 1)) {
    return false;
  }
  
  byte reply[2];
  if (0 != _bus.read(_address, (char *)reply, 2)) {
    return false;
  }
  
  result = ((word)reply[0] << 8) | reply[1];
  return true;
}

Barometer::Barometer(I2C &bus, byte subaddress) 
  : MS5607(bus, subaddress) 
{
  
}

bool Barometer::initialize() 
{
  for (byte idx = 0; idx < 7; idx++) {
    if (!readPROM(idx, PROM[idx])) 
      return false;
  }
  return true;
}

/**
 * Measures and updates both temperature and pressure
 */
bool Barometer::update() {
  p = t = 0;
  if (!startConversion(kTemperature, kOSR1024)) {
    return false;
  }
 
  wait_ms(2);
  long d2;
  if (!readResult24(d2)) {
    return false;
  }
  int32_t dt = d2 - ((int32_t)PROM[5] << 8);
  t = 2000 + ((dt * PROM[6]) >> 23);
  
  if (!startConversion(kPressure, kOSR4096)) {
    return false;
  }

  wait_ms(10);
  long d1;
  if (!readResult24(d1)) {
    return false;
  }
  uint64_t off  = ((uint64_t)PROM[2] << 17) + ((PROM[4] * (int64_t)dt) >> 6);
  uint64_t sens = ((uint64_t)PROM[1] << 16) + ((PROM[3] * (int64_t)dt) >> 7);
  p = (((d1 * sens) >> 21) - off) >> 15;

  //uint16_t off  = PROM[2] + ((PROM[4] * (int32_t)dt) >> 15);
  //uint16_t sens = PROM[1] + ((PROM[3] * (int32_t)dt) >> 15);
  //p = (((uint32_t)d1 * sens) >> 14) - off;

  return true;
}

/// Returns temperature in Celsium x100 (2000 = 20.00 C)
int16_t Barometer::getTemperature() {
  return t;
}

/// Returns pressure in Pascals (100000 = 100000 Pa = 1000 mbar)
uint32_t Barometer::getPressure() {
  return p;
}
