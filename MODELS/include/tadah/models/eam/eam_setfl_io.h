#ifndef TADAH_MODELS_EAM_SETFL_IO_H
#define TADAH_MODELS_EAM_SETFL_IO_H

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace tadah {
namespace models {

/** \brief Output-grid descriptor for the setfl writer. */
struct EAMGrid {
  int    nrho = 0;   ///< number of embedding-function (rho) grid points
  double drho = 0;   ///< rho grid spacing
  int    nr   = 0;   ///< number of radial grid points
  double dr   = 0;   ///< r grid spacing
  double rcut = 0;   ///< cutoff radius written to the header
};

/** \brief Reader / writer / spline-evaluator for tabulated EAM (setfl) potentials.
 *
 * Single-element setfl only (LAMMPS `eam/alloy` and `eam/fs` share an identical
 * single-element layout: one `frho`, one `rhor`, one `z2r`). This class lifts the
 * formerly-duplicated `read_setfl()` / `gen_splines()` out of \ref D2_EAM and
 * \ref DM_EAM into one reusable place, adds a setfl **writer**, and exposes the
 * original tabulated functions (and their cubic-spline tables) so the `tadah
 * refit` machinery can sample them as least-squares targets.
 *
 * Cubic splines and the index arithmetic follow the LAMMPS pair_eam
 * implementation byte-for-byte so the hot-path consumers (\ref D2_EAM /
 * \ref DM_EAM `calc_*`) keep identical numerics and performance after the
 * refactor — they bind a `const&` to the relevant spline table once per call.
 *
 * \note Multi-element setfl and the older DYNAMO funcfl format are deliberately
 *       out of scope for v1; `read_setfl()` throws on `ntypes != 1`.
 */
class EAMSetflIO {
public:
  EAMSetflIO() = default;

  /** Parse a single-element setfl file, as LAMMPS pair eam/alloy reads
   *  one: lines 1-3 are comments; from line 4 on, '#' starts a comment and
   *  a line without words is skipped; then the element line '1 <symbol>',
   *  the grid 'nrho drho nr dr rcut' (exactly these words; it may span
   *  lines), the element record 'Z mass a lattice' (exactly these words; it
   *  may span lines up to its mass, and a and lattice are on the line of the
   *  mass: LAMMPS reads the record to the end of that line) and the F(rho),
   *  rho(r) and z2r(r) tables, each starting on a new line and ending at the
   *  end of one. Lines after the z2r table are not read.
   *
   *  @throws std::runtime_error naming the file (and the line and the field
   *          where there is one) if the file is missing, declares more than
   *          one element type or is malformed: nrho or nr not a whole number
   *          from 3 to 2147483647 (the spline reads a table's third point);
   *          drho, dr or rcut not a positive finite number; r tables that
   *          end more than one step before rcut (nr x dr below rcut by more
   *          than 1e-4 of it: LAMMPS and tadah take a table's last value at
   *          every r past its last point, (nr - 1) x dr; a cutoff at the last
   *          point, before it or one step past it, rcut = nr x dr, is read)
   *          or an rcut / dr above 2147483647 (the spline index of a distance
   *          below rcut is an int); Z not a whole
   *          number; the mass or the lattice constant not a finite number; a
   *          record with a word too many or too few; an element record split
   *          after its mass (a or lattice on a later line); a table value that is
   *          not a finite decimal number (core::parse_decimal_finite); a
   *          table shorter than its count, or whose last line holds words
   *          after its last value. A refused file leaves this object as it
   *          was. */
  void read_setfl(const std::string &path);

  /** Parse the contents @p text of a single-element setfl file, as
   *  read_setfl() parses the file; @p path names it (path() and the
   *  messages). */
  void read_setfl_text(const std::string &text, const std::string &path);

  /** Build LAMMPS-style cubic splines for all three tabulated functions.
   *  Must be called after read_setfl(). */
  void build_splines();

  // -------- raw grid / metadata accessors --------
  int    nrho()  const { return nrho_; }
  double drho()  const { return drho_; }
  int    nr()    const { return nr_; }
  double dr()    const { return dr_; }
  double rdr()   const { return rdr_; }
  double rdrho() const { return rdrho_; }
  double rcut()  const { return rcut_; }
  double rhomax()const { return rhomax_; }
  int    atomic_number() const { return atomic_number_; }
  double atomic_mass()   const { return atomic_mass_; }
  double lattice_param() const { return lattice_param_; }
  /** Ground-state lattice label, normalised to lower case (e.g. "fcc"); the
   *  setfl field has no fixed case convention. The element record must
   *  hold it (read_setfl() refuses one without it). */
  const std::string &lattice() const { return lattice_; }
  /** The lattice label exactly as written in the file (original case), for
   *  faithful messages and round-trip output. */
  const std::string &lattice_raw() const { return lattice_raw_; }
  const std::string &element() const { return element_; }
  const std::string &path()    const { return file_path_; }

  // -------- raw tabulated arrays --------
  const std::vector<double> &frho() const { return frho_; }
  const std::vector<double> &rhor() const { return rhor_; }
  const std::vector<double> &z2r()  const { return z2r_; }

  // -------- spline tables (hot-path consumers bind a const& once) --------
  const std::vector<std::vector<double>> &frho_spline() const { return frho_spline_; }
  const std::vector<std::vector<double>> &rhor_spline() const { return rhor_spline_; }
  const std::vector<std::vector<double>> &z2r_spline()  const { return z2r_spline_; }

  // -------- original-table evaluators (refit sampling) --------
  /** Pair function \f$\phi(r) = z2r(r)/r\f$ (the physical two-body energy). */
  double phi_tab(double r) const;
  /** Raw z2r(r) = phi(r)*r spline value (finite at r=0, unlike phi_tab). */
  double z2_tab(double r) const;
  /** Electron density \f$\rho(r)\f$. */
  double rho_tab(double r) const;
  /** Embedding function \f$F(\rho)\f$. */
  double F_tab(double rho) const;

  // -------- ANALYTIC derivatives of the tabulated functions --------
  // The LAMMPS cubic-spline rows carry the derivative-polynomial coefficients
  // (spline[m][0..2]), so the 1st and 2nd derivatives are exact for the spline
  // — no finite differencing needed. phi = z2r/r, so its derivatives apply the
  // quotient rule to z2r; rho(r) and F(rho) are direct splines.
  double dphi_tab(double r) const;    ///< d phi / dr
  double d2phi_tab(double r) const;   ///< d^2 phi / dr^2
  double drho_tab(double r) const;    ///< d rho / dr
  double d2rho_tab(double r) const;   ///< d^2 rho / dr^2
  double dF_tab(double rho) const;    ///< dF / drho
  double d2F_tab(double rho) const;   ///< d^2 F / drho^2

  /** Convenience: the input grid as an EAMGrid (the writer's default). */
  EAMGrid input_grid() const { return EAMGrid{nrho_, drho_, nr_, dr_, rcut_}; }

  /** Build an output grid scaled relative to the input grid: nr * scale and
   *  nrho * scale points, rounded, over the same spans (n - 1) * d. A count
   *  equal to the input's keeps the input's step, so `scale == 1`, and any
   *  scale whose counts round to the input's, reproduces the input grid bit
   *  for bit (input_grid()). A new r count keeps the r span unless that
   *  leaves rcut more than one of the new steps past the new last point (an
   *  input whose cutoff is one step past its last point, rcut = nr * dr,
   *  scaled up): then dr = rcut / nr, so the grid reaches the cutoff as
   *  read_setfl() requires. `scale > 1` densifies (more points, finer
   *  spacing). rcut is preserved. Throws std::invalid_argument for a
   *  scale that is not a positive finite number, or whose counts fall
   *  below 3 (the spline fitted through a setfl table reads its third
   *  point) or above 2147483647, naming the scale and both counts. */
  EAMGrid scaled_grid(double scale) const;

  /** Write a single-element setfl file. The three callbacks supply the NATIVE
   *  fitted functions sampled on `grid`:
   *    - `frho_fn(rho) -> F`
   *    - `rhor_fn(r)   -> rho`
   *    - `z2r_fn(r)    -> z2r = phi(r) * r`
   *  Element symbol / Z / mass / lattice are carried over from the input file. */
  void write_setfl(const std::string &path,
                   const EAMGrid &grid,
                   const std::function<double(double)> &frho_fn,
                   const std::function<double(double)> &rhor_fn,
                   const std::function<double(double)> &z2r_fn,
                   const std::string &title = "Generated by tadah refit") const;

private:
  /** LAMMPS-style natural cubic spline of `f` (size n) onto `spline` (size
   *  n+1 x 7). Mirrors the former D2_EAM::gen_splines byte-for-byte. */
  static void gen_spline(int n, double delta,
                         const std::vector<double> &f,
                         std::vector<std::vector<double>> &spline);

  /** Evaluate a spline `value` at coordinate `x` with reciprocal spacing
   *  `rdx` over `n` points (1-based index, clamped to [1, n-1]). */
  static double eval_value(const std::vector<std::vector<double>> &spline,
                           double x, double rdx, int n);
  /** Analytic 1st derivative d(value)/dx of the same spline (LAMMPS deriv
   *  coefficients spline[m][0..2]); 2nd derivative d^2(value)/dx^2. */
  static double eval_deriv(const std::vector<std::vector<double>> &spline,
                           double x, double rdx, int n);
  static double eval_deriv2(const std::vector<std::vector<double>> &spline,
                            double x, double rdx, int n);

  std::string file_path_;
  std::string element_;
  std::vector<double> frho_, rhor_, z2r_;
  int    nrho_ = 0;  double drho_ = 0;
  int    nr_   = 0;  double dr_   = 0;
  double rdr_  = 0;  double rdrho_ = 0;
  double rcut_ = 0;  double rhomax_ = 0;
  int    atomic_number_ = 0;
  double atomic_mass_   = 0;
  double lattice_param_ = 0;
  std::string lattice_;       // normalised to lower case
  std::string lattice_raw_;   // original case, as written in the file
  std::vector<std::vector<double>> frho_spline_, rhor_spline_, z2r_spline_;
};

/** \brief The setfl file @p path, read and splined (EAMSetflIO::read_setfl
 *  and build_splines), shared by every caller in the process that reads
 *  the same file with the same contents.
 *
 *  D2_EAM and DM_EAM take their tables from here: one SETFL serves both,
 *  and tadah hpo, which builds the descriptors of every evaluation anew,
 *  parses the file once. Each call reads the file; it returns the table it
 *  parsed earlier for this path (the path as given, in the working
 *  directory it names) when the file's bytes are the bytes it parsed then,
 *  and otherwise parses the file again (a table already handed out is not
 *  changed). So the contents a descriptor is built from are always the
 *  file's contents at that moment. Thread-safe.
 *
 *  @throws std::runtime_error if the file cannot be read or is not a
 *          single-element setfl file (read_setfl's messages). */
std::shared_ptr<const EAMSetflIO> shared_setfl(const std::string &path);

} // namespace models
} // namespace tadah
#endif // TADAH_MODELS_EAM_SETFL_IO_H
