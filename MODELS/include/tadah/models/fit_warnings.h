#ifndef TADAH_MODELS_FIT_WARNINGS_H
#define TADAH_MODELS_FIT_WARNINGS_H

#include <tadah/core/logger.h>

#include <string>
#include <utility>
#include <vector>

namespace tadah {
namespace models {

/** The WARNINGs a fit gives about its own result. */
enum class FitWarningKind {
  /** The evidence approximation of LAMBDA -1 stopped at its iteration
   *  limit without converging (EA::stopped_at_limit_text()). */
  EvidenceApproximationLimit,
  /** An OALGO 3 solve left singular directions of the design matrix at or
   *  below its rank threshold out of the coefficients ("OALGO 3: 1 of 4
   *  singular directions ..."). */
  SingularDirectionsDropped
};

/** One WARNING of a fit: its kind and its text. */
struct FitWarning {
  FitWarningKind kind;  ///< what the WARNING is about
  std::string text;     ///< the WARNING as the Logger would print it
};

class FitWarningCollector;

namespace detail {
/** The innermost FitWarningCollector of the calling thread, or nullptr. */
inline thread_local FitWarningCollector *fit_warning_collector = nullptr;
}  // namespace detail

/** While it lives, collects the WARNINGs of the fits made on the calling
 *  thread (fit_warning()) instead of the Logger, which prints none of
 *  them. A caller that fits many candidates (tadah hpo, one fit per
 *  evaluation) says each kind once instead of once per fit. The fits of
 *  other threads are not collected; a collector made while another lives
 *  on the thread collects until it goes, and the other again after. */
class FitWarningCollector {
public:
  FitWarningCollector() : previous_(detail::fit_warning_collector) {
    detail::fit_warning_collector = this;
  }
  ~FitWarningCollector() { detail::fit_warning_collector = previous_; }
  FitWarningCollector(const FitWarningCollector &) = delete;
  FitWarningCollector &operator=(const FitWarningCollector &) = delete;

  /** The WARNINGs collected, in the order the fits gave them. */
  const std::vector<FitWarning> &warnings() const { return warnings_; }

private:
  friend void fit_warning(FitWarningKind kind, const std::string &text);
  FitWarningCollector *previous_;
  std::vector<FitWarning> warnings_;
};

/** A WARNING of a fit: to the FitWarningCollector of the calling thread
 *  when one lives, else to the Logger at WARNING level. */
inline void fit_warning(FitWarningKind kind, const std::string &text) {
  if (FitWarningCollector *c = detail::fit_warning_collector) {
    c->warnings_.push_back(FitWarning{kind, text});
    return;
  }
  tadah::core::Logger::instance().log(tadah::core::LogLevel::WARNING, text);
}

}  // namespace models
}  // namespace tadah

#endif  // TADAH_MODELS_FIT_WARNINGS_H
