#ifndef SYSTEM_STATE_H
#define SYSTEM_STATE_H

enum SystemState {
  STATE_IDLE = 0,         // Czuwanie
  STATE_FIRE_ALARM = 1,   // Pożar
  STATE_FAULT = 2,        // Awaria
  STATE_DSO_TEST = 3,     // Test Dźwięku
  STATE_SMOKE_VENT = 4,   // Oddymianie
  STATE_POWER_LOSS = 5    // Brak zasilania
};

#endif