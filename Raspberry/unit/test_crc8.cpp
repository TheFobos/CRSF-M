/**
 * @file test_crc8.cpp
 * @brief Unit тесты для модуля вычисления CRC8 контрольной суммы
 * 
 * CRC8 используется в CRSF протоколе для проверки целостности пакетов.
 * Полином 0xD5 соответствует стандарту CRSF протокола.
 * 
 * @version 4.3
 */

#include <gtest/gtest.h>
#include "../libs/crsf/crc8.h"

/**
 * @test Проверка вычисления CRC8 для пустых данных
 * 
 * Тест проверяет граничный случай - вычисление CRC для пустого буфера.
 * Для пустых данных CRC должен быть равен начальному значению (0),
 * так как не было обработано ни одного байта.
 */
TEST(Crc8Test, Calculate_EmptyData_ReturnsInitialCrc) {
    // Arrange: создаем объект CRC8 с полиномом 0xD5 (стандарт CRSF)
    Crc8 crc(0xD5);
    uint8_t data[] = {};
    
    // Act: вычисляем CRC для пустого буфера
    uint8_t result = crc.calc(data, 0);
    
    // Assert: для пустых данных CRC должен быть равен 0
    EXPECT_EQ(result, 0);
}

/**
 * @test Проверка вычисления CRC8 для одного байта
 * 
 * Тест проверяет базовую функциональность - вычисление CRC для одного байта.
 * Результат должен быть ненулевым, так как был обработан хотя бы один байт.
 */
TEST(Crc8Test, Calculate_SingleByte_ReturnsNonZero) {
    // Arrange: создаем объект CRC8 и тестовый байт
    Crc8 crc(0xD5);
    uint8_t data[] = {0x01};
    
    // Act: вычисляем CRC для одного байта
    uint8_t result = crc.calc(data, 1);
    
    // Assert: результат должен быть ненулевым
    EXPECT_NE(result, 0);
}

/**
 * @test Проверка вычисления CRC8 для нескольких байт
 * 
 * Тест проверяет вычисление CRC для массива байт.
 * Это типичный случай использования в CRSF протоколе,
 * где пакеты содержат несколько байт данных.
 */
TEST(Crc8Test, Calculate_MultipleBytes_ReturnsValidCrc) {
    // Arrange: создаем объект CRC8 и тестовый массив байт
    Crc8 crc(0xD5);
    uint8_t data[] = {0x01, 0x02, 0x03};
    
    // Act: вычисляем CRC для массива из 3 байт
    uint8_t result = crc.calc(data, 3);
    
    // Assert: результат должен быть валидным (ненулевым)
    EXPECT_NE(result, 0);
}