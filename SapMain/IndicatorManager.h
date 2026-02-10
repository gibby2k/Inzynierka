#ifndef INDICATOR_MANAGER_H
#define INDICATOR_MANAGER_H

#include <Arduino.h>
#include "SystemState.h"

// Definicja kolorów dla czytelności
enum IndicatorColor {
  COLOR_OFF,
  COLOR_GREEN,  // Czuwanie
  COLOR_RED,    // Alarm
  COLOR_YELLOW, // Awaria/Ostrzeżenie
  COLOR_BLUE    // Praca DSO
};

void initIndicators();
void setIndicatorColor(IndicatorColor color);
void updateIndicatorsFromState(SystemState state, bool dsoActive); // Dodajemy info o DSO

#endif