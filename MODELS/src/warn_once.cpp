#include <tadah/models/descriptors/warn_once.h>

#include <tadah/core/logger.h>

#include <mutex>
#include <set>

namespace tadah {
namespace models {

namespace {
std::mutex &warned_mutex() {
  static std::mutex m;
  return m;
}
std::set<std::string> &warned_keys() {
  static std::set<std::string> keys;
  return keys;
}
}  // namespace

bool log_warning_once(const std::string &key, const std::string &message) {
  auto &log = tadah::core::Logger::instance();
  if (log.getLogLevel() < tadah::core::LogLevel::WARNING) return false;
  {
    std::lock_guard<std::mutex> lock(warned_mutex());
    if (!warned_keys().insert(key).second) return false;
  }
  log.log(tadah::core::LogLevel::WARNING, message);
  return true;
}

bool warning_logged_once(const std::string &key) {
  std::lock_guard<std::mutex> lock(warned_mutex());
  return warned_keys().count(key) != 0;
}

}  // namespace models
}  // namespace tadah
