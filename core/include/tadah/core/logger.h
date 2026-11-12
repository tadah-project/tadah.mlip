#ifndef TADAH_CORE_LOGGER_H
#define TADAH_CORE_LOGGER_H

#include <iostream>
#include <sstream>
#include <mutex>
#include <string>
#include <chrono>
#include <ctime>
#include <fstream>
#include <unordered_map>
#include <stdexcept>
#include <algorithm>
#include <cctype>

#include <tadah/core/utils.h>

namespace tadah {
namespace core {

/**
 * @brief Enumerates severity levels for logging.
 *
 * Lower numeric values output only the most critical messages.
 * Increasing values enable more verbose output.
 */
enum class LogLevel {
  ERROR = 0,    ///< Critical error messages only.
  WARNING = 1,  ///< Warning messages.
  INFO = 2,     ///< Basic informational messages.
  DEBUG = 3,    ///< Debug messages for development.
  TRACE = 4     ///< Detailed trace messages for maximum verbosity.
};

/**
 * @brief Logger class for centralized, thread-safe logging.
 *
 * Implements a singleton pattern: one log level and one optional log
 * file for the process.
 *
 * Log levels can be specified either with an integer or with a string (e.g. "INFO", "ERROR").
 * Log files are opened in append mode.
 */
class Logger {
public:
  static Logger& instance();

  // Global log level setters (overloaded for int or string).
  void setLogLevel(LogLevel level);
  void setLogLevel(int level);
  void setLogLevel(const std::string &levelStr);

  LogLevel getLogLevel() const;

  // Global log file setter.
  bool setLogFile(const std::string &filename);

  // Converts a string (or integer in string form) to a LogLevel.
  static LogLevel stringToLogLevel(const std::string &s) {
    // Try integer conversion first.
    try {
      int num = std::stoi(s);
      if (num < 0 || num > 4)
        throw std::out_of_range("Log level out of range");
      return static_cast<LogLevel>(num);
    } catch (const std::exception&) {
      // Fallback to string comparison.
      std::string tmp = s;
      std::transform(tmp.begin(), tmp.end(), tmp.begin(), ::toupper);
      if (tmp == "TRACE")
        return LogLevel::TRACE;
      else if (tmp == "DEBUG")
        return LogLevel::DEBUG;
      else if (tmp == "INFO")
        return LogLevel::INFO;
      else if (tmp == "WARNING" || tmp == "WARN")
        return LogLevel::WARNING;
      else if (tmp == "ERROR")
        return LogLevel::ERROR;
      else
        throw std::runtime_error("Unrecognized log level: " + s);
    }
  }

  // Global logging function.
  template <typename... Args>
  void log(LogLevel messageLevel, Args&&... args) {
    logInternal(messageLevel, std::forward<Args>(args)...);
  }

private:
  Logger();
  std::string logLevelToString(LogLevel level) const;
  std::string getCurrentTime() const;

  template <typename... Args>
  void logInternal(LogLevel messageLevel, Args&&... args) {
    #if !defined(DEBUG_BUILD)
      if (messageLevel == LogLevel::DEBUG || messageLevel == LogLevel::TRACE)
        return;
    #endif

    if (messageLevel > currentLevel)
      return;

    std::lock_guard<std::mutex> lock(mutex_);
    std::ostringstream oss;
    oss << "[" << getCurrentTime() << "] ";
    oss << logLevelToString(messageLevel) << ": ";
    appendToStream(oss, std::forward<Args>(args)...);
    const std::string finalMessage = oss.str();
    std::cout << finalMessage << "\n";
    if (fileStream.is_open()) {
      fileStream << finalMessage << "\n";
      fileStream.flush();
    }
  }

private:
  LogLevel currentLevel;
  mutable std::mutex mutex_;
  bool fileLoggingEnabled;
  std::ofstream fileStream;

};
} // namespace core
} // namespace tadah
#endif // LOGGER_H
