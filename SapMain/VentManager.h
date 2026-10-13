#ifndef VENT_MANAGER_H
#define VENT_MANAGER_H

#include <Arduino.h>

void initVent();            // Inicjalizacja pinu przekaźnika (GPIO 15)
void setVent(bool active);  // Włączenie/wyłączenie wentylatora

#endif