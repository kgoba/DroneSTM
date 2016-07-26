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


void varioMeasure(void const *argument) {
  static float lastPressure = 0;
  const float dt = 0.020;
  
  if (barometer.update())
  {
    float pressure = barometer.getPressure();
    vario.update(pressure, dt);
    lastPressure = pressure;
  }
  else vario.update(lastPressure, dt);
}

float climbThreshold = 0.10f;
float sinkThreshold = -0.10f;

void varioTask(void const *argument) {    
    //sensorBus.frequency(100000);
    if (barometer.reset()) {
      dbg.printf("Barometer reset!\n");
    }
    else {
      dbg.printf("Barometer not found!\n");
    }
    Thread::wait(50);
    
    if (!barometer.initialize()) {
      dbg.printf("Failed to initialize!\n");
    }
    
    // Calculate initial pressure by averaging measurements
    int idx = 0;
    float avgPressure = 0;
    while (idx < 50) {
      if (barometer.update()) {
        avgPressure += barometer.getPressure();
        idx++;
      }
    }
    avgPressure /= idx;
    vario.reset(avgPressure);

    // Start vario update timer
    RtosTimer measureTimer(varioMeasure, osTimerPeriodic, NULL);  
    measureTimer.start(20);

    // Keep checking vertical speed
    idx = 0;
    int period = 10;
    bool phase = false;
    while (true) {
      if (idx >= period) {
        //if (fabsf(vertSpeed) > 0.1f) {
        //  dbg.printf("Vertical speed: % .1f\tPressure: %.0f\n", vertSpeed, pFilt);
        //}
        
        float vertSpeed = vario.getVerticalSpeed();
        if (vertSpeed > climbThreshold) {
          float frequency = 440 + 220 * vertSpeed;
          buzzer.period(1.0f / frequency);
          period = 20 - 7 * vertSpeed;
          if (period < 3) period = 3;
          buzzer = phase ? 0.5f : 0;
        }
        else if (vertSpeed < sinkThreshold) {
          //float frequency = 440 + 220 * vertSpeed;
          //buzzer.period(1.0f / frequency);
          //buzzer = 0.5f;
          buzzer = 0;
        }
        else {
          buzzer = 0;
        }
        
        phase = !phase;
        idx = 0;
      }
        
      idx++;
      Thread::wait(15);
    }
}
