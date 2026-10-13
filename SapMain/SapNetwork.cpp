#include "SapNetwork.h"
#include "DsoManager.h"
#include <time.h> 
#include "SystemController.h"
#include "AccessManager.h"
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

  // --- DODAJ TĘ LINIJKĘ ---
  client.setBufferSize(512); // Zwiększamy limit z 256 na 512 bajtów
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
    
    if (client.connect(clientId.c_str())) {
      Serial.println("Polaczono!");
      client.subscribe(MQTT_TOPIC_CONTROL); // Nasłuchujemy rozkazów
    } else {
      Serial.print("Blad rc="); Serial.println(client.state());
    }
  }
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

void SapNetworkManager::publishStatus(SystemState state, String stateName, String zone, String source, String user) {
  if (!client.connected()) return;

  struct tm timeinfo;
  char timeString[25];
  if(getLocalTime(&timeinfo)){
    strftime(timeString, 25, "%Y-%m-%d %H:%M:%S", &timeinfo);
  } else {
    strcpy(timeString, "NO_TIME"); 
  }

  // Zbieranie diagnostyki i stanu "na żywo"
  long rssi = WiFi.RSSI();
  String ipAddr = WiFi.localIP().toString();
  int timerValue = sysCtrl.getRemainingTime();
  int currentLevel = (int)accessMgr.getLevel();
  bool isLogin = accessMgr.isLoginInProgress();
  String loginBuf = accessMgr.getBufferMasked();
// --- NOWE: Pobieranie danych o menu ---
  int menuCursor = sysCtrl.getMenuCursor(); 
  int menuPage = sysCtrl.getMenuPage();     

  char jsonBuffer[600]; 
  snprintf(jsonBuffer, sizeof(jsonBuffer),
    "{"
      "\"meta\":{\"ts\":\"%s\"},"
      "\"payload\":{\"code\":%d,\"name\":\"%s\",\"zone\":\"%s\",\"lvl\":%d,\"login\":%s,\"buf\":\"%s\",\"timer\":%d,"
      "\"menu\":{\"cursor\":%d,\"page\":%d}}," // <--- Dodano obiekt menu
      "\"diag\":{\"wifi\":%ld,\"ip\":\"%s\"}"
    "}",
    timeString,                  
    (int)state,                  
    stateName.c_str(),           
    zone.c_str(),                
    currentLevel,                
    isLogin ? "true" : "false",  
    loginBuf.c_str(),            
    timerValue,                  
    menuCursor,                  // Zmienna %d dla kursora
    menuPage,                    // Zmienna %d dla strony menu
    rssi,                        
    ipAddr.c_str()               
  );

  // Wysyłka
  client.publish(MQTT_TOPIC_STATUS, jsonBuffer, true); 
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
    // Dodatkowe sprawdzenie, czy rok nie jest "fabryczny" (1970)
    if (timeinfo.tm_year < (2020 - 1900)) {
        return false;
    }
    return true;
}