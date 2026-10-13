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
#include "AccessManager.h"
#include "EventLogger.h"
#include <esp_system.h>
#include <Preferences.h>

// --- OBIEKTY GLOBALNE ---
LcdManager lcdMgr;
SapNetworkManager netMgr;
SystemController sysCtrl;
AccessManager accessMgr;
EventLogger logger;

SystemState currentState = STATE_IDLE;
String currentName = "CZUWANIE";
String currentZone = "CALY_OBIEKT";
String currentSource = "SYSTEM";
String currentUser = "AUTO";

Preferences configPrefs; // Nowy obiekt do ustawień
uint16_t timeT1 = 30; // Domyślnie 30 sekund
uint16_t timeT2 = 60; // Domyślnie 60 sekund

// --- Funkcja Zmiany Stanu (Globalna) ---
void changeState(SystemState newState, String name, String zone, String source, String user) {
  currentState = newState;
  currentName = name;
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
  Serial.println("========================================\n");

  // --- ZAPIS DO PAMIĘCI ZDARZEŃ (Selektywny) ---
  if (newState == STATE_PRE_ALARM) logger.addEvent("AL.WST", zone); // np. "01.AL.WST L1-E1"
  if (newState == STATE_VERIFICATION) logger.addEvent("POTW.", zone); // np. "02.POTW. L1-E1"
  if (newState == STATE_FIRE_ALARM) logger.addEvent("POZAR", zone); // np. "03.POZAR L1-E1"
  if (newState == STATE_FAULT) logger.addEvent("AWARIA", zone); // np. "04.AWARIA L1-E1"
  // (Celowo pomijamy STATE_IDLE oraz STATE_MENU)
  
  // --- ZMIANA NA LCD ---
  lcdMgr.updateStatus(currentState, (int)accessMgr.getLevel());
}

void setup() {
  Serial.begin(115200);
  
  // sieć
  netMgr.begin(); 
  
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

  logger.begin(); // To już masz

  // Wczytywanie konfiguracji
  configPrefs.begin("sap_cfg", false);
  timeT1 = configPrefs.getUShort("t1", 30); // 30 to wartość domyślna
  timeT2 = configPrefs.getUShort("t2", 60); // 60 to wartość domyślna
  
  // --- Wczytywanie ostatniego czasu ---
  int lastH = configPrefs.getInt("h", 12); // domyślnie 12
  int lastM = configPrefs.getInt("m", 0);  // domyślnie 0

  // Jeśli nie ma internetu, ustaw zegar systemowy na ostatnio zapisaną wartość
  if (!netMgr.isTimeSynced()) {
      struct tm tm;
      tm.tm_year = 2026 - 1900;
      tm.tm_mon = 1; tm.tm_mday = 23;
      tm.tm_hour = lastH;
      tm.tm_min = lastM;
      tm.tm_sec = 0;
      time_t t = mktime(&tm);
      struct timeval now = { .tv_sec = t };
      settimeofday(&now, NULL);
      Serial.printf("Zegar zainicjalizowany z NVS: %02d:%02d\n", lastH, lastM);
  }

  // --- ROZPOZNAWANIE POWODU STARTU SYSTEMU ---
  esp_reset_reason_t reason = esp_reset_reason();
  
  if (reason == ESP_RST_POWERON) {
      // Zimny start - po całkowitym odcięciu prądu
      logger.addEvent("START", "SYSTEMU"); // <-- DWA ARGUMENTY!
  } 
  else if (reason == ESP_RST_SW) {
      // Ciepły start - zresetowano przyciskiem (EN) lub wgrano nowy kod
      logger.addEvent("RESTART", "SYSTEMU"); // <-- DWA ARGUMENTY!
  } 
  else if (reason == ESP_RST_PANIC || reason == ESP_RST_INT_WDT) {
      // Restart po awarii / zacięciu się procesora (tzw. Watchdog)
      logger.addEvent("RESTART", "CRASH!"); // <-- DWA ARGUMENTY!
  } 
  else {
      // Inny powód startu
      logger.addEvent("START", "INNY"); // <-- DWA ARGUMENTY!
  }
}

void loop() {
  checkSensors();
  checkKeypad();
  
  sysCtrl.loop(); 
  
  if (accessMgr.loop()) {
      lcdMgr.showInfo("CZAS MINAL", "WYLOGOWANO (P1)");
      delay(1500);
      if (currentState == STATE_MENU) sysCtrl.exitMenu();
      else changeState(currentState, currentZone, currentZone, currentSource, "AUTO");
  }

  netMgr.loop();

  // Wysyłka kontrolna co 2 sekundy (Heartbeat dla panelu WWW)
  static unsigned long lastMqttPublish = 0;
  if (millis() - lastMqttPublish > 2000) {
      lastMqttPublish = millis();
      netMgr.publishStatus(currentState, currentName, currentZone, currentSource, currentUser);
  }
  
  int czasDoAlarmu = sysCtrl.getRemainingTime();
  if (currentState == STATE_PRE_ALARM || currentState == STATE_VERIFICATION) {
      updateDisplayCountdown(czasDoAlarmu);
  } else {
      updateDisplayClock();
  }

  // --- ODBIÓR Z WIRTUALNEJ KLAWIATURY ---
  String remoteCmd = netMgr.getLatestCommand();
  if (remoteCmd == "RESET") {
    sysCtrl.onResetPressed();
  } 
  else if (remoteCmd.startsWith("KEY_")) {
    char wirtualnyKlawisz = remoteCmd.charAt(4);
    sysCtrl.handleInput(wirtualnyKlawisz); 
    
    // WYMUSZAMY natychmiastowe odświeżenie danych na stronie po kliknięciu!
    netMgr.publishStatus(currentState, currentName, currentZone, currentSource, currentUser);
  }
}
