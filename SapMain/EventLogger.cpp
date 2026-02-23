#include "EventLogger.h"
#include <time.h>

extern int simHour, simMinute; 

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
}