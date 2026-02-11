#ifndef SYSTEM_STATE_H
#define SYSTEM_STATE_H

enum SystemState {
  STATE_IDLE = 0,           // Czuwanie
  STATE_PRE_ALARM = 1,      // T1 - Alarm Wstępny (Miganie, 10s)
  STATE_VERIFICATION = 2,   // T2 - Weryfikacja (Ciągłe, 20s)
  STATE_FIRE_ALARM = 3,     // POŻAR (Pełna moc)
  STATE_FAULT = 4,          // Awaria
  STATE_DSO_TEST = 5,       // Test Dźwięku
  STATE_SMOKE_VENT = 6      // Oddymianie techniczne
};

#endif