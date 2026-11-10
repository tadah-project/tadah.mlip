#pragma once
#ifndef LAMMPS_TADAH_H
#define LAMMPS_TADAH_H

#include <cstddef>
#include <memory>
#include <vector>
#include <tadah/core/context.h>
#include <tadah/models/dc_selector.h>
#include <tadah/models/functions/function_base.h>
#include <tadah/md/m_md_base.h>
#include <tadah/tadah.lammps/eam_collapse.h>

namespace tadah {

/* IFT-adjoint state for the HPO gradients of LAMMPS observables.
 *
 * When LammpsTadah::capture_adjoint_state is true, the pair_tadah compute_*
 * kernels populate this snapshot during the regular force evaluation. The
 * snapshot holds the analytical mixed partials of -d^2E/dx dW at the
 * current atomic configuration:
 *   - dF_dW[(3*atom + axis) * n_desc + c] = d(F_{atom,axis})/dW_c
 *     (atom block; -d^2 E / d x dW per BF_Linear).
 *   - dvirial_dW[alpha * n_desc + c] = -d^2 E / d eps_alpha dW_c
 *     (box block; analogous "force-side" sign so the chain kernel uses
 *     a single + sign across atom and box rows). Iso strain
 *     (`box_dofs == 1`) or aniso Voigt-6 (`box_dofs == 6`).
 *
 * `box_dofs` is read by the compute_* reset/accumulate paths; the host
 * sets it before toggling `capture_adjoint_state=true`. reset() preserves
 * its value so successive captures honour the same configuration.
 *
 * Built only at the relaxed state x*; the HPO host consumes it to
 * assemble dO_k/dW per LAMMPS observable.
 *
 * Production MD does NOT touch this struct: capture_adjoint_state defaults
 * to false and gates a single predicted-false branch per compute_* call. */
struct AdjointSnapshot {
    std::vector<double> dF_dW;          // size = 3 * n_atoms * n_desc
    std::vector<double> dvirial_dW;     // size = box_dofs * n_desc (may be 0)
    std::size_t         n_atoms  = 0;
    std::size_t         n_desc   = 0;
    int                 box_dofs = 0;   // 0 (none), 1 (iso), 6 (Voigt)
    bool                valid    = false;

    void reset(std::size_t n_atoms_, std::size_t n_desc_) {
        n_atoms = n_atoms_;
        n_desc  = n_desc_;
        dF_dW.assign(3 * n_atoms * n_desc, 0.0);
        dvirial_dW.assign(
            static_cast<std::size_t>(box_dofs > 0 ? box_dofs : 0) * n_desc, 0.0);
        valid = false;
    }

    inline double* row_ptr(std::size_t atom, std::size_t axis) {
        return dF_dW.data() + (3 * atom + axis) * n_desc;
    }
    inline const double* row_ptr(std::size_t atom, std::size_t axis) const {
        return dF_dW.data() + (3 * atom + axis) * n_desc;
    }

    inline double* box_row_ptr(std::size_t alpha) {
        return dvirial_dW.data() + alpha * n_desc;
    }
    inline const double* box_row_ptr(std::size_t alpha) const {
        return dvirial_dW.data() + alpha * n_desc;
    }
};

class LammpsTadah
{
public:
    explicit LammpsTadah(const  core::Context& ctx);   // copy
    explicit LammpsTadah(      core::Context&& ctx);   // move
    LammpsTadah(int argc, char** argv);                // from CLI

    void initialize(int argc, char** argv);

    /* … pack/unpack functions unchanged … */

    ~LammpsTadah() = default;                          // unique_ptr ⇒ OK
    LammpsTadah(const LammpsTadah&)            = delete;
    LammpsTadah& operator=(const LammpsTadah&) = delete;

  int pack_reverse_linear(int n, int first, double *buf);
  void unpack_reverse_linear(int n, int *list, double *buf);
  // Pack the derivative of the embedding func
  int pack_forward_linear(int n, int *list, double *buf);
  // Unpack the derivative of the embedding func
  void unpack_forward_linear(int n, int first, double *buf);
  // NONLINEAR
  int pack_reverse_nonlinear(int n, int first, double *buf);
  void unpack_reverse_nonlinear(int n, int *list, double *buf);
  int pack_forward_nonlinear(int n, int *list, double *buf);
  void unpack_forward_nonlinear(int n, int first, double *buf);

public:
    /* ------------ plain data ------------ */
    core::Context  c;

    /* The potential context as given stores EMBF0 (DM_REAM embedding
     * reference). Taken in the constructors' init lists BEFORE `S` is built:
     * the DM_REAM constructor materialises the default ZERO into `c`, after
     * which c.has_stored("EMBF0") is always true. Must stay declared after
     * `c` and before `S`. pair_coeff warns when a DM_REAM potential lacks it. */
    bool embf0_declared{false};

    bool   norm   {};
    size_t bias   {0};
    size_t rsize  {0};

    double cutoff_max_sq {};
    double cutoff_max    {};

    bool   linear {};
    bool   dimer  {};
    double dimer_r{};
    bool   dimer_bond_bool{};

    bool init2b{};
    bool initmb{};

    std::vector<int> Z;

    // Per-LAMMPS-atom-type lookup populated in initialize() once weights are
    // loaded. With per-species BIAS, every atom of a given type contributes a
    // fixed `bias_per_atom_type[type-1]` constant to its energy; the column
    // index for that species is `species_intercept_idx_per_type[type-1]`.
    std::vector<double> bias_per_atom_type;
    std::vector<int>    species_intercept_idx_per_type;

    /* ------------ large sub-objects -------- */
    models::DC_Selector                         S;
    std::unique_ptr<models::Function_Base>      fb;
    std::unique_ptr<md::M_MD_Base>              model;

    core::aeds_type2 aeds;
    core::rhos_type  rhos;

    /* EAM-collapse fast path (linear model + radial D2 + EAM-form DM).
     * Resolved in PairTadah::coeff: when eam_fast is true the pair style
     * runs compute_2b_mb_half_eamfast() with the tables below and scalar
     * (1-double) forward/reverse comm instead of descriptor-vector comm.
     * eam_rho_bar/eam_fp are per-atom scratch, resized in compute;
     * eam_scratch holds the descriptor buffers for the analytic pair term
     * below a table's certified range, sized in setup_eam_fastpath. Like
     * aeds/rhos they make this object shareable across sims only when the
     * sims run sequentially (per-thread sharing). */
    EamCollapseTables   eam_tables;
    bool                eam_fast{false};
    std::vector<double> eam_rho_bar;
    std::vector<double> eam_fp;
    CollapseScratch     eam_scratch;

    /* IFT-adjoint snapshot. See AdjointSnapshot doc above.
     * Default off: production MD pays one predicted-false branch per
     * compute_* call. Toggled true by the HPO host after minimize()
     * reaches the relaxed state x*, then one more pair->compute(0,0)
     * populates the snapshot. */
    bool            capture_adjoint_state{false};
    AdjointSnapshot adjoint_snapshot;
};
} // namespace tadah
#endif
