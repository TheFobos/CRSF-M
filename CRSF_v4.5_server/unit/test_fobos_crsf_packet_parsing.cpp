/**
 * @file test_fobos_crsf_packet_parsing.cpp
 * @brief Unit тесты для парсинга CRSF пакетов
 * 
 * Тесты проверяют парсинг различных типов CRSF пакетов:
 * - RC Channels Packed
 * - GPS
 * - Battery Sensor
 * - Link Statistics
 * - Attitude
 * - Flight Mode
 * - Невалидные пакеты (неверный CRC, длина, адрес)
 * 
 * @version 4.3
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include <cstring>
#include "../libs/crsf/CrsfSerial.h"
#include "../libs/crsf/crsf_protocol.h"
#include "mocks/MockSerialPort.h"

using ::testing::_;
using ::testing::Return;
using ::testing::InSequence;
using ::testing::DoAll;

/**
 * @class CrsfPacketParsingTest
 * @brief Фикстура для тестов парсинга пакетов
 */
class CrsfPacketParsingTest : public ::testing::Test {
protected:
    void SetUp() override {
        mockSerial = std::make_unique<MockSerialPort>();
        crsf = std::make_unique<CrsfSerial>(*mockSerial, 420000);
    }
    
    // Вспомогательная функция для создания валидного пакета
    void createValidPacket(uint8_t* buffer, uint8_t addr, uint8_t type, 
                          const uint8_t* payload, uint8_t payloadLen, uint8_t& totalLen) {
        Crc8 crc(0xD5);
        buffer[0] = addr;
        buffer[1] = payloadLen + 2; // TYPE + PAYLOAD + CRC
        buffer[2] = type;
        memcpy(&buffer[3], payload, payloadLen);
        
        // Вычисляем CRC для TYPE + PAYLOAD
        uint8_t crcData[payloadLen + 1];
        crcData[0] = type;
        memcpy(&crcData[1], payload, payloadLen);
        buffer[3 + payloadLen] = crc.calc(crcData, payloadLen + 1);
        
        totalLen = 4 + payloadLen; // ADDR + LEN + TYPE + PAYLOAD + CRC
    }
    
    std::unique_ptr<MockSerialPort> mockSerial;
    std::unique_ptr<CrsfSerial> crsf;
};

/**
 * @test Парсинг пакета RC Channels Packed
 * 
 * Тест проверяет корректный парсинг пакета с данными каналов.
 * После парсинга каналы должны быть обновлены.
 */
TEST_F(CrsfPacketParsingTest, ParsePacket_RcChannelsPacked_UpdatesChannels) {
    // Создаем пакет с данными каналов
    uint8_t packet[64];
    uint8_t payload[22] = {0}; // 16 каналов по 11 бит = 22 байта
    
    // Устанавливаем канал 0 в значение, соответствующее 1500 мкс
    // CRSF_CHANNEL_VALUE_MID = 992
    // Для простоты заполняем нулями (все каналы в минимуме)
    uint8_t totalLen;
    createValidPacket(packet, CRSF_ADDRESS_FLIGHT_CONTROLLER, 
                     CRSF_FRAMETYPE_RC_CHANNELS_PACKED, payload, 22, totalLen);
    
    // Симулируем чтение пакета по байтам
    InSequence seq;
    for (uint8_t i = 0; i < totalLen; i++) {
        EXPECT_CALL(*mockSerial, readByte(_))
            .WillOnce(DoAll(::testing::SetArgReferee<0>(packet[i]), Return(1)));
    }
    EXPECT_CALL(*mockSerial, readByte(_))
        .WillRepeatedly(Return(0)); // Нет больше данных
    
    // Вызываем loop() для обработки пакета
    crsf->loop();
    
    // Проверяем, что связь установлена
    EXPECT_TRUE(crsf->isLinkUp());
}

/**
 * @test Парсинг пакета GPS
 * 
 * Тест проверяет корректный парсинг GPS пакета.
 */
TEST_F(CrsfPacketParsingTest, ParsePacket_Gps_UpdatesGpsData) {
    uint8_t packet[64];
    uint8_t payload[15] = {0};
    
    // Заполняем GPS данные (big endian)
    // latitude: 0x12345678 (пример)
    payload[0] = 0x12; payload[1] = 0x34; payload[2] = 0x56; payload[3] = 0x78;
    // longitude: 0x9ABCDEF0
    payload[4] = 0x9A; payload[5] = 0xBC; payload[6] = 0xDE; payload[7] = 0xF0;
    // groundspeed, heading, altitude, satellites
    payload[8] = 0x00; payload[9] = 0x64; // groundspeed
    payload[10] = 0x01; payload[11] = 0x2C; // heading
    payload[12] = 0x03; payload[13] = 0xE8; // altitude
    payload[14] = 10; // satellites
    
    uint8_t totalLen;
    createValidPacket(packet, CRSF_ADDRESS_FLIGHT_CONTROLLER, 
                     CRSF_FRAMETYPE_GPS, payload, 15, totalLen);
    
    InSequence seq;
    for (uint8_t i = 0; i < totalLen; i++) {
        EXPECT_CALL(*mockSerial, readByte(_))
            .WillOnce(DoAll(::testing::SetArgReferee<0>(packet[i]), Return(1)));
    }
    EXPECT_CALL(*mockSerial, readByte(_))
        .WillRepeatedly(Return(0));
    
    crsf->loop();
    
    // Проверяем, что GPS данные доступны
    const crsf_sensor_gps_t* gps = crsf->getGpsSensor();
    EXPECT_NE(gps, nullptr);
}

/**
 * @test Парсинг пакета Battery Sensor
 * 
 * Тест проверяет корректный парсинг пакета батареи.
 */
TEST_F(CrsfPacketParsingTest, ParsePacket_BatterySensor_UpdatesBatteryData) {
    uint8_t packet[64];
    uint8_t payload[8] = {0};
    
    // voltage: 12500 мВ = 12.5V (big endian)
    payload[0] = 0x30; payload[1] = 0xD4;
    // current: 5000 мА (big endian)
    payload[2] = 0x13; payload[3] = 0x88;
    // capacity: 1000 мАч (24-bit)
    payload[4] = 0x00; payload[5] = 0x03; payload[6] = 0xE8;
    // remaining: 80%
    payload[7] = 80;
    
    uint8_t totalLen;
    createValidPacket(packet, CRSF_ADDRESS_FLIGHT_CONTROLLER, 
                     CRSF_FRAMETYPE_BATTERY_SENSOR, payload, 8, totalLen);
    
    InSequence seq;
    for (uint8_t i = 0; i < totalLen; i++) {
        EXPECT_CALL(*mockSerial, readByte(_))
            .WillOnce(DoAll(::testing::SetArgReferee<0>(packet[i]), Return(1)));
    }
    EXPECT_CALL(*mockSerial, readByte(_))
        .WillRepeatedly(Return(0));
    
    crsf->loop();
    
    // Проверяем данные батареи
    EXPECT_GT(crsf->getBatteryVoltage(), 0.0);
    EXPECT_GT(crsf->getBatteryCurrent(), 0.0);
}

/**
 * @test Парсинг пакета Link Statistics
 * 
 * Тест проверяет корректный парсинг пакета статистики связи.
 */
TEST_F(CrsfPacketParsingTest, ParsePacket_LinkStatistics_UpdatesLinkStats) {
    uint8_t packet[64];
    uint8_t payload[10] = {0};
    
    // Заполняем статистику связи
    payload[0] = 100; // uplink_RSSI_1
    payload[1] = 95;  // uplink_RSSI_2
    payload[2] = 90;  // uplink_Link_quality
    payload[3] = 10;  // uplink_SNR
    payload[4] = 0;   // active_antenna
    payload[5] = 0;   // rf_Mode
    payload[6] = 0;   // uplink_TX_Power
    payload[7] = 85;  // downlink_RSSI
    payload[8] = 80;  // downlink_Link_quality
    payload[9] = 5;   // downlink_SNR
    
    uint8_t totalLen;
    createValidPacket(packet, CRSF_ADDRESS_FLIGHT_CONTROLLER, 
                     CRSF_FRAMETYPE_LINK_STATISTICS, payload, 10, totalLen);
    
    InSequence seq;
    for (uint8_t i = 0; i < totalLen; i++) {
        EXPECT_CALL(*mockSerial, readByte(_))
            .WillOnce(DoAll(::testing::SetArgReferee<0>(packet[i]), Return(1)));
    }
    EXPECT_CALL(*mockSerial, readByte(_))
        .WillRepeatedly(Return(0));
    
    crsf->loop();
    
    // Проверяем статистику связи
    const crsfLinkStatistics_t* stats = crsf->getLinkStatistics();
    EXPECT_NE(stats, nullptr);
    EXPECT_EQ(stats->uplink_RSSI_1, 100);
}

/**
 * @test Парсинг пакета Attitude
 * 
 * Тест проверяет корректный парсинг пакета attitude.
 */
TEST_F(CrsfPacketParsingTest, ParsePacket_Attitude_UpdatesAttitudeData) {
    uint8_t packet[64];
    uint8_t payload[6] = {0};
    
    // Pitch, Roll, Yaw (big endian int16_t)
    // Pitch: 0 (центр)
    payload[0] = 0x00; payload[1] = 0x00;
    // Roll: 1750 (10 градусов * 175)
    payload[2] = 0x06; payload[3] = 0xD6;
    // Yaw: 3500 (20 градусов * 175)
    payload[4] = 0x0D; payload[5] = 0xAC;
    
    uint8_t totalLen;
    createValidPacket(packet, CRSF_ADDRESS_FLIGHT_CONTROLLER, 
                     CRSF_FRAMETYPE_ATTITUDE, payload, 6, totalLen);
    
    InSequence seq;
    for (uint8_t i = 0; i < totalLen; i++) {
        EXPECT_CALL(*mockSerial, readByte(_))
            .WillOnce(DoAll(::testing::SetArgReferee<0>(packet[i]), Return(1)));
    }
    EXPECT_CALL(*mockSerial, readByte(_))
        .WillRepeatedly(Return(0));
    
    crsf->loop();
    
    // Проверяем attitude данные
    EXPECT_NEAR(crsf->getAttitudeRoll(), 10.0, 0.1);
    EXPECT_NEAR(crsf->getAttitudePitch(), 0.0, 0.1);
    EXPECT_NEAR(crsf->getAttitudeYaw(), 20.0, 0.1);
}

/**
 * @test Отклонение пакета с неверным CRC
 * 
 * Тест проверяет, что пакет с неверным CRC отклоняется
 * и не обрабатывается.
 */
TEST_F(CrsfPacketParsingTest, ParsePacket_InvalidCrc_RejectsPacket) {
    uint8_t packet[64];
    uint8_t payload[22] = {0};
    
    // Создаем валидный пакет
    uint8_t totalLen;
    createValidPacket(packet, CRSF_ADDRESS_FLIGHT_CONTROLLER, 
                     CRSF_FRAMETYPE_RC_CHANNELS_PACKED, payload, 22, totalLen);
    
    // Портим CRC
    packet[totalLen - 1] ^= 0xFF;
    
    InSequence seq;
    for (uint8_t i = 0; i < totalLen; i++) {
        EXPECT_CALL(*mockSerial, readByte(_))
            .WillOnce(DoAll(::testing::SetArgReferee<0>(packet[i]), Return(1)));
    }
    EXPECT_CALL(*mockSerial, readByte(_))
        .WillRepeatedly(Return(0));
    
    bool linkWasUp = crsf->isLinkUp();
    crsf->loop();
    
    // Связь не должна установиться из-за неверного CRC
    // (если она не была установлена ранее)
    if (!linkWasUp) {
        EXPECT_FALSE(crsf->isLinkUp());
    }
}

/**
 * @test Отклонение пакета с неверным адресом
 * 
 * Тест проверяет, что пакет с адресом не FLIGHT_CONTROLLER
 * игнорируется.
 */
TEST_F(CrsfPacketParsingTest, ParsePacket_WrongAddress_IgnoresPacket) {
    uint8_t packet[64];
    uint8_t payload[22] = {0};
    
    uint8_t totalLen;
    // Используем неправильный адрес
    createValidPacket(packet, CRSF_ADDRESS_RADIO_TRANSMITTER, 
                     CRSF_FRAMETYPE_RC_CHANNELS_PACKED, payload, 22, totalLen);
    
    InSequence seq;
    for (uint8_t i = 0; i < totalLen; i++) {
        EXPECT_CALL(*mockSerial, readByte(_))
            .WillOnce(DoAll(::testing::SetArgReferee<0>(packet[i]), Return(1)));
    }
    EXPECT_CALL(*mockSerial, readByte(_))
        .WillRepeatedly(Return(0));
    
    bool linkWasUp = crsf->isLinkUp();
    crsf->loop();
    
    // Связь не должна установиться
    if (!linkWasUp) {
        EXPECT_FALSE(crsf->isLinkUp());
    }
}

/**
 * @test Отклонение пакета с неверной длиной
 * 
 * Тест проверяет, что пакет с длиной вне допустимого диапазона
 * отклоняется.
 */
TEST_F(CrsfPacketParsingTest, ParsePacket_InvalidLength_RejectsPacket) {
    uint8_t packet[64];
    
    // Создаем пакет с неверной длиной (слишком маленькой)
    packet[0] = CRSF_ADDRESS_FLIGHT_CONTROLLER;
    packet[1] = 2; // Минимальная длина должна быть 3 (TYPE + минимум 1 байт данных + CRC)
    packet[2] = CRSF_FRAMETYPE_RC_CHANNELS_PACKED;
    
    InSequence seq;
    for (int i = 0; i < 3; i++) {
        EXPECT_CALL(*mockSerial, readByte(_))
            .WillOnce(DoAll(::testing::SetArgReferee<0>(packet[i]), Return(1)));
    }
    EXPECT_CALL(*mockSerial, readByte(_))
        .WillRepeatedly(Return(0));
    
    crsf->loop();
    
    // Пакет должен быть отклонен
    // (проверяем, что нет краша)
    EXPECT_NO_THROW(crsf->loop());
}

/**
 * @test Парсинг пакета Flight Mode
 * 
 * Тест проверяет корректный парсинг пакета режима полета.
 */
TEST_F(CrsfPacketParsingTest, ParsePacket_FlightMode_ProcessesPacket) {
    uint8_t packet[64];
    const char* modeStr = "ANGLE";
    uint8_t payloadLen = strlen(modeStr) + 1; // +1 для CRC в frame_size
    uint8_t payload[32];
    memcpy(payload, modeStr, strlen(modeStr));
    
    uint8_t totalLen;
    createValidPacket(packet, CRSF_ADDRESS_FLIGHT_CONTROLLER, 
                     CRSF_FRAMETYPE_FLIGHT_MODE, payload, payloadLen, totalLen);
    
    InSequence seq;
    for (uint8_t i = 0; i < totalLen; i++) {
        EXPECT_CALL(*mockSerial, readByte(_))
            .WillOnce(DoAll(::testing::SetArgReferee<0>(packet[i]), Return(1)));
    }
    EXPECT_CALL(*mockSerial, readByte(_))
        .WillRepeatedly(Return(0));
    
    // Пакет должен быть обработан без ошибок
    EXPECT_NO_THROW(crsf->loop());
}

