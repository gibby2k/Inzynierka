#include "EventLogger.h"
#include <time.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "SapNetwork.h"

extern int simHour, simMinute; 
extern SapNetworkManager netMgr;

// =====================================================================
//  KOLEJKA ZADAN CHMUROWYCH (D1) - wszystko poza glowna petla
//  POST /log   -> nowy wpis
//  DELETE /clear -> czyszczenie bazy (wczesniej blokowalo loop())
// =====================================================================
enum CloudJobType : uint8_t { JOB_POST_LOG = 0, JOB_CLEAR = 1 };

struct CloudJob {
    CloudJobType type;
    char payload[200];
};

QueueHandle_t cloudQueue = NULL;

static int cloudRequest(const char* method, const char* path, const char* body) {
    WiFiClientSecure secureClient;
    secureClient.setInsecure();          // brak weryfikacji CA (prototyp) - szyfrowanie dziala
    HTTPClient http;
    http.setTimeout(5000);

    String url = String(CLOUD_API_BASE) + path;
    if (!http.begin(secureClient, url)) return -1;

    http.addHeader("X-API-Key", CLOUD_API_KEY);
    int code;
    if (body != nullptr) {
        http.addHeader("Content-Type", "application/json");
        code = http.sendRequest(method, (uint8_t*)body, strlen(body));
    } else {
        code = http.sendRequest(method);
    }
    http.end();
    return code;
}

void cloudWorkerTask(void *pvParameters) {
    CloudJob job;
    for (;;) {
        if (xQueueReceive(cloudQueue, &job, portMAX_DELAY) == pdTRUE) {
            if (WiFi.status() != WL_CONNECTED) continue;

            int code;
            if (job.type == JOB_CLEAR) {
                code = cloudRequest("DELETE", "/clear", nullptr);
                Serial.printf("Cloud D1: DELETE /clear -> %d\n", code);
            } else {
                code = cloudRequest("POST", "/log", job.payload);
                Serial.printf("Cloud D1: POST /log -> %d\n", code);
            }
        }
    }
}

void initCloudWorker() {
    if (cloudQueue == NULL) {
        cloudQueue = xQueueCreate(20, sizeof(CloudJob));
        // TLS potrzebuje wiekszego stosu niz zwykly HTTP (3072 -> 8192)
        xTaskCreatePinnedToCore(cloudWorkerTask, "CloudTask", 8192, NULL, tskIDLE_PRIORITY + 1, NULL, 0);
    }
}

EventLogger::EventLogger() {
    totalEvents = 0;
    headIndex = 0;
    eventCount = 0;
}

void EventLogger::begin() {
    // Otwieramy przestrzeń w pamięci Flash
    preferences.begin("sap_logs", false);
    
    totalEvents = preferences.getUInt("total", 0);
    headIndex = preferences.getUChar("head", 0);
    eventCount = preferences.getUChar("count", 0);
    
    // ODCZYT: Ładujemy każde zdarzenie z osobnej "szufladki"
    for (int i = 0; i < eventCount; i++) {
        String key = "ev" + String(i);
        preferences.getBytes(key.c_str(), &events[i], sizeof(LogEvent));
    }
    
    Serial.println("EventLogger: Zaladowano pamiec zdarzen (" + String(eventCount) + "/20).");
}

String EventLogger::getCurrentDateTime() {
    struct tm timeinfo;
    char timeString[18]; // <--- Powiększony bufor
    
    // Pobieramy czas (z NTP lub fallbacku - max 10ms czekania)
    if (getLocalTime(&timeinfo, 10)) {
        // ZEGAR NTP: format %d (dzień), %m (miesiąc), %y (rok 2-cyfrowy), %H (godz), %M (min)
        strftime(timeString, sizeof(timeString), "%d-%m-%y | %H:%M", &timeinfo);
    } else {
        // ZEGAR AWARYJNY: Jeśli brak sieci
        sprintf(timeString, "??-??-?? | %02d:%02d", simHour, simMinute);
    }
    return String(timeString);
}

void EventLogger::addEvent(String state, String zone) {
    totalEvents++; 
    
    if (eventCount > 0) {
        headIndex = (headIndex + 1) % MAX_EVENTS;
    }
    
    if (eventCount < MAX_EVENTS) {
        eventCount++;
    }

    String shortState = state.substring(0, 8); 
    String shortZone = zone.substring(0, 8);
    String timeStr = getCurrentDateTime();

    events[headIndex].id = totalEvents;
    strncpy(events[headIndex].state, shortState.c_str(), sizeof(events[headIndex].state) - 1);
    events[headIndex].state[sizeof(events[headIndex].state) - 1] = '\0'; // Zabezpieczenie końca stringa
    
    strncpy(events[headIndex].zone, shortZone.c_str(), sizeof(events[headIndex].zone) - 1);
    events[headIndex].zone[sizeof(events[headIndex].zone) - 1] = '\0';
    
    strncpy(events[headIndex].timestamp, timeStr.c_str(), sizeof(events[headIndex].timestamp) - 1);
    events[headIndex].timestamp[sizeof(events[headIndex].timestamp) - 1] = '\0';

    saveToMemory();
    
    netMgr.publishLog(totalEvents, shortState, shortZone, timeStr);
    // --- ZLECENIE WYSYŁKI DO CHMURY D1 W TLE ---
    initCloudWorker();
    CloudJob job;
    job.type = JOB_POST_LOG;
    LogEvent newest;
    getEvent(0, newest); // Pobieramy najnowsze zdarzenie z bufora wraz z jego ID
    
    snprintf(job.payload, sizeof(job.payload), "{\"id\":%u,\"st\":\"%s\",\"zo\":\"%s\",\"ts\":\"%s\"}", 
             (unsigned int)newest.id, newest.state, newest.zone, newest.timestamp);
    xQueueSend(cloudQueue, &job, 0);
}

void EventLogger::saveToMemory() {
    // Zapisujemy tylko nagłówki
    preferences.putUInt("total", totalEvents);
    preferences.putUChar("head", headIndex);
    preferences.putUChar("count", eventCount);
    
    // ZAPIS: Zapisujemy TYLKO to jedno najnowsze zdarzenie pod jego własnym kluczem
    String key = "ev" + String(headIndex);
    preferences.putBytes(key.c_str(), &events[headIndex], sizeof(LogEvent));
    
    Serial.println("EventLogger: Zapisano do NVM -> " + key);
}

bool EventLogger::getEvent(uint8_t relativeIndex, LogEvent &outEvent) {
    if (relativeIndex >= eventCount) return false;
    
    // Obliczamy rzeczywisty indeks w tablicy (cofamy się od najnowszego)
    // Magia bufora kołowego
    int actualIndex = (headIndex - relativeIndex + MAX_EVENTS) % MAX_EVENTS;
    outEvent = events[actualIndex];
    return true;
}

uint8_t EventLogger::getCount() {
    return eventCount;
}

void EventLogger::clearLogs() {
    preferences.clear(); // Formatowanie partycji sap_logs
    totalEvents = 0;
    headIndex = 0;
    eventCount = 0;
    saveToMemory(); // Zapisanie "czystych" (wyzerowanych) liczników
    Serial.println("EventLogger: Pamiec zostala trwale wykasowana.");
    // --- CZYSZCZENIE BAZY D1 + SLAD AUDYTOWY (w tle, nie blokuje loop) ---
    initCloudWorker();

    CloudJob clearJob;
    clearJob.type = JOB_CLEAR;
    clearJob.payload[0] = '\0';
    xQueueSend(cloudQueue, &clearJob, 0);

    struct tm timeinfo;
    char resetTimeStr[25];
    if (getLocalTime(&timeinfo, 10)) {
        strftime(resetTimeStr, sizeof(resetTimeStr), "%d-%m-%y | %H:%M", &timeinfo);
    } else {
        strcpy(resetTimeStr, "BRAK CZASU");
    }

    // Kolejka FIFO: ten wpis trafi do D1 dopiero PO wykonaniu DELETE
    CloudJob auditJob;
    auditJob.type = JOB_POST_LOG;
    snprintf(auditJob.payload, sizeof(auditJob.payload),
             "{\"id\":0,\"st\":\"PAMIEC\",\"zo\":\"RESET LOKAL.\",\"ts\":\"%s\"}", resetTimeStr);
    xQueueSend(cloudQueue, &auditJob, 0);
}
