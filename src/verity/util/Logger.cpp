#include "verity/util/Logger.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace verity::util {

Logger& Logger::instance() noexcept {
    static Logger logger;
    return logger;
}

void Logger::setLevel(Level level) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    level_ = level;
}

Logger::Level Logger::level() const noexcept { return level_; }

bool Logger::setLogFile(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    file_.close();
    file_.clear();
    file_.open(path, std::ios::app);
    file_ok_ = static_cast<bool>(file_);
    return file_ok_;
}

void Logger::closeLogFile() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_.is_open()) file_.close();
    file_ok_ = false;
}

const char* Logger::levelName(Level level) noexcept {
    switch (level) {
        case Level::Debug: return "DEBUG";
        case Level::Info:  return "INFO ";
        case Level::Warn:  return "WARN ";
        case Level::Error: return "ERROR";
    }
    return "INFO ";
}

void Logger::debug(const std::string& m) { log(Level::Debug, m); }
void Logger::info(const std::string& m) { log(Level::Info, m); }
void Logger::warn(const std::string& m) { log(Level::Warn, m); }
void Logger::error(const std::string& m) { log(Level::Error, m); }

void Logger::log(Level level, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (static_cast<int>(level) < static_cast<int>(level_)) return;
    write(level, message);
}

void Logger::write(Level level, const std::string& message) {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream stamp;
    stamp << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");

    const std::string line =
        stamp.str() + " [" + levelName(level) + "] " + message + "\n";
    std::cerr << line;
    if (file_ok_ && file_.is_open()) {
        file_ << line;
        file_.flush();
    }
}

} // namespace verity::util
