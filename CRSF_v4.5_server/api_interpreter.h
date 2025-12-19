#ifndef API_INTERPRETER_H
#define API_INTERPRETER_H

#include <string>

// Запуск API интерпретатора
// Получает команды через HTTP API и записывает их в /tmp/crsf_command.txt
// Отправляет телеметрию на API сервер
// port - порт для прослушивания входящих команд от API сервера
// apiServerHost - адрес API сервера (например, "192.168.1.100")
// apiServerPort - порт API сервера (по умолчанию 8081)
void startApiInterpreter(int port = 8082, const std::string& apiServerHost = "localhost", int apiServerPort = 8081);

// Остановка API интерпретатора
void stopApiInterpreter();

#endif // API_INTERPRETER_H
