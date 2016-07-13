#include "vario.h"


Variometer::Variometer() {
    pFilt = 0;
    lastPressure = 0;
    dpFilt = 0;

    dpPerMeter = 12.0f;
    baroAvgTime = 0.400f;
    climbAvgTime = 0.400f;  
}

void Variometer::reset(float pressure) {
  pFilt = lastPressure = pressure;
  dpFilt = 0;
}

void Variometer::update(float pressure, float dt) {
  pFilt += (dt / baroAvgTime) * ((float)pressure - pFilt);

  float dp = (pFilt - lastPressure) / dt;
  dpFilt += (dt / climbAvgTime) * (dp - dpFilt); 

  lastPressure = pFilt;
}
