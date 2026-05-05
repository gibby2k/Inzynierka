#include "SensorManager.h"
#include "Config.h"
#include "SystemController.h" // <-- Musimy widzieć Kontroler

// Odwołanie do obiektu globalnego zdefiniowanego w SapMain
extern SystemController sysCtrl;

/* =========================================================
   --- STARA LOGIKA (SYMULACJA ROZWARCIA - ZAKOMENTOWANA) ---
const int LICZBA_LINII = 3;
int pinyLinii[LICZBA_LINII] = {4, 5, 18};
bool stanAlarmu[LICZBA_LINII] = {false, false, false};

String nazwyLinii[LICZBA_LINII] = {"L1 - GREEN", "L2 - YELLOW", "L3 - BLUE"};
int nagraniaLinii[LICZBA_LINII] = {1, 2, 3}; 
========================================================= */

// =========================================================
// --- NOWA LOGIKA (CZUJKA ADR-20 PRZEZ TRANSOPTOR) ---
// =========================================================

const int LICZBA_LINII = 1; // Zostawiamy 1 dla kompatybilności z SystemController
const int PIN_CZUJKI = 4;   // Transoptor podłączony do GPIO 4
bool czujkaAlarm = false;   // Zmienna zatrzaskująca stan pożaru

// Tablice globalne, których oczekuje SystemController.cpp
String nazwyLinii[LICZBA_LINII] = {"ADR-20 DOMOWA"};
int nagraniaLinii[LICZBA_LINII] = {1}; // Numer komunikatu DSO dla tej czujki

void initSensors() {
  /* STARE:
  for(int i=0; i < LICZBA_LINII; i++) {
    pinMode(pinyLinii[i], INPUT_PULLUP);
  }
  */

  // NOWE:
  pinMode(PIN_CZUJKI, INPUT_PULLUP); // Wymusza stan HIGH w czasie czuwania
  Serial.println("SensorManager: Zainicjalizowano czujke ADR-20 na GPIO 4");
}

void checkSensors() {
  /* STARE:
  for(int i=0; i < LICZBA_LINII; i++) {
    if (digitalRead(pinyLinii[i]) == HIGH) { 
      if (!stanAlarmu[i]) {
        stanAlarmu[i] = true;
        sysCtrl.onSensorSignal(i); 
      }
    } else {
      if (stanAlarmu[i]) {
        stanAlarmu[i] = false;
      }
    }
  }
  */

  // NOWE: 
  // Transoptor zwiera do masy (GND) w momencie wykrycia alarmu przez ADR-20.
  // Odczytujemy stan LOW (0V). Ponieważ ADR-20 wysyła impulsy, "zatrzaskujemy"
  // alarm przy pierwszym odebranym sygnale LOW.
  
  if (digitalRead(PIN_CZUJKI) == LOW) { 
    
    // Wykryto stan alarmowy (pojawienie się stanu niskiego z transoptora)
    if (!czujkaAlarm) {
      czujkaAlarm = true; // Zatrzaśnięcie alarmu (zapamiętanie)
      
      // ZGŁOSZENIE DO KONTROLERA (Indeks 0, bo mamy tylko jedną czujkę)
      Serial.println("SensorManager: Wykryto impuls ALARMOWY z czujki ADR-20!");
      sysCtrl.onSensorSignal(0); 
    }
  } 
  // Zauważ, że usunęliśmy część "else". 
  // Czujka po puszczeniu przycisku TEST przestaje wysyłać sygnał, 
  // ale nasz system PPOŻ musi "pamiętać" alarm aż do ręcznego resetu.
}

// Funkcja resetująca pamięć alarmów
// Wywoływana przez SystemController przy przycisku RESET
void resetSensorLatches() {
  /* STARE:
  for (int i = 0; i < LICZBA_LINII; i++) {
    stanAlarmu[i] = false;
  }
  */
  
  // NOWE:
  czujkaAlarm = false; // Zapominamy o pożarze
  Serial.println("SensorManager: Pamiec czujki ADR-20 wyczyszczona (RESET).");
}