#include "SIM808.hh"
#include "UART.hh"
#include "config.h"

extern "C" {
  #include "FreeRTOS.h"
  #include "cmsis_os.h"
  #include "task.h"
  #include "usbd_cdc_if.h"
  #include "usart.h"
}


char buf[80];
char line[200];
SIM808 gsm;

void SIM808_Task(void const * argument) {
  UART::init();  
  
  vTaskDelay(5000);
  
  gsm.initialize();
  gsm.checkPIN();
  gsm.sendPIN(SIM_PIN1);
  
  //gsm.enableCharging();
  
  gsm.enableGPS();

  /* Infinite loop */
  for(;;)
  {
    /* toggle LED */
    HAL_GPIO_TogglePin(LD3_GPIO_Port, GPIO_PIN_9);

    gsm.checkBattery();

    gsm.checkNetwork();

    gsm.getGPSInfo();
    
    while (true) {
      int nRead = TextUART::readLine(buf, 80, '\n');
      if (nRead == 0) break;
      
      while (nRead > 0) {
        char c = buf[nRead - 1];
        if (c == '\r' || c == '\n') {
          buf[nRead - 1] = 0;
          nRead--;
        }
        else break;
      }
        
      //if (nRead > 0) {
        strcpy(line, "<< [");
        strcat(line, buf);
        strcat(line, "]\r\n");
        CDC_Transmit_FS((uint8_t *)line, strlen(line));
      //}
    }
    
    vTaskDelay(2000);     
  }
}


  
  void SIM808::initialize() {
    sendCommand("");
    expectOK();         // in case the command echo is on
    status = OK;
    expectOK();
    sendCommand("E0");
    expectOK();
    expectOK();
  }

  bool SIM808::checkPIN() {
    status = OK;
    sendCommand("+CPIN?");
    expectResponse("+CPIN:");
    expectOK();
    return false;
  }
  
  bool SIM808::checkNetwork() {
    status = OK;
    sendCommand("+CREG?");
    expectResponse("+CREG:");
    expectOK();
    return false;
  }
  
  bool SIM808::checkBattery() {
    status = OK;
    sendCommand("+CBC");
    expectResponse("+CBC:");
    expectOK();
    return false;
  }
  
  void SIM808::sendPIN(const char *pin) {
    status = OK;
    sendCommand("+CPIN=", pin);
    vTaskDelay(3000);
    //expectResponse("+CPIN:");
    expectOK();
  }
  
  bool SIM808::enableCharging() {
    status = OK;
    sendCommand("+ECHARGE=1");
    vTaskDelay(1000);
    expectOK();    
  }
  
  bool SIM808::disableCharging() {
    status = OK;
    sendCommand("+ECHARGE=0");
    vTaskDelay(1000);
    expectOK();    
  }
  
  bool SIM808::enableGPS() {
    status = OK;
    sendCommand("+CGNSPWR=", "1");
    expectOK();    
  }
  
  bool SIM808::disableGPS() {
    status = OK;
    sendCommand("+CGNSPWR=", "0");
    expectOK();    
  }

  bool SIM808::getGPSInfo() {
    status = OK;
    sendCommand("+CGNSINF");
    expectResponse("+CGNSINF: ");
    expectOK();      
  }

  
  bool SIM808::isError() {
    return status != OK;
  }
 
  bool SIM808::expectOK() {
    char buf[20];
    int nRead = uart.readLine(buf, 20, '\n');

    char line[30];
    strcpy(line, "< ");
    strcat(line, buf);
    CDC_Transmit_FS((uint8_t *)line, strlen(line));

    if (strcmp(buf, "OK") != 0) {
      status = NOT_OK;
    }
  }
  
  bool SIM808::expectResponse(const char *prefix) {
    char buf[80];
    uart.readLine(buf, 80, '\n');

    int nRead = uart.readLine(buf, 80, '\n');

    if (nRead > 0) {
      char line[50];
      strcpy(line, "< ");
      strcat(line, buf);
      CDC_Transmit_FS((uint8_t *)line, strlen(line));
    }

    uart.readLine(buf, 80, '\n');
  }
  
  void SIM808::sendCommand(const char *cmd) {
    /*
    char line[40];
    strcpy(line, "> AT");
    strcat(line, cmd);
    strcat(line, "\r\n");
    CDC_Transmit_FS((uint8_t *)line, strlen(line));
    */
    
    uart.write("AT");
    uart.write(cmd);
    uart.write("\r\n");
  }

  void SIM808::sendCommand(const char *cmd, const char *data) {
    /*
    char line[40];
    strcpy(line, "> AT");
    strcat(line, cmd);
    strcat(line, data);
    strcat(line, "\r\n");
    CDC_Transmit_FS((uint8_t *)line, strlen(line));
    */
    
    uart.write("AT");
    uart.write(cmd);
    uart.write(data);
    uart.write("\r\n");
  }  
