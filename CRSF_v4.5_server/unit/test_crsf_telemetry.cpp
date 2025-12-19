/**
 * @file test_crsf_telemetry.cpp
 * @brief Unit тесты для телеметрии CRSF протокола
 * 
 * Тесты проверяют функциональность получения телеметрических данных:
 * - Данные батареи (напряжение, ток, емкость, оставшийся заряд)
 * - Данные положения (attitude: roll, pitch, yaw)
 * - Сырые значения attitude (без преобразования)
 * - Статистика связи
 * - GPS данные (координаты, скорость, высота)
 * 
 * Телеметрия критична для мониторинга состояния дрона в реальном времени
 * и отображения информации пилоту через наземную станцию управления.
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
 * @class CrsfTelemetryTest
 * @brief Фикстура для тестов телеметрии
 * 
 * Настраивает мок последовательного порта и экземпляр CrsfSerial
 * для тестирования функциональности получения телеметрических данных.
 */
class CrsfTelemetryTest : public ::testing::Test {
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
 * @test Проверка начальных значений данных батареи
 * 
 * Тест проверяет, что при инициализации все параметры батареи
 * имеют значения по умолчанию (0.0 или 0).
 * 
 * Параметры батареи:
 * - Напряжение (Voltage): в вольтах, обычно 0.0-25.2V для LiPo
 * - Ток (Current): в амперах, может быть положительным (разряд) или отрицательным (заряд)
 * - Емкость (Capacity): в мАч, накопленная емкость с начала полета
 * - Оставшийся заряд (Remaining): в процентах, 0-100%
 */
TEST_F(CrsfTelemetryTest, GetBatteryData_InitialState_ReturnsDefaultValues) {
    // Act & Assert: проверяем начальные значения батареи
    // До получения телеметрии все значения должны быть нулевыми
    EXPECT_DOUBLE_EQ(crsf->getBatteryVoltage(), 0.0);
    EXPECT_DOUBLE_EQ(crsf->getBatteryCurrent(), 0.0);
    EXPECT_DOUBLE_EQ(crsf->getBatteryCapacity(), 0.0);
    EXPECT_EQ(crsf->getBatteryRemaining(), 0);
}

/**
 * @test Проверка начальных значений данных положения (attitude)
 * 
 * Тест проверяет, что при инициализации все углы положения
 * имеют значения по умолчанию (0.0 градусов).
 * 
 * Attitude (положение) дрона:
 * - Roll (крен): вращение вокруг продольной оси, -180° до +180°
 * - Pitch (тангаж): вращение вокруг поперечной оси, -180° до +180°
 * - Yaw (рыскание): вращение вокруг вертикальной оси, 0° до 360°
 * 
 * Значения возвращаются в градусах после преобразования из сырых данных.
 */
TEST_F(CrsfTelemetryTest, GetAttitudeData_InitialState_ReturnsDefaultValues) {
    // Act & Assert: проверяем начальные значения положения
    // До получения телеметрии все углы должны быть нулевыми
    EXPECT_DOUBLE_EQ(crsf->getAttitudeRoll(), 0.0);
    EXPECT_DOUBLE_EQ(crsf->getAttitudePitch(), 0.0);
    EXPECT_DOUBLE_EQ(crsf->getAttitudeYaw(), 0.0);
}

/**
 * @test Проверка начальных сырых значений attitude
 * 
 * Тест проверяет, что при инициализации все сырые значения attitude
 * равны нулю. Сырые значения - это данные напрямую из CRSF пакета
 * до преобразования в градусы.
 * 
 * Сырые значения используются для:
 * - Отладки протокола
 * - Прямой работы с данными без преобразований
 * - Проверки корректности приема пакетов
 */
TEST_F(CrsfTelemetryTest, GetRawAttitudeData_InitialState_ReturnsZero) {
    // Act & Assert: проверяем начальные сырые значения attitude
    // До получения телеметрии все сырые значения должны быть нулевыми
    EXPECT_EQ(crsf->getRawAttitudeRoll(), 0);
    EXPECT_EQ(crsf->getRawAttitudePitch(), 0);
    EXPECT_EQ(crsf->getRawAttitudeYaw(), 0);
}

/**
 * @test Проверка получения статистики связи в начальном состоянии
 * 
 * Тест проверяет, что метод getLinkStatistics() возвращает
 * валидный указатель на структуру статистики связи даже в начальном состоянии.
 * 
 * Структура статистики связи содержит информацию о качестве связи,
 * которая обновляется при получении пакетов от приемника.
 */
TEST_F(CrsfTelemetryTest, GetLinkStatistics_InitialState_ReturnsValidStructure) {
    // Act: получаем указатель на статистику связи
    const crsfLinkStatistics_t* stats = crsf->getLinkStatistics();
    
    // Assert: проверяем, что структура существует (не nullptr)
    // Это гарантирует, что статистика доступна для чтения даже до установления связи
    EXPECT_NE(stats, nullptr);
}

/**
 * @test Проверка получения GPS данных в начальном состоянии
 * 
 * Тест проверяет, что метод getGpsSensor() возвращает
 * валидный указатель на структуру GPS данных даже в начальном состоянии.
 * 
 * Структура GPS данных содержит:
 * - Координаты (широта, долгота)
 * - Высоту над уровнем моря
 * - Скорость (ground speed)
 * - Количество спутников
 * - Точность позиционирования
 * 
 * Эти данные обновляются при получении GPS телеметрии от полетного контроллера.
 */
TEST_F(CrsfTelemetryTest, GetGpsSensor_InitialState_ReturnsValidPointer) {
    // Act: получаем указатель на GPS данные
    const crsf_sensor_gps_t* gps = crsf->getGpsSensor();
    
    // Assert: проверяем, что структура существует (не nullptr)
    // Это гарантирует, что GPS данные доступны для чтения даже до получения GPS сигнала
    EXPECT_NE(gps, nullptr);
}