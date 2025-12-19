#include "api_interpreter.h"
#include "config.h"
#include <iostream>
#include <thread>
#include <mutex>
#include <fstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <cstring>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <cmath>
#include <fcntl.h>
#include <sys/stat.h>

static int serverSocket = -1;
static bool interpreterRunning = false;
static std::mutex interpreterMutex;
static const std::string COMMAND_FILE = "/tmp/crsf_command.txt";
static const std::string TELEMETRY_FILE = "/tmp/crsf_telemetry.dat";
static std::string apiServerHost = "localhost";
static int apiServerPort = 8081;

// Структура для чтения телеметрии из файла (совместима с записью из main.cpp)
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

// Получение текущего времени в формате строки
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

// Чтение телеметрии из файла
bool readTelemetry(SharedTelemetryData& data) {
    std::ifstream file(TELEMETRY_FILE, std::ios::binary);
    if (!file.is_open() || !file.good()) {
        return false;
    }
    
    file.read(reinterpret_cast<char*>(&data), sizeof(SharedTelemetryData));
    bool success = (file.gcount() == sizeof(SharedTelemetryData));
    file.close();
    
    return success;
}

// Сравнение двух структур телеметрии для определения изменений
bool hasTelemetryChanged(const SharedTelemetryData& oldData, const SharedTelemetryData& newData) {
    // Проверяем основные флаги и счетчики
    if (oldData.linkUp != newData.linkUp ||
        oldData.lastReceive != newData.lastReceive ||
        oldData.packetsReceived != newData.packetsReceived ||
        oldData.packetsSent != newData.packetsSent ||
        oldData.packetsLost != newData.packetsLost ||
        oldData.remaining != newData.remaining) {
        return true;
    }
    
    // Проверяем каналы
    for (int i = 0; i < 16; i++) {
        if (oldData.channels[i] != newData.channels[i]) {
            return true;
        }
    }
    
    // Проверяем GPS данные (с небольшой погрешностью для чисел с плавающей точкой)
    const double gpsEpsilon = 0.000001;
    if (std::abs(oldData.latitude - newData.latitude) > gpsEpsilon ||
        std::abs(oldData.longitude - newData.longitude) > gpsEpsilon ||
        std::abs(oldData.altitude - newData.altitude) > 0.1 ||
        std::abs(oldData.speed - newData.speed) > 0.1) {
        return true;
    }
    
    // Проверяем данные батареи
    const double batteryEpsilon = 0.01;
    if (std::abs(oldData.voltage - newData.voltage) > batteryEpsilon ||
        std::abs(oldData.current - newData.current) > batteryEpsilon ||
        std::abs(oldData.capacity - newData.capacity) > 0.1) {
        return true;
    }
    
    // Проверяем attitude (положение)
    const double attitudeEpsilon = 0.01;
    if (std::abs(oldData.roll - newData.roll) > attitudeEpsilon ||
        std::abs(oldData.pitch - newData.pitch) > attitudeEpsilon ||
        std::abs(oldData.yaw - newData.yaw) > attitudeEpsilon) {
        return true;
    }
    
    // Проверяем сырые значения attitude
    if (oldData.rollRaw != newData.rollRaw ||
        oldData.pitchRaw != newData.pitchRaw ||
        oldData.yawRaw != newData.yawRaw) {
        return true;
    }
    
    return false;
}

// Отправка телеметрии на API сервер
bool sendTelemetryToApiServer(const SharedTelemetryData& data) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        return false;
    }
    
    // Получаем адрес API сервера
    struct hostent* server = gethostbyname(apiServerHost.c_str());
    if (server == nullptr) {
        close(sock);
        return false;
    }
    
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    memcpy(&serverAddr.sin_addr.s_addr, server->h_addr, server->h_length);
    serverAddr.sin_port = htons(apiServerPort);
    
    // Устанавливаем таймаут
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    
    if (connect(sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "❌ Ошибка подключения к API серверу " << apiServerHost << ":" << apiServerPort << std::endl;
        close(sock);
        return false;
    }
    
    // Формируем JSON с телеметрией
    std::stringstream json;
    json << "{";
    json << "\"linkUp\":" << (data.linkUp ? "true" : "false") << ",";
    json << "\"lastReceive\":" << data.lastReceive << ",";
    json << "\"channels\":[";
    for (int i = 0; i < 16; i++) {
        if (i > 0) json << ",";
        json << data.channels[i];
    }
    json << "],";
    json << "\"packetsReceived\":" << data.packetsReceived << ",";
    json << "\"packetsSent\":" << data.packetsSent << ",";
    json << "\"packetsLost\":" << data.packetsLost << ",";
    json << "\"gps\":{";
    json << "\"latitude\":" << std::fixed << std::setprecision(6) << data.latitude << ",";
    json << "\"longitude\":" << data.longitude << ",";
    json << "\"altitude\":" << data.altitude << ",";
    json << "\"speed\":" << data.speed;
    json << "},";
    json << "\"battery\":{";
    json << "\"voltage\":" << data.voltage << ",";
    json << "\"current\":" << data.current << ",";
    json << "\"capacity\":" << data.capacity << ",";
    json << "\"remaining\":" << static_cast<int>(data.remaining);
    json << "},";
    json << "\"attitude\":{";
    json << "\"roll\":" << data.roll << ",";
    json << "\"pitch\":" << data.pitch << ",";
    json << "\"yaw\":" << data.yaw;
    json << "},";
    json << "\"attitudeRaw\":{";
    json << "\"roll\":" << data.rollRaw << ",";
    json << "\"pitch\":" << data.pitchRaw << ",";
    json << "\"yaw\":" << data.yawRaw;
    json << "},";
    json << "\"timestamp\":\"" << getCurrentTime() << "\",";
    json << "\"activePort\":\"UART Active\"";
    json << "}";
    
    std::string jsonStr = json.str();
    
    // Формируем HTTP POST запрос
    std::stringstream request;
    request << "POST /api/telemetry HTTP/1.1\r\n";
    request << "Host: " << apiServerHost << ":" << apiServerPort << "\r\n";
    request << "Content-Type: application/json\r\n";
    request << "Content-Length: " << jsonStr.length() << "\r\n";
    request << "Connection: close\r\n";
    request << "\r\n";
    request << jsonStr;
    
    std::string requestStr = request.str();
    ssize_t sent = send(sock, requestStr.c_str(), requestStr.length(), 0);
    bool success = (sent >= 0);
    
    if (!success) {
        std::cerr << "❌ Ошибка отправки телеметрии на " << apiServerHost << ":" << apiServerPort << std::endl;
    } else {
        std::cout << "📡 Телеметрия отправлена на " << apiServerHost << ":" << apiServerPort << " (" << sent << " байт)" << std::endl;
    }
    
    // Читаем ответ
    char buffer[1024];
    ssize_t received = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (received > 0) {
        buffer[received] = '\0';
        std::cout << "✅ Ответ сервера: " << std::string(buffer, received) << std::endl;
    }
    
    close(sock);
    return success;
}

// Запись команды в файл (как это делает pybind)
void writeCommandToFile(const std::string& command) {
    // В режиме --notel используем неблокирующую запись для предотвращения зависаний
    if (g_ignore_telemetry) {
        // Используем низкоуровневый API для неблокирующей записи
        int fd = open(COMMAND_FILE.c_str(), O_WRONLY | O_CREAT | O_APPEND | O_NONBLOCK, 0644);
        if (fd >= 0) {
            std::string cmdLine = command + "\n";
            write(fd, cmdLine.c_str(), cmdLine.length());
            close(fd);
        } else {
            // Fallback на обычную запись
            std::ofstream cmdFile(COMMAND_FILE, std::ios::app);
            if (cmdFile.is_open()) {
                cmdFile << command << std::endl;
                cmdFile.close();
            }
        }
    } else {
        std::ofstream cmdFile(COMMAND_FILE, std::ios::app);
        if (cmdFile.is_open()) {
            cmdFile << command << std::endl;
            cmdFile.close();
        } else {
            std::cerr << "❌ Ошибка записи в файл команд: " << COMMAND_FILE << std::endl;
        }
    }
}

// Парсинг JSON для setChannel
bool parseSetChannelJson(const std::string& body, unsigned int& channel, int& value) {
    // Формат: {"command":"setChannel","channel":1,"value":1500}
    size_t chPos = body.find("\"channel\"");
    size_t valPos = body.find("\"value\"");
    
    if (chPos == std::string::npos || valPos == std::string::npos) {
        return false;
    }
    
    size_t chStart = body.find(':', chPos) + 1;
    size_t chEnd = body.find_first_of(",}", chStart);
    std::string chStr = body.substr(chStart, chEnd - chStart);
    
    size_t valStart = body.find(':', valPos) + 1;
    size_t valEnd = body.find_first_of(",}", valStart);
    std::string valStr = body.substr(valStart, valEnd - valStart);
    
    try {
        channel = std::stoi(chStr);
        value = std::stoi(valStr);
        return true;
    } catch (...) {
        return false;
    }
}

// Парсинг JSON для setChannels
bool parseSetChannelsJson(const std::string& body, std::string& channelsStr) {
    // Формат: {"command":"setChannels","channelsStr":"setChannels 1=1500 2=1600 ..."}
    size_t strPos = body.find("\"channelsStr\"");
    if (strPos == std::string::npos) {
        return false;
    }
    
    size_t valStart = body.find('"', body.find(':', strPos)) + 1;
    size_t valEnd = body.find('"', valStart);
    if (valStart == std::string::npos || valEnd == std::string::npos) {
        return false;
    }
    
    channelsStr = body.substr(valStart, valEnd - valStart);
    return !channelsStr.empty();
}

// Парсинг JSON для setMode
bool parseSetModeJson(const std::string& body, std::string& mode) {
    // Формат: {"command":"setMode","mode":"joystick"}
    size_t modePos = body.find("\"mode\"");
    if (modePos == std::string::npos) {
        return false;
    }
    
    size_t valStart = body.find('"', body.find(':', modePos)) + 1;
    size_t valEnd = body.find('"', valStart);
    if (valStart == std::string::npos || valEnd == std::string::npos) {
        return false;
    }
    
    mode = body.substr(valStart, valEnd - valStart);
    return (mode == "joystick" || mode == "manual");
}

// Отправка HTTP ответа
void sendHttpResponse(int clientSocket, const std::string& content, const std::string& contentType = "application/json", int statusCode = 200) {
    std::stringstream response;
    response << "HTTP/1.1 " << statusCode << " " << (statusCode == 200 ? "OK" : "Bad Request") << "\r\n";
    response << "Content-Type: " << contentType << "\r\n";
    response << "Content-Length: " << content.length() << "\r\n";
    response << "Access-Control-Allow-Origin: *\r\n";
    response << "Connection: close\r\n\r\n";
    response << content;
    
    std::string responseStr = response.str();
    send(clientSocket, responseStr.c_str(), responseStr.length(), 0);
}

// Обработка HTTP запросов
void handleHttpRequest(int clientSocket, const std::string& request) {
    std::stringstream ss(request);
    std::string method, path, version;
    ss >> method >> path >> version;
    
    // Читаем тело запроса
    std::string body;
    size_t bodyPos = request.find("\r\n\r\n");
    if (bodyPos != std::string::npos) {
        body = request.substr(bodyPos + 4);
    }
    
    if (path == "/" || path == "/index.html") {
        std::string html = R"(<!DOCTYPE html>
<html><head><title>CRSF API Interpreter</title></head>
<body>
<h1>CRSF API Interpreter</h1>
<p>Интерпретатор команд для ведомого узла</p>
<p>Команды записываются в: )" + COMMAND_FILE + R"(</p>
<p>Доступные endpoints:</p>
<ul>
<li>POST /api/command/setChannel - установка одного канала</li>
<li>POST /api/command/setChannels - установка всех каналов</li>
<li>POST /api/command/sendChannels - отправка каналов</li>
<li>POST /api/command/setMode - установка режима</li>
</ul>
</body></html>)";
        sendHttpResponse(clientSocket, html, "text/html");
    } else if (path.find("/api/command/") == 0) {
        std::string command = path.substr(13); // длина "/api/command/" = 13
        
        bool success = false;
        std::string responseJson;
        
        if (command == "setChannel") {
            unsigned int channel;
            int value;
            if (parseSetChannelJson(body, channel, value)) {
                if (channel >= 1 && channel <= 16 && value >= 1000 && value <= 2000) {
                    std::stringstream cmd;
                    cmd << "setChannel " << channel << " " << value;
                    writeCommandToFile(cmd.str());
                    success = true;
                    std::cout << "📝 Команда записана: setChannel " << channel << " " << value << std::endl;
                } else {
                    responseJson = "{\"status\":\"error\",\"message\":\"Invalid channel or value range\"}";
                }
            } else {
                responseJson = "{\"status\":\"error\",\"message\":\"Invalid JSON format\"}";
            }
        } else if (command == "setChannels") {
            std::string channelsStr;
            if (parseSetChannelsJson(body, channelsStr)) {
                writeCommandToFile(channelsStr);
                success = true;
                std::cout << "📝 Команда записана: " << channelsStr << std::endl;
            } else {
                responseJson = "{\"status\":\"error\",\"message\":\"Invalid channels string\"}";
            }
        } else if (command == "sendChannels") {
            writeCommandToFile("sendChannels");
            success = true;
            std::cout << "📝 Команда записана: sendChannels" << std::endl;
        } else if (command == "setMode") {
            std::string mode;
            if (parseSetModeJson(body, mode)) {
                std::stringstream cmd;
                cmd << "setMode " << mode;
                writeCommandToFile(cmd.str());
                success = true;
                std::cout << "📝 Команда записана: setMode " << mode << std::endl;
            } else {
                responseJson = "{\"status\":\"error\",\"message\":\"Invalid mode\"}";
            }
        } else {
            responseJson = "{\"status\":\"error\",\"message\":\"Unknown command\"}";
        }
        
        if (responseJson.empty()) {
            if (success) {
                responseJson = "{\"status\":\"ok\",\"message\":\"Command written to file\"}";
            } else {
                responseJson = "{\"status\":\"error\",\"message\":\"Failed to process command\"}";
            }
        }
        
        sendHttpResponse(clientSocket, responseJson);
    } else {
        sendHttpResponse(clientSocket, "{\"status\":\"error\",\"message\":\"Not Found\"}", "application/json", 404);
    }
}

// Обработка клиентских подключений
void handleClient(int clientSocket) {
    char buffer[8192];
    int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    
    if (bytesReceived > 0) {
        buffer[bytesReceived] = '\0';
        std::string request(buffer);
        handleHttpRequest(clientSocket, request);
    }
    
    close(clientSocket);
}

// Основная функция API интерпретатора
void startApiInterpreter(int port, const std::string& host, int apiPort) {
    std::lock_guard<std::mutex> lock(interpreterMutex);
    
    if (interpreterRunning) {
        std::cout << "⚠️ API интерпретатор уже запущен" << std::endl;
        return;
    }
    
    apiServerHost = host;
    apiServerPort = apiPort;
    
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        std::cerr << "❌ Ошибка создания сокета" << std::endl;
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
    
    interpreterRunning = true;
    std::cout << "🔌 API интерпретатор запущен на порту " << port << std::endl;
    std::cout << "📝 Команды записываются в: " << COMMAND_FILE << std::endl;
    std::cout << "📡 Телеметрия отправляется на: " << apiServerHost << ":" << apiServerPort << std::endl;
    
    // Запускаем поток для отправки телеметрии
    std::thread telemetryThread([&]() {
        SharedTelemetryData lastSentData;
        bool hasLastData = false;
        
        while (interpreterRunning) {
            SharedTelemetryData data;
            if (readTelemetry(data)) {
                // Отправляем только если данные изменились или это первая отправка
                if (!hasLastData || hasTelemetryChanged(lastSentData, data)) {
                    sendTelemetryToApiServer(data);
                    lastSentData = data;
                    hasLastData = true;
                }
            }
            // Проверяем изменения каждые 20мс (50 Гц)
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    });
    telemetryThread.detach();
    
    // Основной цикл сервера
    while (interpreterRunning) {
        struct sockaddr_in clientAddr;
        socklen_t clientAddrLen = sizeof(clientAddr);
        
        int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);
        if (clientSocket < 0) {
            if (interpreterRunning) {
                continue;
            } else {
                break;
            }
        }
        
        std::thread clientThread(handleClient, clientSocket);
        clientThread.detach();
    }
    
    close(serverSocket);
    interpreterRunning = false;
}

// Остановка API интерпретатора
void stopApiInterpreter() {
    std::lock_guard<std::mutex> lock(interpreterMutex);
    if (interpreterRunning) {
        interpreterRunning = false;
        if (serverSocket >= 0) {
            close(serverSocket);
            serverSocket = -1;
        }
    }
}

// Главная функция для запуска API интерпретатора как отдельного приложения
int main(int argc, char* argv[]) {
    int port = 8082;
    std::string apiServerHost = "localhost";
    int apiServerPort = 8081;
    
    // Парсинг аргументов командной строки
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--notel") {
            g_ignore_telemetry = true;
            std::cout << "[INFO] Running in NO-TELEMETRY mode. Safety checks disabled." << std::endl;
        } else if (i == 1 && arg.find_first_not_of("0123456789") == std::string::npos) {
            port = std::stoi(arg);
        } else if (i == 2) {
            apiServerHost = arg;
        } else if (i == 3 && arg.find_first_not_of("0123456789") == std::string::npos) {
            apiServerPort = std::stoi(arg);
        }
    }
    
    std::cout << "🚀 Запуск CRSF API интерпретатора..." << std::endl;
    std::cout << "📡 Порт интерпретатора: " << port << std::endl;
    std::cout << "📝 Команды записываются в: " << COMMAND_FILE << std::endl;
    std::cout << "📡 Телеметрия отправляется на: " << apiServerHost << ":" << apiServerPort << std::endl;
    
    // Запускаем интерпретатор (блокирующий вызов)
    startApiInterpreter(port, apiServerHost, apiServerPort);
    
    return 0;
}
