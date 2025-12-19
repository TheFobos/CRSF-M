#ifndef CONFIG_H
#define CONFIG_H

#define USE_CRSF_RECV true   // включить приём CRSF на Raspberry Pi
#define USE_CRSF_SEND true   // включить отправку телеметрии CRSF
#define USE_LOG false    // включить журналы для отладки yaw

#define CRSF_BAUD 420000     // скорость CRSF

// Пути к последовательным портам Raspberry Pi для CRSF
// Обычно: "/dev/ttyAMA0" (PL011) и "/dev/ttyS0" (miniUART)
#define CRSF_PORT_PRIMARY "/dev/ttyAMA0"
#define CRSF_PORT_SECONDARY "/dev/ttyS0"

// Глобальный флаг для режима "Blind Transmission" (без проверки телеметрии)
// Устанавливается через аргумент командной строки --notel
extern bool g_ignore_telemetry;

#endif
