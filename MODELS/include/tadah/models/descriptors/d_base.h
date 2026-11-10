#ifndef D_BASE_H
#define D_BASE_H

#include <string>
#include <optional>
#include <tadah/core/context.h>
#include <tadah/core/core_types.h>
#include <tadah/core/periodic_table.h>
#include <tadah/core/logger.h>
#include <tadah/core/registry.h>
#include <tadah/models/cut_all.h>
#include <tadah/models/descriptors/param_meta.h>
#include <tadah/models/descriptors/basis_kind.h>
#include <tadah/models/descriptors/warn_once.h>
#include <cmath>
#include <utility>
#include <vector>
#include <bitset>

namespace tadah {
namespace models {

class Bitset2D {
  static constexpr size_t N = 119;
  std::bitset<N * N> data;

public:
  void init(size_t i, size_t j) {
    data[i * N + j] = true;
    data[j * N + i] = true;

  }
  void uninit(size_t i, size_t j) {
    data[i * N + j] = false;
    data[j * N + i] = false;
  }
  bool is_init(size_t i, size_t j) const {
    return data[i * N + j];
  }
};

/** \brief Base class for all descriptor types.
 *
 */
class D_Base {
protected:
  size_t fidx=0;    // first index in aed and fd arrays for this descriptor
  size_t s=0;
  Cut_Base *fcut=nullptr;

public:
  // ---- the descriptor key contract (mJoin + HPO addressing) ----
  // `keys`: the descriptor's OPTIMIZABLE per-instance list parameters.
  //   Every entry is (a) splittable by the *_mJoin meta-descriptors and
  //   (b) addressable by HPO via get_arg_pos/OPTIM.
  // `data_keys`: per-instance list DATA that mJoin must split but that is
  //   NOT a continuous optimisation dimension (e.g. the knot-degree lists
  //   KDEG*: a discrete 3|5 structure choice). Never exposed to OPTIM.
  // TYPE2B/TYPEMB token layout (consumed by D_mJoin::expand_grids and
  // get_init_atoms):
  //   <Label> <fparam family params> <one SIZE token per `keys` entry>
  //           <one SIZE token per `data_keys` entry> <El El ...>
  // with nparams = fparam + keys.size() + data_keys.size() and
  // get_arg_pos(key) = index-in-keys + fparam. A size token of 0 means the
  // pot carries no entries (and no key) for that channel — e.g. the knot
  // families' width channels, where eta is a pure gauge. SIZE tokens are
  // non-negative integers; element_tokens() checks them.
  std::vector<std::string> keys;       // optimizable list parameters
  std::vector<std::string> data_keys;  // split-only list data (no OPTIM)
  // `context_keys`: list keys the descriptor reads whole from the context
  //   besides `keys` and `data_keys`, with no SIZE token on its TYPE line
  //   (the CEMBFUNC/SEMBFUNC of a DM_Poly channel function). A single line
  //   of such a descriptor keeps them whole (D_mJoin::parse_context); a
  //   joined line gives it none, so one that cannot run without them
  //   (`context_keys_required`) cannot be joined.
  std::vector<std::string> context_keys;
  bool context_keys_required = false;
  size_t nparams;  // number of params which follow TYPExB
  std::vector<bool> is_optimizable;
  static void get_grid(const tadah::core::Context &c, const std::string &key, tadah::core::v_type &v);
  static tadah::core::v_type get_grid(const std::vector<std::string>&);  // expand joined grids
  int verbose;
  virtual ~D_Base();

  // return position of an argument given to TYPExB for a given key
  // if key is not used, returns -1
  int get_arg_pos(const std::string &key) const;


  /** \brief Return label of this descriptor.
   */
  virtual std::string label() const=0;

  /** Symmetric per-pair WATOM weights. `weights[Zi][Zj] == weights[Zj][Zi]`.
   *  Filled to 1.0 in every constructor; populated by update_weights() from
   *  either per-species (geometric mean) or pair-triple WATOM input.
   *  Index 0 is unused (no element has Z=0). */
  double weights[119][119];

  /** Convenience read-only accessor for the symmetric pair weight. */
  inline double get_weight(int Zi, int Zj) const { return weights[Zi][Zj]; }

  /** Tracks which WATOM input form populated `weights`. Drives arg_pos→Z
   *  vs arg_pos→pair resolution in dweight_dw_n. */
  enum class WatomMode { Empty, PerSpecies, PairTriple };
  WatomMode watom_mode = WatomMode::Empty;

  D_Base();
  D_Base(const tadah::core::Context &c);

  /** Populate `weights[][]` from the (possibly updated) WATOM key in `c`.
   *  Accepts two input formats:
   *    - Per-species (parallel to ATOM): pair weights computed as the
   *      geometric mean `weights[Zi][Zj] = sqrt(w_i * w_j)`, diagonal
   *      `weights[Zi][Zi] = w_i`. Sets watom_mode = PerSpecies and
   *      populates watom_Z_by_arg_pos.
   *    - Pair triples (sym sym value)+ : symmetric direct writes. Sets
   *      watom_mode = PairTriple and populates watom_pair_by_arg_pos.
   *  The constructor resets `weights` to 1.0 first; a later call writes
   *  only the listed pairs, so the unlisted slots keep their values. */
  void update_weights(const tadah::core::Context &c);

  /** One `weights[Zi][Zj] = weights[Zj][Zi] = w` write of update_weights. */
  struct WatomWrite {
    int Zi;
    int Zj;
    double w;
  };

  /** The WATOM key as update_weights reads it. */
  struct WatomSpec {
    WatomMode mode = WatomMode::Empty;
    /** The listed values: per-species w_n (parallel to ATOM) or the value
     *  of each pair triple, in WATOM order. */
    std::vector<double> values;
    /** PerSpecies: Z of each ATOM entry, in ATOM order. */
    std::vector<int> Zs;
    /** PairTriple: (Zi, Zj) of each triple, in WATOM order. */
    std::vector<std::pair<int, int>> pairs;
    /** The weights[][] writes, in update_weights order (per-species: the
     *  diagonal w_i, then sqrt(w_i w_j) for j > i). */
    std::vector<WatomWrite> writes;
  };

  /** Parse WATOM from @p c exactly as update_weights applies it (the two
   *  input formats documented there). Mode Empty when WATOM is absent or
   *  empty, or when a per-species WATOM has no ATOM list. The list is per
   *  species when its first token is shaped like a number
   *  (watom_number_shaped), pair triples otherwise. Every value is read by
   *  watom_value. Throws for a value outside that grammar, for a
   *  pair-triple list whose length is not a multiple of 3 and for unknown
   *  element symbols. */
  static WatomSpec parse_watom(const tadah::core::Context &c);

  /** The one reader of a WATOM value (@p entry: its 1-based position in the
   *  WATOM list, for the message), used by parse_watom (the model),
   *  density_pair_weights (the density checks, the HPO gates) and so by
   *  every DM_REAM path: the decimal number grammar of potential and
   *  configuration files (core::parse_decimal_finite; a subnormal is a
   *  value). Throws std::runtime_error "WATOM entry N ('tok') is not a
   *  decimal number: ..." for anything else: hexadecimal ("0x1p-1", which
   *  istringstream read as 0 and strtod as 0.5), inf, nan, an overflow or
   *  trailing characters. */
  static double watom_value(const std::string &tok, std::size_t entry);

  /** True when a WATOM token is shaped like a number, which makes the list
   *  per species: a digit or '.' after an optional sign, or an inf/nan word
   *  ("inf", "-Infinity", "nan"). Such a token is then read by watom_value,
   *  so a misspelt number is refused as one instead of being taken for an
   *  element symbol. */
  static bool watom_number_shaped(const std::string &tok);

  /** Density forms that guarantee rho >= 0 (DM_REAM RHOFORM NONNEG and
   *  MONOTONE) need every density pair weight W = weights[Zi][Zj] > 0:
   *  throws DensityFormError (rhoform.h), prefixed by @p who, when a WATOM
   *  value in @p c is not finite and > 0 (per-species values give
   *  W = sqrt(w_i w_j) > 0 exactly when every w_i > 0). Reads the context,
   *  not `weights`: a constructor checks the form before update_weights().
   *  No WATOM (all weights 1.0) passes. */
  static void require_positive_density_weights(const tadah::core::Context &c,
                                               const std::string &who);

  /** The density pair weights W = weights[Zi][Zj] that WATOM in @p c sets,
   *  in update_weights' write order: per species w_i, then sqrt(w_i w_j)
   *  for j > i; pair triples each triple's value. Empty when WATOM is
   *  absent, or per species without ATOM (the weights stay 1.0). Reads
   *  stored values only and resolves no element symbol, so a density check
   *  can run without the periodic table. Throws std::runtime_error, as
   *  parse_watom does, for a pair-triple list whose length is not a
   *  multiple of 3, and for a value outside watom_value's grammar. */
  static std::vector<double> density_pair_weights(const tadah::core::Context &c);

  /** Resolve a 1-based per-species WATOM arg_pos (matching
   *  `OPTIM WATOM (n)`) to the atomic number Z of the n-th ATOM entry
   *  populated by the last update_weights call. Returns 0 when arg_pos
   *  is out of range, the ATOM list was never populated, or the
   *  descriptor is in PairTriple mode. */
  int get_atom_Z_for_arg_pos(int arg_pos) const;

  /** Chain factor `d weights[Zi][Zj] / d θ_{arg_pos}` for the current
   *  WATOM mode.
   *    PerSpecies : geometric-mean derivative w.r.t. per-element w_n;
   *      returns 1.0 when Zi==Zj==Z_target (diagonal direct write),
   *      0.5 * sqrt(w_other / w_n) when exactly one of Zi,Zj equals
   *      Z_target, 0 otherwise.
   *    PairTriple : 1.0 iff (Zi,Zj) matches the arg_pos-th triple's
   *      pair (either order), 0 otherwise.
   *    Empty / out-of-range arg_pos : 0. */
  double dweight_dw_n(int Zi, int Zj, int arg_pos) const;

  /** Returns true when `arg_pos` is in range for the current watom_mode
   *  (i.e. the descriptor owns the WATOM derivative for that θ). Lets
   *  descriptor calc_dDdtheta branches distinguish "I own this θ but
   *  contribute zero for this (Zi,Zj) pair" (return true) from "this θ
   *  is unconfigured, fall back to FD" (return false). */
  bool has_watom_arg_pos(int arg_pos) const;

  /** True when value @p arg_pos (1-based, `OPTIM WATOM (n)`) of the WATOM
   *  read by the last update_weights call sets the weight of a pair of
   *  elements this descriptor is initialised for (is_init_for_atoms):
   *  per species, a pair of the arg_pos-th element of ATOM with an element
   *  of ATOM (itself included); in the pair form, the arg_pos-th pair.
   *  False for a value past the list or without WATOM. Reads the elements
   *  and the pairs, not the values: a weight that a value sets depends on
   *  it whatever the values are. */
  bool watom_value_weighs_a_pair(int arg_pos) const;

  /** \brief Whether the values of this descriptor (its descriptor entries
   *  and their position derivatives) are weighted by the WATOM pair
   *  weights `weights[Zi][Zj]`.
   *
   *  True by default, so that a descriptor added later reads WATOM until
   *  it says otherwise (depends_on() then takes WATOM by finite
   *  differences instead of leaving out its part). False for D2_EAM and
   *  DM_EAM, which evaluate the tables of their setfl file unweighted, and
   *  for D2_Dummy and DM_Dummy, which have no values. D2_mJoin and DM_mJoin
   *  read it when a descriptor they join does. context_reads_watom()
   *  (inert_keys.h) tells the same from the stored values of a
   *  configuration, without building a descriptor. */
  virtual bool reads_watom() const { return true; }

  /** \brief Whether the values of this descriptor (its descriptor entries
   *  and their position derivatives) depend on value @p arg_pos (1-based,
   *  the OPTIM index) of the search parameter @p theta_name (an upper-case
   *  key, or a component path), whatever the atoms of a structure.
   *
   *  The analytical gradient of tadah hpo takes the rows of a search
   *  parameter from the closed forms of the two-body and the many-body
   *  descriptor (calc_dDdtheta, calc_drhoi_dtheta, ...), and claims the
   *  rows only when every descriptor that depends on the parameter
   *  differentiates it in them (mlip::assemble_dphi_train_both_rows). A
   *  descriptor that depends on a parameter and has no closed form for it
   *  so hands the parameter to finite differences (GRAD_STRATEGY HYBRID)
   *  or to the startup refusal of GRAD_STRATEGY ANALYTICAL, instead of
   *  leaving its part out of the gradient: an answer of true for a value
   *  the values do not depend on costs finite differences, one of false
   *  for a value they depend on is a wrong gradient.
   *
   *  Default: WATOM when reads_watom() and watom_value_weighs_a_pair(); a
   *  key of `keys` or `context_keys`, at every index; nothing else.
   *  D2_Base adds RCUT2B and DM_Base RCUTMB (the cutoff function, the
   *  truncation of Cut_Dummy included); D2_ZBL and D2_MIE add their TYPE2B
   *  family parameters; DM_mBlipEnv adds RCUTENV; DM_Embed depends on the
   *  component paths of its ID and on WATOM (its cutoff is its block's,
   *  not RCUTMB); D2_mJoin and DM_mJoin on their own keys and on what a
   *  descriptor they join depends on; D2_Dummy and DM_Dummy on nothing. */
  virtual bool depends_on(const std::string &theta_name, int arg_pos) const;

  /** \brief Print grid to the log.
   *
   * @param v Vector to print.
   * @param key Key name for the vector.
   */
  void printGrid(core::v_type &v, const std::string &key, core::LogLevel logLevel=core::LogLevel::INFO);

  virtual void set_fidx(size_t fidx_);
  virtual size_t get_fidx() const;

  /**
 * @brief Initialize the descriptor.
 *
 * This method sets up the required keys names and number of parameters for the descriptor.
 *
 * Note: The implementation of the descriptor should not rely on these initial values,
 * allowing for greater flexibility in how the tadah::core::Context file is parsed.
 *
 * The keys and parameters are used by other classes inheriting from this implementation,
 * such as meta-descriptors.
 *
 */
  virtual void init() = 0;

  virtual void set_fcut(Cut_Base* cut, bool manage_memory);

  double get_rcut() const;

  /** \brief Return dimension of the descriptor.
  */
  virtual size_t size() const;

  /** \brief What fixes value @p i (0-based) of this descriptor's cutoff key
   *  (RCUT2B for a two-body, RCUTMB for a many-body descriptor), as a
   *  clause for a message, or "" when that value is a free parameter.
   *
   *  D2_EAM and DM_EAM evaluate the tables of their setfl file, which
   *  define the cutoff: every value of the key must be the file's cutoff
   *  (require_setfl_cutoff()), so they answer e.g. "D2_EAM takes the cutoff
   *  5.58190742024 of its setfl file 'Ta.eam.alloy' (SETFL)", and tadah hpo
   *  refuses a search over the value at startup. A D2_mJoin or DM_mJoin
   *  gives value i of the key to its i-th joined descriptor and answers
   *  with that descriptor's value 0 ("" past the last one). Default "".
   */
  virtual std::string cutoff_fixed_by(std::size_t /*i*/) const { return {}; }

  virtual bool is_init_for_atoms(int Zi, int Zj) const;
  /** \brief True when this descriptor applies to every pair of elements,
   *  i.e. its TYPE2B/TYPEMB element pairs include `* *`.
   *
   *  A meta descriptor (D2_mJoin, DM_mJoin) applies to every pair only when
   *  each descriptor it joins does. See require_dimer_element_pairs().
   */
  virtual bool applies_to_every_element_pair() const;
  virtual void init_for_atoms(int Zi, int Zj);
  virtual void init_for_atoms(const std::vector<std::string> &Zs);
  virtual void uninit_for_atoms(int Zi, int Zj);
  virtual std::vector<std::string> get_init_atoms(const tadah::core::Context &c)=0;

  /** \brief Element tokens of this descriptor's TYPE2B/TYPEMB line.
   *
   * `line` is the whole line in the layout of the key contract above: the
   * descriptor name, its `nparams` parameters, then the element pairs,
   * which are returned. The key's format does not know how many
   * parameters a descriptor takes (TYPE2B accepts a bare name, and both
   * keys accept element pairs after too few numbers), so the line is
   * checked here: it must have `nparams` parameters, and each SIZE token
   * (one per `keys` and `data_keys` entry) must be a non-negative integer.
   * Otherwise an element pair standing where a SIZE token belongs would be
   * read as parameters, and the descriptor would apply to no element pair.
   * The family parameters before the SIZE tokens are read and converted
   * by the descriptor itself.
   *
   * \throws std::runtime_error naming `key`, the descriptor and the line
   *         if the line has fewer than `nparams` parameters, or naming the
   *         parameter and its token if a SIZE token is not a non-negative
   *         integer.
   * \throws std::logic_error if the descriptor declares more `keys` and
   *         `data_keys` than `nparams`.
   */
  std::vector<std::string> element_tokens(const std::string &key,
                                          std::vector<std::string> line) const;

  /** \brief Per-descriptor parameter metadata for the HPO bound-validation
   *  pipeline (audit, suggester, per-iter health monitor).
   *
   *  Default `std::nullopt` means "no metadata supplied" — the audit
   *  treats the descriptor as opaque (skips block-level health checks
   *  that depend on metadata). Concrete descriptors override this
   *  method to declare their parameter physics. Canonical shapes:
   *  D2_LJ::meta() (scalar parameters, d2_lj.cpp), D2_Blip::meta() (grid
   *  vectors, d2_blip.cpp) and the DM_mBlipEnv specialisation in
   *  dm_all.cpp (every parameter plus a cross-parameter health check).
   */
  virtual std::optional<DescriptorMeta> meta() const {
    return std::nullopt;
  }

  /** \brief The radial/embedding basis family of this descriptor, when it has a
   *  parametric analytic basis. Default `std::nullopt` = "no selectable
   *  analytic basis" (tabulated / fixed / dummy descriptors — e.g. DM_EAM,
   *  D2_LJ, DM_Dummy). Descriptors with a basis (D2_Blip/Blip5/Knot5/BP,
   *  DM_Blip/EAD, DM_REAM, DM_Poly, ...) override it. This is the single
   *  virtual the basis-analysis engine and capabilities() query instead of
   *  branching on the descriptor name string. */
  virtual std::optional<BasisKind> basis_kind() const { return std::nullopt; }

  /** \brief Declarative capability summary, mirroring the meta() opt-in idiom.
   *  Lets HPO / audits ask structural questions ("real cutoff? compact-support
   *  basis? has metadata?") without a dynamic_cast or a descriptor-name test. */
  struct DescriptorCapabilities {
    bool has_real_cutoff    = false; ///< smooth, tunable cutoff (RCUT is an HPO lever)
    bool is_compact_support = true;  ///< basis self-terminates (Cut_Dummy is sound)
    bool has_meta           = false; ///< meta() returns a DescriptorMeta
  };
  virtual DescriptorCapabilities capabilities() const {
    DescriptorCapabilities cap;
    cap.has_real_cutoff = fcut && fcut->kind() != Cut_Base::Kind::Discontinuous;
    if (auto bk = basis_kind())
      cap.is_compact_support = basis_traits(*bk).compact_support;
    cap.has_meta = meta().has_value();
    return cap;
  }

  /** \brief Warn when a NON-compact basis (e.g. Gaussian) is paired with a
   *  discontinuous (Cut_Dummy) cutoff: the descriptor's forces are then
   *  discontinuous at the cutoff and RCUT is not an HPO lever. Makes the
   *  previously-silent Cut_Dummy(0) install visible. No-op for a
   *  compact-support basis or a descriptor with no analytic basis. Called
   *  after construction from the central builder (DC_Selector), when
   *  basis_kind()/fcut are settled; logged once per process for each
   *  descriptor (log_warning_once), however many are built. */
  void warn_if_discontinuous_noncompact() const {
    const auto bk = basis_kind();
    if (!bk || basis_traits(*bk).compact_support) return;
    if (fcut && fcut->kind() == Cut_Base::Kind::Discontinuous) {
      log_warning_once(
          label() + ": non-compact basis with a Cut_Dummy cutoff",
          "Descriptor " + label() + " uses a non-compact " +
          std::string(basis_name(*bk)) +
          " basis with a discontinuous (Cut_Dummy) cutoff: forces are "
          "discontinuous at the cutoff and RCUT cannot be optimised by HPO. "
          "Configure a smooth cutoff (RCTYPE2B/RCTYPEMB) to make it a lever.");
    }
  }

protected:
  /** \brief Read-path counterpart of the authoring-time bump-support gate
   *  (the bump analogue of D2_Knot5/DM_REAM::warn_if_knots_beyond_rcut):
   *  warn once per process for each descriptor and cutoff key
   *  (log_warning_once) when a bump basis function's support [c - w/eta, c + w/eta]
   *  reaches past rcut under a discontinuous (Cut_Dummy) cutoff — the
   *  overhang is hard-truncated there, i.e. an energy/force step whenever a
   *  neighbour crosses the cutoff. Reading stays warn-only: legacy fitted
   *  pots with spill exist and their truncation is part of the fitted model
   *  (authoring surfaces — hpo preflight, refit, --edit — hard-error
   *  instead). Smooth cutoffs damp the overhang to zero -> no-op. Call from
   *  the ctor of bump descriptors. */
  void warn_if_bump_support_beyond_rcut(const std::string &who,
                                        const std::string &ckey,
                                        const std::string &rkey,
                                        const tadah::core::v_type &cgrid,
                                        const tadah::core::v_type &sgrid,
                                        BasisKind kind) {
    if (!fcut || cgrid.empty() || cgrid.size() != sgrid.size())
      return;
    if (fcut->kind() != Cut_Base::Kind::Discontinuous) return;
    const double w = bump_halfwidth(kind);
    if (w <= 0.0) return;
    double reach = -1.0;
    std::size_t worst = 0;
    for (std::size_t k = 0; k < cgrid.size(); ++k) {
      const double eta = std::fabs(sgrid[k]);
      if (eta <= 0.0) continue;
      const double r = cgrid[k] + w / eta;
      if (r > reach) { reach = r; worst = k; }
    }
    const double rc = get_rcut();
    if (reach <= rc + bump_rcut_tol(rc)) return;
    log_warning_once(
        who + ": bump support past a Cut_Dummy " + rkey,
        who + ": bump " + std::to_string(worst + 1) + " of " + ckey +
        " (centre " + std::to_string(cgrid[worst]) + ", eta " +
        std::to_string(sgrid[worst]) + ") has support reaching " +
        std::to_string(reach) + ", past " + rkey + " = " +
        std::to_string(rc) +
        " — the overhang is hard-truncated at the cutoff (for an existing "
        "fitted pot the truncation is part of the model; predictions are "
        "unchanged). New fits/edits enforce centre + " + std::to_string(w) +
        "/eta <= " + rkey + ".");
  }

private:

  bool manage_memory=false; // who owns fcut
  Bitset2D init_for_atoms_map;  // 0th is unused

  /* Z values in ATOM-list order, populated by update_weights when the
   * per-species WATOM input form is detected.
   * Empty when watom_mode != PerSpecies (default-constructed, pair-triple
   * input, or no WATOM key). `OPTIM WATOM (n)` resolves to
   * `watom_Z_by_arg_pos[n-1]` only in PerSpecies mode. */
  std::vector<int> watom_Z_by_arg_pos;

  /* (Zi, Zj) pairs in the order they appear in WATOM, populated by
   * update_weights when the pair-triple WATOM input form is detected.
   * Empty when watom_mode != PairTriple. `OPTIM WATOM (k)` resolves to
   * watom_pair_by_arg_pos[k-1] only in PairTriple mode. */
  std::vector<std::pair<int,int>> watom_pair_by_arg_pos;
};
} // namespace models
} // namespace tadah
#endif
