#ifndef TADAH_CORE_RETIRED_KEYS_H
#define TADAH_CORE_RETIRED_KEYS_H

#include <tadah/core/blueprint_parser.h>
#include <tadah/core/context.h>

#include <string>
#include <vector>

namespace tadah {
namespace core {

/** @file retired_keys.h
 *  The texts of the retired keys (blueprint field `retired`, RetiredKey):
 *  the notice of a parse that read some, the refusal of a fit whose
 *  configuration records some, and the text of one key.
 *
 *  A file that has a retired key line still parses: CoreUserConfigParser
 *  records the line on the Context it returns (Context::retired_keys())
 *  and gives one notice for the parse (retired_keys_notice()), so every
 *  load of a potential that carries them works. A fit refuses such a
 *  Context with the conversion (require_no_retired_keys()), whatever read
 *  it: tadah train, tadah hpo and a program's own training. */

/** "<KEY> is a retired key: <reason> <replacement>. <conversion>": the
 *  text of the retired key @p key, upper case, whose blueprint entry is
 *  @p info (Context::add() and a read of it refuse it with this text;
 *  tadah explain and the command line give it). */
std::string retired_key_text(const std::string &key, const BlueprintKey &info);

/** The notice of a parse that read the retired lines @p lines (at least
 *  one): "Retired keys EWEIGHT (line 2 of old.tadah) and FWEIGHT (line 3
 *  of old.tadah) are ignored by a prediction, as they change nothing it
 *  gives, and refused by a fit (tadah explain EWEIGHT and tadah explain
 *  FWEIGHT give the reasons and the conversions)." (one key: "Retired key
 *  EWEIGHT (line 2 of old.tadah) is ignored by a prediction, as it changes
 *  nothing it gives, and ..."): what a prediction and what a fit do, as
 *  the parse does not know which reads the file. A line without a file is
 *  named "line N". Without the "WARNING: " that report_key_notice()
 *  adds. */
std::string retired_keys_notice(const std::vector<RetiredKeyLine> &lines);

/** The refusal of a fit whose configuration records the retired lines
 *  @p lines (at least one): "<who>: the configuration has keys retired in
 *  Tadah! 1.6.0, and a fit refuses them: EWEIGHT 1.0 (line 2 of
 *  old.tadah), FWEIGHT 100 (line 3 of old.tadah). EWEIGHT: <conversion>
 *  FWEIGHT: <conversion> ...", each line by its key and values (a line of
 *  more than six values by its first three: "WCOV 28 1.2e-05 3e-07 and 782
 *  more values"), each key's conversion once, in the order
 *  of its first line, then what to do with the lines: "With the converted
 *  keys in place of these lines", ", or the marked lines deleted" when a
 *  line is marked as below, or "With these lines deleted" when every line
 *  is (with a WDBFILE_AUTO line, that the WDBFILE values of a potential go
 *  on the command line or into a file that includes it: a retrain does
 *  not read a potential's WDBFILE line). A line that a deletion converts
 *  says so: every line of a key that nothing replaces (the field
 *  by_deletion of `retired`: UNCERTAINTY, WCOV, WNOISE, ERROR; "(line 4
 *  of pot.tadah; converted by deleting the line)"), whose reason comes
 *  before its conversion, as the conversion does not say why;
 *  WDBFILE_AUTO 0 ("(line 6 of old.tadah; 0: converted by deleting the
 *  line)") and, given the configuration @p context, FWEIGHT with FORCE
 *  false and SWEIGHT with STRESS false ("(line 3 of old.tadah; FORCE false
 *  here: converted by deleting the line)"). A line of a key that no key
 *  converts (the field unconverted of `retired`: EWEIGHT_TEMP) is marked
 *  "(line 1; no key converts it)", the key's reason comes before its
 *  conversion, and the refusal says to delete it ("With the converted keys
 *  in place of the others and the EWEIGHT_TEMP line deleted", or "With
 *  this line deleted") and closes ", and no key reproduces the EWEIGHT_TEMP
 *  line". When @p context is M_KRR
 *  implementation 2 (MODEL
 *  M_KRR <kernel> 2), whose solve of Tadah! 1.0.0 to 1.5.0, (K + LAMBDA I)
 *  w = t, read no retired key, every line is so marked ("M_KRR
 *  implementation 2 here: converted by deleting the line") and the
 *  conversion is that of implementation 2: the lines deleted, with ESIGMA
 *  1000 or LAMBDA times 10^6 with ESIGMA 1. */
std::string retired_keys_refusal(const std::vector<RetiredKeyLine> &lines,
                                 const std::string &who,
                                 const Context *context = nullptr);

/** Refuses a fit of @p context when it records a retired line
 *  (Context::retired_keys()), with retired_keys_refusal(lines, @p who,
 *  &@p context).
 *  Every fit calls it before it reads anything (M_BLR and M_KRR train,
 *  tadah train, tadah hpo when a run starts).
 *  @throws std::runtime_error */
void require_no_retired_keys(const Context &context, const std::string &who);

}  // namespace core
}  // namespace tadah

#endif  // TADAH_CORE_RETIRED_KEYS_H
