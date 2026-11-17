#include "SapNetwork.h"
#include "DsoManager.h"
#include <time.h> 
#include "SystemController.h"
#include "AccessManager.h"
#include "EventLogger.h"

extern EventLogger logger;
extern SystemController sysCtrl;
extern AccessManager accessMgr;

// --- GLOBALNY HACK DLA CALLBACKU ---
SapNetworkManager* globalNetMgrPtr = nullptr;
void globalCallback(char* topic, byte* payload, unsigned int length) {
  if (globalNetMgrPtr != nullptr) {
    globalNetMgrPtr->handleMessage(topic, payload, length);
  }
}
// -----------------------------------

SapNetworkManager::SapNetworkManager() : client(espClient) {
  commandReceived = false;
  lastCommand = "";
  globalNetMgrPtr = this;
}

void SapNetworkManager::begin() {
  Serial.print("Laczenie z WiFi: ");
  Serial.println(WIFI_SSID);
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if(WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi OK!");
    Serial.print("IP: "); Serial.println(WiFi.localIP());
    
    // --- DODAJ TĘ LINIJKĘ ---
    // Konfiguracja czasu dla Polski (Strefa CET/CEST)
    configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org", "time.nist.gov");
    Serial.println("NTP: Uruchomiono synchronizacje czasu.");
    
  } else {
    Serial.println("\nWiFi ERROR (Offline Mode)");
  }

 client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(globalCallback);

  // ZMIEŃ 512 na 1024 bajty
  client.setBufferSize(1024); 
}

void SapNetworkManager::loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}

void SapNetworkManager::reconnect() {
  if(WiFi.status() != WL_CONNECTED) return;

  static unsigned long lastAttempt = 0;
  if (millis() - lastAttempt > 5000) {
    lastAttempt = millis();
    Serial.print("Laczenie z MQTT...");
    
    // Generujemy losowe ID, żeby broker nas nie wyrzucał
    String clientId = "ESP32-SAP-" + String(random(0xffff), HEX);
    
    // LWT: jesli centrala zniknie bez pozegnania, broker sam opublikuje "OFFLINE" (retained)
    if (client.connect(clientId.c_str(), MQTT_TOPIC_STATUS, 1, true, "OFFLINE")) {
      Serial.println("Polaczono!");
      client.subscribe(MQTT_TOPIC_CONTROL); // Nasłuchujemy rozkazów
      client.publish(MQTT_TOPIC_STATUS, "ONLINE", true);

      // Po (ponownym) polaczeniu nadpisujemy retained stan alarmu AKTUALNYM stanem
      // (changeState() w setup() wykonuje sie zanim MQTT jest polaczone)
      extern SystemState currentState;
      extern String currentName, currentZone, currentSource;
      publishAlarm(currentState, currentName, currentZone, currentSource);
      publishTelemetry();
    } else {
      Serial.print("Blad rc="); Serial.println(client.state());
    }
  }
}

void SapNetworkManager::publishTelemetry() {
  if (!client.connected()) return;
  struct tm timeinfo;
  char timeString[25] = "NO_TIME";
  if(getLocalTime(&timeinfo)) strftime(timeString, 25, "%H:%M:%S", &timeinfo);
  
  // --- ZBIERANIE DANYCH DLA MENU WWW (w tym pop-upy i podstrony) ---
  int menuCursor = sysCtrl.getMenuCursor();
  int menuPage = sysCtrl.getPopupActive() ? 99 : sysCtrl.getMenuPage();
  int subCursor = sysCtrl.getMenuSubCursor(); 
  char menuObj[200]; 
  
  if (sysCtrl.getPopupActive()) {
      snprintf(menuObj, sizeof(menuObj), "\"cursor\":0,\"page\":99,\"p1\":\"%s\",\"p2\":\"%s\"", sysCtrl.getPopupL1().c_str(), sysCtrl.getPopupL2().c_str());
  } else if (menuPage == 11) {
      extern uint16_t timeT1, timeT2;
      int editVal = (subCursor == 0) ? timeT1 : timeT2;
      snprintf(menuObj, sizeof(menuObj), "\"cursor\":%d,\"page\":11,\"val\":%d", menuCursor, editVal);
  } else if (menuPage == 4) {
      extern EventLogger logger;
      LogEvent ev; String logSt = ""; String logZo = ""; String logTs = ""; int logId = 0;
      int logTotal = logger.getCount();
      if (logTotal > 0) { logger.getEvent(subCursor, ev); logId = ev.id % 100; logSt = String(ev.state); logZo = String(ev.zone); logTs = String(ev.timestamp); }
      snprintf(menuObj, sizeof(menuObj), "\"cursor\":%d,\"page\":4,\"id\":%d,\"st\":\"%s\",\"zo\":\"%s\",\"ts\":\"%s\",\"tot\":%d", subCursor, logId, logSt.c_str(), logZo.c_str(), logTs.c_str(), logTotal);
  } else if (menuPage == 5 || menuPage == 51) {
      extern bool ntpEnabled;
      snprintf(menuObj, sizeof(menuObj), "\"cursor\":%d,\"page\":%d,\"h\":%d,\"m\":%d,\"p\":%d,\"ntp\":%s", menuCursor, menuPage, sysCtrl.getEditHour(), sysCtrl.getEditMinute(), sysCtrl.getClockPartIndex(), ntpEnabled ? "true" : "false");
  } else {
      snprintf(menuObj, sizeof(menuObj), "\"cursor\":%d,\"page\":%d,\"sub\":%d", menuCursor, menuPage, subCursor);
  }

  // --- BUDOWANIE I WYSYŁKA ---
  char json[400];
  snprintf(json, sizeof(json),
    "{\"ts\":\"%s\",\"timer\":%d,\"lvl\":%d,\"login\":%s,\"buf\":\"%s\",\"menu\":{%s},\"diag\":{\"wifi\":%ld,\"ip\":\"%s\"}}",
    timeString, sysCtrl.getRemainingTime(), (int)accessMgr.getLevel(), 
    accessMgr.isLoginInProgress() ? "true" : "false", accessMgr.getBufferMasked().c_str(), 
    menuObj, WiFi.RSSI(), WiFi.localIP().toString().c_str());
    
  client.publish(MQTT_TOPIC_TELEMETRY, json, false);
}

void SapNetworkManager::publishAlarm(SystemState state, String stateName, String zone, String source) {
  if (!client.connected()) return;
  char json[150];
  snprintf(json, sizeof(json), "{\"code\":%d,\"name\":\"%s\",\"zone\":\"%s\",\"src\":\"%s\"}", 
           (int)state, stateName.c_str(), zone.c_str(), source.c_str());
  client.publish(MQTT_TOPIC_ALARMS, json, true); // RETAINED: nowy klient WWW od razu dostaje ostatni stan alarmu
}

void SapNetworkManager::publishLog(uint32_t id, String st, String zo, String ts) {
  if (!client.connected()) return;
  char json[150];
  snprintf(json, sizeof(json), "{\"id\":%u,\"st\":\"%s\",\"zo\":\"%s\",\"ts\":\"%s\"}", 
           id, st.c_str(), zo.c_str(), ts.c_str());
  client.publish(MQTT_TOPIC_LOGS, json, false);
}

void SapNetworkManager::handleMessage(char* topic, byte* payload, unsigned int length) {
    String msg = "";
    for (int i = 0; i < length; i++) { msg += (char)payload[i]; }
    
    Serial.print("RX [MQTT]: "); Serial.println(msg);

    if (msg.indexOf("ALARM_FIRE") >= 0) {
        playDsoMessage(999);
        lastCommand = "FIRE_ALARM";
        commandReceived = true;
    } 
    else if (msg.indexOf("ALARM_CLEAR") >= 0) {
        playDsoMessage(100);
        lastCommand = "CLEAR_ALARM";
        commandReceived = true;
    }
    else if (msg.indexOf("RESET") >= 0) {
        stopDso();
        lastCommand = "RESET";
        commandReceived = true;
    }
    // --- BARDZIEJ NIEZAWODNY ODBIÓR KLAWISZY ---
    else if (msg.indexOf("KEY:") >= 0) {
        int idx = msg.indexOf("KEY:") + 4;
        if (idx < msg.length()) {
            char key = msg.charAt(idx); 
            lastCommand = String("KEY_") + key;
            commandReceived = true;
        }
    }
}

String SapNetworkManager::getLatestCommand() {
  if (commandReceived) {
    commandReceived = false;
    return lastCommand;
  }
  return "";
}


bool SapNetworkManager::isMqttConnected() {
    return client.connected();
}

bool SapNetworkManager::isTimeSynced() {
    struct tm timeinfo;
    // getLocalTime zwraca false, jeśli czas nie jest ustawiony (brak synchronizacji)
    if (!getLocalTime(&timeinfo, 0)) {
        return false;
    }
    // Sprawdzenie, czy rok jest poprawny (nowszy niż 2020)
    if (timeinfo.tm_year < (2020 - 1900)) {
        return false;
    }
    return true;
}
