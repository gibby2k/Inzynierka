#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>

void initSensors();
void checkSensors();
void resetSensorLatches(); // <-- NOWA FUNKCJA
void setLineBlocked(bool blocked);
bool isLineBlocked();

#endif