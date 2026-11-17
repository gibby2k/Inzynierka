#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>

void initDisplay();
void updateDisplayClock(); // Funkcja do wyświetlania zegara
void clearDisplay();
void updateDisplayCountdown(int seconds);
void updateDisplayEditClock(int hour, int minute, int blinkPart);

#endif