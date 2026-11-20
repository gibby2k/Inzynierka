#include <esp_task_wdt.h>
#include <esp_arduino_version.h>
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
String currentUser = "SYSTEM";
String actionSource = "FIZYCZNIE"; 

Preferences configPrefs; // Zostawiamy tylko jedną deklarację
uint16_t timeT1 = 30; // Domyślnie 30 sekund
uint16_t timeT2 = 60; // Domyślnie 60 sekund

bool ntpEnabled = true; // Globalna flaga NTP

// --- Funkcja Zmiany Stanu (Globalna) ---
void changeState(SystemState newState, String name, String zone, String source, String user) {

  bool nowyStan = (newState != currentState);   // false = tylko odswiezenie ekranu (np. po zalogowaniu)
  currentState = newState;
  currentName = name;
  currentZone = zone;
  currentSource = source;
  currentUser = user;

  // --- LOGI (Serial) --- ODCHUDZONA RAMKA
  Serial.println("\n========================================");
  Serial.print("[SYSTEM] ZMIANA STANU -> "); Serial.println(name);
  Serial.print("[INFO]   Kod Stanu:   "); Serial.println((int)newState);
  Serial.print("[INFO]   Zrodlo:      "); Serial.println(source);
  Serial.print("[INFO]   Uzytkownik:  "); Serial.println(user);
  Serial.println("========================================\n");

  // ... (reszta funkcji pozostaje bez zmian - zapisy logger.addEvent itd.)

  // --- ZAPIS DO PAMIĘCI ZDARZEŃ (Selektywny) ---
  if (nowyStan) {
    if (newState == STATE_PRE_ALARM) logger.addEvent("AL.WST", zone); // np. "01.AL.WST L1-E1"
    if (newState == STATE_VERIFICATION) logger.addEvent("POTW.", zone); // np. "02.POTW. L1-E1"
    if (newState == STATE_FIRE_ALARM) logger.addEvent("POZAR", zone); // np. "03.POZAR L1-E1"
    if (newState == STATE_FAULT) logger.addEvent("AWARIA", zone); // np. "04.AWARIA L1-E1"
  }
  // (Celowo pomijamy STATE_IDLE oraz STATE_MENU)
  
  // --- ZMIANA NA LCD ---
  lcdMgr.updateStatus(currentState, (int)accessMgr.getLevel());

  netMgr.publishAlarm(currentState, currentName, currentZone, currentSource);
}

// =====================================================================
//  serviceDelay() - zamiennik delay() dla popupow i synchronizacji NTP.
//  W trakcie oczekiwania utrzymuje polaczenie MQTT (keep-alive, odbior
//  komend do bufora) i odswieza wyswietlacz 7-seg (odliczanie T1/T2).
//  Logika alarmowa (czujka, timery T1/T2) dziala na millis(), wiec
//  zostaje obsluzona zaraz po powrocie - bez ryzyka reentrancji.
// =====================================================================
void serviceDelay(unsigned long ms) {
  unsigned long start = millis();
  while (millis() - start < ms) {
    esp_task_wdt_reset();
    netMgr.loop();
    if (currentState == STATE_PRE_ALARM || currentState == STATE_VERIFICATION) {
      updateDisplayCountdown(sysCtrl.getRemainingTime());
    } else {
      updateDisplayClock();
    }
    delay(10);
  }
}

// =====================================================================
//  WATCHDOG PETLI GLOWNEJ - jesli logika centrali zawiesi sie na dluzej
//  niz 20 s, procesor zostanie zrestartowany (powod trafi do pamieci zdarzen).
// =====================================================================
void setupWatchdog() {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  esp_task_wdt_config_t cfg = { .timeout_ms = 20000, .idle_core_mask = 0, .trigger_panic = true };
  esp_task_wdt_reconfigure(&cfg);
#else
  esp_task_wdt_init(20, true);     // w starszych wersjach moze zostac domyslne 5 s - tez bezpieczne
#endif
  esp_task_wdt_add(NULL);          // nadzorowane zadanie: petla glowna
  Serial.println("WATCHDOG: aktywny (petla glowna).");
}

void setup() {
  Serial.begin(115200);
  Serial.printf("\nRdzen ESP32 Arduino: %d.%d.%d\n", ESP_ARDUINO_VERSION_MAJOR, ESP_ARDUINO_VERSION_MINOR, ESP_ARDUINO_VERSION_PATCH);
  
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
  timeT1 = configPrefs.getUShort("t1", 30);
  timeT2 = configPrefs.getUShort("t2", 60);
  ntpEnabled = configPrefs.getBool("ntp", true); // Wczytanie flagi NTP
  
  // --- Inicjalizacja czasu ---
  int lastH = configPrefs.getInt("h", 12); 
  int lastM = configPrefs.getInt("m", 0);  

  if (ntpEnabled) {
      // Uruchamiamy synchronizację atomową
      configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org", "time.nist.gov");
      Serial.println("NTP: Uruchomiono synchronizacje czasu.");
  } else {
      // Przywracamy ręczny czas z pamięci NVS
      struct tm tm;
      tm.tm_year = 2026 - 1900;
      tm.tm_mon = 1; tm.tm_mday = 23;
      tm.tm_hour = lastH;
      tm.tm_min = lastM;
      tm.tm_sec = 0;
      tm.tm_isdst = -1; // Zabezpieczenie przed skakaniem czasu!
      time_t t = mktime(&tm);
      struct timeval now = { .tv_sec = t };
      settimeofday(&now, NULL);
      Serial.printf("Zegar zainicjalizowany recznie z NVS: %02d:%02d\n", lastH, lastM);
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
  else if (reason == ESP_RST_TASK_WDT || reason == ESP_RST_WDT || reason == ESP_RST_INT_WDT) {
      // Watchdog: petla glowna zawiesila sie i zostala zrestartowana
      Serial.println("UWAGA: poprzedni restart wywolal WATCHDOG (zawieszenie programu)");
      logger.addEvent("RESTART", "WATCHDOG");
  }
  else if (reason == ESP_RST_BROWNOUT) {
      // Spadek napiecia zasilania (np. skok poboru pradu przez wzmacniacz DSO lub przekaznik)
      Serial.println("UWAGA: poprzedni restart wywolal BROWNOUT (spadek napiecia zasilania)!");
      logger.addEvent("RESTART", "ZASILAN.");
  }
  else if (reason == ESP_RST_PANIC) {
      // Restart po bledzie programu
      logger.addEvent("RESTART", "CRASH!");
  } 
  else {
      // Inny powód startu
      logger.addEvent("START", "INNY"); // <-- DWA ARGUMENTY!
  }

  setupWatchdog();
}

void loop() {
  esp_task_wdt_reset();            // "jestem zywa" dla watchdoga
  checkSensors();
  checkKeypad();
  
  sysCtrl.loop(); 
  
if (accessMgr.loop()) {
      sysCtrl.showPopup("CZAS MINAL", "WYLOGOWANO (P1)"); // <-- Zmiana tutaj
      actionSource = "SYSTEM"; 
      if (currentState == STATE_MENU) sysCtrl.exitMenu();
      else changeState(currentState, currentName, currentZone, actionSource, "SYSTEM");
      actionSource = "FIZYCZNIE";
  }

  netMgr.loop();

  // --- INTELIGENTNE ZARZĄDZANIE RUCHEM MQTT (Event-Driven + Heartbeat) ---
  static unsigned long lastMqttPublish = 0;
  static SystemState lastState = STATE_IDLE;
  static int lastMenuCursor = -1;
  static int lastMenuPage = -1;
  static String lastLoginBuf = "";
  static bool lastLoginState = false; // <-- DODANA ZMIENNA
  static int lastEditHour = -1;     
  static int lastEditMinute = -1;   
  static uint16_t lastT1 = 0;       
  static uint16_t lastT2 = 0; 
  static int lastClockPartIndex = -1;      
  
  bool needsUpdate = false;

  // 1. Wykrycie zmiany głównego stanu systemu
  if (currentState != lastState) {
      needsUpdate = true;
      lastState = currentState;
  }
  
  // 2. Odliczanie czasów T1/T2 (wymusza odświeżanie co 1 sekundę, aby web-timer działał)
  if (currentState == STATE_PRE_ALARM || currentState == STATE_VERIFICATION || currentState == STATE_FIRE_ALARM) {
      if (millis() - lastMqttPublish >= 1000) {
          needsUpdate = true;
      }
  }
  
  // 3. Wykrycie interakcji z UI (klawiatura, menu, wpisywanie hasła, edycja wartości)
  int currentMenuCursor = sysCtrl.getMenuCursor();
  int currentMenuPage = sysCtrl.getMenuPage();
  String currentLoginBuf = accessMgr.getBufferMasked();
  bool currentLoginState = accessMgr.isLoginInProgress();
  
  int currentEditHour = sysCtrl.getEditHour();     
  int currentEditMinute = sysCtrl.getEditMinute(); 
  int currentClockPartIndex = sysCtrl.getClockPartIndex(); // <-- POBRANIE INDEKSU KURSDORA
  extern uint16_t timeT1, timeT2;                  

  if (currentMenuCursor != lastMenuCursor || currentMenuPage != lastMenuPage || 
      currentLoginBuf != lastLoginBuf || currentLoginState != lastLoginState ||
      currentEditHour != lastEditHour || currentEditMinute != lastEditMinute ||
      timeT1 != lastT1 || timeT2 != lastT2 ||
      currentClockPartIndex != lastClockPartIndex) { // <-- DODANY WARUNEK
      
      needsUpdate = true;
      lastMenuCursor = currentMenuCursor;
      lastMenuPage = currentMenuPage;
      lastLoginBuf = currentLoginBuf;
      lastLoginState = currentLoginState;
      lastEditHour = currentEditHour;       
      lastEditMinute = currentEditMinute;   
      lastT1 = timeT1;                      
      lastT2 = timeT2;  
      lastClockPartIndex = currentClockPartIndex; // <-- AKTUALIZACJA ZMIENNEJ
  }

  // 4. Heartbeat - Sygnał życia w stanach stabilnych wysyłany co 60 sekund
  if (millis() - lastMqttPublish >= 60000) {
      needsUpdate = true;
  }

  // Wykonanie publikacji (z dodatkowym zabezpieczeniem przed drganiem styków klawiatury - max 1 paczka na 200ms)
  if (needsUpdate && (millis() - lastMqttPublish >= 200)) {
      lastMqttPublish = millis();
      netMgr.publishTelemetry();
  }
  
  int czasDoAlarmu = sysCtrl.getRemainingTime();
  if (currentState == STATE_PRE_ALARM || currentState == STATE_VERIFICATION) {
      updateDisplayCountdown(czasDoAlarmu);
  } else if (sysCtrl.getMenuPage() == 51) {
      // Tryb edycji zegara ręcznego (strona 51)
      updateDisplayEditClock(sysCtrl.getEditHour(), sysCtrl.getEditMinute(), sysCtrl.getClockPartIndex());
  } else {
      updateDisplayClock();
  }

  // --- NADZOR DSO: awaria modulu nie blokuje centrali, ale musi zostac odnotowana ---
  static bool dsoInitChecked = false, dsoStuckLogged = false;
  if (!dsoInitChecked && isDsoInitDone()) {
      dsoInitChecked = true;
      // Brak odpowiedzi modulu NIE jest awaria - klony DFPlayera nie odpowiadaja,
      // a mimo to odtwarzaja. Pelny nadzor DSO (linia glosnikowa) - sesja AWARIA.
  }
  if (isDsoStuck()) {
      if (!dsoStuckLogged) {
          dsoStuckLogged = true;
          Serial.println("DSO: ZAWIESZENIE modulu - centrala pracuje dalej!");
          logger.addEvent("AWARIA", "DSO BLAD");
      }
  } else {
      dsoStuckLogged = false;
  }

  // --- ODBIÓR Z WIRTUALNEJ KLAWIATURY (jeden klawisz na obieg pętli) ---
  // Klawisz z WWW przechodzi przez TĘ SAMĄ logikę co fizyczna klawiatura,
  // więc obowiązują te same poziomy dostępu (np. RESET tylko od P2).
  char wirtualnyKlawisz;
  if (netMgr.popKey(wirtualnyKlawisz)) {
    actionSource = "WEB";
    sysCtrl.handleInput(wirtualnyKlawisz);
    netMgr.publishTelemetry();
    actionSource = "FIZYCZNIE";
  }
}
