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
 
#ifndef ADAFRUIT_FONA_H
#define ADAFRUIT_FONA_H

#include "mbed.h"

//#define ADAFRUIT_FONA_DEBUG

#define FONA_HEADSETAUDIO 0
#define FONA_EXTAUDIO 1

#define FONA_STTONE_DIALTONE 1
#define FONA_STTONE_BUSY 2
#define FONA_STTONE_CONGESTION 3
#define FONA_STTONE_PATHACK 4
#define FONA_STTONE_DROPPED 5
#define FONA_STTONE_ERROR 6
#define FONA_STTONE_CALLWAIT 7
#define FONA_STTONE_RINGING 8
#define FONA_STTONE_BEEP 16
#define FONA_STTONE_POSTONE 17
#define FONA_STTONE_ERRTONE 18
#define FONA_STTONE_INDIANDIALTONE 19
#define FONA_STTONE_USADIALTONE 20

#define FONA_DEFAULT_TIMEOUT_MS 500

#define FONA_HTTP_GET   0
#define FONA_HTTP_POST  1
#define FONA_HTTP_HEAD  2 

#define RX_BUFFER_SIZE  255

class Adafruit_FONA : public Stream {
    public:
        /**
         * Listener for FONA events.
         */
        class EventListener {
            public:
                /**
                 * Method called when somebody call the FONA.
                 */
                virtual void onRing() = 0;
                
                /**
                 * Method called when the calling person stop his call.
                 */
                virtual void onNoCarrier() = 0;
        };
    
    public:
        Adafruit_FONA(PinName tx, PinName rx, PinName rst, PinName ringIndicator) :
            _rstpin(rst, false), _ringIndicatorInterruptIn(ringIndicator),
            apn("FONAnet"), apnusername(NULL), apnpassword(NULL), httpsredirect(false), useragent("FONA"),
            _incomingCall(false), eventListener(NULL), mySerial(tx, rx), rxBufferInIndex(0), rxBufferOutIndex(0), 
            currentReceivedLineSize(0) {}
        bool begin(int baudrate);
        void setEventListener(EventListener *eventListener);
        
        // Stream
        virtual int _putc(int value);
        virtual int _getc();
        int readable();
        
        // RTC
        bool enableRTC(uint8_t i); // i = 0 <=> disable, i = 1 <=> enable
        
        // Battery and ADC
        bool getADCVoltage(uint16_t *v);
        bool getBattPercent(uint16_t *p);
        bool getBattVoltage(uint16_t *v);
        
        // SIM query
        bool unlockSIM(char *pin);
        uint8_t getSIMCCID(char *ccid);
        uint8_t getNetworkStatus(void);
        uint8_t getRSSI(void);
        
        // IMEI
        uint8_t getIMEI(char *imei);
        
        // set Audio output
        bool setAudio(uint8_t a);
        bool setVolume(uint8_t i);
        uint8_t getVolume(void);
        bool playToolkitTone(uint8_t t, uint16_t len);
        bool setMicVolume(uint8_t a, uint8_t level);
        bool playDTMF(char tone);
        
        // FM radio functions
        bool tuneFMradio(uint16_t station);
        bool FMradio(bool onoff, uint8_t a = FONA_HEADSETAUDIO);
        bool setFMVolume(uint8_t i);
        int8_t getFMVolume();
        int8_t getFMSignalLevel(uint16_t station);
        
        // SMS handling
        bool setSMSInterrupt(uint8_t i);
        uint8_t getSMSInterrupt(void);
        int8_t getNumSMS(void);
        bool readSMS(uint8_t i, char *smsbuff, uint16_t max, uint16_t *readsize);
        bool sendSMS(char *smsaddr, char *smsmsg);
        bool deleteSMS(uint8_t i);
        bool getSMSSender(uint8_t i, char *sender, int senderlen);
        
        // Time
        bool enableNetworkTimeSync(bool onoff);
        bool enableNTPTimeSync(bool onoff, const char* ntpserver=0);
        bool getTime(char* buff, uint16_t maxlen);
        
        // GPRS handling
        bool enableTCPGPRS(bool onoff);
        bool enableGPRS(bool onoff);
        uint8_t GPRSstate(void);
        bool getGSMLoc(uint16_t *replycode, char *buff, uint16_t maxlen);
        bool getGSMLoc(float *lat, float *lon);
        void setGPRSNetworkSettings(const char* apn, const char* username=0, const char* password=0);

        // GPS handling
        bool enableGPS(bool onoff);
        int8_t GPSstatus(void);
        uint8_t getGPS(uint8_t arg, char *buffer, uint8_t maxbuff);
        bool getGPS(float *lat, float *lon, float *altitude=0);
        bool enableGPSNMEA(uint8_t nmea);
        
        // TCP raw connections
        bool TCPconnect(char *server, uint16_t port);
        bool TCPclose(void);
        bool TCPconnected(void);
        bool TCPsend(char *packet, uint8_t len);
        uint16_t TCPavailable(void);
        uint16_t TCPread(uint8_t *buff, uint8_t len);
        
        // HTTP low level interface (maps directly to SIM800 commands).
        bool HTTP_init();
        bool HTTP_term();
        void HTTP_para_start(const char* parameter, bool quoted = true);
        bool HTTP_para_end(bool quoted = true);
        bool HTTP_para(const char* parameter, const char *value);
        bool HTTP_para(const char* parameter, int32_t value);
        bool HTTP_data(uint32_t size, uint32_t maxTime=10000);
        bool HTTP_action(uint8_t method, uint16_t *status, uint16_t *datalen, int32_t timeout = 10000);
        bool HTTP_readall(uint16_t *datalen);
        bool HTTP_ssl(bool onoff);
        
        // HTTP high level interface (easier to use, less flexible).
        bool HTTP_GET_start(char *url, uint16_t *status, uint16_t *datalen);
        void HTTP_GET_end(void);
        bool HTTP_POST_start(char *url, const char* contenttype, const uint8_t *postdata, uint16_t postdatalen, uint16_t *status, uint16_t *datalen);
        void HTTP_POST_end(void);
        void setUserAgent(const char* useragent);
        
        // HTTPS
        void setHTTPSRedirect(bool onoff);
        
        // PWM (buzzer)
        bool setPWM(uint16_t period, uint8_t duty = 50);
        
        // Phone calls
        bool callPhone(char *phonenum);
        bool hangUp(void);
        bool pickUp(void);
        bool callerIdNotification(bool enable);
        bool incomingCallNumber(char* phonenum);
        
        // Helper functions to verify responses.
        bool expectReply(const char* reply, uint16_t timeout = 10000);
        
    private:
        DigitalOut _rstpin;
        InterruptIn _ringIndicatorInterruptIn;
        
        char replybuffer[255];
        char* apn;
        char* apnusername;
        char* apnpassword;
        bool httpsredirect;
        char* useragent;
        
        volatile bool _incomingCall;
        EventListener *eventListener;
        Serial mySerial;
        
        // Circular buffer used to receive serial data from an interruption
        int rxBuffer[RX_BUFFER_SIZE + 1];
        volatile int rxBufferInIndex; // Index where new data is added to the buffer
        volatile int rxBufferOutIndex; // Index where data is removed from the buffer
        char currentReceivedLine[RX_BUFFER_SIZE]; // Array containing the current received line
        int currentReceivedLineSize;
        
        inline bool isRxBufferFull() {
            return ((rxBufferInIndex + 1) % RX_BUFFER_SIZE) == rxBufferOutIndex;
        }
        
        inline bool isRxBufferEmpty() {
            return rxBufferInIndex == rxBufferOutIndex;
        }
        
        inline void incrementRxBufferInIndex() {
            rxBufferInIndex = (rxBufferInIndex + 1) % RX_BUFFER_SIZE;
        }
        
        inline void incrementRxBufferOutIndex() {
            rxBufferOutIndex = (rxBufferOutIndex + 1) % RX_BUFFER_SIZE;
        }
        
        /**
         * Method called when Serial data is received (interrupt routine).
         */
        void onSerialDataReceived();
        
        // HTTP helpers
        bool HTTP_setup(char *url);
        
        void flushInput();
        uint16_t readRaw(uint16_t b);
        uint8_t readline(uint16_t timeout = FONA_DEFAULT_TIMEOUT_MS, bool multiline = false);
        uint8_t getReply(const char* send, uint16_t timeout = FONA_DEFAULT_TIMEOUT_MS);
        uint8_t getReply(const char* prefix, char *suffix, uint16_t timeout = FONA_DEFAULT_TIMEOUT_MS);
        uint8_t getReply(const char* prefix, int32_t suffix, uint16_t timeout = FONA_DEFAULT_TIMEOUT_MS);
        uint8_t getReply(const char* prefix, int32_t suffix1, int32_t suffix2, uint16_t timeout); // Don't set default value or else function call is ambiguous.
        uint8_t getReplyQuoted(const char* prefix, const char* suffix, uint16_t timeout = FONA_DEFAULT_TIMEOUT_MS);
        
        bool sendCheckReply(const char* send, const char* reply, uint16_t timeout = FONA_DEFAULT_TIMEOUT_MS);
        bool sendCheckReply(const char* prefix, char *suffix, const char* reply, uint16_t timeout = FONA_DEFAULT_TIMEOUT_MS);
        bool sendCheckReply(const char* prefix, int32_t suffix, const char* reply, uint16_t timeout = FONA_DEFAULT_TIMEOUT_MS);
        bool sendCheckReply(const char* prefix, int32_t suffix, int32_t suffix2, const char* reply, uint16_t timeout = FONA_DEFAULT_TIMEOUT_MS);
        bool sendCheckReplyQuoted(const char* prefix, const char* suffix, const char* reply, uint16_t timeout = FONA_DEFAULT_TIMEOUT_MS);
        
        bool parseReply(const char* toreply, uint16_t *v, char divider  = ',', uint8_t index=0);
        bool parseReply(const char* toreply, char *v, char divider  = ',', uint8_t index=0);
        bool parseReplyQuoted(const char* toreply, char* v, int maxlen, char divider, uint8_t index);
        
        bool sendParseReply(const char* tosend, const char* toreply, uint16_t *v, char divider = ',', uint8_t index = 0);
        
        void onIncomingCall();
};

#endif
