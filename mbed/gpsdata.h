#include <stdint.h>

class TK102Packet {
public:
  /// Initialize with reasonable defaults
  TK102Packet();
  
  /// Update packet fields from GPSINF data 
  bool update(char *str);
  
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
  int   lastPacketSize;     // 160
  
  // GSM cell information used for location
  char  mcc[5];
  char  mnc[5];
  char  lac[8];
  char  cellID[6];

private:

};
