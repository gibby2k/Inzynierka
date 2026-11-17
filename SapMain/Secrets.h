#ifndef SECRETS_H
#define SECRETS_H

// Używamy #define zamiast const char* - to zapobiega błędom "multiple definition"
// #define WIFI_SSID "Orange_Swiatlowod_2710"
// #define WIFI_PASSWORD "yUTFjgs4PqpHxx5KhG"

#define WIFI_SSID "Orange_IoT_2710"
#define WIFI_PASSWORD "pWFd3M39LGqKKaeTh4"

#define MQTT_SERVER "broker.hivemq.com"
#define MQTT_PORT 1883

#define MQTT_TOPIC_TELEMETRY "pbs/student/sap/telemetry"
#define MQTT_TOPIC_ALARMS    "pbs/student/sap/alarms"
#define MQTT_TOPIC_LOGS      "pbs/student/sap/logs"
#define MQTT_TOPIC_CONTROL   "pbs/student/sap/control"

#endif