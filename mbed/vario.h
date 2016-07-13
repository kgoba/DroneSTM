#include <stdint.h>

class Variometer {
public:
  Variometer();
  
  void reset(float pressure);

  void update(float pressure, float dt);
  
  float getVerticalSpeed() {
    return -dpFilt / dpPerMeter;
  }

private:
    float pFilt = 0;
    float lastPressure = 0;
    float dpFilt = 0;

    float dpPerMeter = 12.0f;
    float baroAvgTime = 0.400f;
    float climbAvgTime = 0.400f;
};
