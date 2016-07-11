/***************************************************
  This is a library for our Adafruit FONA Cellular Module

  Designed specifically to work with the Adafruit FONA
  ----> http://www.adafruit.com/products/1946
  ----> http://www.adafruit.com/products/1963

  These displays use TTL Serial to communicate, 2 pins are required to
  interface
  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing
  products from Adafruit!

  Written by Limor Fried/Ladyada for Adafruit Industries.
  BSD license, all text above must be included in any redistribution
 ****************************************************/
 
 /*
  *  Modified by Marc PLOUHINEC 27/06/2015 for use in mbed
  */
#include <algorithm>
#include "Adafruit_FONA.h"

#define HIGH 1
#define LOW 0

bool Adafruit_FONA::begin(int baudrate) {
    mySerial.baud(baudrate);
    mySerial.attach(this, &Adafruit_FONA::onSerialDataReceived, Serial::RxIrq);
    
    _rstpin = HIGH;
    wait_ms(10);
    _rstpin = LOW;
    wait_ms(100);
    _rstpin = HIGH;
    
    // give 3 seconds to reboot
    wait_ms(3000);
    
    while (readable()) getc();
    
    sendCheckReply("AT", "OK");
    wait_ms(100);
    sendCheckReply("AT", "OK");
    wait_ms(100);
    sendCheckReply("AT", "OK");
    wait_ms(100);
    
    // turn off Echo!
    sendCheckReply("ATE0", "OK");
    wait_ms(100);
    
    if (! sendCheckReply("ATE0", "OK")) {
        return false;
    }
    
    return true;
}

void Adafruit_FONA::setEventListener(EventListener *eventListener) {
    this->eventListener = eventListener;
}

/********* Stream ********************************************/

int Adafruit_FONA::_putc(int value) {
    return mySerial.putc(value);
}

int Adafruit_FONA::_getc() {
    __disable_irq(); // Start Critical Section - don't interrupt while changing global buffer variables
    
    // Wait for data if the buffer is empty
    if (isRxBufferEmpty()) {
        __enable_irq(); // End Critical Section - need to allow rx interrupt to get new characters for buffer
        
        while(isRxBufferEmpty());
        
        __disable_irq(); // Start Critical Section - don't interrupt while changing global buffer variables
    }
    
    int data = rxBuffer[rxBufferOutIndex];
    incrementRxBufferOutIndex();
    
    __enable_irq(); // End Critical Section
    
    return data;
}

int Adafruit_FONA::readable() {
    return !isRxBufferEmpty();
}

void Adafruit_FONA::onSerialDataReceived() {
    while (mySerial.readable() && !isRxBufferFull()) {
        int data = mySerial.getc();
        rxBuffer[rxBufferInIndex] = data;
        
        //
        // Analyze the received data in order to detect events like RING or NO CARRIER
        //
        
        // Copy the data in the current line
        if (currentReceivedLineSize < RX_BUFFER_SIZE && data != '\r' && data != '\n') {
            currentReceivedLine[currentReceivedLineSize] = (char) data;
            currentReceivedLineSize++;
        }
        
        // Check if the line is complete
        if (data == '\n') {
            currentReceivedLine[currentReceivedLineSize] = 0;
            
            if (eventListener != NULL) {
                // Check if we have a special event
                if (strcmp(currentReceivedLine, "RING") == 0) {
                    eventListener->onRing();
                } else if (strcmp(currentReceivedLine, "NO CARRIER") == 0) {
                    eventListener->onNoCarrier();
                }
            }
            
            currentReceivedLineSize = 0;
        }
        
        incrementRxBufferInIndex();
    }
}

/********* Real Time Clock ********************************************/

bool Adafruit_FONA::enableRTC(uint8_t i) {
    if (! sendCheckReply("AT+CLTS=", i, "OK")) 
        return false;
    return sendCheckReply("AT&W", "OK");
}

/********* BATTERY & ADC ********************************************/

/* returns value in mV (uint16_t) */
bool Adafruit_FONA::getBattVoltage(uint16_t *v) {
    return sendParseReply("AT+CBC", "+CBC: ", v, ',', 2);
}

/* returns the percentage charge of battery as reported by sim800 */
bool Adafruit_FONA::getBattPercent(uint16_t *p) {
    return sendParseReply("AT+CBC", "+CBC: ", p, ',', 1);
}

bool Adafruit_FONA::getADCVoltage(uint16_t *v) {
    return sendParseReply("AT+CADC?", "+CADC: 1,", v);
}

/********* SIM ***********************************************************/

bool Adafruit_FONA::unlockSIM(char *pin)
{
    char sendbuff[14] = "AT+CPIN=";
    sendbuff[8] = pin[0];
    sendbuff[9] = pin[1];
    sendbuff[10] = pin[2];
    sendbuff[11] = pin[3];
    sendbuff[12] = NULL;
    
    return sendCheckReply(sendbuff, "OK");
}

uint8_t Adafruit_FONA::getSIMCCID(char *ccid) {
    getReply("AT+CCID");
    // up to 20 chars
    strncpy(ccid, replybuffer, 20);
    ccid[20] = 0;
    
    readline(); // eat 'OK'
    
    return strlen(ccid);
}

/********* IMEI **********************************************************/

uint8_t Adafruit_FONA::getIMEI(char *imei) {
    getReply("AT+GSN");
    
    // up to 15 chars
    strncpy(imei, replybuffer, 15);
    imei[15] = 0;
    
    readline(); // eat 'OK'
    
    return strlen(imei);
}

/********* NETWORK *******************************************************/

uint8_t Adafruit_FONA::getNetworkStatus(void) {
    uint16_t status;
    
    if (! sendParseReply("AT+CREG?", "+CREG: ", &status, ',', 1)) return 0;
    
    return status;
}


uint8_t Adafruit_FONA::getRSSI(void) {
    uint16_t reply;
    
    if (! sendParseReply("AT+CSQ", "+CSQ: ", &reply) ) return 0;
    
    return reply;
}

/********* AUDIO *******************************************************/

bool Adafruit_FONA::setAudio(uint8_t a) {
    // 0 is headset, 1 is external audio
    if (a > 1) return false;
    
    return sendCheckReply("AT+CHFA=", a, "OK");
}

uint8_t Adafruit_FONA::getVolume(void) {
    uint16_t reply;
    
    if (! sendParseReply("AT+CLVL?", "+CLVL: ", &reply) ) return 0;
    
    return reply;
}

bool Adafruit_FONA::setVolume(uint8_t i) {
    return sendCheckReply("AT+CLVL=", i, "OK");
}


bool Adafruit_FONA::playDTMF(char dtmf) {
    char str[4];
    str[0] = '\"';
    str[1] = dtmf;
    str[2] = '\"';
    str[3] = 0;
    return sendCheckReply("AT+CLDTMF=3,", str, "OK");
}

bool Adafruit_FONA::playToolkitTone(uint8_t t, uint16_t len) {
    return sendCheckReply("AT+STTONE=1,", t, len, "OK");
}

bool Adafruit_FONA::setMicVolume(uint8_t a, uint8_t level) {
    // 0 is headset, 1 is external audio
    if (a > 1) return false;
    
    return sendCheckReply("AT+CMIC=", a, level, "OK");
}

/********* FM RADIO *******************************************************/


bool Adafruit_FONA::FMradio(bool onoff, uint8_t a) {
    if (! onoff) {
        return sendCheckReply("AT+FMCLOSE", "OK");
    }
    
    // 0 is headset, 1 is external audio
    if (a > 1) return false;
    
    return sendCheckReply("AT+FMOPEN=", a, "OK");
}

bool Adafruit_FONA::tuneFMradio(uint16_t station) {
    // Fail if FM station is outside allowed range.
    if ((station < 870) || (station > 1090))
        return false;
    
    return sendCheckReply("AT+FMFREQ=", station, "OK");
}

bool Adafruit_FONA::setFMVolume(uint8_t i) {
    // Fail if volume is outside allowed range (0-6).
    if (i > 6) {
    return false;
    }
    // Send FM volume command and verify response.
    return sendCheckReply("AT+FMVOLUME=", i, "OK");
}

int8_t Adafruit_FONA::getFMVolume() {
    uint16_t level;
    
    if (! sendParseReply("AT+FMVOLUME?", "+FMVOLUME: ", &level) ) return 0;
    
    return level;
}

int8_t Adafruit_FONA::getFMSignalLevel(uint16_t station) {
    // Fail if FM station is outside allowed range.
    if ((station < 875) || (station > 1080)) {
        return -1;
    }   
    
    // Send FM signal level query command.
    // Note, need to explicitly send timeout so right overload is chosen.
    getReply("AT+FMSIGNAL=", station, FONA_DEFAULT_TIMEOUT_MS);
    // Check response starts with expected value.
    char *p = strstr(replybuffer, "+FMSIGNAL: ");
    if (p == 0) return -1;
    p+=11;
    // Find second colon to get start of signal quality.
    p = strchr(p, ':');
    if (p == 0) return -1;
    p+=1;
    // Parse signal quality.
    int8_t level = atoi(p);
    readline();  // eat the "OK"
    return level;
}

/********* PWM/BUZZER **************************************************/

bool Adafruit_FONA::setPWM(uint16_t period, uint8_t duty) {
    if (period > 2000) return false;
    if (duty > 100) return false;
    
    return sendCheckReply("AT+SPWM=0,", period, duty, "OK");
}

/********* CALL PHONES **************************************************/
bool Adafruit_FONA::callPhone(char *number) {
    char sendbuff[35] = "ATD";
    strncpy(sendbuff+3, number, min((int)30, (int)strlen(number)));
    uint8_t x = strlen(sendbuff);
    sendbuff[x] = ';';
    sendbuff[x+1] = 0;
    
    return sendCheckReply(sendbuff, "OK");
}

bool Adafruit_FONA::hangUp(void) {
    return sendCheckReply("ATH0", "OK");
}

bool Adafruit_FONA::pickUp(void) {
    return sendCheckReply("ATA", "OK");
}

void Adafruit_FONA::onIncomingCall() {
#ifdef ADAFRUIT_FONA_DEBUG
    printf("> Incoming call...\r\n");
#endif
    _incomingCall = true;
}

bool Adafruit_FONA::callerIdNotification(bool enable) {
    if(enable){
        _ringIndicatorInterruptIn.fall(this, &Adafruit_FONA::onIncomingCall);
        return sendCheckReply("AT+CLIP=1", "OK");
    }
    
    _ringIndicatorInterruptIn.fall(NULL);
    return sendCheckReply("AT+CLIP=0", "OK");
}

bool Adafruit_FONA::incomingCallNumber(char* phonenum) {
    //+CLIP: "<incoming phone number>",145,"",0,"",0
    if(!_incomingCall)
        return false;
    
    readline();
    while(!strcmp(replybuffer, "RING") == 0) {
        flushInput();
        readline();
    }
    
    readline(); //reads incoming phone number line
    
    parseReply("+CLIP: \"", phonenum, '"');
    
#ifdef ADAFRUIT_FONA_DEBUG
    printf("Phone Number: %s\r\n", replybuffer);
#endif
    
    _incomingCall = false;
    return true;
}

/********* SMS **********************************************************/

uint8_t Adafruit_FONA::getSMSInterrupt(void) {
    uint16_t reply;
    
    if (! sendParseReply("AT+CFGRI?", "+CFGRI: ", &reply) ) return 0;
    
    return reply;
}

bool Adafruit_FONA::setSMSInterrupt(uint8_t i) {
    return sendCheckReply("AT+CFGRI=", i, "OK");
}

int8_t Adafruit_FONA::getNumSMS(void) {
    uint16_t numsms;
    
    if (! sendCheckReply("AT+CMGF=1", "OK")) return -1;
    // ask how many sms are stored
    
    if (! sendParseReply("AT+CPMS?", "+CPMS: \"SM_P\",", &numsms) ) return -1;
    
    return numsms;
}

// Reading SMS's is a bit involved so we don't use helpers that may cause delays or debug
// printouts!
bool Adafruit_FONA::readSMS(uint8_t i, char *smsbuff, uint16_t maxlen, uint16_t *readlen) {
    // text mode
    if (! sendCheckReply("AT+CMGF=1", "OK")) return false;
    
    // show all text mode parameters
    if (! sendCheckReply("AT+CSDH=1", "OK")) return false;
    
    // parse out the SMS len
    uint16_t thesmslen = 0;
    
    //getReply(F("AT+CMGR="), i, 1000);  //  do not print debug!
    mySerial.printf("AT+CMGR=%d\r\n", i);
    readline(1000); // timeout
    
    // parse it out...
    if (! parseReply("+CMGR:", &thesmslen, ',', 11)) {
        *readlen = 0;
        return false;
    }
    
    readRaw(thesmslen);
    
    flushInput();
    
    uint16_t thelen = min(maxlen, (uint16_t)strlen(replybuffer));
    strncpy(smsbuff, replybuffer, thelen);
    smsbuff[thelen] = 0; // end the string
    
#ifdef ADAFRUIT_FONA_DEBUG
    printf("%s\r\n", replybuffer);
#endif
    *readlen = thelen;
    return true;
}

// Retrieve the sender of the specified SMS message and copy it as a string to
// the sender buffer.  Up to senderlen characters of the sender will be copied
// and a null terminator will be added if less than senderlen charactesr are
// copied to the result.  Returns true if a result was successfully retrieved,
// otherwise false.
bool Adafruit_FONA::getSMSSender(uint8_t i, char *sender, int senderlen) {
    // Ensure text mode and all text mode parameters are sent.
    if (! sendCheckReply("AT+CMGF=1", "OK")) return false;
    if (! sendCheckReply("AT+CSDH=1", "OK")) return false;
    // Send command to retrieve SMS message and parse a line of response.
    mySerial.printf("AT+CMGR=%d\r\n", i);
    readline(1000);
    // Parse the second field in the response.
    bool result = parseReplyQuoted("+CMGR:", sender, senderlen, ',', 1);
    // Drop any remaining data from the response.
    flushInput();
    return result;
}

bool Adafruit_FONA::sendSMS(char *smsaddr, char *smsmsg) {
    if (! sendCheckReply("AT+CMGF=1", "OK")) return -1;
    
    char sendcmd[30] = "AT+CMGS=\"";
    strncpy(sendcmd+9, smsaddr, 30-9-2);  // 9 bytes beginning, 2 bytes for close quote + null
    sendcmd[strlen(sendcmd)] = '\"';
    
    if (! sendCheckReply(sendcmd, "> ")) return false;
#ifdef ADAFRUIT_FONA_DEBUG
    printf("> %s\r\n", smsmsg);
#endif
    mySerial.printf("%s\r\n\r\n", smsmsg);
    mySerial.putc(0x1A);
#ifdef ADAFRUIT_FONA_DEBUG
    printf("^Z\r\n");
#endif
    readline(10000); // read the +CMGS reply, wait up to 10 seconds!!!
    //Serial.print("* "); Serial.println(replybuffer);
    if (strstr(replybuffer, "+CMGS") == 0) {
        return false;
    }
    readline(1000); // read OK
    //Serial.print("* "); Serial.println(replybuffer);
    
    if (strcmp(replybuffer, "OK") != 0) {
        return false;
    }
    
    return true;
}


bool Adafruit_FONA::deleteSMS(uint8_t i) {
    if (! sendCheckReply("AT+CMGF=1", "OK")) return -1;
    // read an sms
    char sendbuff[12] = "AT+CMGD=000";
    sendbuff[8] = (i / 100) + '0';
    i %= 100;
    sendbuff[9] = (i / 10) + '0';
    i %= 10;
    sendbuff[10] = i + '0';
    
    return sendCheckReply(sendbuff, "OK", 2000);
}

/********* TIME **********************************************************/

bool Adafruit_FONA::enableNetworkTimeSync(bool onoff) {
    if (onoff) {
        if (! sendCheckReply("AT+CLTS=1", "OK"))
            return false;
    } else {
        if (! sendCheckReply("AT+CLTS=0", "OK"))
            return false;
    }
    
    flushInput(); // eat any 'Unsolicted Result Code'
    
    return true;
}

bool Adafruit_FONA::enableNTPTimeSync(bool onoff, const char* ntpserver) {
    if (onoff) {
        if (! sendCheckReply("AT+CNTPCID=1", "OK"))
            return false;
        
        mySerial.printf("AT+CNTP=\"");
        if (ntpserver != 0) {
            mySerial.printf(ntpserver);
        } else {
            mySerial.printf("pool.ntp.org");
        }
        mySerial.printf("\",0\r\n");
        readline(FONA_DEFAULT_TIMEOUT_MS);
        if (strcmp(replybuffer, "OK") != 0)
            return false;
        
        if (! sendCheckReply("AT+CNTP", "OK", 10000))
            return false;
        
        uint16_t status;
        readline(10000);
        if (! parseReply("+CNTP:", &status))
            return false;
    } else {
        if (! sendCheckReply("AT+CNTPCID=0", "OK"))
            return false;
    }
    
    return true;
}

bool Adafruit_FONA::getTime(char* buff, uint16_t maxlen) {
    getReply("AT+CCLK?", (uint16_t) 10000);
    if (strncmp(replybuffer, "+CCLK: ", 7) != 0)
        return false;
    
    char *p = replybuffer+7;
    uint16_t lentocopy = min((uint16_t)(maxlen-1), (uint16_t)strlen(p));
    strncpy(buff, p, lentocopy+1);
    buff[lentocopy] = 0;
    
    readline(); // eat OK
    
    return true;
}

/********* GPS **********************************************************/


bool Adafruit_FONA::enableGPS(bool onoff) {
    uint16_t state;
    
    // first check if its already on or off
    if (! sendParseReply("AT+CGNSPWR?", "+CGNSPWR: ", &state) )
        return false;
    
    if (onoff && !state) {
        if (! sendCheckReply("AT+CGNSPWR=1", "OK"))
            return false;
    } else if (!onoff && state) {
        if (! sendCheckReply("AT+CGNSPWR=0", "OK"))
            return false;
    }
    return true;
}

int8_t Adafruit_FONA::GPSstatus(void) {

    // 808 V2 uses GNS commands and doesn't have an explicit 2D/3D fix status.
    // Instead just look for a fix and if found assume it's a 3D fix.
    getReply("AT+CGNSINF");
    char *p = strstr(replybuffer, "+CGNSINF: ");
    if (p == 0) return -1;
    p+=10;
    readline(); // eat 'OK'
    if (p[0] == '0') return 0; // GPS is not even on!

    p+=2; // Skip to second value, fix status.
    //DEBUG_PRINTLN(p);
    // Assume if the fix status is '1' then we have a 3D fix, otherwise no fix.
    if (p[0] == '1') return 3;
    else return 1;

}

uint8_t Adafruit_FONA::getGPS(uint8_t arg, char *buffer, uint8_t maxbuff) {
    int32_t x = arg;
    
    getReply("AT+CGNSINF");
    
    char *p = strstr(replybuffer, "CGNSINF: ");
    if (p == 0){
        buffer[0] = 0;
        return 0;
    }
    p+=9;
    uint8_t len = max((uint8_t)(maxbuff-1), (uint8_t)strlen(p));
    strncpy(buffer, p, len);
    buffer[len] = 0;
    
    readline(); // eat 'OK'
    return len;
}

bool Adafruit_FONA::getGPS(float *lat, float *lon, float *speed_kph, float *heading, float *altitude) {
    char gpsbuffer[120];
    
    // we need at least a 2D fix
    if (GPSstatus() < 2)
        return false;
    
    // grab the mode 2^5 gps csv from the sim808
    uint8_t res_len = getGPS(32, gpsbuffer, 120);
    
    // make sure we have a response
    if (res_len == 0)
        return false;
    
    // skip mode
    char *tok = strtok(gpsbuffer, ",");
    if (! tok) return false;
    
    // skip date
    tok = strtok(NULL, ",");
    if (! tok) return false;
    
    // skip fix
    tok = strtok(NULL, ",");
    if (! tok) return false;
    
    // grab the latitude
    char *latp = strtok(NULL, ",");
    if (! latp) return false;
    
    // grab latitude direction
    char *latdir = strtok(NULL, ",");
    if (! latdir) return false;
    
    // grab longitude
    char *longp = strtok(NULL, ",");
    if (! longp) return false;
    
    // grab longitude direction
    char *longdir = strtok(NULL, ",");
    if (! longdir) return false;
    
    double latitude = atof(latp);
    double longitude = atof(longp);
    
    // convert latitude from minutes to decimal
    float degrees = floor(latitude / 100);
    double minutes = latitude - (100 * degrees);
    minutes /= 60;
    degrees += minutes;
    
    // turn direction into + or -
    if (latdir[0] == 'S') degrees *= -1;
    
    *lat = degrees;
    
    // convert longitude from minutes to decimal
    degrees = floor(longitude / 100);
    minutes = longitude - (100 * degrees);
    minutes /= 60;
    degrees += minutes;
    
    // turn direction into + or -
    if (longdir[0] == 'W') degrees *= -1;
    
    *lon = degrees;
    
    // only grab speed if needed
    if (speed_kph != NULL) {
        
        // grab the speed in knots
        char *speedp = strtok(NULL, ",");
        if (! speedp) return false;
        
        // convert to kph
        *speed_kph = atof(speedp) * 1.852;
        
    }
    
    // only grab heading if needed
    if (heading != NULL) {
        
        // grab the speed in knots
        char *coursep = strtok(NULL, ",");
        if (! coursep) return false;
        
        *heading = atof(coursep);
    
    }
    
    // no need to continue
    if (altitude == NULL)
        return true;
    
    // we need at least a 3D fix for altitude
    if (GPSstatus() < 3)
        return false;
    
    // grab the mode 0 gps csv from the sim808
    res_len = getGPS(0, gpsbuffer, 120);
    
    // make sure we have a response
    if (res_len == 0)
        return false;
    
    // skip mode
    tok = strtok(gpsbuffer, ",");
    if (! tok) return false;
    
    // skip lat
    tok = strtok(NULL, ",");
    if (! tok) return false;
    
    // skip long
    tok = strtok(NULL, ",");
    if (! tok) return false;
    
    // grab altitude
    char *altp = strtok(NULL, ",");
    if (! altp) return false;
    
    *altitude = atof(altp);
    
    return true;
}

bool Adafruit_FONA::enableGPSNMEA(uint8_t i) {
    char sendbuff[15] = "AT+CGPSOUT=000";
    sendbuff[11] = (i / 100) + '0';
    i %= 100;
    sendbuff[12] = (i / 10) + '0';
    i %= 10;
    sendbuff[13] = i + '0';
    
    return sendCheckReply(sendbuff, "OK", 2000);
}


/********* GPRS **********************************************************/


bool Adafruit_FONA::enableGPRS(bool onoff) {
    if (onoff) {
        // disconnect all sockets
        sendCheckReply("AT+CIPSHUT", "SHUT OK", 5000);
        
        if (! sendCheckReply("AT+CGATT=1", "OK", 10000))
            return false;
       
        // set bearer profile! connection type GPRS
        if (! sendCheckReply("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"", "OK", 10000))
            return false;
        
        // set bearer profile access point name
        if (apn) {
            // Send command AT+SAPBR=3,1,"APN","<apn value>" where <apn value> is the configured APN value.
            if (! sendCheckReplyQuoted("AT+SAPBR=3,1,\"APN\",", apn, "OK", 10000))
                return false;
            
            // set username/password
            if (apnusername) {
                // Send command AT+SAPBR=3,1,"USER","<user>" where <user> is the configured APN username.
                if (! sendCheckReplyQuoted("AT+SAPBR=3,1,\"USER\",", apnusername, "OK", 10000))
                    return false;
            }
            if (apnpassword) {
                // Send command AT+SAPBR=3,1,"PWD","<password>" where <password> is the configured APN password.
                if (! sendCheckReplyQuoted("AT+SAPBR=3,1,\"PWD\",", apnpassword, "OK", 10000))
                    return false;
            }
        }
        
        // open GPRS context
        if (! sendCheckReply("AT+SAPBR=1,1", "OK", 10000))
            return false;
            
        //if (! sendCheckReply("AT+CIICR", "OK", 20000))
        //    return false;
    } else {
        // disconnect all sockets
        if (! sendCheckReply("AT+CIPSHUT", "SHUT OK", 5000))
            return false;
        
        // close GPRS context
        if (! sendCheckReply("AT+SAPBR=0,1", "OK", 10000))
            return false;
        
        if (! sendCheckReply("AT+CGATT=0", "OK", 10000))
            return false;
    }
    return true;
}

uint8_t Adafruit_FONA::GPRSstate(void) {
    uint16_t state;
    
    if (! sendParseReply("AT+CGATT?", "+CGATT: ", &state) )
        return -1;
    
    return state;
}

void Adafruit_FONA::setGPRSNetworkSettings(const char* apn, const char* ausername, const char* apassword) {
    this->apn = (char*) apn;
    this->apnusername = (char*) ausername;
    this->apnpassword = (char*) apassword;
}

bool Adafruit_FONA::getGSMLoc(uint16_t *errorcode, char *buff, uint16_t maxlen) {
    getReply("AT+CIPGSMLOC=1,1", (uint16_t)10000);
    
    if (! parseReply("+CIPGSMLOC: ", errorcode))
        return false;
    
    char *p = replybuffer+14;
    uint16_t lentocopy = min((uint16_t)(maxlen-1), (uint16_t)strlen(p));
    strncpy(buff, p, lentocopy+1);
    
    readline(); // eat OK
    
    return true;
}

bool Adafruit_FONA::getGSMLoc(float *lat, float *lon) {
    uint16_t returncode;
    char gpsbuffer[120];
    
    // make sure we could get a response
    if (! getGSMLoc(&returncode, gpsbuffer, 120))
        return false;
    
    // make sure we have a valid return code
    if (returncode != 0)
        return false;
    
    // tokenize the gps buffer to locate the lat & long
    char *latp = strtok(gpsbuffer, ",");
    if (! latp) return false;
    
    char *longp = strtok(NULL, ",");
    if (! longp) return false;
    
    *lat = atof(latp);
    *lon = atof(longp);
    
    return true;
}

/********* TCP FUNCTIONS  ************************************/


bool Adafruit_FONA::TCPconnect(char *server, uint16_t port) {
    flushInput();
    
    // close all old connections
    if (! sendCheckReply("AT+CIPSHUT", "SHUT OK", 5000) ) return false;
    
    // single connection at a time
    if (! sendCheckReply("AT+CIPMUX=0", "OK") ) return false;
    
    // manually read data
    if (! sendCheckReply("AT+CIPRXGET=1", "OK") ) return false;
    
#ifdef ADAFRUIT_FONA_DEBUG
    printf("AT+CIPSTART=\"TCP\",\"%s\",\"%d\"\r\n", server, port);
#endif
    
    mySerial.printf("AT+CIPSTART=\"TCP\",\"%s\",\"%d\"\r\n", server, port);
    
    if (! expectReply("OK")) return false;
    if (! expectReply("CONNECT OK")) return false;
    return true;
}

bool Adafruit_FONA::TCPclose(void) {
    return sendCheckReply("AT+CIPCLOSE", "OK");
}

bool Adafruit_FONA::TCPconnected(void) {
    if (! sendCheckReply("AT+CIPSTATUS", "OK", 100) ) return false;
    readline(100);
#ifdef ADAFRUIT_FONA_DEBUG
    printf("\t<--- %s\r\n", replybuffer);
#endif
    return (strcmp(replybuffer, "STATE: CONNECT OK") == 0);
}

bool Adafruit_FONA::TCPsend(char *packet, uint8_t len) {
#ifdef ADAFRUIT_FONA_DEBUG
    printf("AT+CIPSEND=%d\r\n", len);
    
    for (uint16_t i=0; i<len; i++) {
        printf(" 0x%#02x", packet[i]);
    }
    printf("\r\n");
#endif
    
    
    mySerial.printf("AT+CIPSEND=%d\r\n", len);
    readline();
#ifdef ADAFRUIT_FONA_DEBUG
    printf("\t<--- %s\r\n", replybuffer);
#endif
    if (replybuffer[0] != '>') return false;
    
    for (uint16_t i=0; i<len; i++) {
        mySerial.putc(packet[i]);
    }
    readline(3000); // wait up to 3 seconds to send the data
#ifdef ADAFRUIT_FONA_DEBUG
    printf("\t<--- %s\r\n", replybuffer);
#endif
    
    return (strcmp(replybuffer, "SEND OK") == 0);
}

uint16_t Adafruit_FONA::TCPavailable(void) {
    uint16_t avail;
    
    if (! sendParseReply("AT+CIPRXGET=4", "+CIPRXGET: 4,", &avail, ',', 0) ) return false;
    
#ifdef ADAFRUIT_FONA_DEBUG
    printf("%d bytes available\r\n", avail);
#endif
    
    return avail;
}


uint16_t Adafruit_FONA::TCPread(uint8_t *buff, uint8_t len) {
    uint16_t avail;
    
    mySerial.printf("AT+CIPRXGET=2,%d\r\n", len);
    readline();
    if (! parseReply("+CIPRXGET: 2,", &avail, ',', 0)) return false;
    
    readRaw(avail);
    
#ifdef ADAFRUIT_FONA_DEBUG
    printf("%d bytes read\r\n", avail);
    for (uint8_t i=0;i<avail;i++) {
        printf(" 0x%#02x", replybuffer[i]);
    }
    printf("\r\n");
#endif
    
    memcpy(buff, replybuffer, avail);
    
    return avail;
}

/********* HTTP LOW LEVEL FUNCTIONS  ************************************/

bool Adafruit_FONA::HTTP_init() {
    return sendCheckReply("AT+HTTPINIT", "OK");
}

bool Adafruit_FONA::HTTP_term() {
    return sendCheckReply("AT+HTTPTERM", "OK");
}

void Adafruit_FONA::HTTP_para_start(const char* parameter, bool quoted) {
    flushInput();
    
#ifdef ADAFRUIT_FONA_DEBUG
    printf("\t---> AT+HTTPPARA=\"%s\"\r\n", parameter);
#endif
    
    mySerial.printf("AT+HTTPPARA=\"%s", parameter);
    if (quoted)
        mySerial.printf("\",\"");
    else
        mySerial.printf("\",");
}

bool Adafruit_FONA::HTTP_para_end(bool quoted) {
    if (quoted)
        mySerial.printf("\"\r\n");
    else
        mySerial.printf("\r\n");
    
    return expectReply("OK");
}

bool Adafruit_FONA::HTTP_para(const char* parameter, const char* value) {
    HTTP_para_start(parameter, true);
    mySerial.printf(value);
    return HTTP_para_end(true);
}

bool Adafruit_FONA::HTTP_para(const char* parameter, int32_t value) {
    HTTP_para_start(parameter, false);
    mySerial.printf("%d", value);
    return HTTP_para_end(false);
}

bool Adafruit_FONA::HTTP_data(uint32_t size, uint32_t maxTime) {
    flushInput();
    
#ifdef ADAFRUIT_FONA_DEBUG
    printf("\t---> AT+HTTPDATA=%d,%d\r\n", size, maxTime);
#endif
    
    mySerial.printf("AT+HTTPDATA=%d,%d\r\n", size, maxTime);
    
    return expectReply("DOWNLOAD");
}

bool Adafruit_FONA::HTTP_action(uint8_t method, uint16_t *status, uint16_t *datalen, int32_t timeout) {
    // Send request.
    if (! sendCheckReply("AT+HTTPACTION=", method, "OK"))
        return false;
    
    // Parse response status and size.
    readline(timeout);
    if (! parseReply("+HTTPACTION:", status, ',', 1))
        return false;
    if (! parseReply("+HTTPACTION:", datalen, ',', 2))
        return false;
    
    return true;
}

bool Adafruit_FONA::HTTP_readall(uint16_t *datalen) {
    getReply("AT+HTTPREAD");
    if (! parseReply("+HTTPREAD:", datalen, ',', 0))
        return false;
    
    return true;
}

bool Adafruit_FONA::HTTP_ssl(bool onoff) {
    return sendCheckReply("AT+HTTPSSL=", onoff ? 1 : 0, "OK");
}

/********* HTTP HIGH LEVEL FUNCTIONS ***************************/

bool Adafruit_FONA::HTTP_GET_start(char *url, uint16_t *status, uint16_t *datalen){
    if (! HTTP_setup(url))
        return false;
    
    // HTTP GET
    if (! HTTP_action(FONA_HTTP_GET, status, datalen))
        return false;
    
#ifdef ADAFRUIT_FONA_DEBUG
    printf("Status: %d\r\n", *status);
    printf("Len: %d\r\n", *datalen);
#endif
    
    // HTTP response data
    if (! HTTP_readall(datalen))
        return false;
    
    return true;
}

void Adafruit_FONA::HTTP_GET_end(void) {
    HTTP_term();
}

bool Adafruit_FONA::HTTP_POST_start(char *url, const char* contenttype, const uint8_t *postdata, uint16_t postdatalen, uint16_t *status, uint16_t *datalen) {
    if (! HTTP_setup(url))
        return false;
    
    if (! HTTP_para("CONTENT", contenttype)) {
        return false;
    }
    
    // HTTP POST data
    if (! HTTP_data(postdatalen, 10000))
        return false;
    for (uint16_t i = 0; i < postdatalen; i++) {
        mySerial.putc(postdata[i]);
    }
    if (! expectReply("OK"))
        return false;
    
    // HTTP POST
    if (! HTTP_action(FONA_HTTP_POST, status, datalen))
        return false;
    
#ifdef ADAFRUIT_FONA_DEBUG
    printf("Status: %d\r\n", *status);
    printf("Len: %d\r\n", *datalen);
#endif
    
    // HTTP response data
    if (! HTTP_readall(datalen))
        return false;
    
    return true;
}

void Adafruit_FONA::HTTP_POST_end(void) {
    HTTP_term();
}

void Adafruit_FONA::setUserAgent(const char* useragent) {
    this->useragent = (char*) useragent;
}

void Adafruit_FONA::setHTTPSRedirect(bool onoff) {
    httpsredirect = onoff;
}

/********* HTTP HELPERS ****************************************/

bool Adafruit_FONA::HTTP_setup(char *url) {
    // Handle any pending
    HTTP_term();
    
    // Initialize and set parameters
    if (! HTTP_init())
        return false;
    if (! HTTP_para("CID", 1))
        return false;
    if (! HTTP_para("UA", useragent))
        return false;
    if (! HTTP_para("URL", url))
        return false;
    
    // HTTPS redirect
    if (httpsredirect) {
        if (! HTTP_para("REDIR",1))
            return false;
        
        if (! HTTP_ssl(true))
            return false;
    }
    
    return true;
}


/********* HELPERS *********************************************/

bool Adafruit_FONA::expectReply(const char* reply, uint16_t timeout) {
    readline(timeout);
#ifdef ADAFRUIT_FONA_DEBUG
    printf("\t<--- %s\r\n", replybuffer);
#endif
    return (strcmp(replybuffer, reply) == 0);
}

/********* LOW LEVEL *******************************************/

void Adafruit_FONA::flushInput() {
    // Read all available serial input to flush pending data.
    uint16_t timeoutloop = 0;
    while (timeoutloop++ < 40) {
        while(readable()) {
            getc();
            timeoutloop = 0;  // If char was received reset the timer
        }
        wait_ms(1);
    }
}

uint16_t Adafruit_FONA::readRaw(uint16_t b) {
    uint16_t idx = 0;
    
    while (b && (idx < sizeof(replybuffer)-1)) {
        if (readable()) {
            replybuffer[idx] = getc();
            idx++;
            b--;
        }
    }
    replybuffer[idx] = 0;
    
    return idx;
}

uint8_t Adafruit_FONA::readline(uint16_t timeout, bool multiline) {
    uint16_t replyidx = 0;
    
    while (timeout--) {
        if (replyidx >= 254) {
            break;
        }
    
        while(readable()) {
            char c =  getc();
            if (c == '\r') continue;
            if (c == 0xA) {
                if (replyidx == 0)   // the first 0x0A is ignored
                    continue;
                
                if (!multiline) {
                    timeout = 0;         // the second 0x0A is the end of the line
                    break;
                }
            }
            replybuffer[replyidx] = c;
            replyidx++;
        }
    
        if (timeout == 0) {
            break;
        }
        wait_ms(1);
    }
    replybuffer[replyidx] = 0;  // null term
    return replyidx;
}

uint8_t Adafruit_FONA::getReply(const char* send, uint16_t timeout) {
    flushInput();

#ifdef ADAFRUIT_FONA_DEBUG
    printf("\t---> %s\r\n", send);
#endif

    mySerial.printf("%s\r\n",send);

    uint8_t l = readline(timeout);
#ifdef ADAFRUIT_FONA_DEBUG
    printf("\t<--- %s\r\n", replybuffer);
#endif
    return l;
}

// Send prefix, suffix, and newline. Return response (and also set replybuffer with response).
uint8_t Adafruit_FONA::getReply(const char* prefix, char* suffix, uint16_t timeout) {
    flushInput();
    
#ifdef ADAFRUIT_FONA_DEBUG
    printf("\t---> %s%s\r\n", prefix, suffix);
#endif
    
    mySerial.printf("%s%s\r\n", prefix, suffix);
    
    uint8_t l = readline(timeout);
#ifdef ADAFRUIT_FONA_DEBUG
    printf("\t<--- %s\r\n", replybuffer);
#endif
    return l;
}

// Send prefix, suffix, and newline. Return response (and also set replybuffer with response).
uint8_t Adafruit_FONA::getReply(const char* prefix, int32_t suffix, uint16_t timeout) {
    flushInput();
    
#ifdef ADAFRUIT_FONA_DEBUG
    printf("\t---> %s%d\r\n", prefix, suffix);
#endif
    
    mySerial.printf("%s%d\r\n", prefix, suffix);
    
    uint8_t l = readline(timeout);
#ifdef ADAFRUIT_FONA_DEBUG
    printf("\t<--- %s\r\n", replybuffer);
#endif
    return l;
}

// Send prefix, suffix, suffix2, and newline. Return response (and also set replybuffer with response).
uint8_t Adafruit_FONA::getReply(const char* prefix, int32_t suffix1, int32_t suffix2, uint16_t timeout) {
    flushInput();
    
#ifdef ADAFRUIT_FONA_DEBUG
    printf("\t---> %s%d,%d\r\n", prefix, suffix1, suffix2);
#endif
    
    mySerial.printf("%s%d,%d\r\n", prefix, suffix1, suffix2);
    
    uint8_t l = readline(timeout);
#ifdef ADAFRUIT_FONA_DEBUG
    printf("\t<--- %s\r\n", replybuffer);
#endif
    return l;
}

// Send prefix, ", suffix, ", and newline. Return response (and also set replybuffer with response).
uint8_t Adafruit_FONA::getReplyQuoted(const char* prefix, const char* suffix, uint16_t timeout) {
    flushInput();
    
#ifdef ADAFRUIT_FONA_DEBUG
    printf("\t---> %s\"%s\"\r\n", prefix, suffix);
#endif
    
    mySerial.printf("%s\"%s\"\r\n", prefix, suffix);
    
    uint8_t l = readline(timeout);
#ifdef ADAFRUIT_FONA_DEBUG
    printf("\t<--- %s\r\n", replybuffer);
#endif
    return l;
}


bool Adafruit_FONA::sendCheckReply(const char *send, const char *reply, uint16_t timeout) {
    getReply(send, timeout);
    
    return (strcmp(replybuffer, reply) == 0);
}

// Send prefix, suffix, and newline.  Verify FONA response matches reply parameter.
bool Adafruit_FONA::sendCheckReply(const char* prefix, char *suffix, const char* reply, uint16_t timeout) {
    getReply(prefix, suffix, timeout);
    return (strcmp(replybuffer, reply) == 0);
}

// Send prefix, suffix, and newline.  Verify FONA response matches reply parameter.
bool Adafruit_FONA::sendCheckReply(const char* prefix, int32_t suffix, const char* reply, uint16_t timeout) {
    getReply(prefix, suffix, timeout);
    return (strcmp(replybuffer, reply) == 0);
}

// Send prefix, suffix, suffix2, and newline.  Verify FONA response matches reply parameter.
bool Adafruit_FONA::sendCheckReply(const char* prefix, int32_t suffix1, int32_t suffix2, const char* reply, uint16_t timeout) {
    getReply(prefix, suffix1, suffix2, timeout);
    return (strcmp(replybuffer, reply) == 0);
}

// Send prefix, ", suffix, ", and newline.  Verify FONA response matches reply parameter.
bool Adafruit_FONA::sendCheckReplyQuoted(const char* prefix, const char* suffix, const char* reply, uint16_t timeout) {
  getReplyQuoted(prefix, suffix, timeout);
  return (strcmp(replybuffer, reply) == 0);
}

bool Adafruit_FONA::parseReply(const char* toreply, uint16_t *v, char divider, uint8_t index) {
    char *p = strstr(replybuffer, toreply);  // get the pointer to the voltage
    if (p == 0) return false;
    p += strlen(toreply);
    
    for (uint8_t i=0; i<index;i++) {
        // increment dividers
        p = strchr(p, divider);
        if (!p) return false;
        p++;
    }
    
    *v = atoi(p);
    
    return true;
}

bool Adafruit_FONA::parseReply(const char* toreply, char *v, char divider, uint8_t index) {
    uint8_t i=0;
    char *p = strstr(replybuffer, toreply);
    if (p == 0) return false;
    p+=strlen(toreply);
    
    for (i=0; i<index;i++) {
        // increment dividers
        p = strchr(p, divider);
        if (!p) return false;
        p++;
    }
    
    for(i=0; i<strlen(p);i++) {
        if(p[i] == divider)
            break;
        v[i] = p[i];
    }
    
    v[i] = '\0';
    
    return true;
}

// Parse a quoted string in the response fields and copy its value (without quotes)
// to the specified character array (v).  Only up to maxlen characters are copied
// into the result buffer, so make sure to pass a large enough buffer to handle the
// response.
bool Adafruit_FONA::parseReplyQuoted(const char* toreply, char* v, int maxlen, char divider, uint8_t index) {
    uint8_t i=0, j;
    // Verify response starts with toreply.
    char *p = strstr(replybuffer, toreply);
    if (p == 0) return false;
    p+=strlen(toreply);
    
    // Find location of desired response field.
    for (i=0; i<index;i++) {
        // increment dividers
        p = strchr(p, divider);
        if (!p) return false;
            p++;
    }
    
    // Copy characters from response field into result string.
    for(i=0, j=0; j<maxlen && i<strlen(p); ++i) {
        // Stop if a divier is found.
        if(p[i] == divider)
            break;
        // Skip any quotation marks.
        else if(p[i] == '"')
            continue;
        v[j++] = p[i];
    }
    
    // Add a null terminator if result string buffer was not filled.
    if (j < maxlen)
        v[j] = '\0';
    
    return true;
}

bool Adafruit_FONA::sendParseReply(const char* tosend, const char* toreply, uint16_t *v, char divider, uint8_t index) {
    getReply(tosend);
    
    if (! parseReply(toreply, v, divider, index)) return false;
    
    readline(); // eat 'OK'
    
    return true;
}
