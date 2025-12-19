/**
 * @file test_crsf_basic_operations.cpp
 * @brief Unit тесты для базовых операций CRSF протокола
 * 
 * Тесты проверяют основные операции работы с CRSF протоколом:
 * - Обработка цикла loop() при отсутствии данных
 * - Запись байт и буферов данных
 * - Отправка пакетов каналов
 * 
 * Используется MockSerialPort для изоляции тестов от реального оборудования.
 * 
 * @version 4.3
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include "../libs/crsf/CrsfSerial.h"
#include "mocks/MockSerialPort.h"

using ::testing::_;
using ::testing::Return;

/**
 * @class CrsfBasicOperationsTest
 * @brief Фикстура для тестов базовых операций CRSF
 * 
 * Настраивает мок последовательного порта и экземпляр CrsfSerial
 * перед каждым тестом для обеспечения изоляции тестов.
 */
class CrsfBasicOperationsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Создаем мок последовательного порта для изоляции тестов
        mockSerial = std::make_unique<MockSerialPort>();
        // Создаем экземпляр CrsfSerial с моком и стандартной скоростью CRSF (420000 бод)
        crsf = std::make_unique<CrsfSerial>(*mockSerial, 420000);
    }
    
    std::unique_ptr<MockSerialPort> mockSerial;  // Мок последовательного порта
    std::unique_ptr<CrsfSerial> crsf;            // Экземпляр CRSF для тестирования
};

/**
 * @test Проверка метода loop() при отсутствии данных
 * 
 * Тест проверяет, что основной цикл обработки CRSF протокола
 * корректно работает когда в последовательном порту нет данных.
 * Это нормальная ситуация, когда передатчик не активен.
 */
TEST_F(CrsfBasicOperationsTest, Loop_NoDataAvailable_NoThrow) {
    // Arrange: настраиваем мок так, чтобы readByte возвращал 0 (нет данных)
    EXPECT_CALL(*mockSerial, readByte(_))
        .WillRepeatedly(Return(0));
    
    // Act & Assert: цикл должен выполняться без исключений
    EXPECT_NO_THROW(crsf->loop());
}

/**
 * @test Проверка записи одного байта
 * 
 * Тест проверяет базовую операцию записи одного байта в CRSF протокол.
 * Это используется для отправки управляющих команд и заголовков пакетов.
 */
TEST_F(CrsfBasicOperationsTest, WriteByte_ValidByte_NoThrow) {
    // Arrange: настраиваем мок для записи одного байта
    EXPECT_CALL(*mockSerial, writeByte(_))
        .WillOnce(Return(1));  // Успешная запись 1 байта
    
    // Act & Assert: запись байта должна выполняться без исключений
    EXPECT_NO_THROW(crsf->write(0xAB));
}

/**
 * @test Проверка записи буфера данных
 * 
 * Тест проверяет запись массива байт в CRSF протокол.
 * Это используется для отправки полных пакетов данных.
 */
TEST_F(CrsfBasicOperationsTest, WriteBuffer_ValidData_NoThrow) {
    // Arrange: создаем тестовый буфер и настраиваем мок
    uint8_t testData[] = {0x01, 0x02, 0x03};
    EXPECT_CALL(*mockSerial, write(_, 3))
        .WillOnce(Return(3));  // Успешная запись всех 3 байт
    
    // Act & Assert: запись буфера должна выполняться без исключений
    EXPECT_NO_THROW(crsf->write(testData, 3));
}

/**
 * @test Проверка отправки пакета каналов
 * 
 * Тест проверяет отправку пакета с данными всех 16 каналов управления.
 * Это основной пакет в CRSF протоколе, отправляемый с частотой ~100 Гц.
 * 
 * Структура пакета каналов CRSF:
 * - Адрес получателя: 1 байт
 * - Длина пакета: 1 байт
 * - Тип пакета: 1 байт (CRSF_FRAMETYPE_RC_CHANNELS_PACKED)
 * - Данные каналов: 22 байта (16 каналов по 11 бит каждый, упакованные)
 * - CRC8: 1 байт
 * Итого: 26 байт
 */
TEST_F(CrsfBasicOperationsTest, PacketChannelsSend_WhenCalled_NoThrow) {
    // Arrange: устанавливаем значения для всех 16 каналов (1500 = центр)
    for (int i = 1; i <= 16; i++) {
        crsf->setChannel(i, 1500);
    }
    
    // Настраиваем ожидание отправки пакета размером 26 байт
    EXPECT_CALL(*mockSerial, write(_, _))
        .WillOnce(Return(26));
    
    // Act & Assert: отправка пакета каналов должна выполняться без исключений
    EXPECT_NO_THROW(crsf->packetChannelsSend());
}