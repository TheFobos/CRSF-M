#pragma once

#include <gmock/gmock.h>
#include "../../libs/SerialPort.h"

class MockSerialPort : public SerialPort {
public:
    MockSerialPort() : SerialPort("/dev/ttyTEST", 420000) {}
    
    MOCK_METHOD(bool, open, (), (override));
    MOCK_METHOD(void, close, (), (override));
    MOCK_METHOD(int, readByte, (uint8_t& b), (override));
    MOCK_METHOD(int, write, (const uint8_t* buf, size_t len), (override));
    MOCK_METHOD(int, writeByte, (uint8_t b), (override));
    MOCK_METHOD(void, flush, (), (override));
};

