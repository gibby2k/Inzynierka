#ifndef SAP_NETWORK_H
#define SAP_NETWORK_H

#include <WiFi.h>
#include <PubSubClient.h>
#include "Secrets.h"
#include "SystemState.h"

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
    
    // --- AKTUALIZACJA: Nowa sygnatura funkcji (Protokół PRO) ---
    void publishStatus(SystemState state, String stateName, String zone, String source, String user);
    
    void handleMessage(char* topic, byte* payload, unsigned int length);
    String getLatestCommand(); 
};

#endif