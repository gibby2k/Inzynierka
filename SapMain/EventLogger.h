#ifndef EVENT_LOGGER_H
#define EVENT_LOGGER_H

#include <Arduino.h>
#include <Preferences.h>

#define MAX_EVENTS 20

// Struktura pojedynczego zdarzenia (zoptymalizowana pod pamięć)
struct LogEvent {
    uint32_t id;         // Globalny numer zdarzenia (np. 123)
    char state[10];      // Np. "POZAR"
    char zone[10];       // Np. "L1-E1"
    char timestamp[18];  // Np. "14:32 23.02.26"
};

class EventLogger {
private:
    Preferences preferences;
    LogEvent events[MAX_EVENTS];
    uint32_t totalEvents; // Ciągły licznik (nigdy się nie zeruje)
    uint8_t headIndex;    // Wskazuje na najnowsze zdarzenie (0-19)
    uint8_t eventCount;   // Ile zdarzeń jest aktualnie w pamięci (max 20)

    void saveToMemory();
    String getCurrentDateTime();

public:
    EventLogger();
    void begin();
    void addEvent(String state, String zone);
    
    // Pobiera zdarzenie po indeksie (0 = najnowsze, 1 = starsze, itd.)
    bool getEvent(uint8_t index, LogEvent &outEvent);
    uint8_t getCount(); // Zwraca ilość zapisanych zdarzeń
    void clearLogs();
};

#endif