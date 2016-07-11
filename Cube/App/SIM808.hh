#pragma once

#if defined (__cplusplus)
extern "C" {
#endif

void SIM808_Task(void const * argument);

#if defined (__cplusplus)
}
#endif

#if defined (__cplusplus)

#include "UART.hh"

class SIM808 {
public:
  enum Status {
    OK = 0,
    NOT_OK = 1
  };

  
  void initialize();

  bool checkPIN();
  
  bool checkNetwork();
  bool checkBattery();
  bool checkGPS();
  bool getGPSInfo();
  
  void sendPIN(const char *pin);
  
  bool configureSMS();
  bool sendSMS(const char *number, const char *message);

  bool enableGPS();
  bool disableGPS();
  
  bool enableCharging();
  bool disableCharging();
  
  bool isError();

private:
  int  status;
  TextUART uart;
  
  bool expectOK();
  bool expectResponse(const char *prefix);
  void sendCommand(const char *cmd);
  void sendCommand(const char *cmd, const char *data);
};

#endif
