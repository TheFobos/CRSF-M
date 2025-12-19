#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>
#include <vector>
#include <string>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <cstdint>
#include "../crsf/crsf.h"
#include "../libs/crsf/CrsfSerial.h"

namespace py = pybind11;

// Глобальные переменные для телеметрии
static CrsfSerial* crsfInstance = nullptr;
static std::mutex telemetryMutex;
static std::string workMode = "manual"; // joystick, manual - по умолчанию ручной режим

// Структура для телеметрии
struct TelemetryData {
    bool linkUp = false;
    std::string activePort = "Unknown";
    uint32_t lastReceive = 0;
    std::vector<int> channels;
    uint32_t packetsReceived = 0;
    uint32_t packetsSent = 0;
    uint32_t packetsLost = 0;
    double latitude = 0.0;
    double longitude = 0.0;
    double altitude = 0.0;
    double speed = 0.0;
    double voltage = 0.0;
    double current = 0.0;
    double capacity = 0.0;
    uint8_t remaining = 0;
    double roll = 0.0;
    double pitch = 0.0;
    double yaw = 0.0;
    int16_t rollRaw = 0;
    int16_t pitchRaw = 0;
    int16_t yawRaw = 0;
    std::string timestamp;
};

// Функция для получения текущего времени
std::string getCurrentTime() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

// Инициализация CRSF (вызывается из Python обертки)
// ВАЖНО: Указатели из другого процесса недействительны!
// Вместо этого используем механизм чтения данных из файла/памяти
// Эта функция больше не используется, но оставлена для совместимости
void initCrsfInstance(uintptr_t crsfPtrValue) {
    // НЕ ИСПОЛЬЗУЕМ указатель из другого процесса - это вызовет segfault!
    // Вместо этого данные будут читаться из разделяемой памяти/файла
    std::lock_guard<std::mutex> lock(telemetryMutex);
    // crsfInstance остается nullptr - данные читаются из файла
    crsfInstance = nullptr;
}

// Автоматическая инициализация (попытка получить указатель через ctypes)
// ВНИМАНИЕ: Эта функция требует, чтобы основное приложение было загружено
// и экспортировало функцию crsfGetActive()
void autoInitCrsfInstance() {
    // Попытка загрузить символ crsfGetActive из основного приложения
    // Это работает только если основное приложение загружено в тот же процесс
    // Для работы через разделяемую библиотеку нужно использовать ctypes в Python
    // Здесь просто проверяем, не был ли уже инициализирован экземпляр
    if (crsfInstance == nullptr) {
        // Если не инициализирован, нужно вызвать init_crsf_instance из Python
        // с указателем, полученным через ctypes
    }
}

// Структура для чтения данных из файла (совместима с записью из main.cpp)
struct SharedTelemetryData {
    bool linkUp;
    uint32_t lastReceive;
    int channels[16];
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

// Получение телеметрии из файла (безопасный способ для межпроцессного взаимодействия)
TelemetryData getTelemetry() {
    std::lock_guard<std::mutex> lock(telemetryMutex);
    TelemetryData data;
    
    // Читаем данные из файла, созданного основным приложением
    std::ifstream file("/tmp/crsf_telemetry.dat", std::ios::binary);
    if (file.is_open() && file.good()) {
        SharedTelemetryData shared;
        file.read(reinterpret_cast<char*>(&shared), sizeof(SharedTelemetryData));
        file.close();
        
        if (file.gcount() == sizeof(SharedTelemetryData)) {
            // Копируем данные из разделяемой структуры
            data.linkUp = shared.linkUp;
            data.lastReceive = shared.lastReceive;
            data.channels.assign(shared.channels, shared.channels + 16);
            data.packetsReceived = shared.packetsReceived;
            data.packetsSent = shared.packetsSent;
            data.packetsLost = shared.packetsLost;
            data.latitude = shared.latitude;
            data.longitude = shared.longitude;
            data.altitude = shared.altitude;
            data.speed = shared.speed;
            data.voltage = shared.voltage;
            data.current = shared.current;
            data.capacity = shared.capacity;
            data.remaining = shared.remaining;
            data.roll = shared.roll;
            data.pitch = shared.pitch;
            data.yaw = shared.yaw;
            data.rollRaw = shared.rollRaw;
            data.pitchRaw = shared.pitchRaw;
            data.yawRaw = shared.yawRaw;
            data.activePort = "UART Active";
        } else {
            data.activePort = "No Connection";
        }
    } else {
        data.activePort = "No Connection";
    }
    
    data.timestamp = getCurrentTime();
    return data;
}

// Установка режима работы через файл команд
void setWorkMode(const std::string& mode) {
    std::lock_guard<std::mutex> lock(telemetryMutex);
    if (mode == "joystick" || mode == "manual") {
        workMode = mode;
        // Записываем команду в файл для основного приложения
        std::ofstream cmdFile("/tmp/crsf_command.txt");
        if (cmdFile.is_open()) {
            cmdFile << "setMode " << mode << std::endl;
            cmdFile.close();
        }
    }
}

// Получение режима работы
std::string getWorkMode() {
    std::lock_guard<std::mutex> lock(telemetryMutex);
    return workMode;
}

// Установка канала через файл команд (добавляет команду, не перезаписывает)
void setChannel(unsigned int channel, int value) {
    if (channel >= 1 && channel <= 16 && value >= 1000 && value <= 2000) {
        // Добавляем команду в файл (append mode) для основного приложения
        std::ofstream cmdFile("/tmp/crsf_command.txt", std::ios::app);
        if (cmdFile.is_open()) {
            cmdFile << "setChannel " << channel << " " << value << std::endl;
            cmdFile.close();
        }
    }
}

// Установка всех каналов одной командой
void setChannels(const std::vector<int>& channels) {
    if (channels.size() >= 16) {
        std::ofstream cmdFile("/tmp/crsf_command.txt");
        if (cmdFile.is_open()) {
            cmdFile << "setChannels";
            for (size_t i = 0; i < 16; i++) {
                if (channels[i] >= 1000 && channels[i] <= 2000) {
                    cmdFile << " " << (i + 1) << "=" << channels[i];
                }
            }
            cmdFile << std::endl;
            cmdFile.close();
        }
    }
}

// Отправка каналов через файл команд
// ВАЖНО: используем append режим, чтобы не перезаписать команды setChannel
void sendChannels() {
    std::ofstream cmdFile("/tmp/crsf_command.txt", std::ios::app);
    if (cmdFile.is_open()) {
        cmdFile << "sendChannels" << std::endl;
        cmdFile.close();
    }
}

// Модуль pybind11
PYBIND11_MODULE(crsf_native, m) {
    m.doc() = "CRSF Native C++ bindings for Python";
    
    // Экспорт структуры TelemetryData
    py::class_<TelemetryData>(m, "TelemetryData")
        .def_readwrite("linkUp", &TelemetryData::linkUp)
        .def_readwrite("activePort", &TelemetryData::activePort)
        .def_readwrite("lastReceive", &TelemetryData::lastReceive)
        .def_readwrite("channels", &TelemetryData::channels)
        .def_readwrite("packetsReceived", &TelemetryData::packetsReceived)
        .def_readwrite("packetsSent", &TelemetryData::packetsSent)
        .def_readwrite("packetsLost", &TelemetryData::packetsLost)
        .def_readwrite("latitude", &TelemetryData::latitude)
        .def_readwrite("longitude", &TelemetryData::longitude)
        .def_readwrite("altitude", &TelemetryData::altitude)
        .def_readwrite("speed", &TelemetryData::speed)
        .def_readwrite("voltage", &TelemetryData::voltage)
        .def_readwrite("current", &TelemetryData::current)
        .def_readwrite("capacity", &TelemetryData::capacity)
        .def_readwrite("remaining", &TelemetryData::remaining)
        .def_readwrite("roll", &TelemetryData::roll)
        .def_readwrite("pitch", &TelemetryData::pitch)
        .def_readwrite("yaw", &TelemetryData::yaw)
        .def_readwrite("rollRaw", &TelemetryData::rollRaw)
        .def_readwrite("pitchRaw", &TelemetryData::pitchRaw)
        .def_readwrite("yawRaw", &TelemetryData::yawRaw)
        .def_readwrite("timestamp", &TelemetryData::timestamp);
    
    // Экспорт функций
    m.def("init_crsf_instance", &initCrsfInstance,
          "Initialize CRSF instance from pointer value (uintptr_t)",
          py::arg("crsf_ptr_value"));
    
    m.def("auto_init_crsf_instance", &autoInitCrsfInstance,
          "Auto-initialize CRSF instance from crsfGetActive()");
    
    m.def("get_telemetry", &getTelemetry,
          "Get telemetry data");
    
    m.def("set_work_mode", &setWorkMode,
          "Set work mode (joystick or manual)",
          py::arg("mode"));
    
    m.def("get_work_mode", &getWorkMode,
          "Get current work mode");
    
    m.def("set_channel", &setChannel,
          "Set channel value",
          py::arg("channel"), py::arg("value"));
    
    m.def("set_channels", &setChannels,
          "Set all channels at once",
          py::arg("channels"));
    
    m.def("send_channels", &sendChannels,
          "Send channels packet");
}

