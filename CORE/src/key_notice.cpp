#include "tadah/core/key_notice.h"

#include <iostream>
#include <mutex>
#include <unordered_set>

namespace tadah {
namespace core {

namespace {
/* The keys report_ignored_key_once() has reported in this process. */
std::mutex &reported_mutex() {
  static std::mutex m;
  return m;
}
std::unordered_set<std::string> &reported_keys() {
  static std::unordered_set<std::string> s;
  return s;
}
} // namespace

namespace {
/* The innermost IgnoredKeyNoticeCapture of this thread, or nullptr. */
thread_local IgnoredKeyNoticeCapture *tls_capture = nullptr;
} // namespace

IgnoredKeyNoticeCapture::IgnoredKeyNoticeCapture() : outer_(tls_capture) {
  tls_capture = this;
}

IgnoredKeyNoticeCapture::~IgnoredKeyNoticeCapture() { tls_capture = outer_; }

void report_key_notice(const std::string &line) {
  if (tls_capture) {
    tls_capture->add(line);
    return;
  }
  std::cerr << ("WARNING: " + line + "\n");
}

void report_ignored_key(const std::string &subject, const std::string &reason) {
  report_key_notice(subject + " is ignored: " + reason);
}

bool report_ignored_key_once(const std::string &key, const std::string &subject,
                             const std::string &reason) {
  bool first;
  {
    std::lock_guard<std::mutex> lock(reported_mutex());
    first = reported_keys().insert(key).second;
  }
  // A capture (pair_style tadah's load of a potential) gets each key once
  // for itself, whatever the process reported before: every LAMMPS
  // instance writes it to its own log. The capture is this thread's.
  if (tls_capture) {
    if (!tls_capture->take_key(key)) return false;
    report_ignored_key(subject, reason);
    return true;
  }
  if (!first) return false;
  report_ignored_key(subject, reason);
  return true;
}

bool ignored_key_reported(const std::string &key) {
  std::lock_guard<std::mutex> lock(reported_mutex());
  return reported_keys().count(key) != 0;
}

} // namespace core
} // namespace tadah
