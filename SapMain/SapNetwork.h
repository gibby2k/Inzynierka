#ifndef SAP_NETWORK_H
#define SAP_NETWORK_H

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "Secrets.h"
#include "SystemState.h"
#include <time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

// =====================================================================
//  ARCHITEKTURA: cala komunikacja sieciowa (Wi-Fi, TLS, MQTT) dziala
//  w OSOBNYM zadaniu FreeRTOS. Petla glowna (logika pozarowa: czujka,
//  klawiatura, DSO, wentylator, LCD) tylko wklada dane do kolejek i
//  NIGDY nie czeka na siec - zawieszone polaczenie nie zablokuje centrali.
// =====================================================================

struct OutMsg {              // wiadomosc czekajaca na wyslanie (alarmy, logi)
  char topic[32];
  char payload[200];
  bool retained;
};

class SapNetworkManager {
  private:
    WiFiClientSecure espClient;   // TLS do prywatnego brokera
    PubSubClient client;

    QueueHandle_t keyQueue = nullptr;     // klawisze z WWW -> petla glowna
    QueueHandle_t outQueue = nullptr;     // alarmy i logi -> broker (kolejnosc zachowana)
    SemaphoreHandle_t lock = nullptr;     // ochrona buforow wspoldzielonych miedzy zadaniami
    char telemetryBuf[512];               // telemetria: wysylamy tylko NAJNOWSZA
    volatile bool telemetryDirty = false;
    char lastAlarmBuf[200];               // ostatni stan alarmu - ponownie po kazdym polaczeniu

    volatile bool mqttUp = false;         // stan polaczenia widziany z petli glownej
    volatile unsigned long netHeartbeat = 0;   // znacznik zycia zadania sieciowego

    // --- nadzor Wi-Fi (zadanie sieciowe) ---
    bool wifiWasConnected = false;
    bool wifiLossLogged = false;
    unsigned long wifiOfflineSince = 0;
    unsigned long wifiLastRetry = 0;

    // --- prosby do petli glownej (dziennik zdarzen nie jest wielowatkowy) ---
    volatile bool reqLogLoss = false;
    volatile bool reqLogRestore = false;
    volatile bool reqRestartWifi = false;

    void ensureQueues();
    static void taskEntry(void* arg);
    void netTaskLoop();
    void reconnect();
    void wifiWatchdog();
    void enqueue(const char* topic, const char* payload, bool retained);

  public:
    SapNetworkManager();
    void begin();                 // laczy Wi-Fi i uruchamia zadanie sieciowe
    void loop();                  // z petli glownej: obsluga prosb (wpisy, restart) - NIE blokuje

    void publishTelemetry();
    void publishAlarm(SystemState state, String stateName, String zone, String source);
    void publishLog(uint32_t id, String st, String zo, String ts);

    void handleMessage(char* topic, byte* payload, unsigned int length);
    bool popKey(char &key);       // nastepny klawisz z WWW (false = brak)

    bool isMqttConnected();
    bool isTimeSynced();
};

#endif
