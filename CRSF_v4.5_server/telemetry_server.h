#ifndef TELEMETRY_SERVER_H
#define TELEMETRY_SERVER_H

#include "libs/crsf/CrsfSerial.h"
#include <string>

// Запуск веб-сервера телеметрии
void startTelemetryServer(CrsfSerial* crsf, int port = 8080, int updateIntervalMs = 50);

// Получить текущий режим работы
std::string getWorkMode();

#endif // TELEMETRY_SERVER_H
