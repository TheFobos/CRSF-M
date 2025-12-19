#include "SerialPort.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/serial.h>
#include <asm/termbits.h>
#include <cerrno>
#include <cstring>

// Реализация SerialPort для Linux с termios2

SerialPort::SerialPort(const std::string &path, uint32_t baud)
    : _path(path), _baud(baud), _fd(-1) {}

SerialPort::~SerialPort() { close(); }

bool SerialPort::open() {
    if (_fd >= 0) return true;
    _fd = ::open(_path.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (_fd < 0) {
        return false; // не удалось открыть устройство
    }

    // ВАЖНО: Сначала настраиваем termios2 с таймаутами
    if (!configureTermios2(_baud)) {
        close();
        return false;
    }

    // ВАЖНО: Сбрасываем флаг O_NONBLOCK ПОСЛЕ настройки termios2
    // Это позволяет использовать таймауты VMIN/VTIME для неблокирующего чтения
    int flags = fcntl(_fd, F_GETFL, 0);
    if (flags >= 0) fcntl(_fd, F_SETFL, flags & ~O_NONBLOCK);

    return true;
}

void SerialPort::close() {
    if (_fd >= 0) {
        ::close(_fd);
        _fd = -1;
    }
}

bool SerialPort::configureTermios2(uint32_t baud) {
    // Используем termios2 для установки произвольной скорости
    struct termios2 tio2;
    if (ioctl(_fd, TCGETS2, &tio2) < 0) return false;

    tio2.c_cflag &= ~CBAUD;
    tio2.c_cflag |= BOTHER | CS8 | CREAD | CLOCAL;
    tio2.c_cflag &= ~(PARENB | CSTOPB);
    tio2.c_iflag = IGNPAR;
    tio2.c_oflag = 0;
    tio2.c_lflag = 0;
    tio2.c_ispeed = baud;
    tio2.c_ospeed = baud;

    // ВАЖНО: Отключаем блокировку чтения!
    // VMIN=0: читать, даже если 0 байт пришло (не ждать минимум 1 байт)
    // VTIME=1: ждать максимум 0.1 сек (1 децисекунда) перед возвратом
    // Без этих настроек read() будет блокироваться навечно, если нет данных
    // и заблокирует API сервер, который пытается записать в порт
    tio2.c_cc[VMIN] = 0;   // Читать, даже если 0 байт пришло
    tio2.c_cc[VTIME] = 1;  // Ждать максимум 0.1 сек (1 децисекунда)

    if (ioctl(_fd, TCSETS2, &tio2) < 0) return false;

    // Сбрасываем буферы через ioctl
    ioctl(_fd, TCFLSH, TCIOFLUSH);
    return true;
}

int SerialPort::readByte(uint8_t &b) {
    uint8_t tmp;
    // С настройками VMIN=0 и VTIME=1, read() вернет:
    // - 1: если байт прочитан
    // - 0: если таймаут (0.1 сек) и нет данных (не EOF!)
    // - -1: если ошибка
    int r = ::read(_fd, &tmp, 1);
    if (r == 1) { b = tmp; return 1; }
    // Возвращаем 0 при таймауте (это нормально, не ошибка)
    // Это позволяет основному циклу "дышать" и не блокировать API
    return r;
}

//БЕСПОЛЕЗНО: функция определена, но нигде не используется (используется только readByte)
/*
int SerialPort::read(uint8_t *buf, size_t len) {
    return ::read(_fd, buf, len);
}
*/

int SerialPort::write(const uint8_t *buf, size_t len) {
    return ::write(_fd, buf, len);
}

int SerialPort::writeByte(uint8_t b) {
    return ::write(_fd, &b, 1);
}

void SerialPort::flush() {
    // Простая очистка буферов ввода/вывода
    ioctl(_fd, TCFLSH, TCIOFLUSH);
}


