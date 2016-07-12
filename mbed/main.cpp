#include "mbed.h"
#include "Adafruit_FONA.h"
#include "rtos.h"

#include "baro.h"

const PinName I2CSDAPin = PB_7;
const PinName I2CSCLPin = PB_6;

#define SIM_PIN1        "2216"

#define GPRS_APN        "internet.lmt.lv"
#define GPRS_USER       ""
#define GPRS_PASS       ""

#define PUBNUB_PUBKEY   "pub-c-fda1ae26-6edf-4fc3-82f7-3943616bcb2e"
#define PUBNUB_SUBKEY   "sub-c-43e0fc72-3f8b-11e6-85a4-0619f8945a4f"
#define PUBNUB_CHANNEL  "Location"
#define PUBNUB_SERVER   "pubsub.pubnub.com"
#define HTTP_USERAGENT  "Mozilla/4.0"

DigitalOut led1(LED3);
DigitalOut led2(LED4);
DigitalOut led3(LED5);
DigitalOut led4(LED6);
DigitalOut led5(LED7);
DigitalOut led8(LED8);
DigitalIn  userButton(USER_BUTTON);

PwmOut buzzer(PB_4);
 
Serial dbg(PA_9, PA_10);
Adafruit_FONA fona(PA_2, PA_3, PF_4, PA_0);

I2C sensorBus(I2CSDAPin, I2CSCLPin);
Barometer barometer(sensorBus);

void led3_ticker() {
  led3 = userButton;
}

void publishData(char *data) 
{
    const char *part1 = "GET /publish/" PUBNUB_PUBKEY "/" PUBNUB_SUBKEY "/0/" PUBNUB_CHANNEL "/0/";
    const char *part2 = " HTTP/1.0\r\n\r\n";

    // Length (with one extra character for the null terminator)
    int str_len = strlen(part1) + strlen(data) + strlen(part2) + 1; 

    // Prepare the character array (the buffer) 
    char http_cmd[str_len];
    strcpy(http_cmd, part1);
    strcat(http_cmd, data);
    strcat(http_cmd, part2);

    dbg.printf("%s\n", http_cmd);

    if (false == fona.TCPconnect(PUBNUB_SERVER, 80)) {
      dbg.printf("connection error\n");
      led5 = 1;
      Thread::wait(100);
      led5 = 0;
      return;
    } else {
      dbg.printf("connected\n");
      //Serial.println(data);
    }

    fona.TCPsend(http_cmd, strlen(http_cmd));

    led5 = 1;
    Thread::wait(500);
    led5 = 0;
    
    uint16_t length = fona.TCPavailable();
    dbg.printf("Response length: %d\n", length);
    if (length > 0) {
      if (length > 512) length = 512;
      fona.TCPread((uint8_t *)http_cmd, length);
      dbg.printf("%s\n", http_cmd);
    }

    fona.TCPclose();

}

int encodeURL(char *output, uint16_t maxSize, const char *input) {
  //const char *reserved = "!*'();:@&=+$,/?%#[] \"-.<>\\^_`{|}~";
  const char *reserved = " !#$&'()*+/;=?@[]{}\"";
  char c;
  while (c = *input) {
    if (maxSize == 1) {
      *output = 0;
      return -1;
    }
    if (strchr(reserved, c) != 0) {
      if (maxSize < 3) return -1;
      char nibble1 = (c >> 4) & 0x0F;
      char nibble2 = c & 0x0F;
      nibble1 += (nibble1 < 10) ? '0' : ('a' - 10);
      nibble2 += (nibble2 < 10) ? '0' : ('a' - 10);
      *output++ = '%';
      *output++ = nibble1;
      *output++ = nibble2;
      maxSize -= 3;
    }
    else {
      if (maxSize < 1) return -1;
      *output++ = c;
      maxSize -= 1;
    }
    input++;
  }
  if (maxSize < 1) return -1;
  *output = 0;
  return 0;
}

void publishData2(char *data) {
  uint16_t statuscode;
  uint16_t length;
  const char part1[] = "http://" PUBNUB_SERVER "/publish/" PUBNUB_PUBKEY "/" PUBNUB_SUBKEY "/0/" PUBNUB_CHANNEL "/0/";
  //const int part1Length = sizeof(part1) / sizeof(char);
  
  char url[512];
  strcpy(url, part1);
  //char *end = url + (sizeof(part1) / sizeof(char));
  //strcpy(url + strlen(url), data);
  encodeURL(url + strlen(url), 512 - strlen(url), data);
  //strcat(url, data);

  //fona.sendSMS("29170012", end);
  dbg.printf("%s\n", data);
  //dbg.printf("%s\n", end);
  dbg.printf("%s\n", url);
   
  if (!fona.HTTP_GET_start(url, &statuscode, (uint16_t *)&length)) {
    led5 = 1;
    Thread::wait(100);
    led5 = 0;
    return;
  }
  dbg.printf("HTTP status: %d\n", statuscode);
  dbg.printf("Response length: %d\n", length);
  while (length > 0) {
    if (fona.readable()) {
      char c = fona.getc();
      dbg.putc(c);
      length--;
    }
  }
  dbg.putc('\n');
  fona.HTTP_GET_end();
  led5 = 1;
  Thread::wait(500);
  led5 = 0;
}

void publishLocation() 
{
  char data[120];
  int nChars = fona.getGPS(0, data, 120);
  
  if (nChars == 0) {
    return;
  }

  char *latitude = 0;
  char *longitude = 0;
  
  int index = 0;
  char *start = data;
  while (true) {
    char *pos = strchr(start, ',');
    if (pos == 0) break;
    *pos = 0;
    if (index == 3) {
      latitude = start;
    }
    else if (index == 4) {
      longitude = start;
    }
    start = pos + 1;
    index++;
  }
  
  if (latitude == 0 || longitude == 0) return;
  
  strcpy(data, "{");
  if (strlen(latitude) > 0) {
    strcat(data, "\"lat\":");
    strcat(data, latitude);
    if (strlen(longitude) > 0) {
      strcat(data, ",\"lng\":");
      strcat(data, longitude);
    }
  }
  strcat(data, "}");

  //publishData2(data);
  //publishData2("{\"lat\":57,\"lng\":24}");
  publishData2("{\"abc\":5}");
}


void trackingTask(void const *argument) 
{
    fona.begin(9600);

    dbg.printf("Unlocking SIM...\n");
    fona.unlockSIM(SIM_PIN1);
        
    dbg.printf("Enabling GPS...\n");
    fona.enableGPS(true);
    
    dbg.printf("Enabling GPRS...\n");
    led4 = fona.enableGPRS(false);
    fona.setGPRSNetworkSettings(GPRS_APN, GPRS_USER, GPRS_PASS);
    led4 = fona.enableGPRS(true);
    
    //fona.setUserAgent(HTTP_USERAGENT);
    //fona.HTTP_ssl(true);
    
    dbg.printf("Entering loop...\n");
    
    while(1) {
        uint8_t status = fona.getNetworkStatus();
        //led1 = !led1;
        
        for (uint8_t idx = 0; idx < status; idx++) {
          led1 = 0;
          Thread::wait(200);
          led1 = 1;
          Thread::wait(200);
        }
        led1 = 0;

        status = 1 + fona.GPSstatus();
        //led1 = !led1;
        
        for (uint8_t idx = 0; idx < status; idx++) {
          led2 = 0;
          Thread::wait(200);
          led2 = 1;
          Thread::wait(200);
        }
        led2 = 0;

        if (userButton) {
          led3 = 1;
          
          publishLocation();
          Thread::wait(1000);
          led3 = 0;
        }

        dbg.printf("Sleeping...\n");
        Thread::wait(1000);
        //led2 = !led2;
    }  
}

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

class TK102Packet {
public:
  TK102Packet();
  
  char  datetime[12];        // 160711201120
  char  allowedNumber[20];   // 29319915175061207 (empty here)
  
  // GPRMC,201120.000,A,5657.5108,N,02410.6618,E,0.00,67.76,110716,,,A*5D
  char  gpsTime[10];
  float latitude;
  float longitude;
  float speedKnots;
  float course;
  char  gpsDate[6];
  
  char  fix;                 // (F)ix / (L)ast
  char  status[8];           // START
  char  imei[16];            // 123456789012345
  int   satCount;            // 10
  float altitude;           // 12.4
  char  batteryStatus;      // (F)ull / (L)ow
  float batteryVoltage;     // 4.24
  int   lastPacketSize;     // 160
  char  mcc[5];
  char  mnc[5];
  char  lac[8];
  char  cellID[6];

  bool update(char *str);
  void buildPacket(char *buf, int bufSize);

private:

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

struct GPSInfo {
  
  //bool getGPSOn() { return substr(0, 1); };
  
public:
  char rawString[95];
};

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

    volatile float pFilt = 0;
    volatile float lastPressure = 0;
    volatile float dpFilt = 0;

    float dpPerMeter = 12.0f;
    float baroAvgTime = 0.400f;
    float climbAvgTime = 0.400f;
    float climbThreshold = 0.10f;
    float sinkThreshold = -0.10f;
    float dt = 0.020f;   // Approximate measurement interval in seconds

void varioMeasure(void const *argument) {
  if (barometer.update())
  {
    float pressure = barometer.getPressure();        
    pFilt += (dt / baroAvgTime) * ((float)pressure - pFilt);     // Approx 33 Hz update rate
  }

  float dp = (pFilt - lastPressure) / dt;
  dpFilt += (dt / climbAvgTime) * (dp - dpFilt); 
  
  lastPressure = pFilt;
}

void varioTask(void const *argument) {
    buzzer = 0;
    
    //sensorBus.frequency(100000);
    if (barometer.reset()) {
      dbg.printf("Barometer reset!\n");
    }
    else {
      dbg.printf("Barometer not found!\n");
    }
    Thread::wait(50);
    
    if (!barometer.initialize()) {
      dbg.printf("Failed to initialize!\n");
    }
    
    int idx = 0;

    
    // Calculate initial pressure by averaging measurements
    while (idx < 50) {
      if (barometer.update())
      {
        float pressure = barometer.getPressure();
        pFilt += pressure;
      }
      idx++;
    }
    pFilt /= idx;
    lastPressure = pFilt;

    RtosTimer measureTimer(varioMeasure, osTimerPeriodic, NULL);  
    measureTimer.start(20);

    dbg.printf("Conversion in progress...\n");
    idx = 0;
    int period = 10;
    bool phase = false;
    while (true) {
      if (idx >= period) {
        //if (fabsf(vertSpeed) > 0.1f) {
        //  dbg.printf("Vertical speed: % .1f\tPressure: %.0f\n", vertSpeed, pFilt);
        //}
        
        float vertSpeed = -dpFilt / dpPerMeter;
        if (vertSpeed > climbThreshold) {
          float frequency = 440 + 220 * vertSpeed;
          buzzer.period(1.0f / frequency);
          period = 20 - 7 * vertSpeed;
          if (period < 3) period = 3;
          buzzer = phase ? 0.5f : 0;
        }
        else if (vertSpeed < sinkThreshold) {
          //float frequency = 440 + 220 * vertSpeed;
          //buzzer.period(1.0f / frequency);
          //buzzer = 0.5f;
          buzzer = 0;
        }
        else {
          buzzer = 0;
        }
        
        phase = !phase;
        idx = 0;
      }
        
      idx++;
      Thread::wait(15);
    }
}

#define STACK_SIZE DEFAULT_STACK_SIZE

int main() 
{
    dbg.baud(9600);
    dbg.printf("Reset!\n");

    Thread varioThread(varioTask, NULL, osPriorityNormal, STACK_SIZE);
    Thread trackingThread(trackingTask, NULL, osPriorityNormal, STACK_SIZE);
        
    //varioTask(); 
    //trackingTask();
    
    while (true) 
    {
        // Just toggle LED to indicate the main (idle) loop
        led8 = !led8;
        Thread::wait(500);
    }
}
