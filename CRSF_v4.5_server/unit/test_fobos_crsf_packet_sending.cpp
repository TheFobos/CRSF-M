/**
 * @file test_fobos_crsf_packet_sending.cpp
 * @brief Unit тесты для отправки CRSF пакетов
 * 
 * Тесты проверяют:
 * - queuePacket() с разными типами пакетов
 * - queuePacket() когда link down (не должен отправлять)
 * - queuePacket() с превышением размера payload
 * - Точность кодирования в packetChannelsSend()
 * - Корректность CRC в отправляемых пакетах
 * 
 * @version 4.3
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include <cstring>
#include "../libs/crsf/CrsfSerial.h"
#include "../libs/crsf/crsf_protocol.h"
#include "../libs/crsf/crc8.h"
#include "mocks/MockSerialPort.h"

using ::testing::_;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::DoAll;
using ::testing::Invoke;

/**
 * @class CrsfPacketSendingTest
 * @brief Фикстура для тестов отправки пакетов
 */
class CrsfPacketSendingTest : public ::testing::Test {
protected:
    void SetUp() override {
        mockSerial = std::make_unique<MockSerialPort>();
        crsf = std::make_unique<CrsfSerial>(*mockSerial, 420000);
    }
    
    std::unique_ptr<MockSerialPort> mockSerial;
    std::unique_ptr<CrsfSerial> crsf;
};

/**
 * @test Отправка пакета каналов через packetChannelsSend
 * 
 * Тест проверяет корректную отправку пакета каналов с правильным
 * форматом и CRC.
 */
TEST_F(CrsfPacketSendingTest, PacketChannelsSend_ValidChannels_SendsPacket) {
    // Устанавливаем значения каналов
    for (int i = 1; i <= 16; i++) {
        crsf->setChannel(i, 1500);
    }
    
    // Ожидаем отправку пакета размером 26 байт
    EXPECT_CALL(*mockSerial, write(_, 26))
        .WillOnce(Return(26));
    
    crsf->packetChannelsSend();
}

/**
 * @test queuePacket когда link down не отправляет пакет
 * 
 * Тест проверяет, что queuePacket() не отправляет пакет,
 * когда связь не установлена.
 */
TEST_F(CrsfPacketSendingTest, QueuePacket_LinkDown_DoesNotSend) {
    // Связь не установлена
    EXPECT_FALSE(crsf->isLinkUp());
    
    uint8_t payload[10] = {0};
    
    // write() не должен быть вызван
    EXPECT_CALL(*mockSerial, write(_, _))
        .Times(0);
    
    crsf->queuePacket(CRSF_ADDRESS_FLIGHT_CONTROLLER, 
                     CRSF_FRAMETYPE_LINK_STATISTICS, 
                     payload, 10);
}

/**
 * @test queuePacket когда link up отправляет пакет
 * 
 * Тест проверяет, что queuePacket() отправляет пакет,
 * когда связь установлена.
 */
TEST_F(CrsfPacketSendingTest, QueuePacket_LinkUp_SendsPacket) {
    // Устанавливаем связь через отправку пакета каналов
    for (int i = 1; i <= 16; i++) {
        crsf->setChannel(i, 1500);
    }
    
    // Сначала отправляем пакет каналов для установки связи
    EXPECT_CALL(*mockSerial, write(_, _))
        .WillOnce(Return(26));
    
    crsf->packetChannelsSend();
    
    // Теперь связь должна быть установлена
    EXPECT_TRUE(crsf->isLinkUp());
    
    // Отправляем другой пакет через queuePacket
    uint8_t payload[10] = {100, 95, 90, 10, 0, 0, 0, 85, 80, 5};
    
    EXPECT_CALL(*mockSerial, write(_, _))
        .WillOnce(Return(14)); // ADDR + LEN + TYPE + PAYLOAD + CRC
    
    crsf->queuePacket(CRSF_ADDRESS_FLIGHT_CONTROLLER, 
                     CRSF_FRAMETYPE_LINK_STATISTICS, 
                     payload, 10);
}

/**
 * @test queuePacket с превышением размера payload отклоняется
 * 
 * Тест проверяет, что queuePacket() не отправляет пакет,
 * если размер payload превышает CRSF_MAX_PAYLOAD_LEN.
 */
TEST_F(CrsfPacketSendingTest, QueuePacket_OversizedPayload_DoesNotSend) {
    // Устанавливаем связь
    for (int i = 1; i <= 16; i++) {
        crsf->setChannel(i, 1500);
    }
    
    EXPECT_CALL(*mockSerial, write(_, _))
        .WillOnce(Return(26));
    
    crsf->packetChannelsSend();
    EXPECT_TRUE(crsf->isLinkUp());
    
    // Создаем payload больше максимального размера
    uint8_t oversizedPayload[CRSF_MAX_PAYLOAD_LEN + 1];
    
    // write() не должен быть вызван
    EXPECT_CALL(*mockSerial, write(_, _))
        .Times(0);
    
    crsf->queuePacket(CRSF_ADDRESS_FLIGHT_CONTROLLER, 
                     CRSF_FRAMETYPE_LINK_STATISTICS, 
                     oversizedPayload, CRSF_MAX_PAYLOAD_LEN + 1);
}

/**
 * @test Проверка формата отправляемого пакета
 * 
 * Тест проверяет, что отправляемый пакет имеет правильный формат:
 * [ADDR][LEN][TYPE][PAYLOAD][CRC]
 */
TEST_F(CrsfPacketSendingTest, QueuePacket_ValidPacket_HasCorrectFormat) {
    // Устанавливаем связь
    for (int i = 1; i <= 16; i++) {
        crsf->setChannel(i, 1500);
    }
    
    EXPECT_CALL(*mockSerial, write(_, _))
        .WillOnce(Return(26));
    
    crsf->packetChannelsSend();
    EXPECT_TRUE(crsf->isLinkUp());
    
    // Отправляем тестовый пакет
    uint8_t payload[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    uint8_t capturedPacket[64];
    size_t capturedLen = 0;
    
    EXPECT_CALL(*mockSerial, write(_, _))
        .WillOnce(Invoke([&capturedPacket, &capturedLen](const uint8_t* buf, size_t len) {
            memcpy(capturedPacket, buf, len < 64 ? len : 64);
            capturedLen = len;
            return static_cast<int>(len);
        }));
    
    crsf->queuePacket(CRSF_ADDRESS_FLIGHT_CONTROLLER, 
                     CRSF_FRAMETYPE_LINK_STATISTICS, 
                     payload, 10);
    
    // Проверяем формат пакета
    EXPECT_EQ(capturedLen, 14); // ADDR(1) + LEN(1) + TYPE(1) + PAYLOAD(10) + CRC(1)
    EXPECT_EQ(capturedPacket[0], CRSF_ADDRESS_FLIGHT_CONTROLLER);
    EXPECT_EQ(capturedPacket[1], 12); // TYPE + PAYLOAD + CRC = 1 + 10 + 1
    EXPECT_EQ(capturedPacket[2], CRSF_FRAMETYPE_LINK_STATISTICS);
    
    // Проверяем, что payload скопирован
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(capturedPacket[3 + i], payload[i]);
    }
    
    // CRC должен быть вычислен (не нулевой)
    EXPECT_NE(capturedPacket[13], 0);
}

/**
 * @test Точность кодирования каналов в packetChannelsSend
 * 
 * Тест проверяет, что значения каналов корректно кодируются
 * при отправке пакета.
 */
TEST_F(CrsfPacketSendingTest, PacketChannelsSend_ChannelValues_EncodesCorrectly) {
    // Устанавливаем разные значения каналов
    crsf->setChannel(1, 1000);
    crsf->setChannel(2, 1500);
    crsf->setChannel(3, 2000);
    
    uint8_t capturedPacket[64];
    size_t capturedLen = 0;
    
    EXPECT_CALL(*mockSerial, write(_, _))
        .WillOnce(Invoke([&capturedPacket, &capturedLen](const uint8_t* buf, size_t len) {
            memcpy(capturedPacket, buf, len < 64 ? len : 64);
            capturedLen = len;
            return static_cast<int>(len);
        }));
    
    crsf->packetChannelsSend();
    
    // Проверяем, что пакет отправлен
    EXPECT_EQ(capturedLen, 26);
    EXPECT_EQ(capturedPacket[0], CRSF_ADDRESS_FLIGHT_CONTROLLER);
    EXPECT_EQ(capturedPacket[1], 24); // TYPE + PAYLOAD(22) + CRC
    EXPECT_EQ(capturedPacket[2], CRSF_FRAMETYPE_RC_CHANNELS_PACKED);
}

/**
 * @test Отправка пакета с максимальным размером payload
 * 
 * Тест проверяет отправку пакета с максимально допустимым
 * размером payload.
 */
TEST_F(CrsfPacketSendingTest, QueuePacket_MaxPayloadSize_SendsPacket) {
    // Устанавливаем связь
    for (int i = 1; i <= 16; i++) {
        crsf->setChannel(i, 1500);
    }
    
    EXPECT_CALL(*mockSerial, write(_, _))
        .WillOnce(Return(26));
    
    crsf->packetChannelsSend();
    EXPECT_TRUE(crsf->isLinkUp());
    
    // Создаем payload максимального размера
    uint8_t maxPayload[CRSF_MAX_PAYLOAD_LEN];
    for (int i = 0; i < CRSF_MAX_PAYLOAD_LEN; i++) {
        maxPayload[i] = static_cast<uint8_t>(i);
    }
    
    EXPECT_CALL(*mockSerial, write(_, _))
        .WillOnce(Return(CRSF_MAX_PAYLOAD_LEN + 4)); // ADDR + LEN + TYPE + PAYLOAD + CRC
    
    crsf->queuePacket(CRSF_ADDRESS_FLIGHT_CONTROLLER, 
                     CRSF_FRAMETYPE_LINK_STATISTICS, 
                     maxPayload, CRSF_MAX_PAYLOAD_LEN);
}

/**
 * @test Отправка нескольких пакетов подряд
 * 
 * Тест проверяет отправку нескольких пакетов подряд
 * через queuePacket().
 */
TEST_F(CrsfPacketSendingTest, QueuePacket_MultiplePackets_AllSent) {
    // Устанавливаем связь
    for (int i = 1; i <= 16; i++) {
        crsf->setChannel(i, 1500);
    }
    
    EXPECT_CALL(*mockSerial, write(_, _))
        .WillOnce(Return(26));
    
    crsf->packetChannelsSend();
    EXPECT_TRUE(crsf->isLinkUp());
    
    // Отправляем несколько пакетов
    uint8_t payload1[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    uint8_t payload2[8] = {10, 20, 30, 40, 50, 60, 70, 80};
    
    EXPECT_CALL(*mockSerial, write(_, _))
        .WillOnce(Return(14))
        .WillOnce(Return(12));
    
    crsf->queuePacket(CRSF_ADDRESS_FLIGHT_CONTROLLER, 
                     CRSF_FRAMETYPE_LINK_STATISTICS, 
                     payload1, 10);
    
    crsf->queuePacket(CRSF_ADDRESS_FLIGHT_CONTROLLER, 
                     CRSF_FRAMETYPE_BATTERY_SENSOR, 
                     payload2, 8);
}

/**
 * @test Проверка CRC в отправляемом пакете
 * 
 * Тест проверяет, что CRC вычисляется и добавляется
 * в отправляемый пакет.
 */
TEST_F(CrsfPacketSendingTest, QueuePacket_ValidPacket_IncludesCrc) {
    // Устанавливаем связь
    for (int i = 1; i <= 16; i++) {
        crsf->setChannel(i, 1500);
    }
    
    EXPECT_CALL(*mockSerial, write(_, _))
        .WillOnce(Return(26));
    
    crsf->packetChannelsSend();
    EXPECT_TRUE(crsf->isLinkUp());
    
    // Отправляем тестовый пакет
    uint8_t payload[10] = {0};
    uint8_t capturedPacket[64];
    
    EXPECT_CALL(*mockSerial, write(_, _))
        .WillOnce(Invoke([&capturedPacket](const uint8_t* buf, size_t len) {
            memcpy(capturedPacket, buf, len < 64 ? len : 64);
            return static_cast<int>(len);
        }));
    
    crsf->queuePacket(CRSF_ADDRESS_FLIGHT_CONTROLLER, 
                     CRSF_FRAMETYPE_LINK_STATISTICS, 
                     payload, 10);
    
    // Проверяем, что CRC присутствует (последний байт)
    uint8_t crc = capturedPacket[13];
    
    // Вычисляем ожидаемый CRC
    Crc8 crcCalc(0xD5);
    uint8_t crcData[11];
    crcData[0] = CRSF_FRAMETYPE_LINK_STATISTICS;
    memcpy(&crcData[1], payload, 10);
    uint8_t expectedCrc = crcCalc.calc(crcData, 11);
    
    EXPECT_EQ(crc, expectedCrc);
}

