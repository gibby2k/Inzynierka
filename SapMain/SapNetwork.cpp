#include "SapNetwork.h"
#include "MqttRootCA.h"
#include <time.h>
#include "SystemController.h"
#include "AccessManager.h"
#include "EventLogger.h"

extern EventLogger logger;
extern SystemController sysCtrl;
extern AccessManager accessMgr;
extern SystemState currentState;
extern bool ntpEnabled;

// --- GLOBALNY HACK DLA CALLBACKU ---
SapNetworkManager* globalNetMgrPtr = nullptr;
void globalCallback(char* topic, byte* payload, unsigned int length) {
  if (globalNetMgrPtr != nullptr) {
    globalNetMgrPtr->handleMessage(topic, payload, length);
  }
}
// -----------------------------------

// =====================================================================
//  NADZOR WI-FI
//  Centrala dziala lokalnie takze bez sieci (detekcja, alarm, DSO).
//  Siec jest odzyskiwana w tle; restart tylko w ostatecznosci i tylko
//  w CZUWANIU - nigdy w trakcie alarmu, menu czy testu.
// =====================================================================
static const unsigned long WIFI_RETRY_MS         = 15000;   // ponowna proba co 15 s
static const unsigned long WIFI_LOG_AFTER_MS     = 60000;   // wpis "BRAK SIECI" po 1 min
static const unsigned long WIFI_RESTART_AFTER_MS = 600000;  // restart po 10 min bez sieci...
static const uint32_t      MAX_AUTO_RESTARTS     = 3;       // ...najwyzej 3 razy z rzedu
static const unsigned long NET_TASK_STUCK_MS     = 120000;  // zadanie sieciowe bez oznak zycia 2 min

// Licznik restartow przezywa programowy restart (pamiec RTC), zeruje sie po wylaczeniu zasilania
RTC_NOINIT_ATTR static uint32_t wifiRestartMagic;
RTC_NOINIT_ATTR static uint32_t wifiRestartCount;
static const uint32_t RESTART_MAGIC = 0x5A9F1C2B;

static uint32_t getRestartCount() {
  if (wifiRestartMagic != RESTART_MAGIC) { wifiRestartMagic = RESTART_MAGIC; wifiRestartCount = 0; }
  return wifiRestartCount;
}

SapNetworkManager::SapNetworkManager() : client(espClient) {
  telemetryBuf[0] = '\0';
  lastAlarmBuf[0] = '\0';
  globalNetMgrPtr = this;
}

void SapNetworkManager::ensureQueues() {
  if (!keyQueue) keyQueue = xQueueCreate(16, sizeof(char));
  if (!outQueue) outQueue = xQueueCreate(16, sizeof(OutMsg));
  if (!lock) lock = xSemaphoreCreateMutex();
}

void SapNetworkManager::begin() {
  ensureQueues();

  Serial.print("Laczenie z WiFi: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);         // nie zapisuj ustawien Wi-Fi w pamieci flash przy kazdej probie
  WiFi.setAutoReconnect(true);    // sterownik sam wraca po krotkich zanikach
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi OK!");
    Serial.print("IP: "); Serial.println(WiFi.localIP());
    // Konfiguracja czasu dla Polski (Strefa CET/CEST)
    configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org", "time.nist.gov");
    Serial.println("NTP: Uruchomiono synchronizacje czasu.");
    wifiWasConnected = true;
  } else {
    Serial.println("\nWiFi ERROR (Offline Mode) - kolejne proby w tle co 15 s");
    wifiOfflineSince = millis();
    wifiLastRetry = millis();
  }

  // --- TLS: weryfikacja certyfikatu brokera (MqttRootCA.h) ---
  espClient.setCACert(MQTT_ROOT_CA);
  espClient.setHandshakeTimeout(5);   // [s]

  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(globalCallback);
  client.setBufferSize(1024);
  client.setSocketTimeout(5);         // [s]

  netHeartbeat = millis();
  // Zadanie sieciowe na rdzeniu 0 (tam dziala tez stos Wi-Fi); petla glowna zostaje na rdzeniu 1
  xTaskCreatePinnedToCore(taskEntry, "NetTask", 10240, this, 2, NULL, 0);
}

void SapNetworkManager::taskEntry(void* arg) {
  static_cast<SapNetworkManager*>(arg)->netTaskLoop();
}

// ---------------------------------------------------------------------
//  ZADANIE SIECIOWE - jedyne miejsce, ktore dotyka polaczenia MQTT
// ---------------------------------------------------------------------
void SapNetworkManager::netTaskLoop() {
  for (;;) {
    netHeartbeat = millis();
    wifiWatchdog();

    if (WiFi.status() == WL_CONNECTED) {
      if (!client.connected()) {
        mqttUp = false;
        reconnect();
      }
      if (client.connected()) {
        client.loop();

        // Alarmy i logi - w kolejnosci, maks. 5 na obieg
        OutMsg m;
        for (int i = 0; i < 5 && xQueueReceive(outQueue, &m, 0) == pdTRUE; i++) {
          client.publish(m.topic, m.payload, m.retained);
        }
        // Telemetria - tylko najnowsza (starsze sa nieaktualne)
        if (telemetryDirty) {
          char buf[sizeof(telemetryBuf)];
          xSemaphoreTake(lock, portMAX_DELAY);
          strcpy(buf, telemetryBuf);
          telemetryDirty = false;
          xSemaphoreGive(lock);
          client.publish(MQTT_TOPIC_TELEMETRY, buf, false);
        }
      }
      mqttUp = client.connected();
    } else {
      mqttUp = false;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void SapNetworkManager::reconnect() {
  static unsigned long lastAttempt = 0;
  if (lastAttempt != 0 && millis() - lastAttempt < 5000) return;
  lastAttempt = millis();
  Serial.print("Laczenie z MQTT...");

  String clientId = "ESP32-SAP-" + String(random(0xffff), HEX);

  // LWT: jesli centrala zniknie bez pozegnania, broker sam opublikuje "OFFLINE" (retained)
  if (client.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD,
                     MQTT_TOPIC_STATUS, 1, true, "OFFLINE")) {
    Serial.println("Polaczono!");
    client.subscribe(MQTT_TOPIC_CONTROL, 1);
    client.publish(MQTT_TOPIC_STATUS, "ONLINE", true);

    // Po (ponownym) polaczeniu nadpisujemy retained stan alarmu AKTUALNYM stanem
    char buf[sizeof(lastAlarmBuf)];
    xSemaphoreTake(lock, portMAX_DELAY);
    strcpy(buf, lastAlarmBuf);
    if (telemetryBuf[0]) telemetryDirty = true;
    xSemaphoreGive(lock);
    if (buf[0]) client.publish(MQTT_TOPIC_ALARMS, buf, true);
  } else {
    // rc=-2: brak polaczenia TCP/TLS (adres, port, certyfikat), rc=4/5: zly login/haslo
    Serial.print("Blad rc="); Serial.print(client.state());
    char tlsErr[100];
    int tlsCode = espClient.lastError(tlsErr, sizeof(tlsErr));
    if (tlsCode != 0) { Serial.print(" | TLS: "); Serial.print(tlsCode); Serial.print(" "); Serial.print(tlsErr); }
    Serial.println();
  }
}

void SapNetworkManager::wifiWatchdog() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!wifiWasConnected) {
      wifiWasConnected = true;
      Serial.print("WiFi: Przywrocono polaczenie, IP: "); Serial.println(WiFi.localIP());
      if (ntpEnabled) configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org", "time.nist.gov");
      if (wifiLossLogged) reqLogRestore = true;
      wifiLossLogged = false;
    }
    getRestartCount();
    wifiRestartCount = 0;          // udane polaczenie zeruje licznik restartow
    reqRestartWifi = false;
    return;
  }

  // --- Brak sieci ---
  if (wifiWasConnected) {
    wifiWasConnected = false;
    wifiOfflineSince = millis();
    wifiLastRetry = millis();
    Serial.println("WiFi: Utracono polaczenie - proby w tle co 15 s");
  }
  unsigned long offline = millis() - wifiOfflineSince;

  if (!wifiLossLogged && offline > WIFI_LOG_AFTER_MS) {
    wifiLossLogged = true;
    reqLogLoss = true;
  }

  if (millis() - wifiLastRetry > WIFI_RETRY_MS) {
    wifiLastRetry = millis();
    Serial.printf("WiFi: Ponowna proba polaczenia (offline %lu s)\n", offline / 1000);
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }

  if (offline > WIFI_RESTART_AFTER_MS && getRestartCount() < MAX_AUTO_RESTARTS) {
    reqRestartWifi = true;         // decyzje podejmuje petla glowna (zna stan alarmu)
  }
}

// ---------------------------------------------------------------------
//  PETLA GLOWNA - obsluga prosb zadania sieciowego (zawsze szybka)
// ---------------------------------------------------------------------
void SapNetworkManager::loop() {
  if (reqLogLoss)    { reqLogLoss = false;    logger.addEvent("BRAK", "SIECI"); }
  if (reqLogRestore) { reqLogRestore = false; logger.addEvent("SIEC", "PRZYWROC."); }

  if (currentState != STATE_IDLE || getRestartCount() >= MAX_AUTO_RESTARTS) return;

  const char* reason = nullptr;
  if (reqRestartWifi) reason = "BRAK WIFI";
  else if (millis() - netHeartbeat > NET_TASK_STUCK_MS) reason = "SIEC ZAWIS";

  if (reason) {
    wifiRestartCount++;
    Serial.printf("SIEC: %s - restart centrali (%u/%u)\n", reason,
                  (unsigned)wifiRestartCount, (unsigned)MAX_AUTO_RESTARTS);
    logger.addEvent("RESTART", reason);
    delay(200);                    // czas na zapis do pamieci NVS
    ESP.restart();
  }
}

// ---------------------------------------------------------------------
//  PUBLIKACJA - wolane z petli glownej, tylko wkladaja dane do kolejek
// ---------------------------------------------------------------------
void SapNetworkManager::enqueue(const char* topic, const char* payload, bool retained) {
  ensureQueues();
  OutMsg m;
  strncpy(m.topic, topic, sizeof(m.topic) - 1); m.topic[sizeof(m.topic) - 1] = '\0';
  strncpy(m.payload, payload, sizeof(m.payload) - 1); m.payload[sizeof(m.payload) - 1] = '\0';
  m.retained = retained;
  if (xQueueSend(outQueue, &m, 0) != pdTRUE) {
    Serial.println("SIEC: kolejka wysylki pelna - pominieto wiadomosc");
  }
}

void SapNetworkManager::publishTelemetry() {
  ensureQueues();
  struct tm timeinfo;
  char timeString[25] = "NO_TIME";
  if (getLocalTime(&timeinfo, 10)) strftime(timeString, 25, "%H:%M:%S", &timeinfo);   // max 10 ms

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

  // --- BUDOWANIE I PRZEKAZANIE DO ZADANIA SIECIOWEGO ---
  char json[sizeof(telemetryBuf)];
  snprintf(json, sizeof(json),
    "{\"ts\":\"%s\",\"timer\":%d,\"lvl\":%d,\"login\":%s,\"buf\":\"%s\",\"menu\":{%s},\"diag\":{\"wifi\":%ld,\"ip\":\"%s\"}}",
    timeString, sysCtrl.getRemainingTime(), (int)accessMgr.getLevel(),
    accessMgr.isLoginInProgress() ? "true" : "false", accessMgr.getBufferMasked().c_str(),
    menuObj, WiFi.RSSI(), WiFi.localIP().toString().c_str());

  xSemaphoreTake(lock, portMAX_DELAY);
  strcpy(telemetryBuf, json);
  telemetryDirty = true;
  xSemaphoreGive(lock);
}

void SapNetworkManager::publishAlarm(SystemState state, String stateName, String zone, String source) {
  ensureQueues();
  char json[sizeof(lastAlarmBuf)];
  // Opis linii dozorowej (tylko w stanach alarmowych): strefa, numer linii, czujka
  extern String strefyLinii[]; extern int numeryLinii[]; extern String nazwyLinii[];
  bool alarmowy = (state == STATE_PRE_ALARM || state == STATE_VERIFICATION || state == STATE_FIRE_ALARM);
  int li = sysCtrl.getDetectedLine();
  snprintf(json, sizeof(json),
           "{\"code\":%d,\"name\":\"%s\",\"zone\":\"%s\",\"src\":\"%s\",\"strefa\":\"%s\",\"line\":%d,\"det\":\"%s\"}",
           (int)state, stateName.c_str(), zone.c_str(), source.c_str(),
           alarmowy ? strefyLinii[li].c_str() : "", alarmowy ? numeryLinii[li] : 0,
           alarmowy ? nazwyLinii[li].c_str() : "");
  xSemaphoreTake(lock, portMAX_DELAY);
  strcpy(lastAlarmBuf, json);      // zapamietany do ponownej publikacji po polaczeniu
  xSemaphoreGive(lock);
  enqueue(MQTT_TOPIC_ALARMS, json, true);   // RETAINED: nowy klient WWW od razu zna stan
}

void SapNetworkManager::publishLog(uint32_t id, String st, String zo, String ts) {
  char json[150];
  snprintf(json, sizeof(json), "{\"id\":%u,\"st\":\"%s\",\"zo\":\"%s\",\"ts\":\"%s\"}",
           id, st.c_str(), zo.c_str(), ts.c_str());
  enqueue(MQTT_TOPIC_LOGS, json, false);
}

// ---------------------------------------------------------------------
//  ODBIOR KOMEND (wywolywane w zadaniu sieciowym przez client.loop)
// ---------------------------------------------------------------------
void SapNetworkManager::handleMessage(char* topic, byte* payload, unsigned int length) {
    // Przyjmujemy WYLACZNIE komendy z tematu sterowania, w formacie "KEY:x".
    // Kazdy klawisz przechodzi potem przez normalna logike centrali (z kontrola
    // poziomu dostepu), tak jak z fizycznej klawiatury.
    if (strcmp(topic, MQTT_TOPIC_CONTROL) != 0) return;
    if (length != 5 || memcmp(payload, "KEY:", 4) != 0) {
        Serial.println("RX [MQTT]: odrzucono nieznana komende");
        return;
    }
    char key = (char)payload[4];
    const char* allowed = "0123456789ABCD*#";
    if (key == '\0' || strchr(allowed, key) == nullptr) {
        Serial.println("RX [MQTT]: odrzucono niedozwolony klawisz");
        return;
    }
    if (xQueueSend(keyQueue, &key, 0) != pdTRUE) {
        Serial.println("RX [MQTT]: kolejka klawiszy pelna - odrzucono");
        return;
    }
    Serial.print("RX [MQTT]: KEY "); Serial.println(key);
}

bool SapNetworkManager::popKey(char &key) {
    if (!keyQueue) return false;
    return xQueueReceive(keyQueue, &key, 0) == pdTRUE;
}

bool SapNetworkManager::isMqttConnected() {
    return mqttUp;
}

bool SapNetworkManager::isTimeSynced() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 0)) return false;
    return timeinfo.tm_year >= (2020 - 1900);
}
