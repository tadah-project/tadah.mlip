#include <tadah/core/logger.h>

namespace tadah {
namespace core {

Logger& Logger::instance() {
  static Logger logger_instance;
  return logger_instance;
}

Logger::Logger() : currentLevel(LogLevel::ERROR), fileLoggingEnabled(false) { }

void Logger::setLogLevel(LogLevel level) {
  std::lock_guard<std::mutex> lock(mutex_);
  currentLevel = level;
}

void Logger::setLogLevel(int level) {
  setLogLevel(static_cast<LogLevel>(level));
}

void Logger::setLogLevel(const std::string &levelStr) {
  setLogLevel(stringToLogLevel(levelStr));
}

LogLevel Logger::getLogLevel() const {
  return currentLevel;
}

bool Logger::setLogFile(const std::string &filename) {
  std::lock_guard<std::mutex> lock(mutex_);
  fileStream.open(filename.c_str(), std::ios::out | std::ios::app);
  fileLoggingEnabled = fileStream.is_open();
  return fileLoggingEnabled;
}

std::string Logger::logLevelToString(LogLevel level) const {
  switch (level) {
    case LogLevel::TRACE: return "TRACE";
    case LogLevel::DEBUG: return "DEBUG";
    case LogLevel::INFO:  return "INFO";
    case LogLevel::WARNING: return "WARNING";
    case LogLevel::ERROR: return "ERROR";
    default:              return "LOG";
  }
}

std::string Logger::getCurrentTime() const {
  using namespace std::chrono;
  auto now = system_clock::now();
  std::time_t now_c = system_clock::to_time_t(now);
  std::string timeStr = std::ctime(&now_c);
  if (!timeStr.empty() && timeStr.back() == '\n')
    timeStr.pop_back();
  return timeStr;
}
} // namespace core
} // namespace tadah
