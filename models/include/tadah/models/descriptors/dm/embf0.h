#ifndef TADAH_MODELS_EMBF0_H
#define TADAH_MODELS_EMBF0_H

#include <tadah/core/context.h>
#include <optional>
#include <string>

namespace tadah {
namespace models {

/** \brief DM_REAM embedding reference, selected by the EMBF0 key.
 *
 *  - Zero (default): the zero-referenced basis g~_m(rho) = g_m(rho) - g_m(0),
 *    so F(0) = 0 exactly for any weights.
 *  - Free: the raw basis g_m of tadah 1.5.0 and earlier.
 *
 *  Key-handling contract: DM_REAM is the only reader that queries EMBF0
 *  through Context::get (embf0_from_context), which materialises the
 *  blueprint default ZERO into the model Context so every DM_REAM potential is
 *  written with an explicit EMBF0 line. Every other reader uses
 *  stored_embf0() / context_has_ream(), which never insert defaults. */
enum class EmbF0Mode { Zero, Free };

/** "ZERO" | "FREE". */
const char *embf0_name(EmbF0Mode m);

/** Case-insensitive parse; throws std::runtime_error for anything other than
 *  ZERO or FREE. */
EmbF0Mode parse_embf0(const std::string &value);

/** Mode read through Context::get: an absent key materialises the blueprint
 *  default ZERO into @p ctx. Reserved for DM_REAM. */
EmbF0Mode embf0_from_context(const tadah::core::Context &ctx);

/** Mode stored in @p ctx, or nullopt when the key is absent. Never inserts
 *  a default; throws for an invalid stored value. */
std::optional<EmbF0Mode> stored_embf0(const tadah::core::Context &ctx);

/** True when @p ctx stores INITMB true and a TYPEMB token DM_REAM (directly or
 *  inside a DM_mJoin token list). Never inserts defaults. */
bool context_has_ream(const tadah::core::Context &ctx);

/** True when the stored MODEL is linear in the descriptor (BF_Linear or
 *  Kern_Linear); false when MODEL is absent or non-linear. Never inserts. */
bool model_is_linear(const tadah::core::Context &ctx);

/** What a trained DM_REAM potential without an EMBF0 line means under the
 *  default ZERO, relative to tadah 1.5.0 and earlier, and how to migrate
 *  ("this potential is evaluated with the default EMBF0 ZERO ... add 'EMBF0
 *  FREE' ..."). One text shared by DM_REAM's once-per-process Logger notice
 *  and the LAMMPS pair style's pair_coeff WARNING. @p linear: the model is
 *  linear in the descriptor (model_is_linear): only the energies shift;
 *  otherwise energies and forces change. */
std::string embf0_absent_consequence(bool linear);

/** EMBF0 and RHOFORM apply only to a DM_REAM many-body block. Reports the
 *  key @p key of a potential or configuration without one (it has no effect
 *  and is not written to a potential) on standard error, once per key and
 *  process: "WARNING: Key <key> is ignored: it applies only to a DM_REAM
 *  many-body block, and the potential has none (...)"
 *  (core::report_ignored_key_once, the notice of a deprecated key: neither
 *  the Logger level nor VERBOSE hides it, in LAMMPS either). */
void report_ream_only_key(const std::string &key);

/** report_ream_only_key() for each of EMBF0 and RHOFORM stored in @p c when
 *  @p c has no DM_REAM block (context_has_ream); nothing otherwise. Called
 *  wherever a model is built from a context (DC_Selector::init: tadah
 *  train, predict, hpo and pair_style tadah). */
void report_ream_only_keys(const tadah::core::Context &c);

/** Potential writers: a context with a DM_REAM block gets an explicit,
 *  canonical EMBF0 line (ZERO when absent); a context without one has a
 *  stray EMBF0 removed (report_ream_only_key), so potentials without a
 *  DM_REAM block never carry the key. */
void normalise_embf0_key(tadah::core::Context &c);

/** Engines that merge a run's configuration over a loaded potential (tadah
 *  refit --edit with -c, analysis bfunc of a program's context): call with
 *  the potential as read and the configuration BEFORE merging. Different
 *  explicit values -> std::runtime_error ("EMBF0 conflict: <pot> declares
 *  EMBF0 ZERO and the configuration file sets EMBF0 FREE. ..."; EMBF0 is a
 *  property of the potential); a configuration value for a potential
 *  without the key -> INFO (the value applies); an EMBF0 on either side of
 *  a potential without a DM_REAM block -> report_ream_only_key (ignored).
 *  An engine whose model is the potential's alone (tadah predict) passes
 *  an empty configuration. */
void check_embf0_precedence(const tadah::core::Context &pot,
                            const tadah::core::Context &config,
                            const std::string &pot_name);

}  // namespace models
}  // namespace tadah

#endif
