#include <stdint.h>

template<uint8_t polynomial>
class CRC8 {
public:

  CRC8();
  
  uint8_t update(uint8_t value);
  uint8_t update(char *string);
  
  static void makeTable();

private:
  uint8_t remainder;
  
  static uint8_t  crcTable[256];
  static bool tableReady;
};


template<uint16_t polynomial, bool reverse = false>
class CRC16 {
public:

  CRC16();
  
  uint16_t update(uint8_t value);
  uint16_t update(char *string);
  
  static void makeTable();

private:
  uint16_t remainder;
  
  static uint16_t  crcTable[256];
  static bool tableReady;
};



template<uint8_t polynomial>
uint8_t  CRC8<polynomial>::crcTable[256];

template<uint8_t polynomial>
bool CRC8<polynomial>::tableReady = false;

template<uint8_t polynomial>
CRC8<polynomial>::CRC8() {
  makeTable();
  remainder = 0;
}

template<uint8_t polynomial>
void CRC8<polynomial>::makeTable(void)
{
  if (tableReady) return;
  
  uint8_t  remainder;

    /*
     * Compute the remainder of each possible dividend.
     */
    for (int dividend = 0; dividend < 256; ++dividend)
    {
        /*
         * Start with the dividend followed by zeros.
         */
        remainder = dividend;

        /*
         * Perform modulo-2 division, a bit at a time.
         */
        for (uint8_t bit = 8; bit > 0; --bit)
        {
            /*
             * Try to divide the current data bit.
             */			
            if (remainder & 0x80)
            {
                remainder = (remainder << 1) ^ polynomial;
            }
            else
            {
                remainder = (remainder << 1);
            }
        }

        /*
         * Store the result into the table.
         */
        crcTable[dividend] = remainder;
    }
    tableReady = true;
}  

template<uint8_t polynomial>
uint8_t CRC8<polynomial>::update(uint8_t value)
{
    /*
     * Divide the message by the polynomial, a byte at a time.
     */
        uint8_t data = value ^ (remainder);
        remainder = crcTable[data] ^ (remainder << 8);

    /*
     * The final remainder is the CRC.
     */
    return remainder;
}

template<uint8_t polynomial>
uint8_t CRC8<polynomial>::update(char *string)
{
  if (!string) return remainder;
  while (*string) {
    update(*string);
    string++;
  }
  return remainder;
}







template<uint16_t polynomial, bool reverse>
uint16_t  CRC16<polynomial, reverse>::crcTable[256];

template<uint16_t polynomial, bool reverse>
bool CRC16<polynomial, reverse>::tableReady = false;

template<uint16_t polynomial, bool reverse>
CRC16<polynomial, reverse>::CRC16() {
  makeTable();
  remainder = 0;
}

template<uint16_t polynomial, bool reverse>
void CRC16<polynomial, reverse>::makeTable(void)
{
  if (tableReady) return;
  
  uint16_t  remainder;

    /*
     * Compute the remainder of each possible dividend.
     */
    for (int dividend = 0; dividend < 256; ++dividend)
    {
        /*
         * Start with the dividend followed by zeros.
         */
        if (reverse) {
          remainder = dividend;
        }
        else {
          remainder = dividend << 8;
        }

        /*
         * Perform modulo-2 division, a bit at a time.
         */
        for (uint8_t bit = 8; bit > 0; --bit)
        {
            /*
             * Try to divide the current data bit.
             */			
            if (reverse) {
              if (remainder & 0x0001)
              {
                  remainder = (remainder >> 1) ^ polynomial;
              }
              else
              {
                  remainder = (remainder >> 1);
              }
            }
            else {
              if (remainder & 0x8000)
              {
                  remainder = (remainder << 1) ^ polynomial;
              }
              else
              {
                  remainder = (remainder << 1);
              }
           }
        }

        /*
         * Store the result into the table.
         */
        crcTable[dividend] = remainder;
    }
    tableReady = true;
}  

template<uint16_t polynomial, bool reverse>
uint16_t CRC16<polynomial, reverse>::update(uint8_t value)
{
  /*
   * Divide the message by the polynomial, a byte at a time.
   */
  if (reverse) {
    uint8_t data = value ^ remainder;
    remainder = crcTable[data] ^ (remainder >> 8);
  }
  else {
    uint8_t data = value ^ (remainder >> 8);
    remainder = crcTable[data] ^ (remainder << 8);
  }

  /*
   * The final remainder is the CRC.
   */
  return remainder;
}

template<uint16_t polynomial, bool reverse>
uint16_t CRC16<polynomial, reverse>::update(char *string)
{
  if (!string) return remainder;
  while (*string) {
    update(*string);
    string++;
  }
  return remainder;
}
