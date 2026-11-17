#ifndef SECRETS_H
#define SECRETS_H

// =====================================================================
//  PLIK LOKALNY - NIE COMMITOWAC DO GITA (jest w .gitignore)
//  Wzor dla innych: Secrets.example.h
// =====================================================================

#define WIFI_SSID "Orange_IoT_2710"
#define WIFI_PASSWORD "pWFd3M39LGqKKaeTh4"

#define MQTT_SERVER "broker.hivemq.com"
#define MQTT_PORT   1883

#define MQTT_TOPIC_TELEMETRY "pbs/student/sap/telemetry"
#define MQTT_TOPIC_ALARMS    "pbs/student/sap/alarms"
#define MQTT_TOPIC_LOGS      "pbs/student/sap/logs"
#define MQTT_TOPIC_CONTROL   "pbs/student/sap/control"
#define MQTT_TOPIC_STATUS    "pbs/student/sap/status"   // NOWE: ONLINE/OFFLINE (LWT)

// --- API Cloudflare Worker (D1) ---
#define CLOUD_API_BASE "https://sap-api.lamot40.workers.dev"
#define CLOUD_API_KEY  "75fc36b1b5e241dbb4dace1ebd75c003"

#endif
