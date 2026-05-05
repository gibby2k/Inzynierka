#ifndef DSO_MANAGER_H
#define DSO_MANAGER_H

#include <Arduino.h>

void initDso();                // Inicjalizacja sprzętowa
void playDsoMessage(int code); // Odtwarzanie na podstawie kodu z JSON
void stopDso();                // Zatrzymanie odtwarzania

#endif