#ifndef TADAH_MODELS_DESCRIPTORS_WARN_ONCE_H
#define TADAH_MODELS_DESCRIPTORS_WARN_ONCE_H

#include <string>

namespace tadah {
namespace models {

/** \brief Logs @p message as a WARNING (core::Logger) at most once per
 *  process for @p key.
 *
 *  For a descriptor's warning about its configuration (a knot past the
 *  cutoff, a bump whose support reaches past a Cut_Dummy cutoff, a
 *  non-compact basis with Cut_Dummy, a DM_mEAD or OrbitalWise DM_Poly
 *  channel function that is not rotationally invariant for L > 0), which
 *  every descriptor object built
 *  from that configuration would give: tadah hpo builds the descriptors of
 *  every evaluation anew, and train, predict and pair_style tadah build
 *  them more than once. The first call for @p key while the Logger shows
 *  WARNINGs logs the message and returns true; every later call for the
 *  same @p key (any thread, any message) logs nothing and returns false. A
 *  call while the Logger hides WARNINGs logs nothing and does not use up
 *  the key. As for the notices of keys without effect
 *  (core::report_ignored_key_once), a second configuration with the same
 *  issue in one process is not warned of again: @p key names the issue,
 *  not the values.
 */
bool log_warning_once(const std::string &key, const std::string &message);

/** \brief Whether log_warning_once() has logged the warning of @p key in
 *  this process. */
bool warning_logged_once(const std::string &key);

}  // namespace models
}  // namespace tadah

#endif  // TADAH_MODELS_DESCRIPTORS_WARN_ONCE_H
