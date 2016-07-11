

/*class String {
public:
  String(char *buf, uint16_t size) {
    _buf = buf;
    _size = size;
  }
  
  String & operator + (const String &other) {
    
  }
  
private:
  char *    _buf;
  uint16_t  _size;
};*/

template<UART_HandleTypeDef *huart, uint32_t timeout>
class BlockingSerial {
public:
  static bool read(uint8_t &b) {
    //if (!noCheck && !isRXComplete()) return false;
    HAL_UART_Receive(huart, &b, 1, timeout);
    return true;
  }
  
  static bool write(uint8_t b) {
    //if (!noCheck && !isTXReady()) return false;
    HAL_UART_Transmit(huart, &b, 1, timeout);
    return true;
  }
  
  static bool isRXComplete() {
    return (__HAL_UART_GET_FLAG(huart, UART_FLAG_RXNE) != RESET);
  }

  static bool isTXReady() {
    return (__HAL_UART_GET_FLAG(huart, UART_FLAG_TXE) != RESET);
  }
  
  static bool write(char *str) {
    while (*str) {
      if (!write(*str)) return false;
      str++;
    }
    return true;
  }
  
  static bool writeLine(char *str, char *newLine = "\r\n") {
    if (!write(str)) return false;
    if (!write(newLine)) return false;
    return true;
  }
  
  static bool readLine() {
  }
};

template<class Stream>
class SIM808 {
public:
  void init();

  bool checkAT() {
    Stream::write("AT");
    Stream::readLine();
  }
  
  bool enterPIN(char *pin);
  bool checkPIN(char *response) {
    /*
     *  Response 
     * TA  returns  an  alphanumeric  string  indicating  whether  
     * some  password  is required or not.
     * +CPIN: <code> 
     * 
     * Parameters
     * <code>
     * READY    MT is not pending for any password 
     * SIM PIN  MT is waiting SIM PIN to be given 
     * SIM PUK  MT is waiting for SIM PUK to be given 
     * PH_SIM   PIN    ME is waiting for phone to SIM card (antitheft) 
     * PH_SIM   PUK    ME  is  waiting  for  SIM  PUK  (antitheft)  
     * SIM PIN2 PIN2, e.g. for editing the FDN book possible only 
     *    if preceding Command was acknowledged with +CME    
     * ERROR:17 
     * SIM    PUK2      Possible only if preceding Command was    
     *    acknowledged with error +CME ERROR: 18. 
     */
  }
  
  bool checkNetwork(char *response);
  
  bool configureSMS();
  bool sendSMS(char *number, char *message);
  
private:
  bool expectOK();
  bool expectResponse(char *prefix);
};
