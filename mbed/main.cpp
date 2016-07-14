#include "mbed.h"
#include "rtos.h"
#include "SDFileSystem.h"
#include "ff.h"

#include "Adafruit_FONA.h"
#include "gpsdata.h"
#include "pubnub.h"
#include "vario.h"
#include "baro.h"

const PinName I2CSDAPin = PB_7;
const PinName I2CSCLPin = PB_6;

const PinName SD_MOSI   = PC_12;
const PinName SD_MISO   = PC_11;
const PinName SD_SCK    = PC_10;
const PinName SD_NSS    = PD_1;

#define SIM_PIN1        "2216"

#define GPRS_APN        "internet.lmt.lv"
#define GPRS_USER       ""
#define GPRS_PASS       ""
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
Variometer vario;

//SDFileSystem sd(SD_MOSI, SD_MISO, SD_SCK, SD_NSS, "sd");

void publishLocation(Adafruit_FONA &fona);

volatile int  gpsStatus = 0;
volatile int  gprsStatus = 0;
volatile int  networkStatus = 0;

void ledTimerTask(void const *argument) {
  static uint8_t gpsCounter = 0;
  static uint8_t gprsCounter = 0;
  static uint8_t netCounter = 0;
  static bool isOn = false;
  
  if (!isOn) {
    led1 = (gpsCounter < 0) ? 1 : 0;
    led2 = (gprsCounter < 0) ? 1 : 0;
    led3 = (netCounter < 0) ? 1 : 0;
  }
  else {
    led1 = (gpsCounter < 0)  ? 1 : (gpsCounter < gpsStatus);
    led2 = (gprsCounter < 0) ? 1 : (gprsCounter < gprsStatus);
    led3 = (netCounter < 0)  ? 1 : (netCounter < networkStatus);
    
    if (++gpsCounter > 4) gpsCounter = 0;
    if (++gprsCounter > 4) gprsCounter = 0;    
    if (++netCounter > 4) netCounter = 0;    
  }
  isOn = !isOn;
}

void trackingTask(void const *argument) 
{
    fona.begin(9600);

    dbg.printf("Unlocking SIM...\n");
    fona.unlockSIM(SIM_PIN1);
        
    dbg.printf("Enabling GPS...\n");
    fona.enableGPS(true);
    
    dbg.printf("Enabling GPRS...\n");
    fona.enableTCPGPRS(false);
    fona.setGPRSNetworkSettings(GPRS_APN, GPRS_USER, GPRS_PASS);
    gprsStatus = fona.enableTCPGPRS(true) ? -1 : 0;
       
    dbg.printf("Entering loop...\n");
    
    while(1) {
        networkStatus = fona.getNetworkStatus();
        gpsStatus = fona.GPSstatus();
        gprsStatus = fona.GPRSstate();
        
        if (userButton) 
        {
          led4 = 1;
          publishLocation(fona);
          Thread::wait(1000);
          led4 = 0;
        }

        dbg.printf("Sleeping...\n");
        Thread::wait(3000);
    }  
}

void varioMeasure(void const *argument) {
  static float lastPressure = 0;
  const float dt = 0.020;
  
  if (barometer.update())
  {
    float pressure = barometer.getPressure();
    vario.update(pressure, dt);
    lastPressure = pressure;
  }
  else vario.update(lastPressure, dt);
}

float climbThreshold = 0.10f;
float sinkThreshold = -0.10f;

void varioTask(void const *argument) {    
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
    
    // Calculate initial pressure by averaging measurements
    int idx = 0;
    float avgPressure = 0;
    while (idx < 50) {
      if (barometer.update()) {
        avgPressure += barometer.getPressure();
        idx++;
      }
    }
    avgPressure /= idx;
    vario.reset(avgPressure);

    // Start vario update timer
    RtosTimer measureTimer(varioMeasure, osTimerPeriodic, NULL);  
    measureTimer.start(20);

    // Keep checking vertical speed
    idx = 0;
    int period = 10;
    bool phase = false;
    while (true) {
      if (idx >= period) {
        //if (fabsf(vertSpeed) > 0.1f) {
        //  dbg.printf("Vertical speed: % .1f\tPressure: %.0f\n", vertSpeed, pFilt);
        //}
        
        float vertSpeed = vario.getVerticalSpeed();
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

void testTask1(void const *argument) {
  FILE *fp = fopen("/sd/myfile.txt", "w");
  fprintf(fp, "Hello World!\n");
  fclose(fp);
  
  Thread::wait(osWaitForever);
}

void testTask2(void const *argument) {
  FATFS fs;
  FIL fp;
  FRESULT fr;

  // Mount filesystem
  fr = f_mount(&fs, "", 1);
  
  while (!userButton) 
  {
    Thread::wait(100);
  }
  // Open file for writing
  fr = f_open(&fp, "myfile.txt", FA_CREATE_NEW);
  if (!fr) {
    // Write single line and close the file
    char str[] = "Hello World!\n";
    //f_puts(str, &fp);
    UINT bw;
    f_write (&fp, (const void*) str, strlen(str), &bw);
    fr = f_close(&fp);
  }
  
  // Unmount filesystem
  fr = f_mount(NULL, "", 0);
  
  Thread::wait(osWaitForever);
}


#define STACK_SIZE DEFAULT_STACK_SIZE

int main() 
{
    buzzer = 0;

    dbg.baud(9600);
    dbg.printf("Reset!\n");

    RtosTimer ledTimer(ledTimerTask, osTimerPeriodic, NULL);  
    ledTimer.start(250);

    //Thread varioThread(varioTask, NULL, osPriorityNormal, STACK_SIZE);
    //Thread trackingThread(trackingTask, NULL, osPriorityNormal, STACK_SIZE);
    Thread testThread(testTask2, NULL, osPriorityNormal, 6000);
        
    //varioTask(); 
    //trackingTask();
    
    /*
    char *dir = "/sd/";
    DIR *dp;
    struct dirent *dirp;
    dp = opendir(dir);
    //read all directory and file names in current directory 
    while((dirp = readdir(dp)) != NULL) {
        dbg.printf("::: %s\n", dirp->d_name);
    }
    closedir(dp);
    */
    
    while (true) 
    {
        // Just toggle LED to indicate the main (idle) loop
        led8 = !led8;
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

void publishLocation(Adafruit_FONA &fona) 
{
  char data[120];
  int nChars = fona.getGPS(0, data, 120);
  
  if (nChars == 0) {
    return;
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
  
  if (latitude == 0 || longitude == 0) return;
  
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
    if (success) {
      led5 = 1;
      //Thread::wait(500);
      //led5 = 0;
    }
    else {
      dbg.printf("Connection error\n");
      //led5 = 1;
      //Thread::wait(100);
      led5 = 0;
    }
  }
}
