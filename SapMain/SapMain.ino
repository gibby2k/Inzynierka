#include "Config.h"
#include "SystemState.h"
#include "LcdManager.h"
#include "KeypadManager.h"
#include "SapNetwork.h"
#include "DsoManager.h"
#include "SensorManager.h"
#include "VentManager.h"
#include "DisplayManager.h"
#include "SystemController.h"
#include "AccessManager.h" // <-- DODAJ TO

// --- OBIEKTY GLOBALNE ---
LcdManager lcdMgr;
SapNetworkManager netMgr;
SystemController sysCtrl;
AccessManager accessMgr; // <-- TU POWSTAJE OBIEKT (bez tego błąd not declared)

SystemState currentState = STATE_IDLE;
String currentZone = "CALY_OBIEKT";
String currentSource = "SYSTEM";
String currentUser = "AUTO";

// --- Funkcja Zmiany Stanu (Globalna) ---
void changeState(SystemState newState, String name, String zone, String source, String user) {
  currentState = newState;
  currentZone = zone;
  currentSource = source;
  currentUser = user;

  // --- LOGI (Serial) ---
  Serial.println("\n========================================");
  Serial.print("[SYSTEM] ZMIANA STANU -> "); Serial.println(name);
  Serial.print("[INFO]   Strefa:      "); Serial.println(zone);
  Serial.print("[INFO]   Zrodlo:      "); Serial.println(source);
  Serial.print("[INFO]   Uzytkownik:  "); Serial.println(user);
  Serial.print("[INFO]   Kod Stanu:   "); Serial.println((int)newState);
  
  // --- ZMIANA TUTAJ: Pobieramy poziom z accessMgr ---
  // Rzutujemy enum na int ((int)accessMgr.getLevel())
  lcdMgr.updateStatus(currentState, (int)accessMgr.getLevel());
  
  Serial.println("========================================\n");
  
  // netMgr.publishStatus(...); // Odkomentuj gdy ruszymy z WiFi
}

void setup() {
  Serial.begin(115200);
  
  initDso();
  lcdMgr.begin();
  lcdMgr.showStartup();
  initSensors();
  initVent();
  initKeypad();
  initDisplay();
  
  sysCtrl.begin(); 
  accessMgr.begin(); // <-- Start Managera Dostępu

  changeState(STATE_IDLE, "CZUWANIE", "CALY_OBIEKT", "SYSTEM", "SYSTEM");
}

void loop() {
  checkSensors();
  checkKeypad();
  
  sysCtrl.loop(); 
  accessMgr.loop(); // <-- Sprawdza autowylogowanie

  int czasDoAlarmu = sysCtrl.getRemainingTime();
  
  if (currentState == STATE_PRE_ALARM || currentState == STATE_VERIFICATION) {
      updateDisplayCountdown(czasDoAlarmu);
  } else {
      updateDisplayClock();
  }

  String remoteCmd = netMgr.getLatestCommand();
  if (remoteCmd == "RESET") {
    sysCtrl.onResetPressed();
  }
}