#pragma once

#include <stdint.h>

#include "usart.h"

#if defined (__cplusplus)
extern "C" {
#endif

void MHAL_UART_IRQHandler(UART_HandleTypeDef *huart);

#if defined (__cplusplus)
}
#endif

class UART {
public:
  static void init();
  static void write(uint8_t b);
  static uint8_t read();
  static int available();
};

class TextUART : public UART {
public:
  static void write (const char * string);  
  static int read (char *string, int nChars);
  static int readLine (char *string, int nChars, char delim = '\r');
};
