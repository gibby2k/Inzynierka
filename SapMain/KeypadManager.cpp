#include "KeypadManager.h"
#include "DsoManager.h"

// Definicja pinów zgodnie z Twoim połączeniem
const int R_PINS[] = {13, 12, 14, 27}; // Row 1, 2, 3, 4
const int C_PINS[] = {26, 25, 33, 32}; // Column 1, 2, 3, 4

unsigned long ostatniCzasACK = 0;

void initKeypad() {
  // Wiersze jako wyjścia
  for (int i = 0; i < 4; i++) {
    pinMode(R_PINS[i], OUTPUT);
    digitalWrite(R_PINS[i], HIGH); // Stan spoczynkowy: HIGH
  }
  // Kolumny jako wejścia z wewnętrznym podciągnięciem
  for (int i = 0; i < 4; i++) {
    pinMode(C_PINS[i], INPUT_PULLUP);
  }
}

void checkKeypad() {
  // Skanujemy Row 1 (tam są przyciski S1, S2, S3, S4)
  digitalWrite(R_PINS[0], LOW); 

  // Sprawdzamy Column 1 (Pin D26) -> to będzie przycisk S1
  if (digitalRead(C_PINS[0]) == LOW) { 
    if (millis() - ostatniCzasACK > 400) { // Debouncing
      Serial.println("Klawiatura: ACK - Wyciszenie przyciskiem S1");
      stopDso();
      ostatniCzasACK = millis();
    }
  }

  // Opcjonalnie: Sprawdzamy Column 4 (Pin D32) -> to będzie przycisk S4
  if (digitalRead(C_PINS[3]) == LOW) { 
    if (millis() - ostatniCzasACK > 400) {
      Serial.println("Klawiatura: ACK - Wyciszenie przyciskiem S4");
      stopDso();
      ostatniCzasACK = millis();
    }
  }

  digitalWrite(R_PINS[0], HIGH); // Powrót do stanu HIGH
}