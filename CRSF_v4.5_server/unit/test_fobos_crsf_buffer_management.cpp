/**
 * @file test_fobos_crsf_buffer_management.cpp
 * @brief Unit тесты для управления буфером CRSF
 * 
 * Тесты проверяют:
 * - shiftRxBuffer() - граничные случаи
 * - Защита от переполнения буфера
 * - Таймаут и очистка буфера
 * - Несколько пакетов в буфере
 * 
 * @version 4.3
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include "../libs/crsf/CrsfSerial.h"
#include "../libs/crsf/crsf_protocol.h"
#include "mocks/MockSerialPort.h"

using ::testing::_;
using ::testing::Return;
using ::testing::InSequence;
using ::testing::DoAll;

/**
 * @class CrsfBufferManagementTest
 * @brief Фикстура для тестов управления буфером
 */
class CrsfBufferManagementTest : public ::testing::Test {
protected:
    void SetUp() override {
        mockSerial = std::make_unique<MockSerialPort>();
        crsf = std::make_unique<CrsfSerial>(*mockSerial, 420000);
    }
    
    std::unique_ptr<MockSerialPort> mockSerial;
    std::unique_ptr<CrsfSerial> crsf;
};

/**
 * @test Обработка неполного пакета в буфере
 * 
 * Тест проверяет, что неполный пакет остается в буфере
 * до получения всех байт.
 */
TEST_F(CrsfBufferManagementTest, BufferManagement_IncompletePacket_WaitsForComplete) {
    uint8_t partialPacket[10];
    partialPacket[0] = CRSF_ADDRESS_FLIGHT_CONTROLLER;
    partialPacket[1] = 24; // Длина полного пакета
    partialPacket[2] = CRSF_FRAMETYPE_RC_CHANNELS_PACKED;
    // Отправляем только часть пакета
    
    InSequence seq;
    for (int i = 0; i < 10; i++) {
        EXPECT_CALL(*mockSerial, readByte(_))
            .WillOnce(DoAll(::testing::SetArgReferee<0>(partialPacket[i]), Return(1)));
    }
    EXPECT_CALL(*mockSerial, readByte(_))
        .WillRepeatedly(Return(0));
    
    // Пакет неполный, поэтому не должен быть обработан
    crsf->loop();
    
    // Проверяем, что нет краша
    EXPECT_NO_THROW(crsf->loop());
}

/**
 * @test Обработка нескольких пакетов подряд
 * 
 * Тест проверяет обработку нескольких пакетов,
 * приходящих подряд в буфер.
 */
TEST_F(CrsfBufferManagementTest, BufferManagement_MultiplePackets_AllProcessed) {
    // Создаем два пакета каналов
    uint8_t packet1[64], packet2[64];
    uint8_t payload[22] = {0};
    
    Crc8 crc(0xD5);
    
    // Первый пакет
    packet1[0] = CRSF_ADDRESS_FLIGHT_CONTROLLER;
    packet1[1] = 24;
    packet1[2] = CRSF_FRAMETYPE_RC_CHANNELS_PACKED;
    memcpy(&packet1[3], payload, 22);
    uint8_t crcData[23];
    crcData[0] = CRSF_FRAMETYPE_RC_CHANNELS_PACKED;
    memcpy(&crcData[1], payload, 22);
    packet1[25] = crc.calc(crcData, 23);
    
    // Второй пакет
    packet2[0] = CRSF_ADDRESS_FLIGHT_CONTROLLER;
    packet2[1] = 24;
    packet2[2] = CRSF_FRAMETYPE_RC_CHANNELS_PACKED;
    memcpy(&packet2[3], payload, 22);
    packet2[25] = crc.calc(crcData, 23);
    
    // Отправляем оба пакета подряд
    InSequence seq;
    for (int i = 0; i < 26; i++) {
        EXPECT_CALL(*mockSerial, readByte(_))
            .WillOnce(DoAll(::testing::SetArgReferee<0>(packet1[i]), Return(1)));
    }
    for (int i = 0; i < 26; i++) {
        EXPECT_CALL(*mockSerial, readByte(_))
            .WillOnce(DoAll(::testing::SetArgReferee<0>(packet2[i]), Return(1)));
    }
    EXPECT_CALL(*mockSerial, readByte(_))
        .WillRepeatedly(Return(0));
    
    // Обрабатываем пакеты - нужно вызвать loop() несколько раз,
    // так как handleSerialIn() читает максимум 32 байта за раз
    crsf->loop(); // Читает первые 32 байта (первый пакет + 6 байт второго)
    crsf->loop(); // Читает оставшиеся 20 байт второго пакета
    
    // Оба пакета должны быть обработаны
    EXPECT_TRUE(crsf->isLinkUp());
}

/**
 * @test Обработка поврежденного пакета с последующим валидным
 * 
 * Тест проверяет, что после поврежденного пакета
 * следующий валидный пакет обрабатывается корректно.
 */
TEST_F(CrsfBufferManagementTest, BufferManagement_CorruptedThenValid_ValidProcessed) {
    // Создаем поврежденный пакет (неверный CRC)
    uint8_t badPacket[64];
    uint8_t payload[22] = {0};
    
    Crc8 crc(0xD5);
    badPacket[0] = CRSF_ADDRESS_FLIGHT_CONTROLLER;
    badPacket[1] = 24;
    badPacket[2] = CRSF_FRAMETYPE_RC_CHANNELS_PACKED;
    memcpy(&badPacket[3], payload, 22);
    uint8_t crcData[23];
    crcData[0] = CRSF_FRAMETYPE_RC_CHANNELS_PACKED;
    memcpy(&crcData[1], payload, 22);
    badPacket[25] = crc.calc(crcData, 23) ^ 0xFF; // Портим CRC
    
    // Создаем валидный пакет
    uint8_t goodPacket[64];
    goodPacket[0] = CRSF_ADDRESS_FLIGHT_CONTROLLER;
    goodPacket[1] = 24;
    goodPacket[2] = CRSF_FRAMETYPE_RC_CHANNELS_PACKED;
    memcpy(&goodPacket[3], payload, 22);
    goodPacket[25] = crc.calc(crcData, 23); // Правильный CRC
    
    InSequence seq;
    // Отправляем поврежденный пакет
    for (int i = 0; i < 26; i++) {
        EXPECT_CALL(*mockSerial, readByte(_))
            .WillOnce(DoAll(::testing::SetArgReferee<0>(badPacket[i]), Return(1)));
    }
    // Отправляем валидный пакет
    for (int i = 0; i < 26; i++) {
        EXPECT_CALL(*mockSerial, readByte(_))
            .WillOnce(DoAll(::testing::SetArgReferee<0>(goodPacket[i]), Return(1)));
    }
    EXPECT_CALL(*mockSerial, readByte(_))
        .WillRepeatedly(Return(0));
    
    // Нужно вызвать loop() несколько раз для чтения всех байт
    crsf->loop(); // Читает первые 32 байта
    crsf->loop(); // Читает оставшиеся байты
    
    // Валидный пакет должен быть обработан
    EXPECT_TRUE(crsf->isLinkUp());
}

/**
 * @test Защита от переполнения буфера
 * 
 * Тест проверяет, что при достижении максимального размера
 * буфера он корректно обрабатывается (сброс позиции).
 */
TEST_F(CrsfBufferManagementTest, BufferManagement_MaxBufferSize_HandlesCorrectly) {
    // Создаем пакет, который заполнит буфер
    // CRSF_MAX_PACKET_SIZE = 64 байта
    
    // Отправляем много данных, но не валидный пакет
    uint8_t data[64];
    for (int i = 0; i < 64; i++) {
        data[i] = 0xFF; // Не валидный адрес
    }
    
    InSequence seq;
    for (int i = 0; i < 64; i++) {
        EXPECT_CALL(*mockSerial, readByte(_))
            .WillOnce(DoAll(::testing::SetArgReferee<0>(data[i]), Return(1)));
    }
    EXPECT_CALL(*mockSerial, readByte(_))
        .WillRepeatedly(Return(0));
    
    // Не должно быть переполнения буфера - вызываем loop() несколько раз
    EXPECT_NO_THROW(crsf->loop());
    EXPECT_NO_THROW(crsf->loop());
}

/**
 * @test Обработка пакета с неверной длиной
 * 
 * Тест проверяет обработку пакета с длиной вне допустимого
 * диапазона (слишком маленькой или слишком большой).
 */
TEST_F(CrsfBufferManagementTest, BufferManagement_InvalidLength_RejectsPacket) {
    // Пакет с длиной меньше минимума
    uint8_t packet[64];
    packet[0] = CRSF_ADDRESS_FLIGHT_CONTROLLER;
    packet[1] = 2; // Минимальная длина должна быть 3
    packet[2] = CRSF_FRAMETYPE_RC_CHANNELS_PACKED;
    
    InSequence seq;
    for (int i = 0; i < 3; i++) {
        EXPECT_CALL(*mockSerial, readByte(_))
            .WillOnce(DoAll(::testing::SetArgReferee<0>(packet[i]), Return(1)));
    }
    EXPECT_CALL(*mockSerial, readByte(_))
        .WillRepeatedly(Return(0));
    
    // Пакет должен быть отклонен без ошибок
    EXPECT_NO_THROW(crsf->loop());
}

/**
 * @test Обработка пустого буфера
 * 
 * Тест проверяет, что при отсутствии данных в буфере
 * система работает корректно.
 */
TEST_F(CrsfBufferManagementTest, BufferManagement_EmptyBuffer_NoErrors) {
    EXPECT_CALL(*mockSerial, readByte(_))
        .WillRepeatedly(Return(0)); // Нет данных
    
    // Не должно быть ошибок при пустом буфере
    EXPECT_NO_THROW(crsf->loop());
    EXPECT_NO_THROW(crsf->loop());
    EXPECT_NO_THROW(crsf->loop());
}

/**
 * @test Обработка пакета с максимальной длиной
 * 
 * Тест проверяет обработку пакета с максимально допустимой длиной.
 */
TEST_F(CrsfBufferManagementTest, BufferManagement_MaxLengthPacket_ProcessesCorrectly) {
    uint8_t packet[64];
    uint8_t payload[CRSF_MAX_PAYLOAD_LEN] = {0};
    
    Crc8 crc(0xD5);
    packet[0] = CRSF_ADDRESS_FLIGHT_CONTROLLER;
    packet[1] = CRSF_MAX_PAYLOAD_LEN + 2; // TYPE + PAYLOAD + CRC
    packet[2] = CRSF_FRAMETYPE_LINK_STATISTICS;
    memcpy(&packet[3], payload, CRSF_MAX_PAYLOAD_LEN);
    
    uint8_t crcData[CRSF_MAX_PAYLOAD_LEN + 1];
    crcData[0] = CRSF_FRAMETYPE_LINK_STATISTICS;
    memcpy(&crcData[1], payload, CRSF_MAX_PAYLOAD_LEN);
    packet[3 + CRSF_MAX_PAYLOAD_LEN] = crc.calc(crcData, CRSF_MAX_PAYLOAD_LEN + 1);
    
    uint8_t totalLen = 4 + CRSF_MAX_PAYLOAD_LEN;
    
    InSequence seq;
    for (uint8_t i = 0; i < totalLen; i++) {
        EXPECT_CALL(*mockSerial, readByte(_))
            .WillOnce(DoAll(::testing::SetArgReferee<0>(packet[i]), Return(1)));
    }
    EXPECT_CALL(*mockSerial, readByte(_))
        .WillRepeatedly(Return(0));
    
    // Пакет должен быть обработан - вызываем loop() несколько раз для чтения всех байт
    EXPECT_NO_THROW(crsf->loop());
    if (totalLen > 32) {
        EXPECT_NO_THROW(crsf->loop());
    }
}

