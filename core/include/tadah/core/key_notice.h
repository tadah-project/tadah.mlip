#ifndef TADAH_CORE_KEY_NOTICE_H
#define TADAH_CORE_KEY_NOTICE_H

#include <set>
#include <string>
#include <vector>

namespace tadah {
namespace core {

/**
 * @brief Writes the notice for a key that is accepted but has no effect:
 *        `WARNING: <subject> is ignored: <reason>` and a newline, to
 *        standard error.
 *
 * The notice every such key gets, once per key and process through
 * report_ignored_key_once(): a deprecated key (CoreUserConfigParser,
 * subject `Deprecated key MBLOCK`) and a key that has no effect on the
 * potential that carries it (EMBF0 or RHOFORM without a DM_REAM block,
 * subject `Key EMBF0`). Retired keys get one notice per parse instead
 * (report_key_notice()).
 *
 * Deliberately not a Logger message: the notice must reach every user of
 * the file whatever the Logger level, including pair_style tadah in LAMMPS
 * (which leaves the Logger at ERROR) and a file whose VERBOSE 0 line
 * lowered the level. The line is composed first and written with one
 * insertion; std::cerr is unit-buffered, so it is flushed at once.
 *
 * While an IgnoredKeyNoticeCapture lives on the calling thread, the
 * notice goes to it instead ("<subject> is ignored: <reason>", without the
 * "WARNING: " prefix and the newline), and nothing to standard error.
 */
void report_ignored_key(const std::string &subject, const std::string &reason);

/**
 * @brief Writes the notice @p line about keys that are accepted but change
 *        nothing: `WARNING: <line>` and a newline, to standard error, or,
 *        while an IgnoredKeyNoticeCapture lives on the calling thread, to
 *        it (@p line as it is), as report_ignored_key() does with its
 *        "<subject> is ignored: <reason>".
 *
 * CoreUserConfigParser gives it once per parse that read retired keys
 * (retired_keys_notice(), retired_keys.h), so each load of a potential
 * that carries them -- pair_style tadah's of each pair_coeff included --
 * says so once.
 */
void report_key_notice(const std::string &line);

/**
 * @brief Collects the notices report_ignored_key() gives on this thread
 *        while it lives, for a caller that delivers them itself.
 *
 * pair_style tadah loads a potential under a capture and writes each
 * notice as a LAMMPS warning on rank 0 (the screen and log.lammps), where
 * standard error reached neither log.lammps nor only one rank. A capture
 * receives each key of report_ignored_key_once() once, also a key the
 * process reported before it (on standard error or to another capture):
 * each pair_coeff that loads such a potential writes the warning, so the
 * log.lammps of every LAMMPS instance of a process has it. Captures nest;
 * the innermost one receives. Not copyable.
 */
class IgnoredKeyNoticeCapture {
public:
  IgnoredKeyNoticeCapture();
  ~IgnoredKeyNoticeCapture();
  IgnoredKeyNoticeCapture(const IgnoredKeyNoticeCapture &) = delete;
  IgnoredKeyNoticeCapture &operator=(const IgnoredKeyNoticeCapture &) = delete;

  /** The notices captured so far, in order: "<subject> is ignored:
   *  <reason>". */
  const std::vector<std::string> &notices() const { return notices_; }

  /** Appends @p line (report_ignored_key() calls it). */
  void add(const std::string &line) { notices_.push_back(line); }

  /** Records that this capture received the notice of @p key
   *  (report_ignored_key_once()); false when it had already. */
  bool take_key(const std::string &key) { return keys_.insert(key).second; }

private:
  std::vector<std::string> notices_;
  std::set<std::string> keys_;
  IgnoredKeyNoticeCapture *outer_;
};

/**
 * @brief report_ignored_key(@p subject, @p reason), at most once per
 *        process for @p key on standard error, and once per capture.
 *
 * Without an IgnoredKeyNoticeCapture on the calling thread, the first call
 * for @p key in the process writes the notice to standard error; every
 * later call for the same @p key (any thread, any subject or reason)
 * writes nothing. While a capture lives on the calling thread, the first
 * call for @p key under it gives it the notice, whatever the process
 * reported before, and nothing goes to standard error. Either way the key
 * counts as reported in the process (ignored_key_reported()). Returns
 * whether this call wrote or captured the notice. Thread-safe.
 */
bool report_ignored_key_once(const std::string &key, const std::string &subject,
                             const std::string &reason);

/**
 * @brief Whether report_ignored_key_once() has written or captured the
 *        notice for @p key in this process.
 */
bool ignored_key_reported(const std::string &key);

} // namespace core
} // namespace tadah

#endif // TADAH_CORE_KEY_NOTICE_H
