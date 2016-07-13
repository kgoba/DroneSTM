#include "gpsdata.h"

#include <cstring>
#include <cstdio>
#include <cmath>

class SimpleTokenizer {
public:
  SimpleTokenizer(char *str, char delimiter) {
    _str = str;
    _del = delimiter;
  }

  char * next() {
    if (_str == 0) return 0;
    
    char *result = _str;
    char *nextPtr = strchr(_str, _del);
    if (nextPtr == 0) {
      _str = 0;
    }
    else {
      _str = nextPtr;
    }
    return result;
  }
  
private:
  char *_str;
  char  _del;
};

class BufString {
public:
  BufString(char *buffer, int bufSize, int length = 0) {
    _buffer = buffer;
    _capacity = bufSize;
    _length = length;
  }

  BufString(char *string) {
    _buffer = string;
    _capacity = _length = strlen(string);
  }
  
  BufString & operator += (const BufString &other) {
    if (other._length + _length > _capacity) {
      // overflow
      //_length = -1;
    }
    else {
      memcpy(_buffer + _length, other._buffer, other._length);
    }
  }

  BufString & operator += (const char c) {
    if (1 + _length > _capacity) {
      // overflow
      //_length = -1;
    }
    else {
      _buffer[_length] = c;
      _length++;
    }
  }
  
  BufString & operator += (const int n) {
    if (8 + _length > _capacity) {
      // overflow
      //_length = -1;
    }
    else {
      int rc = sprintf(_buffer + _length, "%d", n);
      if (rc > 0) {
        _length += rc;
      }
    }
  }
  
  BufString & operator += (const float n) {
    if (8 + _length > _capacity) {
      // overflow
      //_length = -1;
    }
    else {
      int rc = sprintf(_buffer + _length, "%f", n);
      if (rc > 0) {
        _length += rc;
      }
    }
  }
  
private:
  char *  _buffer;
  int  _capacity;
  int  _length;
};




 TK102Packet::TK102Packet() {
    datetime[0] = '\0';
    allowedNumber[0] = '\0';
    gpsTime[0] = '\0';
    gpsDate[0] = '\0';
    status[0] = '\0';
    imei[0] = '\0';
    mcc[0] = '\0';
    mnc[0] = '\0';
    lac[0] = '\0';
    cellID[0] = '\0';
  }

void TK102Packet::buildPacket(char *buf, int bufSize)
{
  float latitudeAbs = fabsf(latitude);
  float latitudeDeg = floorf(latitudeAbs);
  float latitudeMin = 60 * (latitudeAbs - latitudeDeg);
  char latitudeSign = (latitude >= 0) ? 'N' : 'S';

  float longitudeAbs = fabsf(longitude);
  float longitudeDeg = floorf(longitudeAbs);
  float longitudeMin = 60 * (longitudeAbs - longitudeDeg);
  char longitudeSign = (longitude >= 0) ? 'E' : 'W';
  
  uint8_t nmeaChecksum = 0;
  uint16_t checksum = 0;
  
  // GPRMC,201120.000,A,5657.5108,N,02410.6618,E,0.00,67.76,110716,,,A*5D
  
  snprintf(buf, bufSize, 
    "%.12s,%.20s,"  
    "GPRMC,%.10s,A,%02.0f%02.4f,%c,%03.0f%02.4f,%c,"  
    "%.1f,%.1f,%.6s,%s,%s,A*%02x," 
    "%c,%.8s,%.16s,%d,%.1f,%c%.2f"  
    "%d%d%s%s%s%s",
    datetime, allowedNumber, 
    gpsTime, latitudeDeg, latitudeMin, latitudeSign, longitudeDeg, longitudeMin, longitudeSign,
    speedKnots, course, gpsDate, "", "", nmeaChecksum,
    fix, status, imei, satCount, altitude, batteryStatus, batteryVoltage, 
    lastPacketSize, checksum, mcc, mnc, lac, cellID
  );
}


/*
 * getGPS() response: 
 * 
 *  <GNSS run status>,    1   0-1
 * <Fix status>,          1   0-1
 * <UTC date & Time>,     18  yyyyMMddhhmmss.sss
 * <Latitude>,            10  +dd.dddddd
 * <Longitude>,           11  +ddd.dddddd
 * <MSL Altitude>,        8
 * <Speed Over Ground>,   6
 * <Course Over Ground>,  6
 *  <Fix Mode>,
 *  <Reserved1>,
 *  <HDOP>,
 *  <PDOP>,
 *  <VDOP>,
 *  <Reserved2>,
 *  <GNSS Satellites in View>, 
 * <GNSS Satellites Used>, 
 *  <GLONASS Satellites Used>,
 *  <Reserved3>,
 *  <C/N0 max>,
 *  <HPA>,
 *  <VPA> 
 */

bool TK102Packet::update(char *str) {
  SimpleTokenizer tokenizer(str, ',');
  char *tok;
  
  // <GNSS run status>,     1   0-1
  tok = tokenizer.next();
  if (! tok) return false;
  
  // <Fix status>,          1   0-1
  tok = tokenizer.next();
  if (! tok) return false;
  if (strcmp(tok, "1") == 0) {
    fix = 'F';
  }
  else {
    fix = 'L';
  }
  
  // <UTC date & Time>,     18  yyyyMMddhhmmss.sss
  tok = tokenizer.next();
  if (! tok) return false;  
  if (strlen(tok) < 14) return false;
  memcpy(datetime, tok + 2, 12);    // Convert to yyMMddhhmmss
  memcpy(gpsDate, tok + 2, 6);      // Convert to yyMMdd
  memcpy(gpsTime, tok + 8, strlen(tok) - 8);     // Convert to hhmmss[.sss]
  
  // <Latitude>,            10  +dd.dddddd
  tok = tokenizer.next();
  if (! tok) return false;
  sscanf(tok, "%f", &latitude);
  
  // <Longitude>,           11  +ddd.dddddd
  tok = tokenizer.next();
  if (! tok) return false;
  sscanf(tok, "%f", &longitude);
  
  // <MSL Altitude>,        8
  tok = tokenizer.next();
  if (! tok) return false;
  sscanf(tok, "%f", &altitude);  

  // <Speed Over Ground>,   6
  tok = tokenizer.next();
  if (! tok) return false;
  float speedKMH;
  sscanf(tok, "%f", &speedKMH);    
  speedKnots = speedKMH / 1.852;    // Convert km/h to knots
  
  // <Course Over Ground>,  6
  tok = tokenizer.next();
  if (! tok) return false;
  sscanf(tok, "%f", &course);  
  
  // Skip 7 fields
  for (int index = 0; index < 7; index++) {
    tok = tokenizer.next();
    if (! tok) return false;
  }
  
  // <GNSS Satellites Used>
  tok = tokenizer.next();
  if (! tok) return false;
  sscanf(tok, "%d", &satCount);  
  
  return true;
}
