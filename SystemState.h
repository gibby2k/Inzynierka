#ifndef SYSTEM_STATE_H
#define SYSTEM_STATE_H

enum SystemState {
  STATE_IDLE = 0,           // Czuwanie
  STATE_PRE_ALARM = 1,      // T1 - Alarm Wstępny 
  STATE_VERIFICATION = 2,   // T2 - Weryfikacja 
  STATE_FIRE_ALARM = 3,     // POŻAR 
  STATE_FAULT = 4,          // Awaria
  STATE_DSO_TEST = 5,       // Test Dźwięku
  STATE_SMOKE_VENT = 6,     // Oddymianie techniczne
  STATE_MENU = 7            // Obsługa Menu Systemowego
};

#endif