#ifndef API_SERVER_H
#define API_SERVER_H

#include <string>

// Запуск API сервера
// Принимает команды и отправляет их на ведомый узел через HTTP API
// port - порт для прослушивания входящих команд
// targetHost - адрес ведомого узла (например, "192.168.1.100")
// targetPort - порт API интерпретатора на ведомом узле
void startApiServer(int port = 8081, const std::string& targetHost = "localhost", int targetPort = 8082);

// Остановка API сервера
void stopApiServer();

#endif // API_SERVER_H
