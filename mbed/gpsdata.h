#include <stdint.h>
#include <cmath>


#include "Adafruit_FONA.h"

struct Location2D {
  float latitude;
  float longitude;
  float altitude;

  Location2D(float latitude = 0, float longitude = 0, float altitude = 0) {
    this->latitude = latitude;
    this->longitude = longitude;
    this->altitude = altitude;
  }
  
  float metersTo(const Location2D &other) {
    float phi1 = latitude / 180.0f * M_PI;
    float phi2 = other.latitude / 180.0f * M_PI;
    float dphi = phi1 - phi1;
    float dlam = (other.longitude - longitude) / 180.0f * M_PI;

    float a = sinf(dphi/2) * sinf(dphi/2) +
            cosf(phi1) * cosf(phi2) *
            sinf(dlam/2) * sin(dlam/2);
    float c = 2 * atan2f(sqrtf(a), sqrtf(1-a));

    float R = 6371e3; // metres (radius)
    return R * c;
  }
};

class TK102Packet {
public:
  /// Initialize with reasonable defaults
  TK102Packet(char *imei = 0);
  
  void setIMEI(char *imei);
  
  /// Update packet fields from GPSINF data 
  bool update(char *str);
  bool update(Adafruit_FONA &fona);
  
  /// Build TK102 sentence (packet)
  void buildPacket(char *buf, int bufSize);
  
  // GPRMC fields
  char  gpsDate[6];
  char  gpsTime[10];
  float latitude;
  float longitude;
  float speedKnots;
  float course;
  
  // Other data fields
  char  datetime[12];        // 160711201120
  char  allowedNumber[20];   // 29319915175061207 (empty here)
  char  fix;                 // (F)ix / (L)ast
  char  status[8];           // START
  char  imei[16];            // 123456789012345
  int   satCount;            // 10
  float altitude;           // 12.4
  char  batteryStatus;      // (F)ull / (L)ow
  float batteryVoltage;     // 4.24
  bool  externalPower;
  int   lastPacketSize;     // 160
  
  // GSM cell information used for location
  char  mcc[5];
  char  mnc[5];
  char  lac[8];
  char  cellID[6];

private:

  uint8_t getNMEAChecksum(char *nmea);
};
