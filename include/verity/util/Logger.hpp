#pragma once
// VERITY GRAPH — Logger (Singleton).
//
// This is the ONLY singleton in the project (per PHASES.md day-1 rules).
// Meyers singleton => thread-safe initialization guaranteed by C++11 magic
// statics. All sinks (stderr + optional file) are mutex-protected.
//
// Levels: Debug < Info < Warn < Error. Default level: Info.
// Complexity: O(1) per log call (amortized; file writes are line-buffered).

#include <fstream>
#include <mutex>
#include <string>

namespace verity::util {

class Logger {
public:
    enum class Level { Debug = 0, Info = 1, Warn = 2, Error = 3 };

    // Global access point (the single allowed singleton).
    static Logger& instance() noexcept;

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(Logger&&) = delete;

    void setLevel(Level level) noexcept;
    [[nodiscard]] Level level() const noexcept;

    // Enable an additional file sink (append mode). Returns false when the
    // file cannot be opened (stderr logging continues regardless).
    bool setLogFile(const std::string& path);
    void closeLogFile() noexcept;

    void debug(const std::string& message);
    void info(const std::string& message);
    void warn(const std::string& message);
    void error(const std::string& message);

    void log(Level level, const std::string& message);

    static const char* levelName(Level level) noexcept;

private:
    Logger() = default;
    ~Logger() = default;

    void write(Level level, const std::string& message);

    mutable std::mutex mutex_;
    Level level_ = Level::Info;
    std::ofstream file_;
    bool file_ok_ = false;
};

} // namespace verity::util
