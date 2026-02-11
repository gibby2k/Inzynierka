#ifndef SYSTEM_CONTROLLER_H
#define SYSTEM_CONTROLLER_H

#include <Arduino.h>
#include "SystemState.h"
#include "AccessManager.h"

// --- KONFIGURACJA CZASÓW ---
#define TIME_T1_PRE_ALARM  20000 // T1 = 20 sekund
#define TIME_T2_VERIFY     40000 // T2 = 40 sekund

#define PIN_BUILTIN_LED 2

class SystemController {
  // ... (reszta bez zmian) ...
  private:
    unsigned long timerStart;
    unsigned long lastBlink;
    bool ledState;
    int detectedLineId;

    void activateFireAlarm();
    void resetSystem();

  public:
    SystemController();
    void begin();
    void loop();

    void onSensorSignal(int lineId);
    void onAckPressed();
    void onResetPressed();
    void handleInput(char key); 

    int getRemainingTime();
};

#endif