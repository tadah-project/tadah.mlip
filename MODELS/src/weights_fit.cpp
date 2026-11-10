#include <tadah/models/functions/weights_fit.h>

#include <tadah/core/intercept_layout.h>

#include <stdexcept>
#include <string>
#include <vector>

namespace tadah {
namespace models {

namespace {

std::size_t stored_size(const tadah::core::Context &c, const char *key) {
  return c.has_stored(key) ? c.size(key) : 0;
}

std::string atom_list(const tadah::core::Context &c) {
  std::string s;
  const std::size_t n = stored_size(c, "ATOM");
  for (std::size_t i = 0; i < n; ++i)
    s += (i ? " " : "") + c.get<std::string>("ATOM", i);
  return s;
}

}  // namespace

bool is_linear_function(const Function_Base &f) {
  const std::string l = f.get_label();
  return l == "BF_Linear" || l == "Kern_Linear";
}

TrainedSizes trained_sizes(const tadah::core::Context &c) {
  TrainedSizes s;
  s.weights = stored_size(c, "WEIGHTS");
  s.nmean = stored_size(c, "NMEAN");
  s.nstdev = stored_size(c, "NSTDEV");
  return s;
}

void require_weights_fit(const tadah::core::Context &c, std::size_t dsize,
                         const Function_Base &f, const TrainedSizes &s) {
  const bool norm = c.contains("NORM") && c.get<bool>("NORM");
  // A linear model predicts with its weights converted to the raw
  // descriptors (MLIP to_raw_descriptors after the fit, pair_style tadah's
  // linear path): it never reads NMEAN or NSTDEV. A non-linear one
  // normalises every descriptor with them when it predicts.
  const bool linear = is_linear_function(f);
  const std::size_t n_int = tadah::core::n_intercept_columns(c);
  const std::string label = f.get_label();
  const std::string D = std::to_string(dsize);

  if (norm && linear && n_int == 0) {
    // How the conversion of earlier versions went wrong without an
    // intercept column to take the normalisation's constant.
    const std::string history =
        label == "BF_Linear"
            ? "M_BLR BF_Linear converted the weights fitted to the "
              "normalised descriptors with descriptor component 0 taken for "
              "an intercept up to 1.2.0, so the energies and forces of such "
              "a potential are wrong; from 1.3.0-beta.1, when tadah hpo and "
              "the API wrote such a potential (tadah train refused the "
              "configuration), it dropped the mean correction, so the "
              "energies are off by the same constant for every atom (the "
              "forces are right)"
            : "M_KRR Kern_Linear converted the weights fitted to the "
              "normalised descriptors with descriptor component 0 taken for "
              "an intercept until this version, so its energies and forces "
              "are wrong";
    throw std::runtime_error(
        "The potential cannot be used: NORM true with " + label +
        " and BIAS false. tadah no longer trains such a potential (NORM true "
        "with a linear model needs BIAS true), and one written by an earlier "
        "version is wrong in tadah predict and in pair_style tadah: " +
        history + "; retrain it (tadah train -c <its training configuration, "
        "or this potential>) with NORM false, or with BIAS true.");
  }

  std::vector<std::string> problems;
  const std::size_t n_w = f.weights_count(dsize);
  const std::size_t bvs = f.basis_vector_size();
  if (s.weights != n_w)
    problems.push_back(
        "the potential has " + std::to_string(s.weights) + " WEIGHTS, but " +
        label + " needs " + std::to_string(n_w) + " weights" +
        (bvs != 0 ? ", one per basis vector (SBASIS " + std::to_string(n_w) +
                        ")"
                  : " for a descriptor of " + D + " components (DSIZE)"));
  if (bvs != 0 && bvs != dsize)
    problems.push_back("BASIS holds SBASIS " + std::to_string(n_w) +
                       " basis vectors of " + std::to_string(bvs) +
                       " values each, but the descriptor of the potential "
                       "has " + D + " components (DSIZE): BASIS must have "
                       "SBASIS x " + D + " values");
  const bool reads_norm = norm && !linear;
  if (reads_norm) {
    if (s.nmean != dsize)
      problems.push_back("NORM true: NMEAN has " + std::to_string(s.nmean) +
                         " values, but the descriptor has " + D +
                         " components (DSIZE)");
    if (s.nstdev != dsize)
      problems.push_back("NORM true: NSTDEV has " +
                         std::to_string(s.nstdev) +
                         " values, but the descriptor has " + D +
                         " components (DSIZE)");
  }
  if (problems.empty()) return;

  const std::string atoms = atom_list(c);
  std::string layout;
  if (n_int > 0)
    layout = "DSIZE counts " + std::to_string(n_int) + " intercept column" +
             (n_int > 1 ? "s" : "") +
             ": BIAS true, one per ATOM species, ATOM " + atoms;
  else
    layout = "BIAS false: no intercept column" +
             (atoms.empty() ? std::string() : "; ATOM " + atoms);

  std::string msg = "The potential does not fit its descriptor: ";
  for (std::size_t i = 0; i < problems.size(); ++i)
    msg += (i ? "; " : "") + problems[i];
  msg += " (" + layout + ").";

  /* The sizes of the same descriptor with one intercept column instead of
   * N: a multi-species BIAS potential trained before 1.6.0. */
  bool legacy = false;
  if (n_int >= 2 && dsize >= n_int) {
    const std::size_t one = dsize - (n_int - 1);
    legacy = bvs == 0 ? s.weights == f.weights_count(one) : bvs == one;
    if (reads_norm && (s.nmean == one || s.nstdev == one)) legacy = true;
  }
  if (legacy)
    msg += " Its sizes are those of one intercept column shared by all "
           "species, the layout in which tadah before 1.6.0 trained BIAS "
           "true with several species. pair_style tadah has used one "
           "intercept column per species since 1.3.0-beta.1, so such a "
           "potential gave wrong LAMMPS energies and forces with pair_style "
           "tadah from 1.3.0-beta.1 until this version (with 1.2.0 and "
           "earlier, whose pair_style tadah used one shared column too, they "
           "matched tadah predict). Its weights do not fit the descriptor of "
           "this version: retrain it (tadah train -c <its training "
           "configuration, or this potential>).";
  /* NMEAN and NSTDEV of twice the descriptor: before 1.6.0, `tadah train -c
   * <potential>` appended the new fit's values to the potential's. */
  if (reads_norm && (s.nmean == 2 * dsize || s.nstdev == 2 * dsize))
    msg += " NMEAN and NSTDEV of " + std::to_string(2 * dsize) +
           " values are those of a potential retrained from another with "
           "tadah train -c before 1.6.0, which appended the new fit's values "
           "to the potential's: the model normalised with the first " + D +
           ", the other potential's. Retrain it (tadah train -c <its "
           "training configuration, or this potential>).";
  throw std::runtime_error(msg);
}

void require_weights_fit(const tadah::core::Context &c, std::size_t dsize,
                         const Function_Base &f) {
  require_weights_fit(c, dsize, f, trained_sizes(c));
}

} // namespace models
} // namespace tadah
