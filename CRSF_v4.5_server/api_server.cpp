#include "api_server.h"
#include "config.h"
#include <iostream>
#include <thread>
#include <mutex>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <cstring>
#include <sstream>
#include <fstream>
#include <vector>

static int serverSocket = -1;
static bool serverRunning = false;
static std::mutex serverMutex;
static std::mutex telemetryMutex;
static std::string targetHost = "localhost";
static int targetPort = 8082;
static std::string lastTelemetryJson = "{}"; // Последняя полученная телеметрия

// Отправка команды на ведомый узел через HTTP API (используя простые сокеты)
bool sendCommandToTarget(const std::string& command, const std::string& body = "") {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cerr << "❌ Ошибка создания сокета для отправки команды" << std::endl;
        return false;
    }

    // Получаем адрес целевого хоста
    struct hostent* server = gethostbyname(targetHost.c_str());
    if (server == nullptr) {
        std::cerr << "❌ Ошибка разрешения имени хоста: " << targetHost << std::endl;
        close(sock);
        return false;
    }

    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    memcpy(&serverAddr.sin_addr.s_addr, server->h_addr, server->h_length);
    serverAddr.sin_port = htons(targetPort);

    // Устанавливаем таймаут подключения
    // В режиме --notel используем очень короткий таймаут для быстрого ответа
    struct timeval timeout;
    if (g_ignore_telemetry) {
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000; // 100мс для режима без телеметрии
    } else {
        timeout.tv_sec = 2;
        timeout.tv_usec = 0;
    }
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    if (connect(sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "❌ Ошибка подключения к " << targetHost << ":" << targetPort << std::endl;
        close(sock);
        return false;
    }

    // Формируем HTTP POST запрос
    std::stringstream request;
    request << "POST /api/command/" << command << " HTTP/1.1\r\n";
    request << "Host: " << targetHost << ":" << targetPort << "\r\n";
    request << "Content-Type: application/json\r\n";
    request << "Content-Length: " << body.length() << "\r\n";
    request << "Connection: close\r\n";
    request << "\r\n";
    request << body;

    std::string requestStr = request.str();
    if (send(sock, requestStr.c_str(), requestStr.length(), 0) < 0) {
        std::cerr << "❌ Ошибка отправки HTTP запроса" << std::endl;
        close(sock);
        return false;
    }

    // Читаем ответ (необязательно, но полезно для отладки)
    char buffer[1024];
    ssize_t bytesReceived = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (bytesReceived > 0) {
        buffer[bytesReceived] = '\0';
        // Можно проверить статус ответа, но для простоты просто считаем успехом
    }

    close(sock);
    return true;
}

// Парсинг JSON тела запроса для setChannel
bool parseSetChannel(const std::string& body, unsigned int& channel, int& value) {
    // Простой парсинг JSON: {"channel":1,"value":1500}
    size_t chPos = body.find("\"channel\"");
    size_t valPos = body.find("\"value\"");
    
    if (chPos == std::string::npos || valPos == std::string::npos) {
        return false;
    }
    
    // Ищем число после "channel":
    size_t chStart = body.find(':', chPos) + 1;
    size_t chEnd = body.find_first_of(",}", chStart);
    std::string chStr = body.substr(chStart, chEnd - chStart);
    
    // Ищем число после "value":
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
// Парсинг JSON для setChannels
bool parseSetChannels(const std::string& body, std::string& channelsStr) {
    std::cout << "\n🔍 Парсинг setChannels, полученное тело запроса:\n" << body << std::endl;
    
    // Формат: {"channels":[1500,1600,1700,...]}
    size_t arrPos = body.find("\"channels\"");
    if (arrPos == std::string::npos) {
        std::cout << "❌ Ключ 'channels' не найден в теле запроса" << std::endl;
        std::cout << "   Доступные ключи: ";
        size_t quotePos = body.find('"');
        while (quotePos != std::string::npos) {
            size_t nextQuote = body.find('"', quotePos + 1);
            if (nextQuote != std::string::npos) {
                std::cout << body.substr(quotePos, nextQuote - quotePos + 1) << " ";
                quotePos = body.find('"', nextQuote + 1);
            } else {
                break;
            }
        }
        std::cout << std::endl;
        return false;
    }
    
    size_t arrStart = body.find('[', arrPos);
    size_t arrEnd = body.find(']', arrStart);
    if (arrStart == std::string::npos || arrEnd == std::string::npos) {
        std::cout << "❌ Массив каналов не найден (отсутствуют [ или ])" << std::endl;
        return false;
    }
    
    std::string arrContent = body.substr(arrStart + 1, arrEnd - arrStart - 1);
    std::cout << "📊 Содержимое массива: [" << arrContent << "]" << std::endl;
    
    // Проверяем пустой массив
    if (arrContent.empty()) {
        std::cout << "❌ Массив каналов пустой" << std::endl;
        return false;
    }
    
    std::istringstream iss(arrContent);
    std::string token;
    
    // Создаем массив для проверки валидности
    std::vector<int> channels;
    int chNum = 1;
    
    while (std::getline(iss, token, ',')) {
        // Убираем пробелы
        token.erase(0, token.find_first_not_of(" \t\n\r"));
        token.erase(token.find_last_not_of(" \t\n\r") + 1);
        
        // Пропускаем пустые токены
        if (token.empty()) {
            continue;
        }
        
        std::cout << "🔢 Обработка канала " << chNum << ": '" << token << "'" << std::endl;
        
        try {
            int value = std::stoi(token);
            if (value >= 1000 && value <= 2000) {
                channels.push_back(value);
                std::cout << "   ✓ Значение " << value << " в диапазоне" << std::endl;
                chNum++;
            } else {
                // Значение вне диапазона
                std::cout << "❌ Значение " << value << " вне диапазона 1000-2000" << std::endl;
                return false;
            }
        } catch (const std::exception& e) {
            // Ошибка преобразования
            std::cout << "❌ Ошибка преобразования '" << token << "': " << e.what() << std::endl;
            return false;
        } catch (...) {
            std::cout << "❌ Неизвестная ошибка преобразования '" << token << "'" << std::endl;
            return false;
        }
    }
    
    // Проверяем, что получили ровно 16 каналов
    std::cout << "📈 Обработано каналов: " << channels.size() << std::endl;
    if (channels.size() != 16) {
        std::cout << "❌ Неправильное количество каналов: " << channels.size() << " (ожидается 16)" << std::endl;
        return false;
    }
    
    // Формируем строку в формате, который ожидает интерпретатор: "setChannels 1=1500 2=1600 ... 16=1500"
    channelsStr = "setChannels";
    for (size_t i = 0; i < channels.size(); ++i) {
        channelsStr += " " + std::to_string(i + 1) + "=" + std::to_string(channels[i]);
    }
    
    std::cout << "✅ Сформирована команда для интерпретатора:\n   " << channelsStr << std::endl;
    return true;
}

// Парсинг JSON для setMode
bool parseSetMode(const std::string& body, std::string& mode) {
    // Формат: {"mode":"joystick"} или {"mode":"manual"}
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
    
    std::cout << "🔍 Запрос: " << method << " " << path << std::endl;
    
    // Читаем тело запроса (если есть)
    std::string body;
    size_t bodyPos = request.find("\r\n\r\n");
    if (bodyPos != std::string::npos) {
        body = request.substr(bodyPos + 4);
    }
    
    if (path == "/" || path == "/index.html") {
        std::string html = R"(<!DOCTYPE html>
<html><head><title>CRSF API Server</title></head>
<body>
<h1>CRSF API Server</h1>
<p>API сервер для передачи команд на ведомый узел</p>
<p>Целевой узел: )" + targetHost + ":" + std::to_string(targetPort) + R"(</p>
<p>Доступные endpoints:</p>
<ul>
<li>POST /api/command/setChannel - установка одного канала</li>
<li>POST /api/command/setChannels - установка всех каналов</li>
<li>POST /api/command/sendChannels - отправка каналов</li>
<li>POST /api/command/setMode - установка режима</li>
<li>POST /api/telemetry - приём телеметрии от интерпретатора</li>
<li>GET /api/telemetry - получение последней телеметрии</li>
</ul>
</body></html>)";
        sendHttpResponse(clientSocket, html, "text/html");
    } else if (path == "/api/telemetry" && method == "POST") {
        // Приём телеметрии от интерпретатора
        std::cout << "📥 Получена телеметрия: " << body.length() << " байт" << std::endl;
        std::lock_guard<std::mutex> lock(telemetryMutex);
        lastTelemetryJson = body;
        std::cout << "✅ Телеметрия сохранена" << std::endl;
        sendHttpResponse(clientSocket, "{\"status\":\"ok\",\"message\":\"Telemetry received\"}");
    } else if (path == "/api/telemetry" && method == "GET") {
        // Отдача телеметрии клиенту
        std::lock_guard<std::mutex> lock(telemetryMutex);
        sendHttpResponse(clientSocket, lastTelemetryJson);
    } else if (path.find("/api/command/") == 0) {
        // Извлекаем имя команды из пути
        std::string command = path.substr(13); // длина "/api/command/" = 13
        
        bool success = false;
        std::string responseJson;
        
        if (command == "setChannel") {
            unsigned int channel;
            int value;
            if (parseSetChannel(body, channel, value)) {
                // Формируем команду для интерпретатора
                std::stringstream cmdBody;
                cmdBody << "{\"command\":\"setChannel\",\"channel\":" << channel << ",\"value\":" << value << "}";
                success = sendCommandToTarget("setChannel", cmdBody.str());
            } else {
                responseJson = "{\"status\":\"error\",\"message\":\"Invalid JSON format\"}";
            }
        } else if (command == "setChannels") {
            std::string channelsStr;
            if (parseSetChannels(body, channelsStr)) {
                // Отправляем команду в формате, который понимает интерпретатор
                std::stringstream cmdBody;
                cmdBody << "{\"command\":\"setChannels\",\"channelsStr\":\"" << channelsStr << "\"}";
                success = sendCommandToTarget("setChannels", cmdBody.str());
            } else {
                responseJson = "{\"status\":\"error\",\"message\":\"Invalid channels array\"}";
            }
        } else if (command == "sendChannels") {
            success = sendCommandToTarget("sendChannels", "{\"command\":\"sendChannels\"}");
        } else if (command == "setMode") {
            std::string mode;
            if (parseSetMode(body, mode)) {
                std::stringstream cmdBody;
                cmdBody << "{\"command\":\"setMode\",\"mode\":\"" << mode << "\"}";
                success = sendCommandToTarget("setMode", cmdBody.str());
            } else {
                responseJson = "{\"status\":\"error\",\"message\":\"Invalid mode\"}";
            }
        } else {
            responseJson = "{\"status\":\"error\",\"message\":\"Unknown command\"}";
        }
        
        if (responseJson.empty()) {
            if (success) {
                responseJson = "{\"status\":\"ok\",\"message\":\"Command sent to target\"}";
            } else {
                responseJson = "{\"status\":\"error\",\"message\":\"Failed to send command to target\"}";
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

// Основная функция API сервера
void startApiServer(int port, const std::string& host, int targetPortNum) {
    std::lock_guard<std::mutex> lock(serverMutex);
    
    if (serverRunning) {
        std::cout << "⚠️ API сервер уже запущен" << std::endl;
        return;
    }
    
    targetHost = host;
    targetPort = targetPortNum;
    
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
    
    serverRunning = true;
    std::cout << "🌐 API сервер запущен на порту " << port << std::endl;
    std::cout << "📡 Целевой узел: " << targetHost << ":" << targetPort << std::endl;
    
    // Основной цикл сервера
    while (serverRunning) {
        struct sockaddr_in clientAddr;
        socklen_t clientAddrLen = sizeof(clientAddr);
        
        int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);
        if (clientSocket < 0) {
            if (serverRunning) {
                continue;
            } else {
                break;
            }
        }
        
        std::thread clientThread(handleClient, clientSocket);
        clientThread.detach();
    }
    
    close(serverSocket);
    serverRunning = false;
}

// Остановка API сервера
void stopApiServer() {
    std::lock_guard<std::mutex> lock(serverMutex);
    if (serverRunning) {
        serverRunning = false;
        if (serverSocket >= 0) {
            close(serverSocket);
            serverSocket = -1;
        }
    }
}

// Главная функция для запуска API сервера как отдельного приложения
int main(int argc, char* argv[]) {
    int port = 8081;
    std::string targetHost = "localhost";
    int targetPort = 8082;
    
    // Парсинг аргументов командной строки
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--notel") {
            g_ignore_telemetry = true;
            std::cout << "[INFO] Running in NO-TELEMETRY mode. Safety checks disabled." << std::endl;
        } else if (i == 1 && arg.find_first_not_of("0123456789") == std::string::npos) {
            port = std::stoi(arg);
        } else if (i == 2) {
            targetHost = arg;
        } else if (i == 3 && arg.find_first_not_of("0123456789") == std::string::npos) {
            targetPort = std::stoi(arg);
        }
    }
    
    std::cout << "🚀 Запуск CRSF API сервера..." << std::endl;
    std::cout << "📡 Порт сервера: " << port << std::endl;
    std::cout << "🎯 Целевой узел: " << targetHost << ":" << targetPort << std::endl;
    
    // Запускаем сервер (блокирующий вызов)
    startApiServer(port, targetHost, targetPort);
    
    return 0;
}
