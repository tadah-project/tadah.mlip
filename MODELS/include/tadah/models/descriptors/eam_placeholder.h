#ifndef TADAH_MODELS_DESCRIPTORS_EAM_PLACEHOLDER_H
#define TADAH_MODELS_DESCRIPTORS_EAM_PLACEHOLDER_H

#include <tadah/core/context.h>

#include <string>

namespace tadah {
namespace models {

/** \brief Reports the TYPE2B / TYPEMB parameter of \ref D2_EAM or
 *  \ref DM_EAM when it is not the placeholder 0.
 *
 * Both descriptors read their tables from the setfl file named by SETFL.
 * Each takes one parameter on its TYPE line, which the key formats require
 * (TYPEMB admits only an unsigned integer there): it has no effect and is
 * written 0, `TYPE2B D2_EAM 0 Ta Ta`, `TYPEMB DM_EAM 0 Ta Ta`. Any other
 * token (`12345`, `0.0`, a file path) is reported on standard error once
 * per process (core::report_ignored_key_once, the notice of every key that
 * has no effect), e.g.
 *
 *     WARNING: TYPEMB DM_EAM parameter '7' is ignored: it is a placeholder
 *     (write 0); DM_EAM reads its setfl file from SETFL.
 *
 * (one line). The parameter is token 1 of the stored @p type_key line
 * (token 0 is the descriptor name); a line without it reports nothing (the
 * descriptor's element_tokens() refuses such a line).
 *
 * @param c        the descriptor's context
 * @param type_key `TYPE2B` or `TYPEMB`
 * @param label    the descriptor name, `D2_EAM` or `DM_EAM`
 */
void report_eam_placeholder(const tadah::core::Context &c,
                            const std::string &type_key,
                            const std::string &label);

}  // namespace models
}  // namespace tadah

#endif  // TADAH_MODELS_DESCRIPTORS_EAM_PLACEHOLDER_H
