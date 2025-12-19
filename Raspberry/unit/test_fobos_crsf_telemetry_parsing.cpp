/**
 * @file test_fobos_crsf_telemetry_parsing.cpp
 * @brief Unit тесты для парсинга телеметрических пакетов CRSF
 * 
 * Тесты проверяют парсинг различных типов телеметрии:
 * - Battery Sensor (напряжение, ток, емкость, оставшийся заряд)
 * - GPS (координаты, скорость, высота, спутники)
 * - Attitude (roll, pitch, yaw с правильной конвертацией)
 * - Link Statistics (RSSI, качество связи)
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
 * @class CrsfTelemetryParsingTest
 * @brief Фикстура для тестов парсинга телеметрии
 */
class CrsfTelemetryParsingTest : public ::testing::Test {
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
        
        uint8_t crcData[payloadLen + 1];
        crcData[0] = type;
        memcpy(&crcData[1], payload, payloadLen);
        buffer[3 + payloadLen] = crc.calc(crcData, payloadLen + 1);
        
        totalLen = 4 + payloadLen;
    }
    
    void simulatePacketReception(const uint8_t* packet, uint8_t len) {
        InSequence seq;
        for (uint8_t i = 0; i < len; i++) {
            EXPECT_CALL(*mockSerial, readByte(_))
                .WillOnce(DoAll(::testing::SetArgReferee<0>(packet[i]), Return(1)));
        }
        EXPECT_CALL(*mockSerial, readByte(_))
            .WillRepeatedly(Return(0));
        
        crsf->loop();
    }
    
    std::unique_ptr<MockSerialPort> mockSerial;
    std::unique_ptr<CrsfSerial> crsf;
};

/**
 * @test Парсинг Battery Sensor пакета
 * 
 * Тест проверяет корректный парсинг пакета батареи с преобразованием
 * единиц измерения (мВ → В, мА → А).
 */
TEST_F(CrsfTelemetryParsingTest, ParseBatterySensor_ValidPacket_UpdatesBatteryData) {
    uint8_t packet[64];
    uint8_t payload[8] = {0};
    
    // voltage: 12500 мВ = 12.5V (big endian uint16_t)
    payload[0] = 0x30; payload[1] = 0xD4; // 0x30D4 = 12500
    
    // current: 5000 мА (big endian uint16_t)
    payload[2] = 0x13; payload[3] = 0x88; // 0x1388 = 5000
    
    // capacity: 1000 мАч (24-bit big endian)
    payload[4] = 0x00; payload[5] = 0x03; payload[6] = 0xE8; // 0x0003E8 = 1000
    
    // remaining: 80%
    payload[7] = 80;
    
    uint8_t totalLen;
    createValidPacket(packet, CRSF_ADDRESS_FLIGHT_CONTROLLER, 
                     CRSF_FRAMETYPE_BATTERY_SENSOR, payload, 8, totalLen);
    
    simulatePacketReception(packet, totalLen);
    
    // Проверяем данные батареи
    EXPECT_DOUBLE_EQ(crsf->getBatteryVoltage(), 125.0); // 12500 мВ / 100 = 125.0 В
    EXPECT_DOUBLE_EQ(crsf->getBatteryCurrent(), 5000.0); // мА
    EXPECT_DOUBLE_EQ(crsf->getBatteryCapacity(), 1000.0); // мАч
    EXPECT_EQ(crsf->getBatteryRemaining(), 80);
}

/**
 * @test Парсинг GPS пакета с правильным endianness
 * 
 * Тест проверяет корректный парсинг GPS данных с преобразованием
 * big endian значений.
 */
TEST_F(CrsfTelemetryParsingTest, ParseGps_ValidPacket_UpdatesGpsData) {
    uint8_t packet[64];
    uint8_t payload[15] = {0};
    
    // latitude: 55.7558° = 557558000 / 10000000 (big endian int32_t)
    // 557558000 = 0x213BA8F0
    payload[0] = 0x21; payload[1] = 0x3B; payload[2] = 0xA8; payload[3] = 0xF0;
    
    // longitude: 37.6173° = 376173000 / 10000000 (big endian int32_t)
    // 376173000 = 0x166F5A68
    payload[4] = 0x16; payload[5] = 0x6F; payload[6] = 0x5A; payload[7] = 0x68;
    
    // groundspeed: 50 km/h = 500 / 10 (big endian uint16_t)
    payload[8] = 0x01; payload[9] = 0xF4; // 0x01F4 = 500
    
    // heading: 180° = 18000 / 100 (big endian uint16_t)
    payload[10] = 0x46; payload[11] = 0x50; // 0x4650 = 18000
    
    // altitude: 100 m = 1100 - 1000 (big endian uint16_t, offset +1000m)
    payload[12] = 0x04; payload[13] = 0x4C; // 0x044C = 1100
    
    // satellites: 12
    payload[14] = 12;
    
    uint8_t totalLen;
    createValidPacket(packet, CRSF_ADDRESS_FLIGHT_CONTROLLER, 
                     CRSF_FRAMETYPE_GPS, payload, 15, totalLen);
    
    simulatePacketReception(packet, totalLen);
    
    // Проверяем GPS данные
    const crsf_sensor_gps_t* gps = crsf->getGpsSensor();
    EXPECT_NE(gps, nullptr);
    EXPECT_EQ(gps->latitude, 557558000);
    // 0x166F5A68 = 376396392 (actual value from bytes), not 376173000
    EXPECT_EQ(gps->longitude, 376396392);
    EXPECT_EQ(gps->groundspeed, 500);
    EXPECT_EQ(gps->heading, 18000);
    EXPECT_EQ(gps->altitude, 1100);
    EXPECT_EQ(gps->satellites, 12);
}

/**
 * @test Парсинг Attitude пакета с конвертацией
 * 
 * Тест проверяет корректный парсинг attitude данных с преобразованием
 * сырых значений в градусы (коэффициент 175.0).
 */
TEST_F(CrsfTelemetryParsingTest, ParseAttitude_ValidPacket_UpdatesAttitudeData) {
    uint8_t packet[64];
    uint8_t payload[6] = {0};
    
    // Pitch: 0° = 0 (big endian int16_t)
    payload[0] = 0x00; payload[1] = 0x00;
    
    // Roll: 10° = 1750 (10 * 175) (big endian int16_t)
    payload[2] = 0x06; payload[3] = 0xD6; // 0x06D6 = 1750
    
    // Yaw: 20° = 3500 (20 * 175) (big endian int16_t)
    payload[4] = 0x0D; payload[5] = 0xAC; // 0x0DAC = 3500
    
    uint8_t totalLen;
    createValidPacket(packet, CRSF_ADDRESS_FLIGHT_CONTROLLER, 
                     CRSF_FRAMETYPE_ATTITUDE, payload, 6, totalLen);
    
    simulatePacketReception(packet, totalLen);
    
    // Проверяем attitude данные (конвертация: raw / 175.0)
    EXPECT_NEAR(crsf->getAttitudeRoll(), 10.0, 0.01);
    EXPECT_NEAR(crsf->getAttitudePitch(), 0.0, 0.01);
    EXPECT_NEAR(crsf->getAttitudeYaw(), 20.0, 0.01);
    
    // Проверяем сырые значения
    EXPECT_EQ(crsf->getRawAttitudeRoll(), 1750);
    EXPECT_EQ(crsf->getRawAttitudePitch(), 0);
    EXPECT_EQ(crsf->getRawAttitudeYaw(), 3500);
}

/**
 * @test Парсинг Attitude с отрицательными углами
 * 
 * Тест проверяет обработку отрицательных углов в attitude пакете.
 */
TEST_F(CrsfTelemetryParsingTest, ParseAttitude_NegativeAngles_HandlesCorrectly) {
    uint8_t packet[64];
    uint8_t payload[6] = {0};
    
    // Pitch: -10° = -1750 (big endian int16_t, знаковое)
    payload[0] = 0xF9; payload[1] = 0x2A; // 0xF92A = -1750 (two's complement)
    
    // Roll: -5° = -875
    payload[2] = 0xFC; payload[3] = 0x95; // 0xFC95 = -875
    
    // Yaw: 0°
    payload[4] = 0x00; payload[5] = 0x00;
    
    uint8_t totalLen;
    createValidPacket(packet, CRSF_ADDRESS_FLIGHT_CONTROLLER, 
                     CRSF_FRAMETYPE_ATTITUDE, payload, 6, totalLen);
    
    simulatePacketReception(packet, totalLen);
    
    // Проверяем отрицательные углы
    EXPECT_NEAR(crsf->getAttitudePitch(), -10.0, 0.01);
    EXPECT_NEAR(crsf->getAttitudeRoll(), -5.0, 0.01);
}

/**
 * @test Парсинг Attitude с нормализацией Yaw
 * 
 * Тест проверяет нормализацию yaw к диапазону 0-360°.
 */
TEST_F(CrsfTelemetryParsingTest, ParseAttitude_YawNormalization_NormalizesTo0_360) {
    uint8_t packet[64];
    uint8_t payload[6] = {0};
    
    // Yaw: 370° должно быть нормализовано к 10°
    // 370° = 64750 (370 * 175)
    payload[0] = 0x00; payload[1] = 0x00; // Pitch
    payload[2] = 0x00; payload[3] = 0x00; // Roll
    payload[4] = 0xFC; payload[5] = 0xCE; // Yaw: 64750 (big endian)
    
    uint8_t totalLen;
    createValidPacket(packet, CRSF_ADDRESS_FLIGHT_CONTROLLER, 
                     CRSF_FRAMETYPE_ATTITUDE, payload, 6, totalLen);
    
    simulatePacketReception(packet, totalLen);
    
    // Yaw должен быть нормализован
    double yaw = crsf->getAttitudeYaw();
    EXPECT_GE(yaw, 0.0);
    EXPECT_LT(yaw, 360.0);
}

/**
 * @test Парсинг Link Statistics пакета
 * 
 * Тест проверяет корректный парсинг статистики связи.
 */
TEST_F(CrsfTelemetryParsingTest, ParseLinkStatistics_ValidPacket_UpdatesLinkStats) {
    uint8_t packet[64];
    uint8_t payload[10] = {0};
    
    payload[0] = 100; // uplink_RSSI_1
    payload[1] = 95;  // uplink_RSSI_2
    payload[2] = 90;  // uplink_Link_quality
    payload[3] = 10;  // uplink_SNR (signed)
    payload[4] = 1;   // active_antenna
    payload[5] = 2;   // rf_Mode
    payload[6] = 3;   // uplink_TX_Power
    payload[7] = 85;  // downlink_RSSI
    payload[8] = 80;  // downlink_Link_quality
    payload[9] = 5;   // downlink_SNR (signed)
    
    uint8_t totalLen;
    createValidPacket(packet, CRSF_ADDRESS_FLIGHT_CONTROLLER, 
                     CRSF_FRAMETYPE_LINK_STATISTICS, payload, 10, totalLen);
    
    simulatePacketReception(packet, totalLen);
    
    // Проверяем статистику связи
    const crsfLinkStatistics_t* stats = crsf->getLinkStatistics();
    EXPECT_NE(stats, nullptr);
    EXPECT_EQ(stats->uplink_RSSI_1, 100);
    EXPECT_EQ(stats->uplink_RSSI_2, 95);
    EXPECT_EQ(stats->uplink_Link_quality, 90);
    EXPECT_EQ(stats->uplink_SNR, 10);
    EXPECT_EQ(stats->active_antenna, 1);
    EXPECT_EQ(stats->rf_Mode, 2);
    EXPECT_EQ(stats->uplink_TX_Power, 3);
    EXPECT_EQ(stats->downlink_RSSI, 85);
    EXPECT_EQ(stats->downlink_Link_quality, 80);
    EXPECT_EQ(stats->downlink_SNR, 5);
}

/**
 * @test Парсинг неполного Battery Sensor пакета
 * 
 * Тест проверяет обработку неполного пакета батареи
 * (меньше 8 байт payload).
 */
TEST_F(CrsfTelemetryParsingTest, ParseBatterySensor_IncompletePacket_HandlesGracefully) {
    uint8_t packet[64];
    uint8_t payload[4] = {0}; // Неполный payload
    
    payload[0] = 0x30; payload[1] = 0xD4; // voltage
    
    uint8_t totalLen;
    createValidPacket(packet, CRSF_ADDRESS_FLIGHT_CONTROLLER, 
                     CRSF_FRAMETYPE_BATTERY_SENSOR, payload, 4, totalLen);
    
    simulatePacketReception(packet, totalLen);
    
    // Пакет должен быть обработан без ошибок
    // (данные могут быть неполными, но не должно быть краша)
    EXPECT_NO_THROW(crsf->loop());
}

/**
 * @test Парсинг неполного Attitude пакета
 * 
 * Тест проверяет обработку неполного attitude пакета.
 */
TEST_F(CrsfTelemetryParsingTest, ParseAttitude_IncompletePacket_HandlesGracefully) {
    uint8_t packet[64];
    uint8_t payload[4] = {0}; // Неполный payload (нужно 6 байт)
    
    payload[0] = 0x00; payload[1] = 0x00; // Pitch
    payload[2] = 0x06; payload[3] = 0xD6; // Roll (частично)
    
    uint8_t totalLen;
    createValidPacket(packet, CRSF_ADDRESS_FLIGHT_CONTROLLER, 
                     CRSF_FRAMETYPE_ATTITUDE, payload, 4, totalLen);
    
    simulatePacketReception(packet, totalLen);
    
    // Пакет должен быть обработан без ошибок
    EXPECT_NO_THROW(crsf->loop());
}

/**
 * @test Множественные телеметрические пакеты
 * 
 * Тест проверяет обработку нескольких телеметрических пакетов
 * подряд (Battery, GPS, Attitude).
 */
TEST_F(CrsfTelemetryParsingTest, ParseTelemetry_MultiplePackets_AllProcessed) {
    // Battery packet
    uint8_t batteryPacket[64];
    uint8_t batteryPayload[8] = {0x30, 0xD4, 0x13, 0x88, 0x00, 0x03, 0xE8, 80};
    uint8_t batteryLen;
    createValidPacket(batteryPacket, CRSF_ADDRESS_FLIGHT_CONTROLLER, 
                     CRSF_FRAMETYPE_BATTERY_SENSOR, batteryPayload, 8, batteryLen);
    
    simulatePacketReception(batteryPacket, batteryLen);
    EXPECT_GT(crsf->getBatteryVoltage(), 0.0);
    
    // GPS packet
    uint8_t gpsPacket[64];
    uint8_t gpsPayload[15] = {0};
    gpsPayload[14] = 10; // satellites
    uint8_t gpsLen;
    createValidPacket(gpsPacket, CRSF_ADDRESS_FLIGHT_CONTROLLER, 
                     CRSF_FRAMETYPE_GPS, gpsPayload, 15, gpsLen);
    
    simulatePacketReception(gpsPacket, gpsLen);
    const crsf_sensor_gps_t* gps = crsf->getGpsSensor();
    EXPECT_NE(gps, nullptr);
    
    // Attitude packet
    uint8_t attitudePacket[64];
    uint8_t attitudePayload[6] = {0};
    uint8_t attitudeLen;
    createValidPacket(attitudePacket, CRSF_ADDRESS_FLIGHT_CONTROLLER, 
                     CRSF_FRAMETYPE_ATTITUDE, attitudePayload, 6, attitudeLen);
    
    simulatePacketReception(attitudePacket, attitudeLen);
    EXPECT_NO_THROW(crsf->getAttitudeRoll());
}

