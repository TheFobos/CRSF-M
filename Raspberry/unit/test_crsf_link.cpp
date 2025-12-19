/**
 * @file test_crsf_link.cpp
 * @brief Unit тесты для состояния связи CRSF протокола
 * 
 * Тесты проверяют функциональность мониторинга состояния связи:
 * - Проверка начального состояния связи (должно быть false)
 * - Получение статистики связи (качество сигнала, потеря пакетов и т.д.)
 * 
 * Состояние связи критично для безопасной работы дрона,
 * так как при потере связи должен активироваться fail-safe режим.
 * 
 * @version 4.3
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include "../libs/crsf/CrsfSerial.h"
#include "../libs/crsf/crsf_protocol.h"
#include "mocks/MockSerialPort.h"

/**
 * @class CrsfLinkTest
 * @brief Фикстура для тестов состояния связи
 * 
 * Настраивает мок последовательного порта и экземпляр CrsfSerial
 * для тестирования функциональности мониторинга связи.
 */
class CrsfLinkTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Создаем мок последовательного порта
        mockSerial = std::make_unique<MockSerialPort>();
        // Создаем экземпляр CrsfSerial с моком
        crsf = std::make_unique<CrsfSerial>(*mockSerial, 420000);
    }
    
    std::unique_ptr<MockSerialPort> mockSerial;  // Мок последовательного порта
    std::unique_ptr<CrsfSerial> crsf;            // Экземпляр CRSF для тестирования
};

/**
 * @test Проверка начального состояния связи
 * 
 * Тест проверяет, что при инициализации CrsfSerial
 * состояние связи должно быть false (связь не установлена).
 * 
 * Это важно для корректной работы fail-safe механизма:
 * до установления связи система должна находиться в безопасном состоянии.
 */
TEST_F(CrsfLinkTest, IsLinkUp_InitialState_ReturnsFalse) {
    // Act & Assert: проверяем начальное состояние связи
    // После создания объекта связь еще не установлена
    EXPECT_FALSE(crsf->isLinkUp());
}

/**
 * @test Проверка получения статистики связи
 * 
 * Тест проверяет, что метод getLinkStatistics() возвращает
 * валидный указатель на структуру со статистикой связи.
 * 
 * Структура crsfLinkStatistics_t содержит информацию о:
 * - Качестве сигнала (RSSI)
 * - Количестве потерянных пакетов
 * - Количестве успешно принятых пакетов
 * - Времени последнего успешного пакета
 * 
 * Эта информация используется для мониторинга качества связи
 * и принятия решений о fail-safe активации.
 */
TEST_F(CrsfLinkTest, GetLinkStatistics_ReturnsValidPointer) {
    // Act: получаем указатель на статистику связи
    const crsfLinkStatistics_t* stats = crsf->getLinkStatistics();
    
    // Assert: проверяем, что указатель не равен nullptr
    // Это гарантирует, что статистика доступна для чтения
    EXPECT_NE(stats, nullptr);
}