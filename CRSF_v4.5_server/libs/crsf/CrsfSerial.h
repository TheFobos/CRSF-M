#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <atomic>
#include "crc8.h"
#include "crsf_protocol.h"
#include "../SerialPort.h"
#include "../rpi_hal.h"

//БЕСПОЛЕЗНО: enum определен, но нигде не используется
//enum eFailsafeAction { fsaNoPulses, fsaHold };

// Реализация CRSF поверх SerialPort (Raspberry Pi)
class CrsfSerial
{
public:
// Packet timeout where buffer is flushed if no data is received in this time
static const unsigned int CRSF_PACKET_TIMEOUT_MS = 100;
static const unsigned int CRSF_FAILSAFE_STAGE1_MS = 120000;  // 2 минуты вместо 60 секунд для стабильной работы
uint32_t _lastReceive; // время последнего приёма (мс), rpi_millis()

// Конструктор: принимает ссылку на SerialPort и скорость
CrsfSerial(SerialPort& port, uint32_t baud = CRSF_BAUDRATE);
void loop();
void write(uint8_t b);
void write(const uint8_t* buf, size_t len);
void queuePacket(uint8_t addr, uint8_t type, const void* payload, uint8_t len);

// Return current channel value (1-based) in us
int getChannel(unsigned int ch) const
{
    if (ch >= 1 && ch <= CRSF_NUM_CHANNELS) {
        std::lock_guard<std::mutex> lock(_channelsMutex);
        return _channels[ch - 1];
    }
    return 1500; // Safe default value for invalid channel
}

void setChannel(unsigned int ch, int value)
{
    if (ch >= 1 && ch <= CRSF_NUM_CHANNELS) {
        std::lock_guard<std::mutex> lock(_channelsMutex);
        _channels[ch - 1] = value;
        _needSendPacket = true; // Флаг для асинхронной отправки
    }
}

    const crsfLinkStatistics_t* getLinkStatistics() const { return &_linkStatistics; }
    const crsf_sensor_gps_t* getGpsSensor() const { return &_gpsSensor; }
    
    // Методы для получения данных батареи
    double getBatteryVoltage() const { return _batteryVoltage; }
    double getBatteryCurrent() const { return _batteryCurrent; }
    double getBatteryCapacity() const { return _batteryCapacity; }
    uint8_t getBatteryRemaining() const { return _batteryRemaining; }
    
    // Методы для получения данных положения
    double getAttitudeRoll() const { return _attitudeRoll; }
    double getAttitudePitch() const { return _attitudePitch; }
    double getAttitudeYaw() const { return _attitudeYaw; }
    
    // Методы для получения сырых значений attitude
    // ВНИМАНИЕ: _rawAttitudeBytes[0]=Pitch, [1]=Roll, [2]=Yaw (поменяны местами!)
    int16_t getRawAttitudeRoll() const { return _rawAttitudeBytes[1]; }   // bytes 2-3
    int16_t getRawAttitudePitch() const { return _rawAttitudeBytes[0]; }  // bytes 0-1
    int16_t getRawAttitudeYaw() const { return _rawAttitudeBytes[2]; }
    
    bool isLinkUp() const { return _linkIsUp; }
    //БЕСПОЛЕЗНО: функции определены, но нигде не вызываются
    //bool getPassthroughMode() const { return _passthroughMode; }
    //void setPassthroughMode(bool val, unsigned int baud = 0);

    // Event Handlers
    void (*onLinkUp)();
    void (*onLinkDown)();
    void (*onPacketChannels)();
    //БЕСПОЛЕЗНО: указатели на функции устанавливаются, но никогда не вызываются
    //void (*onShiftyByte)(uint8_t b);
    //void (*onPacketLinkStatistics)(crsfLinkStatistics_t* ls);
    //void (*onPacketGps)(crsf_sensor_gps_t* gpsSensor);

    // Асинхронная обработка отправки (вызывать из основного цикла)
    void processSend();
    void packetAttitude(const crsf_header_t* p);
    void packetFlightMode(const crsf_header_t* p);
    void packetBatterySensor(const crsf_header_t* p);
private:
    SerialPort& _port;
    uint8_t _rxBuf[CRSF_MAX_PACKET_SIZE];
    uint8_t _rxBufPos;
    Crc8 _crc;
    crsfLinkStatistics_t _linkStatistics;
    crsf_sensor_gps_t _gpsSensor;
    
    // Данные батареи
    double _batteryVoltage;
    double _batteryCurrent;
    double _batteryCapacity;
    uint8_t _batteryRemaining;
    
    // Данные положения
    double _attitudeRoll;
    double _attitudePitch;
    double _attitudeYaw;
    
    // Сырые значения attitude (raw int16_t из CRSF пакета)
    int16_t _rawAttitudeBytes[3];  // [0]=pitch, [1]=roll, [2]=yaw (порядок изменен!)
    
    uint32_t _baud;
    uint32_t _lastChannelsPacket;
    bool _linkIsUp;
    int _channels[CRSF_NUM_CHANNELS];
    
    // Мьютекс для защиты данных каналов и флаг для асинхронной отправки
    mutable std::mutex _channelsMutex;
    std::atomic<bool> _needSendPacket{false};

    void handleSerialIn();
    void handleByteReceived();
    void shiftRxBuffer(uint8_t cnt);
    void processPacketIn(uint8_t len);
    void checkPacketTimeout();
    void checkLinkDown();

    // Packet Handlers
    void packetChannelsPacked(const crsf_header_t* p);
    void packetLinkStatistics(const crsf_header_t* p);
    void packetGps(const crsf_header_t* p);
    
    // Приватный метод для реальной отправки (вызывается из processSend)
    void packetChannelsSend();
};
