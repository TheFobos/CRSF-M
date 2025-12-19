#pragma once

// Простой HAL для Raspberry Pi 5: время, GPIO, PWM
// Все функции снабжены русскими комментариями

#include <cstdint>
#include <string>

// Время
uint32_t rpi_millis();        // миллисекунды с момента запуска процесса
//БЕСПОЛЕЗНО: функция определена, но нигде не используется
//uint32_t rpi_micros();
void rpi_delay_ms(uint32_t);  // пауза в миллисекундах

// GPIO режимы
enum class RpiGpioMode { Input, Output };

// Идентификатор пина — используем BCM-номер
using RpiPin = int;

// Инициализация и управление GPIO через libgpiod (предпочтительно) или sysfs (fallback)
bool rpi_gpio_export(RpiPin pin);
bool rpi_gpio_set_mode(RpiPin pin, RpiGpioMode mode);
bool rpi_gpio_write(RpiPin pin, bool high);
//БЕСПОЛЕЗНО: функция определена, но нигде не используется
//bool rpi_gpio_read(RpiPin pin, bool &high);

// Простой PWM через sysfs pwmchip интерфейс
// Внимание: на Raspberry Pi 5 доступность pwmchip зависит от оверлеев ядра.
// Эти функции делают best-effort и возвращают false при ошибке.
struct RpiPwmChannel {
    int chip;   // номер pwmchipN
    int chan;   // номер канала внутри чипа
};

bool rpi_pwm_export(const RpiPwmChannel &ch);
bool rpi_pwm_set_frequency(const RpiPwmChannel &ch, uint32_t hz);
bool rpi_pwm_set_duty_us(const RpiPwmChannel &ch, uint32_t duty_us);
bool rpi_pwm_enable(const RpiPwmChannel &ch, bool enable);

// Утилита: запись текста в файл
bool rpi_write_text_file(const std::string &path, const std::string &text);
bool rpi_read_text_file(const std::string &path, std::string &out);


