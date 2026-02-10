#include "SensorManager.h"
#include "SystemState.h"
#include "DsoManager.h"
#include "LcdManager.h"

extern LcdManager lcdMgr;
extern void changeState(SystemState newState, String name, String zone, String source, String user);

const int LICZBA_LINII = 3;
int pinyLinii[LICZBA_LINII] = {4, 5, 18};
bool stanAlarmu[LICZBA_LINII] = {false, false, false};
String nazwyLinii[LICZBA_LINII] = {"L1 - GREEN", "L2 - YELLOW", "L3 - BLUE"};
int nagraniaLinii[LICZBA_LINII] = {1, 2, 3};

void initSensors() {
  for(int i=0; i < LICZBA_LINII; i++) {
    pinMode(pinyLinii[i], INPUT_PULLUP);
  }
}

void checkSensors() {
  bool czyJakikolwiekAlarm = false;

  for(int i=0; i < LICZBA_LINII; i++) {
    if (digitalRead(pinyLinii[i]) == HIGH) { // Jest PRZERWA
      czyJakikolwiekAlarm = true;
      if (!stanAlarmu[i]) {
        stanAlarmu[i] = true;
        // Ważne: najpierw zmieniamy stan (LCD), potem dźwięk
        changeState(STATE_FIRE_ALARM, "ALARM", nazwyLinii[i], "IN" + String(i+1), "FIZ");
        playDsoMessage(nagraniaLinii[i]);
      }
    } else { // Linia ZAMKNIĘTA
      if (stanAlarmu[i]) {
        stanAlarmu[i] = false;
        Serial.println("Naprawiono: " + nazwyLinii[i]);
      }
    }
  }

  static bool poprzedniStanGlobalny = false;
  if (!czyJakikolwiekAlarm && poprzedniStanGlobalny) {
    // Odwołanie alarmu - tylko raz
    changeState(STATE_IDLE, "CZUWANIE", "CALY_OBIEKT", "SYSTEM", "AUTO");
    playDsoMessage(4); 
  }
  poprzedniStanGlobalny = czyJakikolwiekAlarm;
}