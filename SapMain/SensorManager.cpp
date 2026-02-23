#include "SensorManager.h"
#include "Config.h"
#include "SystemController.h" // <-- Musimy widzieć Kontroler

// Odwołanie do obiektu globalnego zdefiniowanego w SapMain
extern SystemController sysCtrl;

// --- KONFIGURACJA LINII ---
const int LICZBA_LINII = 3;
int pinyLinii[LICZBA_LINII] = {4, 5, 18};
bool stanAlarmu[LICZBA_LINII] = {false, false, false};

// Tablice globalne (używane przez SystemController do nazw)
String nazwyLinii[LICZBA_LINII] = {"L1 - GREEN", "L2 - YELLOW", "L3 - BLUE"};
int nagraniaLinii[LICZBA_LINII] = {1, 2, 3}; 

void initSensors() {
  for(int i=0; i < LICZBA_LINII; i++) {
    pinMode(pinyLinii[i], INPUT_PULLUP);
  }
}

void checkSensors() {
  // Skanowanie linii w poszukiwaniu dymu
  for(int i=0; i < LICZBA_LINII; i++) {
    
    // Jeśli czujka zwarta (LOW) lub rozwarta (HIGH) - zależnie od typu.
    // U Ciebie w Configu masz INPUT_PULLUP, a alarm to HIGH (przerwanie pętli).
    if (digitalRead(pinyLinii[i]) == HIGH) { 
      
      // Wykryto stan alarmowy
      if (!stanAlarmu[i]) {
        stanAlarmu[i] = true;
        
        // ZGŁOSZENIE DO KONTROLERA
        // Nie włączamy tu wiatraka ani DSO. Tylko meldujemy.
        sysCtrl.onSensorSignal(i); 
      }
    } else {
      // Linia wróciła do normy
      if (stanAlarmu[i]) {
        stanAlarmu[i] = false;
        // Opcjonalnie: sysCtrl.onSensorClear(i);
      }
    }
  }
}

// Funkcja resetująca pamięć alarmów
// Wywoływana przez SystemController przy przycisku RESET (S4)
void resetSensorLatches() {
  for (int i = 0; i < LICZBA_LINII; i++) {
    stanAlarmu[i] = false; // Zapominamy, że ta linia była w alarmie
  }
  Serial.println("SensorManager: Pamięć czujek wyczyszczona.");
}