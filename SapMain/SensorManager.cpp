#include "SensorManager.h"
#include "Config.h"
#include "SystemController.h"

extern SystemController sysCtrl;

const int LICZBA_LINII = 1;
const int PIN_CZUJKI = 4;
bool czujkaAlarm = false;

// NOWA ZMIENNA:
bool lineBlocked = false; 

String nazwyLinii[LICZBA_LINII] = {"ADR-20 DOMOWA"};
int nagraniaLinii[LICZBA_LINII] = {1};

void initSensors() {
  pinMode(PIN_CZUJKI, INPUT_PULLUP);
  Serial.println("SensorManager: Zainicjalizowano czujke ADR-20 na GPIO 4");
}

// NOWE FUNKCJE DO OBSŁUGI BLOKADY:
void setLineBlocked(bool blocked) {
    lineBlocked = blocked;
    Serial.println(blocked ? "SensorManager: Linia dozorowa ZABLOKOWANA" : "SensorManager: Linia dozorowa ODBLOKOWANA");
}

bool isLineBlocked() {
    return lineBlocked;
}

void checkSensors() {
  // Jeśli linia jest zablokowana, w ogóle nie sprawdzamy stanu pinu czujki
  if (lineBlocked) {
      return; 
  }

  if (digitalRead(PIN_CZUJKI) == LOW) { 
    if (!czujkaAlarm) {
      czujkaAlarm = true;
      Serial.println("SensorManager: Wykryto impuls ALARMOWY z czujki ADR-20!");
      sysCtrl.onSensorSignal(0); 
    }
  } 
}

void resetSensorLatches() {
  czujkaAlarm = false;
  Serial.println("SensorManager: Pamiec czujki ADR-20 wyczyszczona (RESET).");
}