#ifndef TADAH_MODELS_INERT_KEYS_H
#define TADAH_MODELS_INERT_KEYS_H

#include <tadah/core/context.h>

#include <string>
#include <vector>

namespace tadah {
namespace models {

/** @file inert_keys.h
 *  Keys that change nothing a fit gives. A fit (tadah train, tadah hpo,
 *  tadah refit, M_BLR and M_KRR train) refuses a configuration that has a
 *  line of one, naming the key, the line and the file
 *  (require_no_inert_keys()), as it refuses a retired key
 *  (core::require_no_retired_keys()):
 *   - a deprecated key (MPIWPCKG, MBLOCK, NBLOCK, SIGMA), whose lines the
 *     parse records (core::Context::deprecated_keys());
 *   - EMBF0 and RHOFORM when the configuration has no DM_REAM block
 *     (context_has_ream(), embf0.h);
 *   - WATOM with a value other than 1 when no descriptor of the
 *     configuration reads it (context_reads_watom()).
 *  A potential loaded to predict with (tadah predict, pair_style tadah, a
 *  program that builds a model from one) loads, with one notice per key
 *  and process for each (the notice of a deprecated key, report_ream_only_
 *  keys() and report_unread_watom()). */

/** Whether a descriptor of @p c weights its pairs by WATOM: an initialised
 *  (INIT2B, INITMB true) TYPE2B or TYPEMB line names a descriptor other than
 *  D2_EAM, DM_EAM, D2_Dummy and DM_Dummy (the descriptors of a D2_mJoin or
 *  DM_mJoin line each), or a component (an @id of a DESCRIPTOR block), or
 *  the configuration has DESCRIPTOR blocks without a TYPEMB line and not
 *  INITMB false (the blocks are then the many-body term). D2_EAM and DM_EAM
 *  take their pair and density functions from the SETFL file, unweighted;
 *  the answer is that of D_Base::reads_watom() of the descriptors a model
 *  of @p c builds. Reads stored values only (an INIT key that is not stored
 *  is its blueprint default). */
bool context_reads_watom(const core::Context &c);

/** Whether @p c has a WATOM with a value other than 1: a per-species value
 *  or the value of a pair triple (D_Base::parse_watom's two forms). No
 *  WATOM, or one whose every value is 1, sets no weight. Reads stored
 *  values only; a value that is not a number is refused where WATOM is
 *  read (D_Base::watom_value), and is taken here as one other than 1. */
bool watom_sets_a_weight(const core::Context &c);

/** A line of a key that changes nothing a fit gives. */
struct InertKeyLine {
  std::string key;     ///< The key, upper case.
  std::string text;    ///< The key and its values: "MBLOCK 64" (see inert_keys_text()).
  std::string where;   ///< "line 3 of run.train"; "" for values no line gave.
  std::string reason;  ///< Why it changes nothing.
};

/** Which rules inert_key_lines() applies. */
struct InertKeyRules {
  /** EMBF0 and RHOFORM without a DM_REAM block. false for a fit that reads
   *  them itself from a configuration without descriptor lines (tadah
   *  refit, whose potential's density component declares them). */
  bool ream_keys = true;
  /** When not empty, a fit that writes its own WATOM (tadah refit writes
   *  WATOM 1): a WATOM with a value other than 1 changes nothing, whatever
   *  the descriptor lines, for this reason. */
  std::string watom_never_read;
};

/** The lines of @p c that change nothing a fit of it gives, in the order:
 *  the deprecated lines (as recorded), EMBF0 and RHOFORM without a DM_REAM
 *  block (@p rules), WATOM with a value other than 1 when no descriptor of
 *  @p c reads it (or always, with rules.watom_never_read). */
std::vector<InertKeyLine> inert_key_lines(const core::Context &c,
                                          const InertKeyRules &rules = {});

/** What a fit says of the lines @p lines (at least one): "the
 *  configuration has lines that change nothing a fit gives, and a fit
 *  refuses them: MBLOCK 64 (line 3 of run.train): <reason>; EMBF0 FREE
 *  (line 4 of run.train): <reason>. With these lines deleted, the fit is
 *  the same." (one line: "has a line that changes ..., and a fit refuses
 *  it: ... With this line deleted, ..."). With @p also, "the configuration
 *  also has", ending "With these lines deleted as well, the configuration
 *  trains.": the sentence a fit adds to its refusal of retired keys (MLIP
 *  require_no_unused_keys). A line of more than six values is named
 *  by its key, its first three values and the number of the others
 *  ("SIGMA 3 5.48e-09 3.2e-09 and 7 more values"). */
std::string inert_keys_text(const std::vector<InertKeyLine> &lines,
                            bool also);

/** The refusal of a fit of a configuration with the lines @p lines (at
 *  least one): "<who>: " and inert_keys_text(@p lines, false). */
std::string inert_keys_refusal(const std::vector<InertKeyLine> &lines,
                               const std::string &who);

/** Refuses a fit of @p c that has a line of inert_key_lines(@p c, @p rules)
 *  with inert_keys_refusal(); @p who begins the message ("tadah train").
 *  @throws std::runtime_error */
void require_no_inert_keys(const core::Context &c, const std::string &who,
                           const InertKeyRules &rules = {});

/** The notice of a model built from @p c (DC_Selector::init: tadah train,
 *  predict, hpo, pair_style tadah) whose WATOM sets a weight that no
 *  descriptor reads: "Key WATOM (2.5) is ignored: no descriptor of the
 *  potential reads it (D2_EAM and DM_EAM take their pair and density
 *  functions from the SETFL file, unweighted); the potential works
 *  unchanged without it", once per process
 *  (core::report_ignored_key_once). Nothing otherwise. */
void report_unread_watom(const core::Context &c);

}  // namespace models
}  // namespace tadah

#endif  // TADAH_MODELS_INERT_KEYS_H
