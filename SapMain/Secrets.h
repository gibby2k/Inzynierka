#ifndef SECRETS_H
#define SECRETS_H

// Używamy #define zamiast const char* - to zapobiega błędom "multiple definition"
// #define WIFI_SSID "Orange_Swiatlowod_2710"
// #define WIFI_PASSWORD "yUTFjgs4PqpHxx5KhG"

#define WIFI_SSID "wifissid"
#define WIFI_PASSWORD "haslo"

#define MQTT_SERVER "broker.hivemq.com"
#define MQTT_PORT 1883

// Unikalny temat
#define MQTT_TOPIC_STATUS  "pbs/student/sap/status"
#define MQTT_TOPIC_CONTROL "pbs/student/sap/control"


#endif
