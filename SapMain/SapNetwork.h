#ifndef SAP_NETWORK_H
#define SAP_NETWORK_H

#include <WiFi.h>
#include <PubSubClient.h>
#include "Secrets.h"
#include "SystemState.h"
#include <time.h>

class SapNetworkManager {
  private:
    WiFiClient espClient;
    PubSubClient client;
    
    // Bufor na komendy
    String lastCommand; 
    bool commandReceived;

    void reconnect();

  public:
    SapNetworkManager();
    void begin();
    void loop();
    
    void publishTelemetry();
    void publishAlarm(SystemState state, String stateName, String zone, String source);
    void publishLog(uint32_t id, String st, String zo, String ts);
    
    void handleMessage(char* topic, byte* payload, unsigned int length);
    String getLatestCommand(); 

    bool isMqttConnected();
    bool isTimeSynced();
};

#endif