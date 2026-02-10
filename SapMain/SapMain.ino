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
}

void setup() {
  Serial.begin(115200);
  
  initDso();      // Start odtwarzacza
  lcdMgr.begin();
  lcdMgr.showStartup();
  
  initSensors();  // Start linii dozorowych (4, 5, 18)
  initKeypad();   // Start klawiatury (piny INPUT_PULLUP)
  
  // Stan początkowy
  changeState(STATE_IDLE, "CZUWANIE", "CALY_OBIEKT", "SYSTEM", "SYSTEM");
}

void loop() {
  // 1. Sprawdzanie fizycznych linii pożarowych (Transoptor)
  checkSensors(); 

  // 2. Obsługa klawiatury - Przycisk 1 jako WYCISZENIE (ACK)
  checkKeypad();

  // 3. Odbiór rozkazów z sieci
  String remoteCmd = netMgr.getLatestCommand();
  if (remoteCmd == "RESET") {
    changeState(STATE_IDLE, "CZUWANIE", "CALY_OBIEKT", "PANEL_WWW", "WebAdmin");
    stopDso(); // Zdalny reset też ucisza system
  }
}