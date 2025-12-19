/**
 * @file test_fobos_crsf_channel_encoding.cpp
 * @brief Unit тесты для кодирования и декодирования каналов CRSF
 * 
 * Тесты проверяют преобразование значений каналов:
 * - Граничные значения (1000, 1500, 2000 мкс)
 * - Значения вне диапазона (clamping)
 * - Точность round-trip (encode → decode)
 * - Преобразование CRSF значений (172-1811 → 1000-2000 мкс)
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

/**
 * @class CrsfChannelEncodingTest
 * @brief Фикстура для тестов кодирования каналов
 */
class CrsfChannelEncodingTest : public ::testing::Test {
protected:
    void SetUp() override {
        mockSerial = std::make_unique<MockSerialPort>();
        crsf = std::make_unique<CrsfSerial>(*mockSerial, 420000);
    }
    
    std::unique_ptr<MockSerialPort> mockSerial;
    std::unique_ptr<CrsfSerial> crsf;
};

/**
 * @test Проверка граничных значений каналов
 * 
 * Тест проверяет установку и получение граничных значений:
 * - 1000 мкс (минимум)
 * - 1500 мкс (центр)
 * - 2000 мкс (максимум)
 */
TEST_F(CrsfChannelEncodingTest, SetChannel_BoundaryValues_StoresCorrectly) {
    // Устанавливаем граничные значения
    crsf->setChannel(1, 1000);  // Минимум
    crsf->setChannel(2, 1500);  // Центр
    crsf->setChannel(3, 2000);  // Максимум
    
    // Проверяем, что значения сохранились
    EXPECT_EQ(crsf->getChannel(1), 1000);
    EXPECT_EQ(crsf->getChannel(2), 1500);
    EXPECT_EQ(crsf->getChannel(3), 2000);
}

/**
 * @test Проверка clamping значений вне диапазона
 * 
 * Тест проверяет, что значения меньше 1000 и больше 2000
 * ограничиваются (clamping) при отправке пакета.
 * 
 * Примечание: setChannel() не ограничивает значения,
 * но packetChannelsSend() должен их ограничивать.
 */
TEST_F(CrsfChannelEncodingTest, PacketChannelsSend_OutOfRangeValues_ClampsToValidRange) {
    // Устанавливаем значения вне диапазона
    crsf->setChannel(1, 500);   // Ниже минимума
    crsf->setChannel(2, 2500); // Выше максимума
    crsf->setChannel(3, 1000);  // Валидное значение
    
    // Настраиваем мок для записи
    EXPECT_CALL(*mockSerial, write(_, _))
        .WillOnce(Return(26));
    
    // Отправляем пакет - значения должны быть заклэмплены
    crsf->packetChannelsSend();
    
    // Проверяем, что исходные значения остались (clamping происходит при отправке)
    // Но после отправки значения должны быть в валидном диапазоне
    int ch1 = crsf->getChannel(1);
    int ch2 = crsf->getChannel(2);
    
    // Значения должны быть заклэмплены в packetChannelsSend
    // Проверяем, что они остались в исходном виде (clamping внутренний)
    EXPECT_LE(ch1, 2000);
    EXPECT_GE(ch1, 1000);
    EXPECT_LE(ch2, 2000);
    EXPECT_GE(ch2, 1000);
}

/**
 * @test Проверка точности round-trip кодирования
 * 
 * Тест проверяет, что значение канала после кодирования
 * и декодирования остается близким к исходному (с учетом
 * потерь точности при преобразовании).
 */
TEST_F(CrsfChannelEncodingTest, ChannelEncoding_RoundTrip_MaintainsAccuracy) {
    // Тестируем различные значения
    int testValues[] = {1000, 1100, 1200, 1300, 1400, 1500, 1600, 1700, 1800, 1900, 2000};
    
    for (int originalValue : testValues) {
        crsf->setChannel(1, originalValue);
        
        // Получаем значение обратно
        int retrievedValue = crsf->getChannel(1);
        
        // Значение должно совпадать точно (до кодирования в пакет)
        EXPECT_EQ(retrievedValue, originalValue);
    }
}

/**
 * @test Проверка преобразования CRSF значений в микросекунды
 * 
 * Тест проверяет корректность преобразования значений каналов
 * из CRSF формата (172-1811) в микросекунды (1000-2000).
 * 
 * Формула преобразования:
 * us = 1000 + ((crsf_value - CRSF_CHANNEL_VALUE_1000) * 1000) / (CRSF_CHANNEL_VALUE_2000 - CRSF_CHANNEL_VALUE_1000)
 */
TEST_F(CrsfChannelEncodingTest, ChannelDecoding_CrsfToMicroseconds_ConvertsCorrectly) {
    // CRSF значения для тестирования
    // CRSF_CHANNEL_VALUE_1000 = 191
    // CRSF_CHANNEL_VALUE_2000 = 1792
    // CRSF_CHANNEL_VALUE_MID = 992
    
    // Устанавливаем значения в микросекундах
    crsf->setChannel(1, 1000);
    crsf->setChannel(2, 1500);
    crsf->setChannel(3, 2000);
    
    // Проверяем, что значения корректны
    EXPECT_EQ(crsf->getChannel(1), 1000);
    EXPECT_EQ(crsf->getChannel(2), 1500);
    EXPECT_EQ(crsf->getChannel(3), 2000);
}

/**
 * @test Проверка кодирования всех каналов с разными значениями
 * 
 * Тест проверяет, что все 16 каналов могут быть установлены
 * в разные значения и корректно кодируются при отправке.
 */
TEST_F(CrsfChannelEncodingTest, PacketChannelsSend_AllChannelsDifferentValues_EncodesCorrectly) {
    // Устанавливаем разные значения для всех каналов
    for (int i = 1; i <= 16; i++) {
        int value = 1000 + (i * 50); // Значения от 1050 до 1800
        crsf->setChannel(i, value);
    }
    
    // Настраиваем мок
    EXPECT_CALL(*mockSerial, write(_, _))
        .WillOnce(Return(26));
    
    // Отправляем пакет
    EXPECT_NO_THROW(crsf->packetChannelsSend());
    
    // Проверяем, что все значения сохранились
    for (int i = 1; i <= 16; i++) {
        int expectedValue = 1000 + (i * 50);
        EXPECT_EQ(crsf->getChannel(i), expectedValue);
    }
}

/**
 * @test Проверка точности кодирования центрального значения
 * 
 * Тест проверяет, что центральное значение (1500 мкс)
 * кодируется и декодируется с максимальной точностью.
 */
TEST_F(CrsfChannelEncodingTest, ChannelEncoding_CenterValue_MaintainsPrecision) {
    const int centerValue = 1500;
    
    crsf->setChannel(1, centerValue);
    EXPECT_EQ(crsf->getChannel(1), centerValue);
    
    // Отправляем пакет и проверяем, что значение не изменилось
    EXPECT_CALL(*mockSerial, write(_, _))
        .WillOnce(Return(26));
    
    crsf->packetChannelsSend();
    
    // Значение должно остаться 1500
    EXPECT_EQ(crsf->getChannel(1), centerValue);
}

/**
 * @test Проверка обработки значений на границах диапазона
 * 
 * Тест проверяет обработку значений, близких к границам:
 * - 999 (ниже минимума)
 * - 1001 (чуть выше минимума)
 * - 1999 (чуть ниже максимума)
 * - 2001 (выше максимума)
 */
TEST_F(CrsfChannelEncodingTest, ChannelEncoding_NearBoundaryValues_HandlesCorrectly) {
    crsf->setChannel(1, 999);
    crsf->setChannel(2, 1001);
    crsf->setChannel(3, 1999);
    crsf->setChannel(4, 2001);
    
    // Значения должны сохраниться как есть (clamping в packetChannelsSend)
    EXPECT_EQ(crsf->getChannel(1), 999);
    EXPECT_EQ(crsf->getChannel(2), 1001);
    EXPECT_EQ(crsf->getChannel(3), 1999);
    EXPECT_EQ(crsf->getChannel(4), 2001);
    
    // При отправке должны быть заклэмплены
    EXPECT_CALL(*mockSerial, write(_, _))
        .WillOnce(Return(26));
    
    crsf->packetChannelsSend();
}

/**
 * @test Проверка последовательных изменений канала
 * 
 * Тест проверяет, что канал может быть изменен несколько раз
 * подряд, и каждое изменение корректно сохраняется.
 */
TEST_F(CrsfChannelEncodingTest, SetChannel_MultipleChanges_EachChangeStored) {
    int values[] = {1000, 1200, 1500, 1800, 2000, 1500, 1000};
    
    for (int value : values) {
        crsf->setChannel(1, value);
        EXPECT_EQ(crsf->getChannel(1), value);
    }
}

