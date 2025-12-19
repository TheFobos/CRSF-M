#include "rpi_hal.h"

#include <chrono>
#include <thread>
#include <fstream>
#include <sstream>
#include <filesystem>

using ClockSteady = std::chrono::steady_clock;
static const auto processStartTime = ClockSteady::now();

// Время
uint32_t rpi_millis() {
    // Возвращает миллисекунды с момента запуска процесса
    auto now = ClockSteady::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - processStartTime).count();
    return static_cast<uint32_t>(ms & 0xFFFFFFFFu);
}

//БЕСПОЛЕЗНО: функция определена, но нигде не используется
/*
uint32_t rpi_micros() {
    // Возвращает микросекунды с момента запуска процесса
    auto now = ClockSteady::now();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(now - processStartTime).count();
    return static_cast<uint32_t>(us & 0xFFFFFFFFu);
}
*/

void rpi_delay_ms(uint32_t ms) {
    // Простаивает текущий поток на указанное количество миллисекунд
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

// Простые файловые утилиты
bool rpi_write_text_file(const std::string &path, const std::string &text) {
    std::ofstream f(path);
    if (!f.is_open()) {
        // Ошибка открытия файла sysfs
        return false;
    }
    f << text;
    return f.good();
}

bool rpi_read_text_file(const std::string &path, std::string &out) {
    std::ifstream f(path);
    if (!f.is_open()) {
        return false;
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

// GPIO через sysfs (устаревший, но доступен без доп. библиотек)
static inline std::string sysfs_gpio_path(int pin, const char *entry) {
    return std::string("/sys/class/gpio/gpio") + std::to_string(pin) + "/" + entry;
}

bool rpi_gpio_export(RpiPin pin) {
    // Экспортируем пин, если ещё не экспортирован
    std::string path = "/sys/class/gpio/gpio" + std::to_string(pin);
    if (std::filesystem::exists(path)) return true;
    if (!rpi_write_text_file("/sys/class/gpio/export", std::to_string(pin))) {
        return false;
    }
    // Подождём, пока система создаст директорию gpioN
    for (int i = 0; i < 50; ++i) {
        if (std::filesystem::exists(path)) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return std::filesystem::exists(path);
}

bool rpi_gpio_set_mode(RpiPin pin, RpiGpioMode mode) {
    // Устанавливаем направление пина (in/out)
    if (!rpi_gpio_export(pin)) return false;
    std::string dir = (mode == RpiGpioMode::Output) ? "out" : "in";
    return rpi_write_text_file(sysfs_gpio_path(pin, "direction"), dir);
}

bool rpi_gpio_write(RpiPin pin, bool high) {
    // Записываем уровень (0/1) в пин
    if (!rpi_gpio_export(pin)) return false;
    return rpi_write_text_file(sysfs_gpio_path(pin, "value"), high ? "1" : "0");
}

//БЕСПОЛЕЗНО: функция определена, но нигде не используется
/*
bool rpi_gpio_read(RpiPin pin, bool &high) {
    // Читаем уровень из пина
    if (!rpi_gpio_export(pin)) return false;
    std::string s;
    if (!rpi_read_text_file(sysfs_gpio_path(pin, "value"), s)) return false;
    high = (!s.empty() && s[0] == '1');
    return true;
}
*/

// PWM через sysfs pwmchip
static inline std::string sysfs_pwm_path(const RpiPwmChannel &ch, const char *entry) {
    return std::string("/sys/class/pwm/pwmchip") + std::to_string(ch.chip) + "/pwm" + std::to_string(ch.chan) + "/" + entry;
}

bool rpi_pwm_export(const RpiPwmChannel &ch) {
    // Экспортируем канал PWM
    std::string base = std::string("/sys/class/pwm/pwmchip") + std::to_string(ch.chip) + "/pwm" + std::to_string(ch.chan);
    if (std::filesystem::exists(base)) return true;
    if (!rpi_write_text_file(std::string("/sys/class/pwm/pwmchip") + std::to_string(ch.chip) + "/export", std::to_string(ch.chan))) {
        return false;
    }
    // Ждём появления директории pwmN
    for (int i = 0; i < 50; ++i) {
        if (std::filesystem::exists(base)) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return std::filesystem::exists(base);
}

bool rpi_pwm_set_frequency(const RpiPwmChannel &ch, uint32_t hz) {
    // Устанавливаем период через period (нс), duty корректируется отдельно
    if (hz == 0) return false;
    if (!rpi_pwm_export(ch)) return false;
    uint64_t period_ns = 1000000000ull / hz;
    return rpi_write_text_file(sysfs_pwm_path(ch, "period"), std::to_string(period_ns));
}

bool rpi_pwm_set_duty_us(const RpiPwmChannel &ch, uint32_t duty_us) {
    // Устанавливаем скважность в микросекундах через duty_cycle (нс)
    if (!rpi_pwm_export(ch)) return false;
    uint64_t duty_ns = static_cast<uint64_t>(duty_us) * 1000ull;
    return rpi_write_text_file(sysfs_pwm_path(ch, "duty_cycle"), std::to_string(duty_ns));
}

bool rpi_pwm_enable(const RpiPwmChannel &ch, bool enable) {
    // Включаем/выключаем PWM канал
    if (!rpi_pwm_export(ch)) return false;
    return rpi_write_text_file(sysfs_pwm_path(ch, "enable"), enable ? "1" : "0");
}


