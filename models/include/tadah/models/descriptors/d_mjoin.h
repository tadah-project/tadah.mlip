#ifndef D_MJOIN_H
#define D_MJOIN_H
#include <tadah/core/context.h>
#include <string>
#include <vector>

namespace tadah {
namespace models {

/** \brief Base class to be used by meta descriptors for joining
*/
class D_mJoin {
protected:
  struct D_Type {
    std::string type;
    std::string rc_type;
    double rcut;
    std::vector<std::string> params;
    tadah::core::Context c;
  };
  std::vector<D_Type> dt;
  std::string lab = "D_mJoin";
public:
  virtual ~D_mJoin();
  D_mJoin();
  /** \brief One context per descriptor of the @p type_str line (TYPE2B or
   *  TYPEMB) of @p c, each with that descriptor's TYPE line, its RCTYPE and
   *  RCUT value and the list values (CGRID*, SGRID*, AMPGRIDMB, CEMBFUNC,
   *  SEMBFUNC, KDEG*) its SIZE parameters ask for, taken in order.
   *
   *  \throws std::runtime_error naming the descriptor's line when the line
   *  lacks the descriptor's parameters or a SIZE parameter is not a
   *  non-negative integer (D_Base::element_tokens), when a list key that a
   *  SIZE parameter asks values of is not set ('TYPE2B D2_BP takes 2 CGRID2B
   *  values and 2 SGRID2B values, but CGRID2B and SGRID2B are not set:
   *  TYPE2B D2_BP 2 2 Ta Ta'; a SIZE of 0 needs no key) or has fewer
   *  values left than the SIZE parameter asks for (on a joined line the
   *  descriptors before it take theirs first: 'TYPE2B D2_BP (descriptor 2
   *  of 2) takes 2 CGRID2B values, but CGRID2B has 1 value left of 3 (the
   *  descriptors before it take 2): TYPE2B D2_BP 2 2 Ta Ta'; the values are
   *  counted after LIN/GEOM expansion), when RCTYPE or RCUT has fewer or
   *  more values than the line has descriptors, and when a list key of the
   *  line's type has more values than the descriptors take (a key that no
   *  descriptor of the line reads included: 'TYPE2B D2_LJ takes 0 CGRID2B
   *  values, but CGRID2B has 3 values: TYPE2B D2_LJ Ta Ta'). A refusal of a
   *  count names the lines that gave the key its values when @p c records
   *  them (core::Context::key_lines(), a parse of a file: 'TYPE2B names 1
   *  descriptor (D2_Blip) but RCUT2B has 2 values, set on line 6 of
   *  config.train: ...'), and every refusal ends with the lines of the TYPE
   *  key (' (TYPE2B on line 4 of config.train)'), which locate a TYPE line
   *  of an included file.
   *
   *  A potential (@p c with WEIGHTS) is read as it was written: values its
   *  model does not read are reported once per key and process
   *  (core::report_ignored_key_once: 'Part of RCUT2B (6) is ignored: TYPE2B
   *  names 2 descriptors (D2_Blip, D2_Knot5), which take its first 2
   *  values; the potential works unchanged without it') instead of
   *  refused. Those are the RCTYPE, RCUT and list values past the ones a
   *  joined line's descriptors take, the RCTYPE values past the first and
   *  the RCUT values but the largest (its cutoff) of a single descriptor,
   *  and the values of a key no descriptor of the line reads. A single
   *  descriptor reads every value of the list keys it declares: their
   *  count is checked as in a configuration. That holds for the values of
   *  the potential's own lines read by its own descriptors: a key that a
   *  line outside them gave a value, and every key of a TYPE line that a
   *  line outside them gave a descriptor (core::KeyLine::potential false: a
   *  line of a file that includes the potential with CONFIG, above or below
   *  the CONFIG line), is read as in a configuration, its values counted
   *  with the potential's ('TYPE2B names 2 descriptors (D2_Blip, D2_BP) but
   *  RCUT2B has 3 values, set on line 1 of run.train and on line 20 of
   *  pot.tadah (CONFIG on line 2 of run.train): ...'). Values that no line
   *  gave (the program set them) are the potential's.
   *
   *  The list keys a descriptor reads whole (D_Base::context_keys, the
   *  CEMBFUNC/SEMBFUNC of a DM_Poly channel function) go whole, expanded,
   *  to a single line's descriptor. A joined line gives such a descriptor
   *  none; one that cannot run without them is refused ('TYPEMB DM_PS_RLR
   *  (descriptor 2 of 2) reads CEMBFUNC and SEMBFUNC whole and cannot be
   *  joined: ...').
   */
  static std::vector<tadah::core::Context> parse_context(const tadah::core::Context &c, std::string type_str);

  /** \brief The checks of parse_context() on the TYPE2B and TYPEMB lines
   *  of @p c that hold one descriptor (D2_mJoin and DM_mJoin check their
   *  own lines), for a model built from @p c (DC_Selector): a potential
   *  read by tadah predict or pair_style tadah reports the values its
   *  descriptors do not read, as a joined line's does.
   */
  static void check_single_lines(const tadah::core::Context &c);

  /** \brief The check of a potential that a command reads with -c: @p c is
   *  the configuration that the command @p command (its blueprint task
   *  name: "train", "hpo", "analysis.suggest_bounds") reads from its
   *  configuration file (-c), with the lines of its keys
   *  (core::Context::key_lines()).
   *
   *  A potential has one line for each key of its descriptors, and its
   *  TYPE2B and TYPEMB keys as tadah writes them: lines one after another
   *  (only comments and blank lines between them, core::KeyLine::ordinal),
   *  a new line at each descriptor's name and at each DESCRIPTOR block's ID
   *  (a D2_mJoin or DM_mJoin key as a line with the name of the join, then
   *  one line per descriptor it joins; core::Context::written_line_sizes()).
   *  The keys of the TYPE2B and TYPEMB lines are judged, those
   *  parse_context() reads: the TYPE key, its RCTYPE and RCUT keys and the
   *  list keys of the line's type (CGRID, SGRID and KDEG; AMPGRIDMB,
   *  CEMBFUNC and SEMBFUNC). Such a key set on two lines or more of one
   *  potential file (core::KeyLine::potential, of one source) is refused,
   *  naming the key, its lines and @p command ("Key RCUT2B is set on line 6
   *  of copy.tadah and on line 10 of copy.tadah, lines of one potential (a
   *  file with WEIGHTS): tadah train reads a key of a potential's
   *  descriptors from the line the potential has for it, and a potential's
   *  value is changed by editing its line in a copy of the potential
   *  (...)"), unless the key is the TYPE key and its lines there are the
   *  lines tadah writes for their values. A TYPE key whose first lines that
   *  follow one another are those tadah writes is refused naming the lines
   *  after them ("Line 12 of jtype.tadah is a TYPE2B line of a potential (a
   *  file with WEIGHTS) outside its TYPE2B lines (lines 4 to 6): tadah
   *  train reads the TYPE2B key of a potential from the lines tadah writes
   *  for it, which follow one another, a new line at each descriptor's
   *  name, and a potential's value is changed by editing its line in a copy
   *  of the potential (...)"); any other is refused naming every line ("Key
   *  TYPE2B is set on line 4 of ... and on line 5 of ..., lines of one
   *  potential (a file with WEIGHTS): tadah train reads the TYPE2B key of a
   *  potential from the lines tadah writes for it, ..."). A TYPEMB key that
   *  names a DESCRIPTOR block adds "and at each DESCRIPTOR block's ID
   *  (@ta)", with its first block's ID. The values of one line that the
   *  descriptors do not take (a potential of Tadah! 1.5.0) are not refused:
   *  parse_context() reports them, and a potential read for prediction is
   *  read with the values of all its lines. Keys without lines (values the
   *  program set) are not judged.
   *  \throws std::runtime_error for such a key;
   *  \throws std::invalid_argument for an empty @p command;
   *  \throws std::logic_error when the lines of the TYPE key do not give
   *  its values. */
  static void check_potential_lines(const tadah::core::Context &c,
                                    const std::string &command);

  static void expand_grids(tadah::core::Context &c, std::vector<tadah::core::Context> &contexts, std::string type_str);

  static void expand_grids(tadah::core::Context &c, std::string type_str);
  /** \brief The values of a key of a potential that expand_grids() left
   *  out: the key's values as the potential gives them (list keys expanded)
   *  and those kept, in order. */
  struct LeftOut {
    std::string key;
    std::vector<std::string> given;
    std::vector<std::string> kept;
  };

  /** \brief The TYPE2B and TYPEMB lines of @p c split and checked
   *  (parse_context()), and their grids expanded in @p c, for tadah train
   *  and tadah hpo. For a potential (@p c with WEIGHTS: training from a
   *  potential) the values of its own lines that its descriptors do not
   *  read are reported and left out of @p c, so the training context
   *  builds the potential's model: a single descriptor keeps its largest
   *  RCUT value, its cutoff.
   *  Returns the keys whose values were left out (tadah hpo names them
   *  when an OPTIM index is past the values kept).
   */
  static std::vector<LeftOut> expand_grids(tadah::core::Context &c);
};
} // namespace models
} // namespace tadah
#endif

