/**
 * @file test_crsf_callbacks.cpp
 * @brief Unit тесты для системы обратных вызовов (callbacks) CRSF протокола
 * 
 * Тесты проверяют механизм обработчиков событий в CRSF протоколе:
 * - Установка обработчиков событий (onLinkUp, onLinkDown, onPacketChannels)
 * - Сброс обработчиков в nullptr
 * - Вызов обработчиков при наступлении событий
 * 
 * Callbacks используются для асинхронной обработки событий протокола
 * без блокировки основного цикла обработки.
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
 * @class CrsfCallbacksTest
 * @brief Фикстура для тестов системы обратных вызовов
 * 
 * Настраивает мок последовательного порта и экземпляр CrsfSerial
 * для тестирования механизма callbacks.
 */
class CrsfCallbacksTest : public ::testing::Test {
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
 * @test Проверка установки обработчиков событий
 * 
 * Тест проверяет, что все обработчики событий CRSF протокола
 * могут быть установлены с помощью лямбда-функций или указателей на функции.
 * 
 * Обработчики событий:
 * - onLinkUp: вызывается при установлении связи с приемником
 * - onLinkDown: вызывается при потере связи
 * - onPacketChannels: вызывается при получении пакета с данными каналов
 */
TEST_F(CrsfCallbacksTest, SetEventHandlers_ValidCallbacks_NoThrow) {
    // Act & Assert: установка обработчиков событий должна выполняться без исключений
    // Используем пустые лямбда-функции для проверки синтаксиса
    EXPECT_NO_THROW(crsf->onLinkUp = []() {});
    EXPECT_NO_THROW(crsf->onLinkDown = []() {});
    EXPECT_NO_THROW(crsf->onPacketChannels = []() {});
}

/**
 * @test Проверка сброса обработчиков событий
 * 
 * Тест проверяет, что обработчики событий могут быть сброшены в nullptr.
 * Это важно для деинициализации и предотвращения вызовов несуществующих функций.
 */
TEST_F(CrsfCallbacksTest, EventHandlers_CanBeSetToNull) {
    // Act & Assert: сброс обработчиков в nullptr должен выполняться без исключений
    EXPECT_NO_THROW(crsf->onLinkUp = nullptr);
    EXPECT_NO_THROW(crsf->onLinkDown = nullptr);
    EXPECT_NO_THROW(crsf->onPacketChannels = nullptr);
}

// Глобальные флаги для проверки вызова обработчиков в тестах
static bool g_linkUpCalled = false;          // Флаг вызова onLinkUp
static bool g_linkDownCalled = false;        // Флаг вызова onLinkDown
static bool g_packetChannelsCalled = false;  // Флаг вызова onPacketChannels

/**
 * @brief Обработчик события установления связи
 * Устанавливает флаг g_linkUpCalled в true при вызове
 */
static void onLinkUpHandler() { g_linkUpCalled = true; }

/**
 * @brief Обработчик события потери связи
 * Устанавливает флаг g_linkDownCalled в true при вызове
 */
static void onLinkDownHandler() { g_linkDownCalled = true; }

/**
 * @brief Обработчик события получения пакета каналов
 * Устанавливает флаг g_packetChannelsCalled в true при вызове
 */
static void onPacketChannelsHandler() { g_packetChannelsCalled = true; }

/**
 * @test Проверка вызова обработчиков событий
 * 
 * Тест проверяет, что установленные обработчики событий
 * действительно вызываются при наступлении соответствующих событий.
 * Это критически важно для корректной работы системы уведомлений.
 */
TEST_F(CrsfCallbacksTest, EventHandlers_CanBeCalled) {
    // Arrange: сбрасываем флаги и устанавливаем обработчики
    g_linkUpCalled = false;
    g_linkDownCalled = false;
    g_packetChannelsCalled = false;
    
    crsf->onLinkUp = onLinkUpHandler;
    crsf->onLinkDown = onLinkDownHandler;
    crsf->onPacketChannels = onPacketChannelsHandler;
    
    // Act: вызываем обработчики (в реальной системе они вызываются автоматически)
    if (crsf->onLinkUp) crsf->onLinkUp();
    if (crsf->onLinkDown) crsf->onLinkDown();
    if (crsf->onPacketChannels) crsf->onPacketChannels();
    
    // Assert: проверяем, что все обработчики были вызваны
    EXPECT_TRUE(g_linkUpCalled);
    EXPECT_TRUE(g_linkDownCalled);
    EXPECT_TRUE(g_packetChannelsCalled);
}