#pragma once

// Класс SerialPort для Linux (Raspberry Pi 5) с поддержкой нестандартных скоростей (в т.ч. 420000 бод)
// Использует termios2 (ioctl TCGETS2/TCSETS2)

#include <cstdint>
#include <cstddef>
#include <string>

class SerialPort {
public:
    // Конструктор: path — например "/dev/ttyAMA0" или "/dev/ttyS0"
    SerialPort(const std::string &path, uint32_t baud);
    virtual ~SerialPort();

    bool isOpen() const { return _fd >= 0; }
    virtual bool open();
    virtual void close();

    // Неблокирующее чтение/запись (по умолчанию блокирующее с таймаутами через termios)
    virtual int readByte(uint8_t &b);
    virtual int write(const uint8_t *buf, size_t len);
    virtual int writeByte(uint8_t b);

    virtual void flush();
    
    // Получить файловый дескриптор (для неблокирующих операций)
    int getFd() const { return _fd; }

private:
    std::string _path;
    uint32_t _baud;
    int _fd;
    bool configureTermios2(uint32_t baud);
};


