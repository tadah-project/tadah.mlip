#ifndef TADAH_MLIP_CLI_LOGGING_H
#define TADAH_MLIP_CLI_LOGGING_H

#include <fstream>
#include <iostream>
#include <string>

/**
 * @brief Provides logging functionality to the console and optionally a file.
 *
 * Supports basic logging for CLI and engine operations.
 */
class Logger {
public:
  /**
   * @brief Sets the log file.
   * @param filename The file name to log output to.
   */
  static void set_log_file(const std::string &filename) {
    instance().log_stream_.open(filename, std::ios::out);
    if (!instance().log_stream_.is_open())
      std::cerr << "Error: Unable to open log file: " << filename << std::endl;
  }

  /**
   * @brief Logs a message.
   * @param message The message string.
   */
  static void log(const std::string &message) {
    std::cout << message << std::endl;
    if (instance().log_stream_.is_open())
      instance().log_stream_ << message << std::endl;
  }

private:
  Logger() = default;
  ~Logger() {
    if (log_stream_.is_open())
      log_stream_.close();
  }
  static Logger &instance() {
    static Logger logger;
    return logger;
  }
  std::ofstream log_stream_;
};

#endif // TADAH_MLIP_CLI_LOGGING_H

