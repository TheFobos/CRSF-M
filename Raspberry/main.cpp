#include "config.h"
#include <thread>
#include <string>
#include <fstream>
#include <cstdio>
#include <sstream>
#include <iostream>
#include <atomic>
#include <array>
#include <chrono>
#include <mutex>

#include "crsf/crsf.h"
#include "libs/rpi_hal.h"
#include "libs/joystick.h"
#include "libs/crsf/CrsfSerial.h"

// Простая функция для получения режима работы
std::string getWorkMode() {
    return "manual";
}

// Константы для лучшей читаемости
namespace Constants {
    constexpr uint32_t CRSF_SEND_PERIOD_MS = 10;     // ~100 Гц
    constexpr uint32_t TELEMETRY_UPDATE_MS = 20;     // 50 Гц
    constexpr int CHANNEL_MIN = 1000;
    constexpr int CHANNEL_MAX = 2000;
    constexpr int CHANNEL_COUNT = 16;
    constexpr int JOYSTICK_AXIS_MIN = -32768;
    constexpr int JOYSTICK_AXIS_MAX = 32767;
    constexpr float JOYSTICK_SCALE_FACTOR = 500.0f;
    constexpr int16_t JOYSTICK_DEADZONE = 100;       // Мертвая зона для джойстика
    
    // Значения каналов по умолчанию
    constexpr int DEFAULT_CHANNEL_VALUE = 1500;
    constexpr int DEFAULT_THROTTLE_VALUE = 1000; // Безопасное значение для газа
}

// Глобальные переменные для каналов
std::mutex channelsMutex;
int currentChannels[Constants::CHANNEL_COUNT] = {
    Constants::DEFAULT_CHANNEL_VALUE, // 1
    Constants::DEFAULT_CHANNEL_VALUE, // 2
    Constants::DEFAULT_CHANNEL_VALUE, // 3
    Constants::DEFAULT_CHANNEL_VALUE, // 4
    Constants::DEFAULT_CHANNEL_VALUE, // 5
    Constants::DEFAULT_CHANNEL_VALUE, // 6
    Constants::DEFAULT_CHANNEL_VALUE, // 7
    Constants::DEFAULT_CHANNEL_VALUE, // 8
    Constants::DEFAULT_CHANNEL_VALUE, // 9
    Constants::DEFAULT_CHANNEL_VALUE, // 10
    Constants::DEFAULT_CHANNEL_VALUE, // 11
    Constants::DEFAULT_CHANNEL_VALUE, // 12
    Constants::DEFAULT_CHANNEL_VALUE, // 13
    Constants::DEFAULT_CHANNEL_VALUE, // 14
    Constants::DEFAULT_CHANNEL_VALUE, // 15
    Constants::DEFAULT_CHANNEL_VALUE  // 16
};

// Оптимизированная структура для телеметрии (выровнена по 8 байтам)
struct __attribute__((aligned(8))) SharedTelemetryData {
    bool linkUp;
    uint32_t lastReceive;
    int channels[Constants::CHANNEL_COUNT];
    
    // GPS данные
    double latitude;
    double longitude;
    double altitude;
    double speed;
    
    // Данные батареи
    float voltage;
    float current;
    float capacity;
    uint8_t remaining;
    
    // Данные положения
    float roll;
    float pitch;
    float yaw;
    int16_t rollRaw;
    int16_t pitchRaw;
    int16_t yawRaw;
    
    // Временная метка для синхронизации
    uint64_t timestamp;
};

// Проверка начала строки (замена для starts_with в C++17)
inline bool startsWith(const std::string& str, const std::string& prefix) {
    return str.compare(0, prefix.length(), prefix) == 0;
}

// Быстрое преобразование оси джойстика в значение канала
inline int axisToUs(int16_t value) {
    // Применяем мертвую зону
    if (value < Constants::JOYSTICK_DEADZONE && value > -Constants::JOYSTICK_DEADZONE) {
        return 1500;
    }
    
    // Оптимизированное вычисление с использованием целых чисел
    // (value * 500) / 32768 + 1500
    int32_t scaled = static_cast<int32_t>(value) * static_cast<int32_t>(Constants::JOYSTICK_SCALE_FACTOR);
    int result = 1500 + (scaled / 32768); // Используем деление вместо сдвига для точности
    
    // Ограничение диапазона
    if (result < Constants::CHANNEL_MIN) return Constants::CHANNEL_MIN;
    if (result > Constants::CHANNEL_MAX) return Constants::CHANNEL_MAX;
    return result;
}

// Безопасная установка канала
void safeSetChannel(unsigned int channel, int value) {
    if (channel >= 1 && channel <= Constants::CHANNEL_COUNT && 
        value >= Constants::CHANNEL_MIN && value <= Constants::CHANNEL_MAX) {
        std::lock_guard<std::mutex> lock(channelsMutex);
        currentChannels[channel - 1] = value;
    }
}

// Парсинг команд из файла
bool parseCommand(const std::string& cmd) {
    if (cmd.empty() || cmd[0] == '#') {
        return false; // Пропускаем пустые строки и комментарии
    }
    
    // setChannels команда
    if (startsWith(cmd, "setChannels")) {
        std::istringstream iss(cmd);
        std::string token;
        iss >> token; // пропускаем "setChannels"
        
        while (iss >> token) {
            size_t pos = token.find('=');
            if (pos != std::string::npos) {
                try {
                    unsigned int ch = std::stoi(token.substr(0, pos));
                    int value = std::stoi(token.substr(pos + 1));
                    safeSetChannel(ch, value);
                } catch (...) {
                    continue; // Игнорируем некорректные значения
                }
            }
        }
        return true;
    }
    
    // setChannel команда
    else if (startsWith(cmd, "setChannel")) {
        unsigned int ch;
        int value;
        if (sscanf(cmd.c_str(), "setChannel %u %d", &ch, &value) == 2) {
            safeSetChannel(ch, value);
        }
        return true;
    }
    
    // sendChannels команда (для совместимости)
    else if (cmd == "sendChannels") {
        // Теперь эта команда триггерит отправку каналов
        return true;
    }
    
    // setMode команда
    else if (startsWith(cmd, "setMode")) {
        // Режим управляется через pybind, поэтому просто логируем
        std::cout << "[DEBUG] Mode change requested: " << cmd << std::endl;
        return true;
    }
    
    return false;
}

// Поток для записи телеметрии
void telemetryWriterWorker() {
    CrsfSerial* crsf = static_cast<CrsfSerial*>(crsfGetActive());
    if (crsf == nullptr) {
        std::cerr << "[ERROR] CRSF not initialized for telemetry" << std::endl;
        return;
    }
    
    SharedTelemetryData shared;
    auto lastUpdate = std::chrono::steady_clock::now();
    
    while (true) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - lastUpdate).count();
            
        if (elapsed < Constants::TELEMETRY_UPDATE_MS) {
            rpi_delay_ms(Constants::TELEMETRY_UPDATE_MS - elapsed);
            continue;
        }
        
        lastUpdate = now;
        
        // Заполнение данных телеметрии
        shared.linkUp = crsf->isLinkUp();
        shared.lastReceive = crsf->_lastReceive;
        shared.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
        
        // Каналы из CRSF (для обратной связи)
        for (int i = 0; i < Constants::CHANNEL_COUNT; ++i) {
            shared.channels[i] = crsf->getChannel(i + 1);
        }
        
        // GPS данные
        const crsf_sensor_gps_t* gps = crsf->getGpsSensor();
        if (gps) {
            shared.latitude = gps->latitude / 1e7;
            shared.longitude = gps->longitude / 1e7;
            shared.altitude = gps->altitude - 1000;
            shared.speed = gps->groundspeed / 10.0;
        }
        
        // Данные батареи
        shared.voltage = crsf->getBatteryVoltage();
        shared.current = crsf->getBatteryCurrent();
        shared.capacity = crsf->getBatteryCapacity();
        shared.remaining = crsf->getBatteryRemaining();
        
        // Данные положения
        shared.roll = crsf->getAttitudeRoll();
        shared.pitch = crsf->getAttitudePitch();
        shared.yaw = crsf->getAttitudeYaw();
        shared.rollRaw = crsf->getRawAttitudeRoll();
        shared.pitchRaw = crsf->getRawAttitudePitch();
        shared.yawRaw = crsf->getRawAttitudeYaw();
        
        // Атомарная запись в файл
        std::ofstream file("/tmp/crsf_telemetry.dat", std::ios::binary | std::ios::trunc);
        if (file.is_open()) {
            file.write(reinterpret_cast<const char*>(&shared), sizeof(shared));
            file.close();
        }
    }
}

int main(int argc, char* argv[]) {
    // Парсинг аргументов командной строки
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--notel") {
            g_ignore_telemetry = true;
            std::cout << "[INFO] NO-TELEMETRY mode. Safety checks disabled." << std::endl;
        }
    }
    
    // Инициализация CRSF
#if USE_CRSF_RECV == true
    crsfInitRecv();
#endif
#if USE_CRSF_SEND == true
    crsfInitSend();
#endif

    // Инициализация джойстика
    bool joystickAvailable = false;
    if (js_open("/dev/input/js0")) {
        std::cout << "Джойстик подключен: " << js_num_axes() 
                  << " осей, " << js_num_buttons() << " кнопок" << std::endl;
        joystickAvailable = true;
    } else {
        std::cout << "Предупреждение: джойстик недоступен" << std::endl;
    }

    // Запуск потока телеметрии
    std::thread telemetryThread(telemetryWriterWorker);
    telemetryThread.detach();
    std::cout << "✓ Поток телеметрии запущен" << std::endl;

    // Главный цикл
    uint32_t lastSendMs = 0;
    bool needToSendChannels = false;
    
    // Установка безопасных значений по умолчанию
    {
        std::lock_guard<std::mutex> lock(channelsMutex);
        // Канал 3 (Throttle/Gas) устанавливаем в минимальное значение для безопасности
        currentChannels[2] = Constants::DEFAULT_THROTTLE_VALUE;
    }
    
    while (true) {
        const uint32_t currentMillis = rpi_millis();
        
        // Обработка приема CRSF
#if USE_CRSF_RECV == true
        loop_ch();
#endif

        // Обработка команд из файла
        std::ifstream cmdFile("/tmp/crsf_command.txt");
        if (cmdFile.is_open()) {
            std::string cmd;
            while (std::getline(cmdFile, cmd)) {
                if (parseCommand(cmd)) {
                    needToSendChannels = true; // Флаг, что нужно отправить каналы
                }
            }
            cmdFile.close();
            std::remove("/tmp/crsf_command.txt");
        }

#if USE_CRSF_SEND == true
        // Обновление событий джойстика
        if (joystickAvailable) {
            js_poll();
        }

        // Обработка джойстика в соответствующем режиме
        if (getWorkMode() == "joystick" && joystickAvailable) {
            int16_t axes[4] = {0};
            bool axesOk[4] = {
                js_get_axis(0, axes[0]),
                js_get_axis(1, axes[1]),
                js_get_axis(2, axes[2]),
                js_get_axis(3, axes[3])
            };
            
            // Маппинг осей джойстика на каналы
            if (axesOk[0]) safeSetChannel(4, axisToUs(axes[0]));  // Yaw
            if (axesOk[1]) safeSetChannel(3, axisToUs(-axes[1])); // Throttle
            if (axesOk[2]) safeSetChannel(1, axisToUs(axes[2]));  // Roll
            if (axesOk[3]) safeSetChannel(2, axisToUs(-axes[3])); // Pitch
            
            needToSendChannels = true;
        }

        // Периодическая отправка каналов (каждые 10 мс)
        if (currentMillis - lastSendMs >= Constants::CRSF_SEND_PERIOD_MS) {
            std::lock_guard<std::mutex> lock(channelsMutex);
            
            // Устанавливаем текущие значения каналов в CRSF
            for (int i = 0; i < Constants::CHANNEL_COUNT; ++i) {
                crsfSetChannel(i + 1, currentChannels[i]);
            }
            
            // Отправляем каналы
            crsfSendChannels();
            lastSendMs = currentMillis;
            needToSendChannels = false;
        }
        
        // Если были команды из файла, отправляем немедленно (но не чаще чем раз в 10 мс)
        else if (needToSendChannels) {
            uint32_t timeSinceLastSend = currentMillis - lastSendMs;
            if (timeSinceLastSend >= 2) { // Минимальная задержка 2 мс
                std::lock_guard<std::mutex> lock(channelsMutex);
                
                // Устанавливаем текущие значения каналов в CRSF
                for (int i = 0; i < Constants::CHANNEL_COUNT; ++i) {
                    crsfSetChannel(i + 1, currentChannels[i]);
                }
                
                // Отправляем каналы
                crsfSendChannels();
                lastSendMs = currentMillis;
                needToSendChannels = false;
            }
        }
#endif

        // Небольшая пауза для снижения нагрузки на CPU
        rpi_delay_ms(1);
    }

    return 0;
}