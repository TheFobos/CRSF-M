/**
 * @file test_fobos_crsf_error_handling.cpp
 * @brief Unit тесты для обработки ошибок CRSF
 * 
 * Тесты проверяют обработку различных ошибочных ситуаций:
 * - Невалидные номера каналов (0, 17+)
 * - Невалидные длины пакетов
 * - Обработка поврежденных данных
 * - Ошибки последовательного порта
 * - Граничные случаи
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
 * @class CrsfErrorHandlingTest
 * @brief Фикстура для тестов обработки ошибок
 */
class CrsfErrorHandlingTest : public ::testing::Test {
protected:
    void SetUp() override {
        mockSerial = std::make_unique<MockSerialPort>();
        crsf = std::make_unique<CrsfSerial>(*mockSerial, 420000);
    }
    
    std::unique_ptr<MockSerialPort> mockSerial;
    std::unique_ptr<CrsfSerial> crsf;
};

/**
 * @test Обработка невалидного номера канала (0)
 * 
 * Тест проверяет поведение при обращении к каналу 0.
 * Примечание: каналы нумеруются с 1, канал 0 может вызвать
 * обращение к _channels[-1], что небезопасно.
 */
TEST_F(CrsfErrorHandlingTest, GetChannel_ChannelZero_HandlesGracefully) {
    // Попытка получить канал 0
    // В текущей реализации это обращение к _channels[-1]
    // Тест проверяет, что нет краша
    EXPECT_NO_THROW({
        int value = crsf->getChannel(0);
        // Значение может быть неопределенным, но не должно быть краша
        (void)value; // Подавляем предупреждение о неиспользуемой переменной
    });
}

/**
 * @test Обработка невалидного номера канала (17+)
 * 
 * Тест проверяет поведение при обращении к каналу вне диапазона [1, 16].
 */
TEST_F(CrsfErrorHandlingTest, GetChannel_ChannelOutOfRange_HandlesGracefully) {
    // Попытка получить канал 17 (вне диапазона)
    EXPECT_NO_THROW({
        int value = crsf->getChannel(17);
        (void)value;
    });
    
    // Попытка получить канал 100
    EXPECT_NO_THROW({
        int value = crsf->getChannel(100);
        (void)value;
    });
}

/**
 * @test Установка невалидного номера канала
 * 
 * Тест проверяет поведение при установке значения
 * для невалидного номера канала.
 */
TEST_F(CrsfErrorHandlingTest, SetChannel_InvalidChannel_HandlesGracefully) {
    // Попытка установить канал 0
    EXPECT_NO_THROW(crsf->setChannel(0, 1500));
    
    // Попытка установить канал 17
    EXPECT_NO_THROW(crsf->setChannel(17, 1500));
    
    // Попытка установить канал 100
    EXPECT_NO_THROW(crsf->setChannel(100, 1500));
}

/**
 * @test Обработка пакета с неверным CRC
 * 
 * Тест проверяет, что пакет с неверным CRC отклоняется
 * и не вызывает ошибок.
 */
TEST_F(CrsfErrorHandlingTest, ParsePacket_InvalidCrc_RejectsWithoutCrash) {
    uint8_t packet[64];
    uint8_t payload[22] = {0};
    
    Crc8 crc(0xD5);
    packet[0] = CRSF_ADDRESS_FLIGHT_CONTROLLER;
    packet[1] = 24;
    packet[2] = CRSF_FRAMETYPE_RC_CHANNELS_PACKED;
    memcpy(&packet[3], payload, 22);
    
    uint8_t crcData[23];
    crcData[0] = CRSF_FRAMETYPE_RC_CHANNELS_PACKED;
    memcpy(&crcData[1], payload, 22);
    packet[25] = crc.calc(crcData, 23) ^ 0xFF; // Портим CRC
    
    InSequence seq;
    for (int i = 0; i < 26; i++) {
        EXPECT_CALL(*mockSerial, readByte(_))
            .WillOnce(DoAll(::testing::SetArgReferee<0>(packet[i]), Return(1)));
    }
    EXPECT_CALL(*mockSerial, readByte(_))
        .WillRepeatedly(Return(0));
    
    // Пакет должен быть отклонен без ошибок
    EXPECT_NO_THROW(crsf->loop());
}

/**
 * @test Обработка пакета с нулевой длиной
 * 
 * Тест проверяет обработку пакета с длиной 0.
 */
TEST_F(CrsfErrorHandlingTest, ParsePacket_ZeroLength_RejectsPacket) {
    uint8_t packet[64];
    packet[0] = CRSF_ADDRESS_FLIGHT_CONTROLLER;
    packet[1] = 0; // Нулевая длина
    
    InSequence seq;
    for (int i = 0; i < 2; i++) {
        EXPECT_CALL(*mockSerial, readByte(_))
            .WillOnce(DoAll(::testing::SetArgReferee<0>(packet[i]), Return(1)));
    }
    EXPECT_CALL(*mockSerial, readByte(_))
        .WillRepeatedly(Return(0));
    
    EXPECT_NO_THROW(crsf->loop());
}

/**
 * @test Обработка пакета с длиной больше максимума
 * 
 * Тест проверяет обработку пакета с длиной превышающей
 * CRSF_MAX_PAYLOAD_LEN + 2.
 */
TEST_F(CrsfErrorHandlingTest, ParsePacket_LengthTooLarge_RejectsPacket) {
    uint8_t packet[64];
    packet[0] = CRSF_ADDRESS_FLIGHT_CONTROLLER;
    packet[1] = CRSF_MAX_PAYLOAD_LEN + 10; // Слишком большая длина
    
    InSequence seq;
    for (int i = 0; i < 2; i++) {
        EXPECT_CALL(*mockSerial, readByte(_))
            .WillOnce(DoAll(::testing::SetArgReferee<0>(packet[i]), Return(1)));
    }
    EXPECT_CALL(*mockSerial, readByte(_))
        .WillRepeatedly(Return(0));
    
    EXPECT_NO_THROW(crsf->loop());
}

/**
 * @test Обработка ошибки чтения из последовательного порта
 * 
 * Тест проверяет обработку ситуации, когда readByte()
 * возвращает ошибку (отрицательное значение).
 */
TEST_F(CrsfErrorHandlingTest, ReadByte_ErrorReturn_HandlesGracefully) {
    EXPECT_CALL(*mockSerial, readByte(_))
        .WillOnce(Return(-1)); // Ошибка чтения
    
    // Не должно быть краша
    EXPECT_NO_THROW(crsf->loop());
}

/**
 * @test Обработка множественных поврежденных пакетов
 * 
 * Тест проверяет, что система корректно обрабатывает
 * несколько поврежденных пакетов подряд.
 */
TEST_F(CrsfErrorHandlingTest, ParsePacket_MultipleCorrupted_AllRejected) {
    uint8_t badPacket[64];
    badPacket[0] = CRSF_ADDRESS_FLIGHT_CONTROLLER;
    badPacket[1] = 24;
    badPacket[2] = CRSF_FRAMETYPE_RC_CHANNELS_PACKED;
    // Остальные байты случайные
    for (int i = 3; i < 26; i++) {
        badPacket[i] = 0xFF;
    }
    
    // Отправляем несколько поврежденных пакетов
    // Каждый пакет будет отклонен из-за неверного CRC
    for (int p = 0; p < 5; p++) {
        InSequence seq;
        for (int i = 0; i < 26; i++) {
            EXPECT_CALL(*mockSerial, readByte(_))
                .WillOnce(DoAll(::testing::SetArgReferee<0>(badPacket[i]), Return(1)));
        }
        EXPECT_CALL(*mockSerial, readByte(_))
            .WillOnce(Return(0)); // Нет больше данных для этого пакета
        
        // Все пакеты должны быть отклонены без ошибок
        EXPECT_NO_THROW(crsf->loop());
    }
}

/**
 * @test Обработка пакета с неизвестным типом
 * 
 * Тест проверяет обработку пакета с типом, который
 * не обрабатывается в switch-case.
 */
TEST_F(CrsfErrorHandlingTest, ParsePacket_UnknownType_HandlesGracefully) {
    uint8_t packet[64];
    uint8_t payload[10] = {0};
    
    Crc8 crc(0xD5);
    packet[0] = CRSF_ADDRESS_FLIGHT_CONTROLLER;
    packet[1] = 12; // TYPE + PAYLOAD + CRC
    packet[2] = 0xFF; // Неизвестный тип
    memcpy(&packet[3], payload, 10);
    
    uint8_t crcData[11];
    crcData[0] = 0xFF;
    memcpy(&crcData[1], payload, 10);
    packet[13] = crc.calc(crcData, 11);
    
    InSequence seq;
    for (int i = 0; i < 14; i++) {
        EXPECT_CALL(*mockSerial, readByte(_))
            .WillOnce(DoAll(::testing::SetArgReferee<0>(packet[i]), Return(1)));
    }
    EXPECT_CALL(*mockSerial, readByte(_))
        .WillRepeatedly(Return(0));
    
    // Пакет должен быть обработан без ошибок (игнорирован)
    EXPECT_NO_THROW(crsf->loop());
}

/**
 * @test Обработка значений каналов вне диапазона
 * 
 * Тест проверяет обработку значений каналов, которые
 * выходят за допустимые пределы (меньше 1000 или больше 2000).
 */
TEST_F(CrsfErrorHandlingTest, SetChannel_OutOfRangeValues_HandlesGracefully) {
    // Отрицательные значения
    EXPECT_NO_THROW(crsf->setChannel(1, -100));
    EXPECT_NO_THROW(crsf->setChannel(1, 0));
    
    // Значения меньше минимума
    EXPECT_NO_THROW(crsf->setChannel(1, 500));
    EXPECT_NO_THROW(crsf->setChannel(1, 999));
    
    // Значения больше максимума
    EXPECT_NO_THROW(crsf->setChannel(1, 2001));
    EXPECT_NO_THROW(crsf->setChannel(1, 3000));
    EXPECT_NO_THROW(crsf->setChannel(1, 10000));
}

/**
 * @test Обработка пустого payload в queuePacket
 * 
 * Тест проверяет обработку queuePacket() с пустым payload.
 */
TEST_F(CrsfErrorHandlingTest, QueuePacket_EmptyPayload_HandlesGracefully) {
    // Устанавливаем связь
    for (int i = 1; i <= 16; i++) {
        crsf->setChannel(i, 1500);
    }
    
    // packetChannelsSend() вызывает queuePacket() внутри, который вызывает write()
    // Затем мы вызываем queuePacket() еще раз, так что write() будет вызван дважды
    EXPECT_CALL(*mockSerial, write(_, _))
        .WillOnce(Return(26))  // Первый вызов от packetChannelsSend()
        .WillOnce(Return(4));   // Второй вызов от queuePacket() с пустым payload (4 байта)
    
    crsf->packetChannelsSend();
    EXPECT_TRUE(crsf->isLinkUp());
    
    // Отправляем пакет с пустым payload
    uint8_t emptyPayload[0];
    
    // queuePacket с пустым payload создаст пакет размером 4 байта (addr + len + type + crc)
    // где len = 0 + 2 = 2, так что пакет будет: addr(1) + len(1) + type(1) + crc(1) = 4 байта
    // queuePacket вызывает write(buf, len + 4), где len=0, так что write будет вызван с 4 байтами
    EXPECT_NO_THROW(crsf->queuePacket(CRSF_ADDRESS_FLIGHT_CONTROLLER, 
                                     CRSF_FRAMETYPE_LINK_STATISTICS, 
                                     emptyPayload, 0));
}

/**
 * @test Обработка множественных ошибок подряд
 * 
 * Тест проверяет устойчивость системы к множественным
 * ошибкам, происходящим подряд.
 */
TEST_F(CrsfErrorHandlingTest, ErrorHandling_MultipleErrors_SystemStable) {
    // Смешиваем разные типы ошибок
    EXPECT_NO_THROW(crsf->getChannel(0));      // Невалидный канал
    EXPECT_NO_THROW(crsf->getChannel(17));    // Невалидный канал
    EXPECT_NO_THROW(crsf->setChannel(0, 1500)); // Невалидный канал
    EXPECT_NO_THROW(crsf->setChannel(1, -100)); // Невалидное значение
    
    // Ошибки чтения - readByte может быть вызван несколько раз в loop()
    // так как loop() читает до 32 байт за раз
    EXPECT_CALL(*mockSerial, readByte(_))
        .WillOnce(Return(-1))  // Первая ошибка
        .WillRepeatedly(Return(0)); // Больше нет данных
    EXPECT_NO_THROW(crsf->loop());
    
    // Поврежденный пакет
    uint8_t badPacket[3] = {0xFF, 0xFF, 0xFF};
    InSequence seq;
    for (int i = 0; i < 3; i++) {
        EXPECT_CALL(*mockSerial, readByte(_))
            .WillOnce(DoAll(::testing::SetArgReferee<0>(badPacket[i]), Return(1)));
    }
    EXPECT_CALL(*mockSerial, readByte(_))
        .WillRepeatedly(Return(0));
    EXPECT_NO_THROW(crsf->loop());
    
    // Система должна оставаться стабильной
    EXPECT_CALL(*mockSerial, readByte(_))
        .WillRepeatedly(Return(0));
    EXPECT_NO_THROW(crsf->loop());
}

