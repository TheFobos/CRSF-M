#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include "libs/crsf/CrsfSerial.h"

// Глобальные переменные для телеметрии
struct TelemetryData {
    // Статус связи
    bool linkUp = false;
    std::string activePort = "Unknown";
    uint32_t lastReceive = 0;
    
    // RC каналы
    int channels[16] = {0};
    
    // Статистика связи
    uint32_t packetsReceived = 0;
    uint32_t packetsSent = 0;
    uint32_t packetsLost = 0;
    
    // GPS данные (если доступны)
    double latitude = 0.0;
    double longitude = 0.0;
    double altitude = 0.0;
    double speed = 0.0;
    
    // Батарея
    double voltage = 0.0;
    double current = 0.0;
    double capacity = 0.0;
    uint8_t remaining = 0;
    
    // Положение
    double roll = 0.0;
    double pitch = 0.0;
    double yaw = 0.0;
    
    // Сырые значения attitude (raw bytes)
    int16_t rawAttitudeBytes[3] = {0};  // [0]=roll, [1]=pitch, [2]=yaw
    
    // Режим работы
    std::string workMode = "joystick"; // joystick, manual
    
    std::string timestamp;
};

static TelemetryData telemetryData;
static std::mutex telemetryMutex;
static CrsfSerial* crsfInstance = nullptr;

// Функция для получения текущего режима работы
std::string getWorkMode() {
    std::lock_guard<std::mutex> lock(telemetryMutex);
    return telemetryData.workMode;
}

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

// Функция для обновления телеметрии
void updateTelemetry() {
    std::lock_guard<std::mutex> lock(telemetryMutex);
    
    if (crsfInstance) {
        telemetryData.linkUp = crsfInstance->isLinkUp();
        telemetryData.lastReceive = crsfInstance->_lastReceive;
        
        // Получаем каналы
        for (int i = 0; i < 16; i++) {
            telemetryData.channels[i] = crsfInstance->getChannel(i + 1);
        }
        
        // Получаем статистику связи
        const crsfLinkStatistics_t* stats = crsfInstance->getLinkStatistics();
        if (stats) {
            telemetryData.packetsReceived = stats->uplink_RSSI_1;
            telemetryData.packetsSent = stats->uplink_RSSI_2;
            telemetryData.packetsLost = 100 - stats->uplink_Link_quality; // Потерянные пакеты = 100 - качество связи
        }
        
        // Получаем GPS данные
        const crsf_sensor_gps_t* gps = crsfInstance->getGpsSensor();
        if (gps) {
            // Конвертируем из формата CRSF (degree / 10,000,000) в обычные градусы
            telemetryData.latitude = gps->latitude / 10000000.0;
            telemetryData.longitude = gps->longitude / 10000000.0;
            // Высота в метрах, +1000м offset
            telemetryData.altitude = gps->altitude - 1000;
            // Скорость в км/ч / 10
            telemetryData.speed = gps->groundspeed / 10.0;
        }
        
        // Получаем данные батареи
        telemetryData.voltage = crsfInstance->getBatteryVoltage();
        telemetryData.current = crsfInstance->getBatteryCurrent();
        telemetryData.capacity = crsfInstance->getBatteryCapacity();
        telemetryData.remaining = crsfInstance->getBatteryRemaining();
        
        // Получаем данные положения
        telemetryData.roll = crsfInstance->getAttitudeRoll();
        telemetryData.pitch = crsfInstance->getAttitudePitch();
        telemetryData.yaw = crsfInstance->getAttitudeYaw();
        
        // Получаем сырые значения attitude
        telemetryData.rawAttitudeBytes[0] = crsfInstance->getRawAttitudeRoll();
        telemetryData.rawAttitudeBytes[1] = crsfInstance->getRawAttitudePitch();
        telemetryData.rawAttitudeBytes[2] = crsfInstance->getRawAttitudeYaw();
    }
    
    telemetryData.timestamp = getCurrentTime();
    
    // Определяем активный порт
    if (crsfInstance) {
        telemetryData.activePort = "UART Active";
    } else {
        telemetryData.activePort = "No Connection";
    }
}

// Функция для отправки HTTP ответа
void sendHttpResponse(int clientSocket, const std::string& content, const std::string& contentType = "text/html") {
    std::stringstream response;
    response << "HTTP/1.1 200 OK\r\n";
    response << "Content-Type: " << contentType << "\r\n";
    response << "Content-Length: " << content.length() << "\r\n";
    response << "Access-Control-Allow-Origin: *\r\n";
    response << "Connection: close\r\n\r\n";
    response << content;
    
    std::string responseStr = response.str();
    send(clientSocket, responseStr.c_str(), responseStr.length(), 0);
}

// Функция для создания JSON телеметрии
std::string createTelemetryJson() {
    std::lock_guard<std::mutex> lock(telemetryMutex);
    
    std::stringstream json;
    json << "{";
    json << "\"linkUp\":" << (telemetryData.linkUp ? "true" : "false") << ",";
    json << "\"activePort\":\"" << telemetryData.activePort << "\",";
    json << "\"lastReceive\":" << telemetryData.lastReceive << ",";
    json << "\"timestamp\":\"" << telemetryData.timestamp << "\",";
    
    // RC каналы
    json << "\"channels\":[";
    for (int i = 0; i < 16; i++) {
        if (i > 0) json << ",";
        json << telemetryData.channels[i];
    }
    json << "],";
    
    // Статистика
    json << "\"packetsReceived\":" << telemetryData.packetsReceived << ",";
    json << "\"packetsSent\":" << telemetryData.packetsSent << ",";
    json << "\"packetsLost\":" << telemetryData.packetsLost << ",";
    
    // GPS
    json << "\"gps\":{";
    json << "\"latitude\":" << telemetryData.latitude << ",";
    json << "\"longitude\":" << telemetryData.longitude << ",";
    json << "\"altitude\":" << telemetryData.altitude << ",";
    json << "\"speed\":" << telemetryData.speed;
    json << "},";
    
    // Батарея
    json << "\"battery\":{";
    json << "\"voltage\":" << telemetryData.voltage << ",";
    json << "\"current\":" << telemetryData.current << ",";
    json << "\"capacity\":" << telemetryData.capacity << ",";
    json << "\"remaining\":" << (int)telemetryData.remaining;
    json << "},";
    
    // Положение
    json << "\"attitude\":{";
    json << "\"roll\":" << telemetryData.roll << ",";
    json << "\"pitch\":" << telemetryData.pitch << ",";
    json << "\"yaw\":" << telemetryData.yaw;
    json << "},";
    
    // Сырые значения attitude (raw CRSF bytes)
    json << "\"attitudeRaw\":{";
    json << "\"roll\":" << telemetryData.rawAttitudeBytes[0] << ",";
    json << "\"pitch\":" << telemetryData.rawAttitudeBytes[1] << ",";
    json << "\"yaw\":" << telemetryData.rawAttitudeBytes[2];
    json << "},";
    
    // Режим работы
    json << "\"workMode\":\"" << telemetryData.workMode << "\"";
    
    json << "}";
    return json.str();
}

// Функция для обработки команд управления
void handleCommand(const std::string& command, const std::string& value) {
    std::lock_guard<std::mutex> lock(telemetryMutex);
    
    if (command == "setMode") {
        if (value == "joystick" || value == "manual") {
            telemetryData.workMode = value;
            std::cout << "🔧 Режим изменен на: " << value << std::endl;
        }
    } else if (command == "setChannel") {
        // Формат: channel=value (например: 1=1500)
        size_t pos = value.find('=');
        if (pos != std::string::npos) {
            int channel = std::stoi(value.substr(0, pos));
            int val = std::stoi(value.substr(pos + 1));
            if (channel >= 1 && channel <= 16 && val >= 1000 && val <= 2000) {
                if (crsfInstance) {
                    crsfInstance->setChannel(channel, val);
                    std::cout << "🎮 Канал " << channel << " установлен в " << val << " мкс" << std::endl;
                }
            }
        }
    }
}

// Функция для обработки HTTP запросов
void handleHttpRequest(int clientSocket, const std::string& request) {
    std::stringstream ss(request);
    std::string method, path, version;
    ss >> method >> path >> version;
    
    if (path == "/" || path == "/index.html") {
        // Простая информационная страница
        std::string html = R"(<!DOCTYPE html>
<html><head><title>CRSF API</title></head>
<body>
<h1>CRSF Телеметрия API</h1>
<p>Доступные endpoints:</p>
<ul>
<li><a href="/api/telemetry">/api/telemetry</a> - JSON данные телеметрии</li>
<li><a href="/api/command">/api/command</a> - Команды управления</li>
</ul>
</body></html>)";
        sendHttpResponse(clientSocket, html);
    } else if (path == "/api/telemetry") {
        // API для получения телеметрии
        std::string json = createTelemetryJson();
        sendHttpResponse(clientSocket, json, "application/json");
    } else if (path.find("/api/command") == 0) {
        // API для команд управления
        size_t pos = path.find("?");
        if (pos != std::string::npos) {
            std::string query = path.substr(pos + 1);
            size_t cmdPos = query.find("cmd=");
            size_t valPos = query.find("&value=");
            
            if (cmdPos != std::string::npos && valPos != std::string::npos) {
                std::string command = query.substr(cmdPos + 4, valPos - cmdPos - 4);
                std::string value = query.substr(valPos + 7);
                handleCommand(command, value);
            }
        }
        
        sendHttpResponse(clientSocket, "{\"status\":\"ok\"}", "application/json");
    } else {
        // 404 Not Found
        std::string response = "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\n\r\n<h1>404 Not Found</h1>";
        send(clientSocket, response.c_str(), response.length(), 0);
    }
}

// Функция для обработки клиентских подключений
void handleClient(int clientSocket) {
    char buffer[4096];
    int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    
    if (bytesReceived > 0) {
        buffer[bytesReceived] = '\0';
        std::string request(buffer);
        handleHttpRequest(clientSocket, request);
    }
    
    close(clientSocket);
}

// Основная функция веб-сервера
void startTelemetryServer(CrsfSerial* crsf, int port = 8080, int updateIntervalMs = 10) {
    std::cout << "🌐 Запуск веб-сервера телеметрии (реалтайм " << updateIntervalMs << "мс)..." << std::endl;
    crsfInstance = crsf;
    
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        std::cerr << "Ошибка создания сокета" << std::endl;
        return;
    }
    
    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);
    
    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "❌ Ошибка привязки к порту " << port << std::endl;
        close(serverSocket);
        return;
    }
    
    if (listen(serverSocket, 5) < 0) {
        std::cerr << "❌ Ошибка прослушивания порта" << std::endl;
        close(serverSocket);
        return;
    }
    
    std::cout << "🌐 Веб-сервер телеметрии запущен на порту " << port << std::endl;
    std::cout << "📱 Откройте браузер: http://localhost:" << port << std::endl;
    
    // Запускаем поток для обновления телеметрии (реалтайм)
    std::thread telemetryThread([updateIntervalMs]() {
        while (true) {
            updateTelemetry();
            std::this_thread::sleep_for(std::chrono::milliseconds(updateIntervalMs));
        }
    });
    telemetryThread.detach();
    
    // Основной цикл сервера
    while (true) {
        struct sockaddr_in clientAddr;
        socklen_t clientAddrLen = sizeof(clientAddr);
        
        int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);
        if (clientSocket < 0) {
            continue;
        }
        
        std::thread clientThread(handleClient, clientSocket);
        clientThread.detach();
    }
    
    close(serverSocket);
}
