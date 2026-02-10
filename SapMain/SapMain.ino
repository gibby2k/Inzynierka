/*
 * SYSTEM SAP + DSO (Projekt Inżynierski)
 * Wersja: v1.1 (Zaktualizowana o logikę ACK i SensorManager)
 */

#include "Config.h"
#include "SystemState.h"
#include "LcdManager.h"
#include "KeypadManager.h"
#include "SapNetwork.h"
#include "DsoManager.h"
#include "SensorManager.h"
#include "VentManager.h"
#include "IndicatorManager.h"

// --- OBIEKTY ---
LcdManager lcdMgr;
// Usunięto KeypadManager keypadMgr, bo używamy funkcji globalnych z KeypadManager.cpp
SapNetworkManager netMgr;

SystemState currentState = STATE_IDLE;

// Zmienne pomocnicze
String currentZone = "CALY_OBIEKT";
String currentSource = "SYSTEM";
String currentUser = "AUTO";

void changeState(SystemState newState, String name, String zone, String source, String user) {
  currentState = newState;
  currentZone = zone;
  currentSource = source;
  currentUser = user;

  lcdMgr.updateStatus(currentState);
  // netMgr.publishStatus(currentState, name, zone, source, user); // Odkomentuj gdy WiFi będzie gotowe

  // Załóżmy, że DSO gra tylko przy alarmie lub teście.
    // Przekazujemy 'false' dla dsoActive, bo obsłużymy to logiką stanów
    updateIndicatorsFromState(currentState, false);
}

void setup() {
  Serial.begin(115200); // [cite: 76]
  
  initDso(); // [cite: 79]
  lcdMgr.begin(); // [cite: 80]
  lcdMgr.showStartup();
  
  initSensors(); // [cite: 77, 82]
  initVent();    // Inicjalizacja wentylatora (GPIO 15)
  initKeypad();  // Klawiatura matrycowa [cite: 80, 89]
  
  initIndicators();

  changeState(STATE_IDLE, "CZUWANIE", "CALY_OBIEKT", "SYSTEM", "SYSTEM");
}

void loop() {
  // 1. Sprawdzanie fizycznych linii pożarowych (Transoptor)
  checkSensors(); 

  // 2. Obsługa klawiatury - Przycisk 1 jako WYCISZENIE (ACK)
  checkKeypad();

  // Pobierz status DSO (trzeba dodać małą funkcję w DsoManager lub użyć zmiennej globalnej)
  // Na razie załóżmy, że przekazujemy false, dopóki nie dodamy gettera.
  extern bool audioWyciszone; // Możemy użyć tego jako hacka, albo dodać flagę w DsoManager

  // 3. Odbiór rozkazów z sieci
  String remoteCmd = netMgr.getLatestCommand();
  if (remoteCmd == "RESET") {
    changeState(STATE_IDLE, "CZUWANIE", "CALY_OBIEKT", "PANEL_WWW", "WebAdmin");
    stopDso(); // Zdalny reset też ucisza system
  }
}