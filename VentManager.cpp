#include "VentManager.h"
#include "Config.h"

// Definicja pinu przekaźnika - sprawdzamy czy jest w Config, jesli nie to wymuszamy 15
#ifndef PIN_RELAY_FAN
  #define PIN_RELAY_FAN 15
#endif

void initVent() {
    pinMode(PIN_RELAY_FAN, OUTPUT);
    // Przy zworce na HIGH (H), stan LOW oznacza wyłączony przekaźnik
    digitalWrite(PIN_RELAY_FAN, LOW); 
    Serial.println("VentManager: Zainicjalizowano wentylator na GPIO 15");
}

void setVent(bool active) {
    if (active) {
        digitalWrite(PIN_RELAY_FAN, HIGH); // Załączenie (zworka na High Level Trigger)
        Serial.println("VentManager: PRZEKAZNIK ON - WENTYLATOR PRACUJE");
    } else {
        digitalWrite(PIN_RELAY_FAN, LOW);  // Wyłączenie
        Serial.println("VentManager: PRZEKAZNIK OFF - WENTYLATOR STOP");
    }
}