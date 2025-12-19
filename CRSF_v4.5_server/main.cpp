#include "config.h"
#include <thread>
#include <string>
#include <fstream>
#include <unistd.h>
#include <cstdio>
#include <sstream>
#include <iostream>

#include "crsf/crsf.h"
#include "libs/rpi_hal.h"
#include "libs/joystick.h"
#include "libs/crsf/CrsfSerial.h"

// g_ignore_telemetry определена в globals.cpp

// Простая функция для получения режима работы
// Режим теперь управляется через pybind модуль, но для совместимости
// используем режим по умолчанию "manual"
std::string getWorkMode() {
    return "manual"; // По умолчанию ручной режим управления
}

// Главная точка входа Linux-приложения для Raspberry Pi
// Полная замена Arduino setup()/loop()
int main(int argc, char* argv[]) {
    // Парсинг аргументов командной строки
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--notel") {
            g_ignore_telemetry = true;
            std::cout << "[INFO] Running in NO-TELEMETRY mode. Safety checks disabled." << std::endl;
        }
    }
#if USE_CRSF_RECV == true
  crsfInitRecv(); // Запуск CRSF приёма
#endif
#if USE_CRSF_SEND == true
  crsfInitSend(); // Запуск CRSF передачи
#endif

  //БЕСПОЛЕЗНО: устаревший Arduino код - закомментированная неиспользуемая переменная
  // флаг доступности (не используется, можно удалить/раскомментировать при необходимости)
  // bool isCan = true;
  const uint32_t crsfSendPeriodMs = 10; // ~100 Гц отправка каналов для реалтайма
  uint32_t lastSendMs = 0;
  // Инициализация джойстика (не критично, если недоступен)
  if (js_open("/dev/input/js0")) {
    printf("Джойстик подключен: %d осей, %d кнопок\n", js_num_axes(), js_num_buttons());
  } else {
    printf("Предупреждение: джойстик недоступен, работа без управления\n");
  }

  // Структура для записи телеметрии в файл (для Python обертки)
  struct SharedTelemetryData {
    bool linkUp;
    uint32_t lastReceive;
    int channels[16];
    // Статистика связи - отключена (поля оставлены для совместимости)
    uint32_t packetsReceived;
    uint32_t packetsSent;
    uint32_t packetsLost;
    double latitude;
    double longitude;
    double altitude;
    double speed;
    double voltage;
    double current;
    double capacity;
    uint8_t remaining;
    double roll;
    double pitch;
    double yaw;
    int16_t rollRaw;
    int16_t pitchRaw;
    int16_t yawRaw;
  };
  
  // Запускаем поток для периодической записи телеметрии в файл
  std::thread telemetryWriterThread([&]() {
    CrsfSerial* crsf = static_cast<CrsfSerial*>(crsfGetActive());
    if (crsf == nullptr) return;
    
    while (true) {
      SharedTelemetryData shared;
      
      shared.linkUp = crsf->isLinkUp();
      shared.lastReceive = crsf->_lastReceive;
      
      // Каналы
      for (int i = 0; i < 16; i++) {
        shared.channels[i] = crsf->getChannel(i + 1);
      }
      
      // Статистика связи - отключена
      shared.packetsReceived = 0;
      shared.packetsSent = 0;
      shared.packetsLost = 0;
      
      // GPS
      const crsf_sensor_gps_t* gps = crsf->getGpsSensor();
      if (gps) {
        shared.latitude = gps->latitude / 10000000.0;
        shared.longitude = gps->longitude / 10000000.0;
        shared.altitude = gps->altitude - 1000;
        shared.speed = gps->groundspeed / 10.0;
      }
      
      // Батарея
      shared.voltage = crsf->getBatteryVoltage();
      shared.current = crsf->getBatteryCurrent();
      shared.capacity = crsf->getBatteryCapacity();
      shared.remaining = crsf->getBatteryRemaining();
      
      // Положение
      shared.roll = crsf->getAttitudeRoll();
      shared.pitch = crsf->getAttitudePitch();
      shared.yaw = crsf->getAttitudeYaw();
      
      // Сырые значения attitude
      shared.rollRaw = crsf->getRawAttitudeRoll();
      shared.pitchRaw = crsf->getRawAttitudePitch();
      shared.yawRaw = crsf->getRawAttitudeYaw();
      
      // Записываем в файл
      std::ofstream file("/tmp/crsf_telemetry.dat", std::ios::binary);
      if (file.is_open()) {
        file.write(reinterpret_cast<const char*>(&shared), sizeof(SharedTelemetryData));
        file.close();
      }
      
      rpi_delay_ms(20); // Обновляем каждые 20мс для реалтайма
    }
  });
  telemetryWriterThread.detach();
  
  printf("✓ Поток записи телеметрии запущен для Python обертки\n");



  // Главный цикл
  for (;;) {
#if USE_CRSF_RECV == true
    loop_ch();
#endif

    // Обработка команд из файла (от Python обертки)
    std::ifstream cmdFile("/tmp/crsf_command.txt");
    if (cmdFile.is_open()) {
      std::string cmd;
      // Обрабатываем все команды из файла (многострочный формат)
      while (std::getline(cmdFile, cmd)) {
        if (cmd.find("setChannels") == 0) {
          // Формат: setChannels 1=1500 2=1600 3=1700 ...
          std::istringstream iss(cmd);
          std::string token;
          iss >> token; // пропускаем "setChannels"
          while (iss >> token) {
            size_t pos = token.find('=');
            if (pos != std::string::npos) {
              unsigned int ch = std::stoi(token.substr(0, pos));
              int value = std::stoi(token.substr(pos + 1));
              if (ch >= 1 && ch <= 16 && value >= 1000 && value <= 2000) {
                crsfSetChannel(ch, value);
              }
            }
          }
        } else if (cmd.find("setChannel") == 0) {
          unsigned int ch;
          int value;
          if (sscanf(cmd.c_str(), "setChannel %u %d", &ch, &value) == 2) {
            if (ch >= 1 && ch <= 16 && value >= 1000 && value <= 2000) {
              crsfSetChannel(ch, value);
            }
          }
        } else if (cmd == "sendChannels") {
          // Команда sendChannels больше не нужна - отправка происходит автоматически
          // через processSend() в основном цикле, но оставляем для совместимости
          // (просто триггерим отправку через processSend)
          crsfSendChannels();
        } else if (cmd.find("setMode") == 0) {
          std::string mode = cmd.substr(8); // "setMode " = 8 символов
          if (mode == "joystick" || mode == "manual") {
            // Режим сохраняется в глобальной переменной workMode
            // (управляется через pybind модуль, но для совместимости оставляем)
          }
        }
      }
      cmdFile.close();
      // Удаляем файл после обработки всех команд
      remove("/tmp/crsf_command.txt");
    }

#if USE_CRSF_SEND == true
    uint32_t currentMillis = rpi_millis();

    // Читать события джойстика (неблокирующе)
    js_poll();

    // Преобразуем оси джойстика [-32767..32767] в CRSF [1000..2000]
    auto axisToUs = [](int16_t v) -> int {
      // нормируем к [-1..1]
      const float nf = (v >= 0) ? (static_cast<float>(v) / 32767.0f)
                                : (static_cast<float>(v) / 32768.0f);
      // диапазон [1000..2000]
      float us = 1500.0f + nf * 500.0f;
      int ius = static_cast<int>(us + 0.5f);
      if (ius < 1000) ius = 1000;
      if (ius > 2000) ius = 2000;
      return ius;
    };

    // Обработка осей джойстика только в режиме joystick
    std::string mode = getWorkMode();
    if (mode == "joystick") {
      int16_t ax0 = 0, ax1 = 0, ax2 = 0, ax3 = 0;
      bool axis0_ok = js_get_axis(0, ax0);
      bool axis1_ok = js_get_axis(1, ax1);
      bool axis2_ok = js_get_axis(2, ax2);
      bool axis3_ok = js_get_axis(3, ax3);
      
      if (axis0_ok) crsfSetChannel(1, axisToUs(ax2)); // Roll
      if (axis1_ok) crsfSetChannel(2, axisToUs(-ax3)); // Pitch
      if (axis2_ok) crsfSetChannel(3, axisToUs(-ax1)); // Throttle
      if (axis3_ok) crsfSetChannel(4, axisToUs(ax0)); // Yaw
    }

    // Асинхронная отправка RC-каналов с частотой ~100 Гц для реалтайма
    // processSend() отправляет пакет только если были изменения каналов (флаг _needSendPacket)
    // или периодически, если нужно поддерживать связь
    if (currentMillis - lastSendMs >= crsfSendPeriodMs) {
      lastSendMs = currentMillis;
      crsfSendChannels(); // Вызывает processSend() внутри
    }


#endif
    //БЕСПОЛЕЗНО: закомментированный код
    // Реалтайм без задержек - максимальная скорость обработки
    // rpi_delay_ms(0); // Полностью убираем задержку для реалтайма
  }

  return 0;
}
