#include <tadah/models/eam/eam_refit.h>
#include <tadah/models/eam/eam_lattice.h>
#include <tadah/models/linear_regressor.h>
#include <tadah/models/descriptors/d_basis_functions.h>
#include <tadah/models/descriptors/basis_dispatch.h>
#include <tadah/models/descriptors/dm/embf0.h>
#include <tadah/models/descriptors/dm/rhoform.h>
#include <tadah/models/nnls.h>
#include <tadah/core/logger.h>
#include <tadah/core/number_format.h>
#include <tadah/core/utils.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <stdexcept>

namespace tadah {
namespace models {

EAMRefit::EAMRefit(const EAMSetflIO &eam, const EAMRefitConfig &cfg)
  : eam_(eam), cfg_(cfg) {
  // Shared, single-source-of-truth parser (Blip/Blip5/Gaussian/BP/Knot5 and the
  // Knot3/KnotMix/Mixed/M6 aliases). The Knot5 family's per-atom degree arrays
  // then select cubic vs quintic knot atoms.
  family_ = basis_kind_from_string(cfg.basis);
  if(cfg_.component) {
    cfg_.rho_form=cfg_.component->form;
    cfg_.embf0_zero=cfg_.component->reference==EmbF0Mode::Zero;
    component_evaluator_=std::make_unique<EmbedEvaluator>(*cfg_.component);
  }
  rcut_   = eam_.rcut();
  r_lo_   = std::max(cfg_.r_lo_frac * rcut_, eam_.dr());
  rhomax_ = eam_.rhomax();
  spatial_cutoff_.reset(core::factory<Cut_Base,double&>(cfg_.rctype,rcut_));
  rho_join_ = rhomax_;  // dense default: F_setfl == F_fit on the table range
  if (cfg_.embf0_zero) {
    // EMBF0 ZERO: the table's F(0), removed from every absolute use of the
    // table embedding (F_ref). Cached once; the spline is never modified.
    if (eam_.frho_spline().empty())
      throw std::runtime_error(
          "EAMRefit: the EAM table splines are not built — call "
          "EAMSetflIO::build_splines() before constructing EAMRefit.");
    F0tab_ = eam_.F_tab(0.0);
  }
  if (!cfg_.component && cfg_.rho_form == RhoForm::Monotone) {
    // MONOTONE fits integrated-bump steps: the bump families only, under a
    // cutoff that keeps rho non-increasing (the checks DM_REAM applies when
    // it loads the written potential, made before any fitting work).
    if (family_ == BasisKind::Knot5)
      throw DensityFormError(
          "EAMRefit: RHOFORM MONOTONE is not defined for the knot family "
          "(basis " + cfg_.basis + "); RHOFORM NONNEG fits it: " +
          monotone_knot_refusal_text() + ".");
    if (!rhoform_cutoff_whitelisted(cfg_.rctype))
      throw DensityFormError(
          "EAMRefit: RHOFORM MONOTONE requires a non-negative, "
          "non-increasing many-body cutoff: rctype must be one of " +
          rhoform_cutoff_whitelist_text() + " (got " + cfg_.rctype + ").");
  }
}

double EAMRefit::F_ref(double rho) const {
  if (!cfg_.embf0_zero) return eam_.F_tab(rho);   // FREE: the table verbatim
  return eam_.F_tab(rho) - F0tab_;                // ZERO: exactly 0 at rho = 0
}

// The spatial cutoff the written potential evaluates (EAMRefitConfig::rctype,
// RCTYPE2B and the density's cutoff): f_c(r) inside r_cut, 0 beyond. The
// hard Cut_Dummy (f_c = 1 inside r_cut) matches LAMMPS eam/alloy, so the
// native fit reproduces the original tabulated potential.
double EAMRefit::cutoff(double r) const {
  return r<=rcut_?spatial_cutoff_->calc(r):0;
}

double EAMRefit::basisval(double x, double center, double width,
                          bool emb, int deg) const {
  // The SAME shared dispatcher the DM_REAM / D2 descriptors use, so a refitted
  // potential evaluates the identical basis at fit time and at run time (no
  // parallel hand-copy of the basis math to drift). `width` is the eta gauge
  // (1.0 for the knot family, whose knot arms ignore it); `emb` picks the
  // RIGHT-active embedding orientation vs the LEFT-active phi/rho orientation.
  return emb ? embedding_value(family_, x, width, center, deg)
             : radial_value_bare(family_, x, width, center, deg);
}

void EAMRefit::make_grid(double lo, double hi, int n,
                         tadah::core::v_type &centers,
                         tadah::core::v_type &widths, bool radial) const {
  centers.assign(n, 0.0);
  widths.assign(n, 0.0);
  // Radial bump grids (Blip w=2, Blip5 w=3) against the hard rcut at `hi`:
  // dx = (hi-lo)/(n-1+w) puts the outermost centre at hi - w*dx, so with
  // eta = 1/dx its support ends EXACTLY at hi — the curve vanishes smoothly
  // at the cutoff instead of leaving a truncated tail (the bump-support
  // invariant `c + w/eta <= RCUT` that hpo/refit/edit enforce).
  const double wb = bump_halfwidth(family_);
  const bool cap = radial && wb > 0.0;
  double dx = cap     ? (hi - lo) / (n - 1 + wb)
            : (n > 1) ? (hi - lo) / (n - 1)
                      : (hi - lo);
  if (dx <= 0.0) dx = (hi > 0.0 ? hi : 1.0);
  for (int k = 0; k < n; ++k) {
    centers[k] = lo + dx * k;
    // Blip: eta = 1/dx  -> support |r-rs| < 2*dx, i.e. each blip overlaps its
    //   two nearest neighbours on each side (cubic-B-spline-like coverage).
    // Gaussian: eta = 0.5/dx^2 (sigma ~ dx).
    // Knot5: eta is a pure gauge — pinned to 1 (the knot mu is the shape).
    widths[k] = (family_ == BasisKind::Gaussian) ? 0.5 / (dx * dx)
              : (family_ == BasisKind::Knot5)    ? 1.0
                                              : 1.0 / dx;  // Blip + Blip5
  }
}

// Two uniform sub-grids: the bulk of the basis on the physically REACHABLE
// window [win_lo,win_hi] and a small uniform allocation on the unreachable
// tail(s), so the tabulated output stays defined everywhere.
//
// Why uniform-per-region and not curvature-equidistributed: nonuniformly
// stretched blips are NOT a B-spline basis on nonuniform knots — irregular
// spacing destroys the approximation power of uniform translates. Measured
// (Ti, accurate): curvature placement gave 49 meV/atom ambient error where
// uniform gave 0.35 meV. Region-wise uniform keeps uniform-grid quality while
// concentrating capacity where a crystal can actually go.
void EAMRefit::make_grid_windowed(double lo, double hi, int n,
                                  double win_lo, double win_hi,
                                  tadah::core::v_type &centers,
                                  tadah::core::v_type &widths,
                                  bool radial) const {
  win_lo = std::max(win_lo, lo);
  win_hi = std::min(win_hi, hi);
  const double span = hi - lo, wspan = win_hi - win_lo;
  if (n < 8 || wspan <= 0.0 || wspan >= 0.999 * span) {
    make_grid(lo, hi, n, centers, widths, radial);
    return;
  }
  // tail blip count: proportional to tail length but capped so the window
  // keeps >= 80% of the basis; at least 4 blips per non-empty tail
  const double tail_lo_len = win_lo - lo, tail_hi_len = hi - win_hi;
  auto tail_n = [&](double len) {
    if (len <= 0.0) return 0;
    return std::max(4, static_cast<int>(std::floor(n * std::min(0.2, len / span))));
  };
  int n_lo = tail_n(tail_lo_len), n_hi = tail_n(tail_hi_len);
  if (n_lo + n_hi > n / 3) {  // keep the window dominant
    const double f = (n / 3.0) / (n_lo + n_hi);
    n_lo = static_cast<int>(n_lo * f);
    n_hi = static_cast<int>(n_hi * f);
  }
  const int n_win = n - n_lo - n_hi;

  centers.clear(); widths.clear();
  centers.reserve(n); widths.reserve(n);
  // cap_b: this segment's upper edge b IS the hard rcut — for radial bump
  // grids use dx = (b-a)/(m-1+w) so the outermost support ends exactly at b
  // (see make_grid).
  const double wb = bump_halfwidth(family_);
  const bool cap_bump = radial && wb > 0.0;
  auto add_uniform = [&](double a, double b, int m, bool incl_b, bool cap_b) {
    if (m <= 0) return;
    double dx = (cap_b && cap_bump) ? (b - a) / (m - 1 + wb)
              : (m > 1)             ? (b - a) / (incl_b ? (m - 1) : m)
                                    : std::max(b - a, 1e-12);
    if (dx <= 0.0) dx = std::max(b - a, 1e-12);
    for (int k = 0; k < m; ++k) {
      centers.push_back(a + dx * k);
      widths.push_back((family_ == BasisKind::Gaussian) ? 0.5 / (dx * dx)
                     : (family_ == BasisKind::Knot5)    ? 1.0
                                                     : 1.0 / dx);
    }
  };
  add_uniform(lo, win_lo, n_lo, false, false);      // lower tail (excl. win_lo)
  add_uniform(win_lo, win_hi, n_win, n_hi == 0, n_hi == 0);  // the window
  if (n_hi > 0) {
    add_uniform(win_hi, hi, n_hi, true, true);      // upper tail (ends at rcut)
  }
}

FitStats EAMRefit::lsq_fit_windowed(const std::vector<double> &xs,
                                    const std::vector<double> &ys,
                                    double win_lo, double win_hi,
                                    const tadah::core::v_type &centers,
                                    const tadah::core::v_type &widths,
                                    bool apply_cutoff,
                                    tadah::core::v_type &coeffs,
                                    bool pin, double pin_x, double pin_y,
                                    bool zero_ref, RhoForm form) const {
  const std::size_t Nd = xs.size();
  const std::size_t Nb = centers.size();
  coeffs.assign(Nb, 0.0);

  // RHOFORM NONNEG / MONOTONE (density channel only): every block is solved
  // by non-negative least squares, NONNEG on the bump columns, MONOTONE on
  // the integrated-bump step columns S_k. SIGNED (and the phi / F channels)
  // keep the GELSD statements below unchanged.
  const bool positive = rhoform_is_positive(form);
  if (positive && zero_ref)
    throw std::logic_error(
        "EAMRefit::lsq_fit_windowed: a density form applies to the density "
        "channel only, zero_ref to the embedding channel only.");
  auto column = [&](double x, std::size_t k) {
    return form == RhoForm::Monotone
               ? radial_step_value_bare(family_, x, widths[k], centers[k])
               : basisval(x, centers[k], widths[k]);
  };

  // EMBF0 ZERO (zero_ref): every value use below takes the zero-referenced
  // column b_k(x) - b_k(0), b_k(0) from the SAME evaluator as the columns.
  // Computed only under ZERO; the raw-basis (FREE) statements are unchanged.
  tadah::core::v_type b0;
  if (zero_ref) {
    b0.assign(Nb, 0.0);
    for (std::size_t k = 0; k < Nb; ++k)
      b0[k] = basisval(0.0, centers[k], widths[k]);
  }

  // Partition blips by support overlap with the window (blip support: |x-c| <
  // 2/eta for blips, ~3 sigma for gaussians) and samples by location.
  // NOTE the dense windowed solve is NOT used for the one-sided Knot5 family
  // (its support is one-sided and unbounded toward the active side, so the
  // two-stage block separation does not apply; the engine restricts Knot5 to
  // compact mode, which injects externally-optimised parameters instead).
  std::vector<std::size_t> bA, bB, sA, sB;
  for (std::size_t k = 0; k < Nb; ++k) {
    const double half = (family_ == BasisKind::Gaussian)
                            ? 3.0 / std::sqrt(2.0 * widths[k])
                        : (family_ == BasisKind::Blip5)
                            ? 3.0 / widths[k]
                            : 2.0 / widths[k];
    // A step column S_k is 1 below the bump, so its support (-inf,
    // c + half] reaches the window whenever its outer edge does.
    const bool in_window =
        form == RhoForm::Monotone
            ? centers[k] + half >= win_lo
            : centers[k] + half >= win_lo && centers[k] - half <= win_hi;
    if (in_window)
      bA.push_back(k);
    else
      bB.push_back(k);
  }
  for (std::size_t i = 0; i < Nd; ++i)
    (xs[i] >= win_lo && xs[i] <= win_hi ? sA : sB).push_back(i);

  // Positive forms: the same block (held coefficients subtracted, optional
  // pin row) as a column-major array for nnls(); LAMBDA enters as its
  // appended sqrt(lambda) I rows, the LinearRegressor convention.
  auto solve_block_nnls = [&](const std::vector<std::size_t> &rows,
                              const std::vector<std::size_t> &cols,
                              bool with_pin) {
    const std::size_t m = rows.size() + (with_pin ? 1 : 0);
    const std::size_t n = cols.size();
    std::vector<double> A(m * n, 0.0), b(m, 0.0), x(n, 0.0);
    for (std::size_t r = 0; r < rows.size(); ++r) {
      const std::size_t i = rows[r];
      const double fc = apply_cutoff ? cutoff(xs[i]) : 1.0;
      double held = 0.0;
      for (std::size_t k = 0; k < Nb; ++k)
        if (coeffs[k] != 0.0) held += coeffs[k] * column(xs[i], k) * fc;
      b[r] = ys[i] - held;
      for (std::size_t q = 0; q < n; ++q)
        A[q * m + r] = column(xs[i], cols[q]) * fc;
    }
    if (with_pin) {  // heavily-weighted single constraint row
      const double sw = 1e4;
      const std::size_t r = rows.size();
      b[r] = pin_y * sw;
      for (std::size_t q = 0; q < n; ++q)
        A[q * m + r] = column(pin_x, cols[q]) * sw;
    }
    const NnlsResult res =
        nnls(A.data(), m, n, b.data(), x.data(), nullptr, nullptr,
             cfg_.lambda);
    if (!res.converged)
      tadah::core::Logger::instance().log(tadah::core::LogLevel::WARNING,
          std::string("EAMRefit: the RHOFORM ") + rhoform_name(form) +
          " non-negative least-squares block stopped before a Kuhn-Tucker "
          "point (" + nnls_stop_text(res.stop) + ", after " +
          std::to_string(res.iterations) + " iterations): the amplitudes "
          "are >= 0 but not proven optimal.");
    for (std::size_t q = 0; q < n; ++q) coeffs[cols[q]] = x[q];
  };

  auto solve_block = [&](const std::vector<std::size_t> &rows,
                         const std::vector<std::size_t> &cols,
                         bool with_pin) {
    if (cols.empty()) return;
    const std::size_t extra = with_pin ? 1 : 0;
    if (rows.size() + extra == 0) return;
    if (positive) {
      solve_block_nnls(rows, cols, with_pin);
      return;
    }
    tadah::core::phi_type Phi(rows.size() + extra + cols.size(), cols.size());
    Phi.set_zero();
    tadah::core::t_type T(rows.size() + extra + cols.size());
    for (std::size_t r = 0; r < rows.size(); ++r) {
      const std::size_t i = rows[r];
      const double fc = apply_cutoff ? cutoff(xs[i]) : 1.0;
      if (zero_ref) {  // EMBF0 ZERO: the same row, zero-referenced columns
        double held = 0.0;
        for (std::size_t k = 0; k < Nb; ++k)
          if (coeffs[k] != 0.0)
            held += coeffs[k] *
                    (basisval(xs[i], centers[k], widths[k]) - b0[k]) * fc;
        T(r) = ys[i] - held;
        for (std::size_t q = 0; q < cols.size(); ++q)
          Phi(r, q) = (basisval(xs[i], centers[cols[q]], widths[cols[q]]) -
                       b0[cols[q]]) * fc;
        continue;
      }
      // subtract the contribution of already-solved (other-block) blips
      double held = 0.0;
      for (std::size_t k = 0; k < Nb; ++k)
        if (coeffs[k] != 0.0)
          held += coeffs[k] * basisval(xs[i], centers[k], widths[k]) * fc;
      T(r) = ys[i] - held;
      for (std::size_t q = 0; q < cols.size(); ++q)
        Phi(r, q) = basisval(xs[i], centers[cols[q]], widths[cols[q]]) * fc;
    }
    if (with_pin && zero_ref) {  // EMBF0 ZERO (fit() passes no pin: the
      const double sw = 1e4;     // F(0) row would be identically zero)
      const std::size_t r = rows.size();
      T(r) = pin_y * sw;
      for (std::size_t q = 0; q < cols.size(); ++q)
        Phi(r, q) = (basisval(pin_x, centers[cols[q]], widths[cols[q]]) -
                     b0[cols[q]]) * sw;
    } else if (with_pin) {  // heavily-weighted single constraint row
      const double sw = 1e4;
      const std::size_t r = rows.size();
      T(r) = pin_y * sw;
      for (std::size_t q = 0; q < cols.size(); ++q)
        Phi(r, q) = basisval(pin_x, centers[cols[q]], widths[cols[q]]) * sw;
    }
    tadah::core::Context lc;
    // GELSD (SVD, rank-revealing): a tail block can be legitimately
    // rank-deficient (a blip whose support holds no samples); GELS (QR)
    // aborts on that (DGELS info=1).
    lc.add("OALGO", 1);
    lc.add("LAMBDA", cfg_.lambda);
    tadah::core::t_type w;
    LinearRegressor::train(lc, Phi, T, w);
    for (std::size_t q = 0; q < cols.size(); ++q) coeffs[cols[q]] = w(q);
  };

  const bool pinA = pin && pin_x >= win_lo && pin_x <= win_hi;
  solve_block(sA, bA, pinA);
  solve_block(sB, bB, pin && !pinA);

  // stats over the WINDOW samples (the physically relevant region)
  FitStats st;
  double se = 0.0, ae = 0.0, mx = 0.0;
  for (std::size_t i : sA) {
    const double fc = apply_cutoff ? cutoff(xs[i]) : 1.0;
    double pred = 0.0;
    if (zero_ref)  // EMBF0 ZERO: zero-referenced columns
      for (std::size_t k = 0; k < Nb; ++k)
        pred += coeffs[k] *
                (basisval(xs[i], centers[k], widths[k]) - b0[k]) * fc;
    else if (positive)  // RHOFORM NONNEG / MONOTONE: the form's columns
      for (std::size_t k = 0; k < Nb; ++k)
        pred += coeffs[k] * column(xs[i], k) * fc;
    else
      for (std::size_t k = 0; k < Nb; ++k)
        pred += coeffs[k] * basisval(xs[i], centers[k], widths[k]) * fc;
    const double res = pred - ys[i];
    se += res * res; ae += std::fabs(res); mx = std::max(mx, std::fabs(res));
  }
  st.n = sA.size();
  if (!sA.empty()) { st.rmse = std::sqrt(se / sA.size()); st.mae = ae / sA.size(); }
  st.maxabs = mx;
  return st;
}

// Reachability analysis on the native lattice (header a0/lattice): what pair
// distances / embedding densities can a crystal actually visit between strong
// compression (a = reach_smin*a0, V/V0 ~ smin^3) and mild expansion? Basis
// capacity concentrates there. Called by fit(); compact mode calls it
// directly because it needs the windows BEFORE any fitting.
void EAMRefit::analyze_reachability() {
  lattice_known_ = eam_lattice::supported(eam_.lattice());
  const std::string lat = lattice_known_ ? eam_.lattice() : "bcc";
  const double a0 = eam_.lattice_param();
  r_reach_ = 0.0; rho_reach_ = 0.0;
  if (a0 > 0.0) {
    r_reach_ = eam_lattice::reach_distance(lat, a0, cfg_.reach_smin);
    // densest reachable embedding density over the compression sweep
    const auto dists = eam_lattice::neighbour_distances(
        lat, a0, rcut_ / std::max(cfg_.reach_smin, 0.1));
    double rho_top = 0.0;
    for (int is = 0; is <= 28; ++is) {
      const double s = cfg_.reach_smin + (1.10 - cfg_.reach_smin) * is / 28.0;
      double acc = 0.0;
      for (double d : dists) {
        const double rs = s * d;
        if (rs < rcut_) acc += eam_.rho_tab(rs);
      }
      rho_top = std::max(rho_top, acc);
    }
    rho_reach_ = std::min(cfg_.reach_margin * rho_top, rhomax_);
    if (rho_reach_ <= 0.0) rho_reach_ = rhomax_;
  }
}

void EAMRefit::fit() {
  const bool adaptive = (cfg_.placement != "uniform");
  analyze_reachability();
  if (!adaptive) { r_reach_ = 0.0; rho_reach_ = 0.0; }  // legacy uniform mode

  // ---- radial dense sample grid (shared by phi and rho) ----
  const int nr_samp = std::max(
      {2048, cfg_.n2b * cfg_.oversample, cfg_.nrho * cfg_.oversample});
  std::vector<double> xr, yphi, yrho;
  xr.reserve(nr_samp); yphi.reserve(nr_samp); yrho.reserve(nr_samp);
  for (int i = 0; i < nr_samp; ++i) {
    const double r = r_lo_ + (rcut_ - r_lo_) * i / (nr_samp - 1);
    if (r <= 0.0) continue;
    xr.push_back(r);
    yphi.push_back(eam_.phi_tab(r));
    yrho.push_back(eam_.rho_tab(r));
  }

  // Radial fit window. The window's lower edge introduces a boundary
  // error-decay zone of a few blip spacings (measured phi err 5.8e-3 at the
  // edge vs 4.5e-5 interior), and that zone is FATAL when it touches visited
  // shells of a material with steep F' (Ti: drho ~ 0.01 x F'=+14.7 -> 172
  // meV/atom). So the window shrinks below the full range ONLY when a genuine
  // unfittable wall is detected (|phi| or rho exceeding K x its reachable-
  // window scale): tame files keep the full uniform grid and no interior
  // boundary at all.
  double rwin_lo = r_lo_;
  if (adaptive && r_reach_ > 0.0) {
    const double Kfit = 30.0;
    auto p2p = [&](const std::vector<double> &ys, double xmin) {
      double lo = 1e300, hi = -1e300;
      for (std::size_t i = 0; i < xr.size(); ++i)
        if (xr[i] >= xmin) { lo = std::min(lo, ys[i]); hi = std::max(hi, ys[i]); }
      return std::max(hi - lo, 1e-300);
    };
    const double phi_sc = p2p(yphi, r_reach_), rho_sc = p2p(yrho, r_reach_);
    double r_K = r_lo_;
    for (std::size_t i = 0; i < xr.size(); ++i) {
      if (std::fabs(yphi[i]) > Kfit * phi_sc || yrho[i] > Kfit * rho_sc)
        r_K = xr[i];  // xr ascending: ends at the LARGEST offending r
    }
    // never let the wall edge encroach on the reachable zone
    rwin_lo = std::min(std::max(r_lo_, r_K), 0.8 * r_reach_);
  }

  // The window phi and rho are fitted on: the samples (xr) and the grid
  // (make_grid_windowed clamps the window to its range) start at r_lo_, so
  // a wall edge 0.8 r_reach below r_lo_ leaves the window at r_lo_.
  fit_r_lo_ = std::max(rwin_lo, r_lo_);

  // ---- RHOFORM NONNEG / MONOTONE: can the form follow the table? ----
  // Checked before any fitting: a table outside the form would otherwise
  // burn the whole escalation ladder on fits that can never pass. On the
  // fit window, fit_r_lo_: never below r_lo_, since the table below it
  // (where 0.8 r_reach < r_lo_) is fitted by nothing, so the form need not
  // follow it there. With a wall (rwin_lo > r_lo_) the samples in
  // [r_lo_, rwin_lo) are fitted in lsq_fit_windowed's tail block (NNLS
  // under NONNEG/MONOTONE, so in the form; not in the window's stats) and
  // are not checked.
  check_rho_table(fit_r_lo_);

  // ---- 1. pair phi(r) -> 2-body linear weights ----
  if (adaptive) make_grid_windowed(r_lo_, rcut_, cfg_.n2b, rwin_lo, rcut_,
                                   cgrid2b_, sgrid2b_, true);
  else          make_grid(r_lo_, rcut_, cfg_.n2b, cgrid2b_, sgrid2b_, true);
  phi_stats_ = lsq_fit_windowed(xr, yphi, rwin_lo, rcut_, cgrid2b_, sgrid2b_,
                                true, c2b_, false, 0.0, 0.0);

  if(cfg_.component) { fit_components(rwin_lo); return; }

  // ---- 2. density rho(r) -> baked coefficients ----
  if (adaptive) make_grid_windowed(r_lo_, rcut_, cfg_.nrho, rwin_lo, rcut_,
                                   crho_, srho_, true);
  else          make_grid(r_lo_, rcut_, cfg_.nrho, crho_, srho_, true);
  rho_stats_ = lsq_fit_windowed(xr, yrho, rwin_lo, rcut_, crho_, srho_,
                                true, arho_, false, 0.0, 0.0,
                                /*zero_ref=*/false, cfg_.rho_form);

  // ---- 3. embedding F(rho) over [0, rhomax] -> many-body linear weights ----
  const int nrho_samp = std::max(2048, cfg_.nF * cfg_.oversample);
  std::vector<double> xrho, yF;
  xrho.reserve(nrho_samp); yF.reserve(nrho_samp);
  for (int i = 0; i < nrho_samp; ++i) {
    const double rho = rhomax_ * i / (nrho_samp - 1);
    xrho.push_back(rho);
    yF.push_back(F_ref(rho));   // FREE: eam_.F_tab(rho) verbatim
  }
  const double rho_win_hi = (adaptive && rho_reach_ > 0.0) ? rho_reach_
                                                           : rhomax_;
  fit_rho_hi_ = rho_win_hi;
  if (adaptive) make_grid_windowed(0.0, rhomax_, cfg_.nF, 0.0, rho_win_hi,
                                   cF_, sF_, false);   // F: no r cutoff
  else          make_grid(0.0, rhomax_, cfg_.nF, cF_, sF_, false);
  if (cfg_.embf0_zero) {
    // EMBF0 ZERO: fit what DM_REAM evaluates — the zero-referenced basis
    // against F_ref. F(0) = 0 exactly by construction, so no pin row.
    F_stats_ = lsq_fit_windowed(xrho, yF, 0.0, rho_win_hi, cF_, sF_, false,
                                cmb_, false, 0.0, 0.0, /*zero_ref=*/true);
  } else {
    // pin = the isolated-atom reference F(0) (the table's own value)
    F_stats_ = lsq_fit_windowed(xrho, yF, 0.0, rho_win_hi, cF_, sF_, false,
                                cmb_, cfg_.constrain_F0, 0.0, eam_.F_tab(0.0));
  }
  refresh_embedding_reference();

  // ---- concatenated linear weights [c2b | cmb] ----
  weights_.clear();
  weights_.reserve(c2b_.size() + cmb_.size());
  for (double v : c2b_) weights_.push_back(v);
  for (double v : cmb_) weights_.push_back(v);
}

namespace {
inline int deg_at(const std::vector<int> &d, std::size_t k) {
  return (k < d.size()) ? d[k] : 5;
}
}  // namespace

// g_m(0) with F_fit's own evaluator (embedding orientation, per-atom degree)
// — the value DM_REAM subtracts (embedding_value at rho = 0).
void EAMRefit::refresh_embedding_reference() {
  g0F_.clear();
  if (!cfg_.embf0_zero) return;   // FREE: raw basis, no reference
  g0F_.resize(cF_.size());
  for (std::size_t m = 0; m < cF_.size(); ++m)
    g0F_[m] = basisval(0.0, cF_[m], sF_[m], /*emb=*/true, deg_at(degF_, m));
}

double EAMRefit::phi_fit(double r) const {
  const double fc = cutoff(r);
  if (fc == 0.0) return 0.0;
  double acc = 0.0;
  for (std::size_t k = 0; k < cgrid2b_.size(); ++k)
    acc += c2b_[k] * basisval(r, cgrid2b_[k], sgrid2b_[k], false,
                              deg_at(deg2b_, k));
  return acc * fc;
}

double EAMRefit::rho_term(double r, std::size_t n) const {
  if (cfg_.rho_form == RhoForm::Monotone)  // the integrated bump S_n
    return radial_step_value_bare(family_, r, srho_[n], crho_[n]);
  return basisval(r, crho_[n], srho_[n], false, deg_at(degrho_, n));
}

// The terms in DM_REAM's order (ascending n, a_n times the term, the cutoff
// outside the sum, f_c(r) of the configured spatial cutoff): for r < rcut
// this is the pair density of the written potential, including under
// MONOTONE.
double EAMRefit::rho_fit(double r) const {
  if(component_evaluator_) return component_evaluator_->density(r).value;
  const double fc = cutoff(r);
  if (fc == 0.0) return 0.0;
  double acc = 0.0;
  for (std::size_t n = 0; n < crho_.size(); ++n)
    acc += arho_[n] * rho_term(r, n);
  return acc * fc;
}

void EAMRefit::check_rho_table(double win_lo) const {
  if (!rhoform_is_positive(cfg_.rho_form)) return;
  // The table nodes r_i = i dr of the fit window, not spline samples: the
  // cubic interpolant of a monotone table can overshoot between nodes near
  // a steep edge, which no form has to follow (the fit only approximates
  // it). The tolerance covers the rounding of the printed table values.
  const std::vector<double> &tab = eam_.rhor();
  std::vector<std::size_t> nodes;
  double amax = 0.0;
  for (std::size_t i = 0; i < tab.size(); ++i) {
    const double r = static_cast<double>(i) * eam_.dr();
    if (r < win_lo || r > rcut_) continue;
    nodes.push_back(i);
    amax = std::max(amax, std::fabs(tab[i]));
  }
  const double tol = 8.0 * std::numeric_limits<double>::epsilon() * amax;
  const std::string form = rhoform_name(cfg_.rho_form);
  const std::string window = "the fit window [" +
                             tadah::core::to_string(win_lo, 6) + ", " +
                             tadah::core::to_string(rcut_, 6) + "] A";
  // The key that declares the form, and its remedy: a component's FORM (a
  // configuration with DESCRIPTOR blocks refuses the DM_REAM key RHOFORM),
  // or RHOFORM.
  const std::string who =
      cfg_.component ? "the density component " +
                           tadah::core::to_upper(cfg_.component->id) +
                           "'s FORM "
                     : "RHOFORM ";
  const auto use = [&](const std::string &forms) {
    return cfg_.component ? "FORM " + forms + " in its DENSITY block fits it."
                          : "RHOFORM " + forms + " fits it.";
  };
  for (const std::size_t i : nodes) {
    if (!(tab[i] < -tol)) continue;
    throw std::runtime_error(
        "tadah refit: " + who + form + " cannot fit this table: the "
        "tabulated density is rho = " + tadah::core::format_roundtrip(tab[i]) +
        " < 0 at r = " +
        tadah::core::to_string(static_cast<double>(i) * eam_.dr(), 6) +
        " A, inside " + window + ", and a " + form + " density is >= 0 "
        "(every amplitude a_n >= 0). " + use("SIGNED"));
  }
  if (cfg_.rho_form != RhoForm::Monotone) return;
  for (std::size_t q = 0; q + 1 < nodes.size(); ++q) {
    const std::size_t i = nodes[q], j = nodes[q + 1];
    if (!(tab[j] - tab[i] > tol)) continue;
    throw std::runtime_error(
        "tadah refit: " + who + "MONOTONE cannot fit this table: the tabulated "
        "density rises from rho = " + tadah::core::format_roundtrip(tab[i]) +
        " at r = " +
        tadah::core::to_string(static_cast<double>(i) * eam_.dr(), 6) +
        " A to " + tadah::core::format_roundtrip(tab[j]) + " at r = " +
        tadah::core::to_string(static_cast<double>(j) * eam_.dr(), 6) +
        " A, inside " + window + ", and a MONOTONE density is "
        "non-increasing. " + use("NONNEG or SIGNED"));
  }
}

double EAMRefit::F_fit(double rho) const {
  if(component_evaluator_) {
    double out=0;
    for(std::size_t i=0;i<cmb_.size();++i) out+=cmb_[i]*component_evaluator_->embedding(i,rho).value;
    return out;
  }
  if (cfg_.embf0_zero) {
    // EMBF0 ZERO: F(rho) = sum_m c_m (g_m(rho) - g_m(0)), as DM_REAM; exactly
    // 0.0 at rho == 0 whatever the FP contraction.
    if (rho == 0.0) return 0.0;
    double acc = 0.0;
    for (std::size_t m = 0; m < cF_.size(); ++m)
      acc += cmb_[m] * (basisval(rho, cF_[m], sF_[m], /*emb=*/true,
                                 deg_at(degF_, m)) - g0F_[m]);
    return acc;
  }
  double acc = 0.0;
  for (std::size_t m = 0; m < cF_.size(); ++m)
    acc += cmb_[m] * basisval(rho, cF_[m], sF_[m], /*emb=*/true,
                              deg_at(degF_, m));
  return acc;
}

void EAMRefit::set_blip_fit(Fn which, const std::vector<double> &mu,
                            const std::vector<double> &eta,
                            const std::vector<double> &c,
                            const std::vector<int> &deg) {
  auto assign = [](tadah::core::v_type &dst, const std::vector<double> &src) {
    dst.assign(src.begin(), src.end());
  };
  switch (which) {
    case Fn::PHI: assign(cgrid2b_, mu); assign(sgrid2b_, eta);
                  assign(c2b_, c); deg2b_ = deg; break;
    case Fn::RHO:
      // The positive forms keep every amplitude >= 0 (what DM_REAM
      // validates on load); an injected fit must honour it.
      if (rhoform_is_positive(cfg_.rho_form))
        for (std::size_t n = 0; n < c.size(); ++n)
          if (!std::isfinite(c[n]) || c[n] < 0.0)
            throw DensityFormError(
                std::string("EAMRefit: RHOFORM ") +
                rhoform_name(cfg_.rho_form) + " density amplitudes must be "
                "finite and >= 0; injected amplitude " +
                std::to_string(n + 1) + " is " +
                tadah::core::format_roundtrip(c[n]) + ".");
      assign(crho_, mu); assign(srho_, eta);
      assign(arho_, c); degrho_ = deg; break;
    case Fn::F:   assign(cF_, mu); assign(sF_, eta);
                  assign(cmb_, c); degF_ = deg;
                  refresh_embedding_reference(); break;
  }
}

void EAMRefit::finalize_injected(double rwin_lo, double rho_win_hi) {
  // the windows the injected fit was made on (fit_window_r_lo/_rho_hi)
  fit_r_lo_ = rwin_lo;
  fit_rho_hi_ = rho_win_hi;
  weights_.clear();
  weights_.reserve(c2b_.size() + cmb_.size());
  for (double v : c2b_) weights_.push_back(v);
  for (double v : cmb_) weights_.push_back(v);

  auto stats_on = [&](const std::function<double(double)> &fitf,
                      const std::function<double(double)> &tabf, double lo,
                      double hi) {
    FitStats st;
    const int N = 1000;
    double se = 0, ae = 0, mx = 0;
    for (int i = 0; i < N; ++i) {
      const double x = lo + (hi - lo) * i / (N - 1);
      const double d = fitf(x) - tabf(x);
      se += d * d; ae += std::fabs(d); mx = std::max(mx, std::fabs(d));
    }
    st.n = N; st.rmse = std::sqrt(se / N); st.mae = ae / N; st.maxabs = mx;
    return st;
  };
  phi_stats_ = stats_on([&](double r) { return phi_fit(r); },
                        [&](double r) { return eam_.phi_tab(r); }, rwin_lo,
                        rcut_);
  rho_stats_ = stats_on([&](double r) { return rho_fit(r); },
                        [&](double r) { return eam_.rho_tab(r); }, rwin_lo,
                        rcut_);
  F_stats_ = stats_on([&](double x) { return F_fit(x); },
                      [&](double x) { return F_ref(x); }, 0.0,  // FREE: F_tab
                      rho_win_hi);
}

// EMBF0 ZERO needs no change here: F_setfl(0) = F_fit(0) = 0.0, and F_tab(0)
// cancels in the join offset, so beyond the join the continuation is
// F_ref + (F_fit - F_ref)(rho_join).
double EAMRefit::F_setfl(double rho) const {
  if (rho <= rho_join_) return F_fit(rho);
  return eam_.F_tab(rho) + (F_fit(rho_join_) - eam_.F_tab(rho_join_));
}

// Below r_lo the most faithful tabulation is the ORIGINAL table itself, shifted
// by the (small, ~fit-error-sized) offset that makes it C0-continuous with the
// fit at the join (z2_setfl, phi_setfl and rho_setfl). No invented functional
// form, no hyper-parameters; sentinel walls and deep-core shapes are preserved
// exactly as the source potential has them. (A C2-matched exp continuation was
// tried first and ABANDONED: it faithfully extrapolates the blip-edge
// curvature artefact and collapses to zero — see cycle2 report.)
double EAMRefit::z2_setfl(double r) const {
  if (r >= r_lo_) return phi_fit(r) * r;
  // original z2r + (C0 offset in phi) * r — finite at r = 0
  return eam_.z2_tab(r) + (phi_fit(r_lo_) - eam_.phi_tab(r_lo_)) * r;
}

double EAMRefit::phi_setfl(double r) const {
  if (r >= r_lo_) return phi_fit(r);
  return r > 1e-12 ? z2_setfl(r) / r : 0.0;
}

double EAMRefit::rho_setfl(double r) const {
  if (r >= r_lo_) return rho_fit(r);
  const double v = eam_.rho_tab(r) + (rho_fit(r_lo_) - eam_.rho_tab(r_lo_));
  // RHOFORM NONNEG / MONOTONE: the offset continuation stays >= 0 as well
  if (rhoform_is_positive(cfg_.rho_form)) return v > 0.0 ? v : 0.0;
  return v;
}

tadah::core::Context EAMRefit::build_pot_context() const {
  tadah::core::Context c;
  std::string el = eam_.element();
  if (el.empty()) el = "X";
  const int bcode = static_cast<int>(family_);  // DM_REAM TYPEMB basis code

  c.add("MODEL", "M_BLR");
  c.add("MODEL", "BF_Linear");
  c.add("ATOM", el);
  // Pin the density weight explicitly: when WATOM is ABSENT the descriptors
  // default it to 1.0, but the HPO pipeline fills missing keys from blueprint
  // defaults (atomic number!) — which scaled rho by Z and produced ~500 eV/atom
  // garbage in the hpo evaluation of an otherwise-exact pot. Measured 2026-06-10.
  c.add("WATOM", 1.0);
  c.add("BIAS", "false");
  c.add("NORM", "false");
  c.add("FORCE", "false");
  c.add("STRESS", "false");
  c.add("LAMBDA", 0);
  c.add("OALGO", 2);

  // ---- two-body descriptor: must match the family the coefficients were
  //      FIT with (basisval), else the pot evaluates a different basis than
  //      was trained — D2_BP is the Gaussian twin of D2_Blip (same
  //      CGRID2B/SGRID2B keys); D2_Knot5 is the one-sided knot descriptor
  //      (knot positions only — NO width key, eta is a pure gauge; optional
  //      KDEG2B data list carries per-atom degrees 3|5).
  c.add("INIT2B", "true");
  const bool knot2b = (family_ == BasisKind::Knot5);
  c.add("TYPE2B", family_ == BasisKind::Gaussian ? "D2_BP"
                : knot2b                      ? "D2_Knot5"
                : family_ == BasisKind::Blip5    ? "D2_Blip5"
                                              : "D2_Blip");
  if (knot2b) {
    // contract: D2_Knot5 <nC> <nKdeg> El El
    c.add("TYPE2B", static_cast<int>(cgrid2b_.size()));
    c.add("TYPE2B", static_cast<int>(deg2b_.size()));
  } else {
    // contract: <name> <nC> <nS> El El
    c.add("TYPE2B", static_cast<int>(cgrid2b_.size()));
    c.add("TYPE2B", static_cast<int>(sgrid2b_.size()));
  }
  c.add("TYPE2B", el);
  c.add("TYPE2B", el);
  for (double v : cgrid2b_) c.add("CGRID2B", v);
  if (!knot2b)
    for (double v : sgrid2b_) c.add("SGRID2B", v);
  // optional per-atom degrees (KnotMix/Knot3; absent = all quintic)
  for (int d : deg2b_) c.add("KDEG2B", static_cast<double>(d));
  // Knot family: the basis vanishes C^4-smoothly at its largest knot, so the
  // TRUE effective cutoff IS max(knot) — emit that instead of the source
  // table's cutoff (prediction-exact; keeps the pot self-consistent for the
  // RCUT-derivation checks in edit/hpo/predict). Bump families keep the
  // table cutoff: their hard truncation at rcut is part of the fitted model.
  c.add("RCUT2B", knot2b && !cgrid2b_.empty()
                      ? *std::max_element(cgrid2b_.begin(), cgrid2b_.end())
                      : rcut_);
  c.add("RCTYPE2B", cfg_.rctype);

  // ---- many-body: DM_REAM ----
  // contract: DM_REAM <bcode> <nC> <nS> <nAmp> <nEc> <nEs> <nKd> <nKe> El El
  // (knot family: width sizes are 0 — eta is a pure gauge, no SGRID data)
  const bool knotmb = (family_ == BasisKind::Knot5);
  c.add("INITMB", "true");
  c.add("TYPEMB", "DM_REAM");
  c.add("TYPEMB", bcode);
  c.add("TYPEMB", static_cast<int>(crho_.size()));
  c.add("TYPEMB", knotmb ? 0 : static_cast<int>(srho_.size()));
  c.add("TYPEMB", static_cast<int>(arho_.size()));
  c.add("TYPEMB", static_cast<int>(cF_.size()));
  c.add("TYPEMB", knotmb ? 0 : static_cast<int>(sF_.size()));
  c.add("TYPEMB", static_cast<int>(degrho_.size()));
  c.add("TYPEMB", static_cast<int>(degF_.size()));
  c.add("TYPEMB", el);
  c.add("TYPEMB", el);
  for (double v : crho_) c.add("CGRIDMB", v);
  if (!knotmb)
    for (double v : srho_) c.add("SGRIDMB", v);
  for (double v : arho_) c.add("AMPGRIDMB", v);
  for (int d : degrho_) c.add("KDEGMB", static_cast<double>(d));
  for (double v : cF_) c.add("CEMBFUNC", v);
  if (!knotmb)
    for (double v : sF_) c.add("SEMBFUNC", v);
  for (int d : degF_) c.add("KDEGEMB", static_cast<double>(d));
  // See the RCUT2B comment: knot density channel derives its cutoff from
  // the largest density knot.
  c.add("RCUTMB", knotmb && !crho_.empty()
                      ? *std::max_element(crho_.begin(), crho_.end())
                      : rcut_);
  c.add("RCTYPEMB", cfg_.rctype);
  // The embedding reference the F weights were fitted in (every DM_REAM
  // potential carries it explicitly).
  c.add("EMBF0", std::string(embf0_name(cfg_.embf0_zero ? EmbF0Mode::Zero
                                                        : EmbF0Mode::Free)));
  // The density form the amplitudes were fitted in (mandatory on every
  // DM_REAM potential; canonical upper case).
  c.add("RHOFORM", std::string(rhoform_name(cfg_.rho_form)));

  // ---- linear weights [c2b | cmb] ----
  for (double v : weights_) c.add("WEIGHTS", v);

  // ---- HPO coefficient-search seed ----
  // FIXINDEX + FIXWEIGHT freeze every linear weight at its fitted value so HPO
  // can *search* the coefficients via `OPTIM FIXWEIGHT` (the standard linear
  // solve would otherwise overwrite them each iteration). `tadah predict`
  // ignores both keys — it reads WEIGHTS — so this does not change predictions.
  // `OPTIM FIXWEIGHT (i)` indices name MODEL WEIGHTS; because FIXINDEX is 1-K
  // here, weight index == FIXWEIGHT slot and the emitted target is valid
  // under both readings.
  const std::size_t K = weights_.size();
  if (K > 0) {
    c.add("FIXINDEX", std::string("1-") + std::to_string(K));
    for (double v : weights_) c.add("FIXWEIGHT", v);
  }

  if(cfg_.component || cfg_.unified_output) {
    EmbedSpec spec;
    if(cfg_.component) spec=*cfg_.component;
    else {
      spec.id="eam"; spec.pairs={el,el};
      spec.density.basis=embed_basis_from_name(basis_name(family_));
      spec.embedding.basis=spec.density.basis;
      spec.density.centres=crho_; spec.density.scales=srho_;
      spec.density.amplitudes=arho_; spec.density.degrees=degrho_;
      spec.embedding.centres=cF_; spec.embedding.scales=sF_; spec.embedding.degrees=degF_;
      if(family_==BasisKind::Knot5) {
        spec.density.scales.clear();spec.embedding.scales.clear();
        if(spec.density.degrees.empty()) spec.density.degrees.assign(crho_.size(),5);
        if(spec.embedding.degrees.empty()) spec.embedding.degrees.assign(cF_.size(),5);
      }
      spec.form=cfg_.rho_form; spec.reference=cfg_.embf0_zero?EmbF0Mode::Zero:EmbF0Mode::Free;
      spec.cutoff=cfg_.rctype=="Cut_Dummy"?"NONE":cfg_.rctype;
      spec.radius=c.get<double>("RCUTMB");
    }
    for(const char *key:{"TYPEMB","RCUTMB","RCTYPEMB","CGRIDMB","SGRIDMB","AMPGRIDMB","CEMBFUNC","SEMBFUNC","KDEGMB","KDEGEMB","RHOFORM","EMBF0"}) c.remove(key);
    c.append_block(embed_block(spec)); resolve_embed_blocks(c);
  }
  return c;
}

void EAMRefit::fit_components(double rwin_lo) {
  // The density's fit window, as the legacy fit's (fit()): the wall edge
  // rwin_lo may lie below r_lo (0.8 r_reach < r_lo), but nothing below r_lo
  // is fitted, so neither the samples nor the NONNEG/MONOTONE table check
  // start below it.
  const double win=std::max(rwin_lo,r_lo_);
  auto spec=*cfg_.component;
  const auto nr=static_cast<std::size_t>(std::max(2048,cfg_.oversample*static_cast<int>(spec.density.centres.size())));
  const auto nf=static_cast<std::size_t>(std::max(2048,cfg_.oversample*static_cast<int>(spec.embedding.centres.size())));
  auto solve=[&](std::size_t rows,std::size_t cols,auto column,auto target,bool positive,double lo,double hi,core::v_type &coef) {
    core::phi_type phi(rows+cols,cols);phi.set_zero();
    core::t_type y(rows+cols);y.set_zero();
    std::vector<double> a(rows*cols),b(rows);
    for(std::size_t i=0;i<rows;++i) {
      double q=lo+(hi-lo)*i/(rows-1); y(i)=b[i]=target(q);
      for(std::size_t j=0;j<cols;++j) phi(i,j)=a[j*rows+i]=column(j,q);
    }
    coef.assign(cols,0);
    if(positive) {
      auto result=nnls(a.data(),rows,cols,b.data(),coef.data(),nullptr,nullptr,cfg_.lambda);
      if(!result.converged) core::Logger::instance().log(core::LogLevel::WARNING,
        "DM_Embed density NNLS: ",nnls_stop_text(result.stop),"; fit statistics report the resulting residual");
    } else {
      core::Context context;context.add("OALGO",1);context.add("LAMBDA",cfg_.lambda);
      core::t_type w;LinearRegressor::train(context,phi,y,w);
      for(std::size_t j=0;j<cols;++j) coef[j]=w(j);
    }
    FitStats st;double sum=0,abs=0;
    for(std::size_t i=0;i<rows;++i) { double err=-b[i];
      for(std::size_t j=0;j<cols;++j) err+=coef[j]*a[j*rows+i];
      sum+=err*err;abs+=std::fabs(err);st.maxabs=std::max(st.maxabs,std::fabs(err));
    }
    st.n=rows;st.rmse=std::sqrt(sum/rows);st.mae=abs/rows;return st;
  };
  if(cfg_.fit_density_amplitudes) {
    check_rho_table(win);
    rho_stats_=solve(nr,spec.density.centres.size(),
      [&](std::size_t i,double q){return component_evaluator_->density(q,'a',i).value;},
      [&](double q){return eam_.rho_tab(q);},rhoform_is_positive(spec.form),win,spec.radius,spec.density.amplitudes);
  }
  cfg_.component=spec;
  component_evaluator_=std::make_unique<EmbedEvaluator>(spec);
  if(!cfg_.fit_density_amplitudes) {
    double square=0,absolute=0;
    for(std::size_t i=0;i<nr;++i) {
      double q=win+(spec.radius-win)*i/(nr-1);
      double error=component_evaluator_->density(q).value-eam_.rho_tab(q);
      square+=error*error;absolute+=std::abs(error);rho_stats_.maxabs=std::max(rho_stats_.maxabs,std::abs(error));
    }
    rho_stats_.n=nr;rho_stats_.rmse=std::sqrt(square/nr);rho_stats_.mae=absolute/nr;
  }
  F_stats_=solve(nf,spec.embedding.centres.size(),
    [&](std::size_t i,double q){return component_evaluator_->embedding(i,q).value;},
    [&](double q){return F_ref(q);},false,0,rhomax_,cmb_);
  crho_=spec.density.centres;arho_=spec.density.amplitudes;
  srho_.clear();for(std::size_t i=0;i<crho_.size();++i) srho_.push_back(embed_eta(spec.density,i));
  cF_=spec.embedding.centres;sF_.clear();for(std::size_t i=0;i<cF_.size();++i) sF_.push_back(embed_eta(spec.embedding,i));
  degrho_=spec.density.degrees;degF_=spec.embedding.degrees;
  fit_r_lo_=win;fit_rho_hi_=rhomax_;
  weights_=c2b_;weights_.insert(weights_.end(),cmb_.begin(),cmb_.end());
}

} // namespace models
} // namespace tadah
