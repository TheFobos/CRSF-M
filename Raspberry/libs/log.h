#pragma once

// Простой вывод в консоль для отладки. Управляется флагом USE_LOG из config.h

#include <string>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include "../config.h"

inline std::string log_timestamp() {
    using namespace std::chrono;
    auto now = system_clock::now();
    auto now_time_t = system_clock::to_time_t(now);
    auto now_ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
    std::tm tm{};
    localtime_r(&now_time_t, &tm);
    std::ostringstream oss;
    oss << '['
        << std::setfill('0')
        << std::setw(4) << (tm.tm_year + 1900) << '-'
        << std::setw(2) << (tm.tm_mon + 1) << '-'
        << std::setw(2) << tm.tm_mday << ' '
        << std::setw(2) << tm.tm_hour << ':'
        << std::setw(2) << tm.tm_min << ':'
        << std::setw(2) << tm.tm_sec << '.'
        << std::setw(3) << now_ms.count()
        << "] ";
    return oss.str();
}

inline void log_info(const std::string &msg) {
#if USE_LOG
    std::cout << log_timestamp() << "[INFO] " << msg << std::endl;
#else
    (void)msg;
#endif
}

inline void log_warn(const std::string &msg) {
#if USE_LOG
    std::cout << log_timestamp() << "[WARN] " << msg << std::endl;
#else
    (void)msg;
#endif
}

inline void log_error(const std::string &msg) {
#if USE_LOG
    std::cerr << log_timestamp() << "[ERROR] " << msg << std::endl;
#else
    (void)msg;
#endif
}


