#include "IndicatorManager.h"

// Piny dla RGB (zgodnie z naszym ustaleniem: 2, 19, 23)
#define PIN_LED_R 2
#define PIN_LED_G 19
#define PIN_LED_B 23

void initIndicators() {
  pinMode(PIN_LED_R, OUTPUT);
  pinMode(PIN_LED_G, OUTPUT);
  pinMode(PIN_LED_B, OUTPUT);
  
  // Test sekwencji startowej (Red-Green-Blue)
  setIndicatorColor(COLOR_RED); delay(300);
  setIndicatorColor(COLOR_GREEN); delay(300);
  setIndicatorColor(COLOR_BLUE); delay(300);
  setIndicatorColor(COLOR_OFF);
}

void setIndicatorColor(IndicatorColor color) {
  // Reset (wyłączamy wszystko)
  digitalWrite(PIN_LED_R, LOW);
  digitalWrite(PIN_LED_G, LOW);
  digitalWrite(PIN_LED_B, LOW);

  switch (color) {
    case COLOR_GREEN:
      digitalWrite(PIN_LED_G, HIGH);
      break;
    case COLOR_RED:
      digitalWrite(PIN_LED_R, HIGH);
      break;
    case COLOR_YELLOW:
      digitalWrite(PIN_LED_R, HIGH);
      digitalWrite(PIN_LED_G, HIGH); // Mieszanie R+G daje żółty
      break;
    case COLOR_BLUE:
      digitalWrite(PIN_LED_B, HIGH);
      break;
    case COLOR_OFF:
      break;
  }
}

void updateIndicatorsFromState(SystemState state, bool dsoActive) {
  // Priorytet 1: Jeśli DSO gra (komunikat), świecimy na niebiesko (lub migamy)
  // Ale zazwyczaj Alarm (Czerwony) jest ważniejszy. Ustalmy priorytety:
  
  if (state == STATE_FIRE_ALARM) {
    setIndicatorColor(COLOR_RED); // Pożar zawsze czerwony
  } 
  else if (dsoActive) {
    setIndicatorColor(COLOR_BLUE); // Jeśli nie ma alarmu pożarowego, a gra DSO (np. test/komunikat)
  }
  else if (state == STATE_FAULT || state == STATE_SMOKE_VENT) {
    setIndicatorColor(COLOR_YELLOW); // Awaria lub wentylacja techniczna
  }
  else {
    setIndicatorColor(COLOR_GREEN); // Wszystko OK
  }
}