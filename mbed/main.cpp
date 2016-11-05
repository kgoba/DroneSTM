#include "mbed.h"
#include "rtos.h"
#include "SDFileSystem.h"

#include "debug.h"
#include "Adafruit_FONA.h"
#include "gpsdata.h"
#include "pubnub.h"
#include "vario.h"
#include "baro.h"
#include "crc.h"

const PinName I2CSDAPin = PB_7;
const PinName I2CSCLPin = PB_6;

const PinName SD_MOSI   = PC_12;
const PinName SD_MISO   = PC_11;
const PinName SD_SCK    = PC_10;
const PinName SD_NSS    = PD_1;

struct Settings {
  char pin[8];
  char gprsAPN[32];
  char gprsUser[32];
  char gprsPass[32];
  int  rangeHor;
  int  rangeVert;
  char alertPhone[32];
};

Settings gSettings;

/*
#define SIM_PIN1        "8398"

//#define GPRS_APN        "internet.lmt.lv"
#define GPRS_APN        "data.tele2.lv"
#define GPRS_USER       ""
#define GPRS_PASS       ""
* */

#define HTTP_USERAGENT  "Mozilla/4.0"

#define TRACK_SERVER	"sns.lv"
#define TRACK_PORT		9001

DigitalOut led1(LED3);		// red		indicates GPRS connection status
DigitalOut led2(LED4);		// blue		indicates GPS lock
DigitalOut led3(LED5);		// orange	indicated GSM status
DigitalOut led4(LED6);		// green
DigitalOut led5(LED7);		// green
DigitalOut led6(LED8);		// orange
DigitalIn  userButton(USER_BUTTON);

PwmOut buzzer(PB_4);

DigitalOut fonaRST(PA_1, 1);
DigitalOut fonaKey(PF_4, 1);
DigitalIn  fonaPstat(PA_4);

// PinName tx, PinName rx, PinName rst, PinName ringIndicator
Adafruit_FONA fona(PA_2, PA_3, PF_4, PA_0);

I2C sensorBus(I2CSDAPin, I2CSCLPin);
Barometer barometer(sensorBus);

//Variometer vario;

SDFileSystem sd(SD_MOSI, SD_MISO, SD_SCK, SD_NSS, "sd");

bool publishLocation(Adafruit_FONA &fona);
bool publishLocation2(Adafruit_FONA &fona);

TK102Packet packet;

volatile int  gpsStatus = 0;
volatile int  gprsStatus = 0;
volatile int  networkStatus = 0;

void ledTimerTask(void const *argument) {
  static uint8_t gpsCounter = 0;
  static uint8_t gprsCounter = 0;
  static uint8_t netCounter = 0;
  static bool isOn = false;
  
  if (!isOn) {
    led1 = (gprsCounter < 0) ? 1 : 0;
    led2 = (gpsCounter < 0) ? 1 : 0;
    led3 = (netCounter < 0) ? 1 : 0;
  }
  else {
    led1 = (gprsCounter < 0) ? 1 : (gprsCounter < gprsStatus);
    led2 = (gpsCounter < 0)  ? 1 : (gpsCounter < gpsStatus);
    led3 = (netCounter < 0)  ? 1 : (netCounter < networkStatus);
    
    if (++gpsCounter > 4) gpsCounter = 0;
    if (++gprsCounter > 4) gprsCounter = 0;    
    if (++netCounter > 4) netCounter = 0;    
  }
  isOn = !isOn;
}

void beepTimes(uint8_t times) {
	int frequency = 880;
	buzzer.period(1.0f / frequency);
	for (; times > 0; times--) {
		buzzer = 0.2f;
		Thread::wait(100);
		buzzer = 0;
		if (times > 1) {
			Thread::wait(400);
		}
	}
}

void beepSuccess(bool success) {
	int frequency1 = 440;
	int frequency2 = 660;
	buzzer.period(1.0f / (success ? frequency1 : frequency2));
	buzzer = 0.5f;
	Thread::wait(success ? 200 : 100);
	buzzer.period(1.0f / (success ? frequency2 : frequency1));
	Thread::wait(success ? 100 : 400);
	buzzer = 0;
}

void testFonaTask(void const *argument) 
{	
    // Check if FONA is powered and turn it on if not
    while (fonaPstat.read() == 0) {
      dbg.printf("Powering up FONA...\n");
      fonaKey = 0;
      Thread::wait(2000);
      fonaKey = 1;
      Thread::wait(500);
    }
  
    fona.begin(9600);

    char imei[16];
    if (fona.getIMEI(imei)) {
      dbg.printf("IMEI: %.16s\n", imei);
      packet.setIMEI(imei);
    }

    dbg.printf("Unlocking SIM...\n");
    fona.unlockSIM(gSettings.pin);
    
    Thread::wait(500);
    fona.printf("AT+CGNSPWR=1\n");
    Thread::wait(500);
    fona.printf("AT+CGNSTST=1\n");
    
    while (true) {
      if (fona.readable()) {
        dbg.putc(fona.getc());
      }
      if (dbg.readable()) {
        int c = dbg.getc();
        dbg.putc(c);
        fona.putc(c);
      }
    }
}

void trackingTask(void const *argument) 
{	
    //CRC16<0xa001, true> crc;
    //uint16_t crcValue = crc.update("123456789");
    //dbg.printf("CRC check value: %04x\n", crcValue);

    // Check if FONA is powered and turn it on if not
    while (fonaPstat.read() == 0) {
      dbg.printf("Powering up FONA...\n");
      fonaKey = 0;
      Thread::wait(2000);
      fonaKey = 1;
      Thread::wait(500);
    }
  
    fona.begin(9600);

    char imei[16];
    if (fona.getIMEI(imei)) {
      dbg.printf("IMEI: %.16s\n", imei);
      packet.setIMEI(imei);
    }

    dbg.printf("Unlocking SIM...\n");
    fona.unlockSIM(gSettings.pin);
            
    dbg.printf("Enabling GPS...\n");
    fona.enableGPS(true);
    
    dbg.printf("Enabling GPRS...\n");
    fona.enableTCPGPRS(false);                                      // Disable 
    fona.setGPRSNetworkSettings(gSettings.gprsAPN, gSettings.gprsUser, gSettings.gprsPass);    // Configure
    gprsStatus = fona.enableTCPGPRS(true) ? -1 : 0;                 // Reenable
    
    bool startLocationValid = false;
    Location2D startLocation;
    Location2D currentLocation;
    bool wasOutside = false;
    
    //fona.sendSMS("0037129170012", "Test");
    //fona.sendSMS(gSettings.alertPhone, "Test");
    
    dbg.printf("Entering loop...\n");
    while(1) {
      networkStatus = fona.getNetworkStatus();
      int newGPSStatus = fona.GPSstatus();
      if (newGPSStatus != gpsStatus) {
        gpsStatus = newGPSStatus;
        dbg.printf("GPS status: %d\n", gpsStatus);
        if (gpsStatus > 0) {
          uint8_t times = (gpsStatus > 3) ? 3 : gpsStatus;
          beepTimes(times);
        }
      }
      gprsStatus = fona.GPRSstate();
          
      if (!fona.TCPconnected()) {
        dbg.printf("Establishing TCP/IP connection...");
        bool success = fona.TCPconnect(TRACK_SERVER, TRACK_PORT);
        dbg.printf(success ? "SUCCESS\n" : "FAILED\n");
      }
          
      //if (userButton) 
      if (gpsStatus == 3) 
      {
        float lat, lon, alt;
        if (fona.getGPS(&lat, &lon, &alt)) {
          if (!startLocationValid) {
            dbg.printf("Setting starting location: (%.5f, %.5f, %.1f)\n", lat, lon, alt);
            startLocation.latitude = lat;
            startLocation.longitude = lon;
            startLocation.altitude = alt;
            startLocationValid = true;
          }
          else {
            currentLocation.latitude = lat;
            currentLocation.longitude = lon;
            currentLocation.altitude = alt;
            float range = currentLocation.metersTo(startLocation);
            float vrange = alt - startLocation.altitude;
            dbg.printf("Current location: (%.5f, %.5f, %.1f), range %.1fm, vrange %.1fm\n", lat, lon, alt, range, vrange);
            
            if (range > gSettings.rangeHor || vrange > gSettings.rangeVert) {
              // Start continuous beeping
              buzzer = 0.5f;
              if (!wasOutside) {
                wasOutside = true;
                fona.sendSMS(gSettings.alertPhone, "Drone outside boundaries");
              }
            }
            else {
              // Stop beeping
              buzzer = 0;
              if (wasOutside) {
                wasOutside = false;
                fona.sendSMS(gSettings.alertPhone, "Drone back inside boundaries");
              }
            }
          }
        }
        
        dbg.printf("Publishing location...");
        led4 = 1;
        bool success = publishLocation2(fona);
        beepSuccess(success);
        if (success) {
          dbg.printf("SUCCESS\n");
          led5 = 1;
        }
        else {
          dbg.printf("FAILED\n");
          led5 = 0;
        }          
        Thread::wait(1000);
        led4 = 0;
      }

      //dbg.printf("Sleeping...\n");
      Thread::wait(3000);
    }  
}

bool readSettings() {
  FILE *fp = fopen("/sd/config.txt", "r");
  if (fp == 0) {
    dbg.printf("SD read error (file cannot be opened)\n");
    return false;
  }
  else {
    char line[100];
    line[0] = 0;

    int idx = 0;
    while (true) {
      if (0 == fgets(line, 100, fp)) break;
      
      // Remove trailing newline symbols
      char *ptr = line;
      while (*ptr) {
        if (*ptr == '\r' || *ptr == '\n') {
          *ptr = '\0';
          break;
        }
        ptr++;
      }
      
      if (line[0] != '#' && line[0] != '\0') {
        dbg.printf("SD Read: %s\n", line);
        switch (idx) {
          case 0: strncpy(gSettings.pin, line, 8); break;
          case 1: sscanf(line, "%d", &gSettings.rangeVert); break;
          case 2: sscanf(line, "%d", &gSettings.rangeHor); break;
          case 3: strncpy(gSettings.alertPhone, line, 32); break;
          case 4: strncpy(gSettings.gprsAPN, line, 32); break;
          case 5: strncpy(gSettings.gprsUser, line, 32); break;
          case 6: strncpy(gSettings.gprsPass, line, 32); break;
        }
        idx++;
      }
    }
    
    fclose(fp);   
    if (idx < 6) return false;
  }
  
  return true;
}


#define STACK_SIZE DEFAULT_STACK_SIZE

int main() 
{
    buzzer = 0;

    dbg.baud(9600);
    dbg.printf("Reset!\n");
    
    Thread::wait(500);
    
    int nTry = 5;
    while (nTry > 0) {
      if (readSettings()) break;
      Thread::wait(100);
      nTry--;
    }
    
    if (nTry == 0) {
      for (int i = 0; i < 5; i++) {
        beepSuccess(false);
        Thread::wait(50);
      }
    }

    RtosTimer ledTimer(ledTimerTask, osTimerPeriodic, NULL);  
    ledTimer.start(250);

    Thread trackingThread(trackingTask, NULL, osPriorityNormal, STACK_SIZE);
            
    while (true) 
    {
        // Just toggle LED to indicate the main (idle) loop
        //led8 = !led8;
        Thread::wait(500);
    }
}





#define PUBNUB_PUBKEY   "pub-c-fda1ae26-6edf-4fc3-82f7-3943616bcb2e"
#define PUBNUB_SUBKEY   "sub-c-43e0fc72-3f8b-11e6-85a4-0619f8945a4f"
#define PUBNUB_CHANNEL  "Location"
#define PUBNUB_SERVER   "pubsub.pubnub.com"

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


bool publishData(Adafruit_FONA &fona, char *data) 
{
  const char *part1 = "GET /publish/" PUBNUB_PUBKEY "/" PUBNUB_SUBKEY "/0/" PUBNUB_CHANNEL "/0/";
  const char *part2 = " HTTP/1.0\r\n\r\n";

  // Length (with one extra character for the null terminator)
  //int str_len = strlen(part1) + strlen(data) + strlen(part2) + 1; 

  // Prepare the character array (the buffer) 
  char http_cmd[512];
  strcpy(http_cmd, part1);
  //strcat(http_cmd, data);
  encodeURL(http_cmd + strlen(http_cmd), 512 - strlen(http_cmd), data);
  strcat(http_cmd, part2);

  dbg.printf("%s\n", http_cmd);

  if (false == fona.TCPconnect(PUBNUB_SERVER, 80)) {
    return false;
  } else {
    dbg.printf("connected\n");
    //Serial.println(data);
  }

  fona.TCPsend(http_cmd, strlen(http_cmd));
  
  uint16_t length = fona.TCPavailable();
  dbg.printf("Response length: %d\n", length);
  while (length > 0) {
    if (length > 512) length = 512;
    fona.TCPread((uint8_t *)http_cmd, length);
    dbg.printf("%.512s", http_cmd);
    length = fona.TCPavailable();
  }
  dbg.printf("\n");

  fona.TCPclose();
  return true;
}


bool publishData2(Adafruit_FONA &fona, char *data) {
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
    return false;
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
  return true;
}

char publishStr[120];

bool publishLocation(Adafruit_FONA &fona) 
{
  char data[120];
  int nChars = fona.getGPS(0, data, 120);
  
  if (nChars == 0) {
    return false;
  }

  char *latitude = "\"\"";
  char *longitude = "\"\"";
  char *altitude = "\"\"";
  char *speed = "\"\"";
  char *satUsed = "\"\"";
  
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
    else if (index == 5) {
      altitude = start;
    }
    else if (index == 6) {
      speed = start;
    }    
    else if (index == 15) {
      satUsed = start;
    }
    start = pos + 1;
    index++;
  }
  
  if (latitude == 0 || longitude == 0) return false;
  
  bool validLocation = false;
  strcpy(publishStr, "{");
  strcat(publishStr, "\"lat\":");
  strcat(publishStr, latitude);
  strcat(publishStr, ",\"lng\":");
  strcat(publishStr, longitude);
  strcat(publishStr, ",\"alt\":");
  strcat(publishStr, altitude);
  strcat(publishStr, ",\"spd\":");
  strcat(publishStr, speed);
  strcat(publishStr, ",\"sat\":");
  strcat(publishStr, satUsed);  
  validLocation = true;
  strcat(publishStr, "}");

  if (validLocation) {
    bool success = publishData(fona, publishStr);
    return success;
  }
  return false;
}

bool publishLocation2(Adafruit_FONA &fona) 
{
	char data[180];
	packet.update(fona);
	packet.buildPacket(data, 180);
  
	//dbg.printf("TK102: %s\n", data);
  
	if (false == fona.TCPsend(data, strlen(data)))
		return false;
	return fona.TCPsend("\n", 1);
}
