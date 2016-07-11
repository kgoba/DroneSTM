#include "UART.hh"

#include "usart.h"
#include "usbd_cdc_if.h"

#include "FreeRTOS.h"
#include "cmsis_os.h"
#include "task.h"

#include "fifo.h"

#define RX_FIFO_SIZE    128
#define TX_FIFO_SIZE    128

FIFO(RXFIFO, uint8_t, RX_FIFO_SIZE);
FIFO(TXFIFO, uint8_t, TX_FIFO_SIZE);

void serInit() {
  FIFO_INIT(RXFIFO);
  FIFO_INIT(TXFIFO);
  __HAL_UART_ENABLE_IT(&huart2, UART_IT_RXNE);
}

int serOnReceive() {
  uint8_t b = huart2.Instance->RDR;

  if (FIFO_FULL(RXFIFO)) {
    // buffer overrun
    return -1;
  }

  UBaseType_t uxSavedInterruptStatus;
  uxSavedInterruptStatus = taskENTER_CRITICAL_FROM_ISR();

    FIFO_PUSH(RXFIFO, b);

  taskEXIT_CRITICAL_FROM_ISR(uxSavedInterruptStatus);

  return 0;
}

int serOnTransmitEmpty() {
  if (FIFO_EMPTY(TXFIFO)) {
    // buffer underrun
    __HAL_UART_DISABLE_IT(&huart2, UART_IT_TXE);
    return -1;
  }

  uint8_t b;

  UBaseType_t uxSavedInterruptStatus;
  uxSavedInterruptStatus = taskENTER_CRITICAL_FROM_ISR();

    FIFO_POP(TXFIFO, b);
   
  taskEXIT_CRITICAL_FROM_ISR(uxSavedInterruptStatus);

  huart2.Instance->TDR = b;
  return 0;
}

int serRead(uint8_t *b) {
  if (FIFO_EMPTY(RXFIFO)) {
    HAL_GPIO_WritePin(LD3_GPIO_Port, GPIO_PIN_10, GPIO_PIN_RESET);
    return -1;
  }
  taskENTER_CRITICAL();
  FIFO_POP(RXFIFO, *b);
  taskEXIT_CRITICAL();
  return 0;
}

int serWrite(uint8_t b) {
  if (FIFO_FULL(TXFIFO)) {
    // buffer overrun
    return -1;
  }
  taskENTER_CRITICAL();
  FIFO_PUSH(TXFIFO, b);
  taskEXIT_CRITICAL();

  __HAL_UART_ENABLE_IT(&huart2, UART_IT_TXE);
  return 0;
}

uint16_t serReadCount() {
  return FIFO_COUNT(RXFIFO);
}

uint16_t serWriteCount() {
  return FIFO_COUNT(TXFIFO);
}


int serWriteString(const char *str) {
  //HAL_UART_Transmit(&huart2, str, strlen(str), 200);
  //return 0;
  
  uint8_t nRetries = 100;
  while (*str) {
    //huart2.Instance->TDR = (*str & (uint8_t)0xFF);
    //UART_WaitOnFlagUntilTimeout(&huart2, UART_FLAG_TXE, RESET, 200);
    if (0 != serWrite(*str)) {
      if (0 == nRetries) return -1;
      vTaskDelay(10);
      nRetries--;
    }
    else {
      str++;
    }
    //str++;
  }
  //UART_WaitOnFlagUntilTimeout(&huart2, UART_FLAG_TC, RESET, 200);
  return 0;
}

int serReadLine(char *str, uint16_t maxSize) {
  uint8_t nRetries = 100;
  uint32_t nReceived = 0;
  while (maxSize > 0) {
    if (0 != serRead((uint8_t *)str)) {
      if (0 == nRetries) break;
      vTaskDelay(10);
      nRetries--;
    }
    else {
      if (*str == '\r' || *str == '\n') break;
      str++;
      maxSize--;
      nReceived++;
    }
  }
  return nReceived;
}



void UART::init() {
  serInit();
}

void UART::write(uint8_t b) {
  serWrite(b);
}

uint8_t UART::read() {
  uint8_t b = 0;
  serRead(&b);
  return b;
}
int UART::available() {
  return serReadCount();
}

void TextUART::write (const char * string) {
  while (*string != 0) {
    UART::write(*string);
    string++;
  }
}

int TextUART::read (char *string, int nChars) {
  int nRead = 0;
  while (nChars > 0) {
    int nRetries = 200;
    while (UART::available() == 0) {
      nRetries--;
      if (nRetries == 0) {
        *string = 0;
        return nRead;
      }
      vTaskDelay(10);
    }
    *string = UART::read();
    nRead++;
    string++;
    nChars--;
  }
  return nRead;
}

int TextUART::readLine (char *string, int nChars, char delim) {
  int nRead = 0;
  const char *line = string;
  while (nChars > 1) {
    int nRetries = 200;
    while (UART::available() == 0) {
      nRetries--;
      if (nRetries == 0) return nRead;
      vTaskDelay(10);
    }
    uint8_t b = UART::read();
    *string++ = b;
    nRead++;
    nChars--;
    if (b == delim) {      
      break;
    }
  }
  *string = 0;
  return nRead;
}


/**
  * @brief Handle UART interrupt request.
  * @param huart: UART handle.
  * @retval None
  */
void MHAL_UART_IRQHandler(UART_HandleTypeDef *huart)
{
  /* UART parity error interrupt occurred -------------------------------------*/
  if((__HAL_UART_GET_IT(huart, UART_IT_PE) != RESET) && (__HAL_UART_GET_IT_SOURCE(huart, UART_IT_PE) != RESET))
  {
    __HAL_UART_CLEAR_IT(huart, UART_CLEAR_PEF);

    huart->ErrorCode |= HAL_UART_ERROR_PE;
  }

  /* UART frame error interrupt occurred --------------------------------------*/
  if((__HAL_UART_GET_IT(huart, UART_IT_FE) != RESET) && (__HAL_UART_GET_IT_SOURCE(huart, UART_IT_ERR) != RESET))
  {
    __HAL_UART_CLEAR_IT(huart, UART_CLEAR_FEF);

    huart->ErrorCode |= HAL_UART_ERROR_FE;
  }

  /* UART noise error interrupt occurred --------------------------------------*/
  if((__HAL_UART_GET_IT(huart, UART_IT_NE) != RESET) && (__HAL_UART_GET_IT_SOURCE(huart, UART_IT_ERR) != RESET))
  {
    __HAL_UART_CLEAR_IT(huart, UART_CLEAR_NEF);

    huart->ErrorCode |= HAL_UART_ERROR_NE;
  }

  /* UART Over-Run interrupt occurred -----------------------------------------*/
  if((__HAL_UART_GET_IT(huart, UART_IT_ORE) != RESET) && (__HAL_UART_GET_IT_SOURCE(huart, UART_IT_ERR) != RESET))
  {
    __HAL_UART_CLEAR_IT(huart, UART_CLEAR_OREF);

    huart->ErrorCode |= HAL_UART_ERROR_ORE;
  }

  /* UART wakeup from Stop mode interrupt occurred -------------------------------------*/
  if((__HAL_UART_GET_IT(huart, UART_IT_WUF) != RESET) && (__HAL_UART_GET_IT_SOURCE(huart, UART_IT_WUF) != RESET))
  {
    __HAL_UART_CLEAR_IT(huart, UART_CLEAR_WUF);
    /* Set the UART state ready to be able to start again the process */
    huart->gState = HAL_UART_STATE_READY;
    huart->RxState = HAL_UART_STATE_READY;

    //HAL_UARTEx_WakeupCallback(huart);
  }

  /* UART in mode Receiver ---------------------------------------------------*/
  if((__HAL_UART_GET_IT(huart, UART_IT_RXNE) != RESET) && (__HAL_UART_GET_IT_SOURCE(huart, UART_IT_RXNE) != RESET))
  {
    UBaseType_t uxSavedInterruptStatus;
    
    /* toggle LED */
    HAL_GPIO_TogglePin(LD3_GPIO_Port, GPIO_PIN_10);

    serOnReceive();
  }

  /* UART in mode Transmitter ------------------------------------------------*/
  if((__HAL_UART_GET_IT(huart, UART_IT_TXE) != RESET) &&(__HAL_UART_GET_IT_SOURCE(huart, UART_IT_TXE) != RESET))
  {

    /* toggle LED */
    HAL_GPIO_TogglePin(LD3_GPIO_Port, GPIO_PIN_11);
    
    serOnTransmitEmpty();
  }

  /* UART in mode Transmitter (transmission end) -----------------------------*/
  if((__HAL_UART_GET_IT(huart, UART_IT_TC) != RESET) &&(__HAL_UART_GET_IT_SOURCE(huart, UART_IT_TC) != RESET))
  {
    HAL_GPIO_WritePin(LD3_GPIO_Port, GPIO_PIN_11, GPIO_PIN_RESET);
    //UART_EndTransmit_IT(huart);
  }

  if(huart->ErrorCode != HAL_UART_ERROR_NONE)
  {
    /* Set the UART state ready to be able to start again the Tx/Rx process */
    huart->gState = HAL_UART_STATE_READY;
    huart->RxState = HAL_UART_STATE_READY;

    //HAL_UART_ErrorCallback(huart);
  }  
}
