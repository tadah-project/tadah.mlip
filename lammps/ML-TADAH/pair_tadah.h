/* -*- c++ -*- ----------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   http://lammps.sandia.gov, Sandia National Laboratories
   Steve Plimpton, sjplimp@sandia.gov

   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government rgrid_2bins
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level LAMMPS directory.
------------------------------------------------------------------------- */

/*----------------------------------------------------------------------*/
/* Contributing author: Marcin Kirsz | marcin.kirsz@ed.ac.uk            */
/* The University of Edinburgh       | https://github.com/tadah-project  */
/*----------------------------------------------------------------------*/

#ifdef PAIR_CLASS
// clang-format off
PairStyle(tadah,PairTadah)
// clang-format on
#else

#ifndef LMP_PAIR_TADAH_H
#define LMP_PAIR_TADAH_H

#include "pair.h"
#include <memory>
#include <string>
#include <vector>
#include <tadah/tadah.lammps/lammps_tadah.h>

namespace LAMMPS_NS {

  class PairTadah : public Pair {
    private:
      /** Stops the run (error->one) for two atoms at the same position,
       *  naming their atom IDs: every force would be nan. */
      [[noreturn]] void refuse_coincident(int i, int j);

      /** Linear kernel + two-body descriptor */
      void compute_2b(int eflag, int vflag);

      /** (Non)Linear +  2b+mb or mb  */
      void compute_2b_mb_half(int eflag, int vflag);
      void compute_2b_mb_full(int eflag, int vflag);

      /** EAM-collapsed fast path: linear model + radial 2b + EAM-form mb
       *  (e.g. the refit family D2_Knot5 + DM_REAM). Same physics as
       *  compute_2b_mb_half via phi/rho tables inside their certified
       *  ranges (analytic below) + analytic embedding; gated in coeff()
       *  (lt->eam_fast). */
      void compute_2b_mb_half_eamfast(int eflag, int vflag);
      /** Pair-collapsed fast path: linear model + radial 2b only
       *  (INITMB false). Same physics as compute_2b via the phi table
       *  inside its certified range (analytic below); no ghost comm at all.
       *  Gated in coeff() (lt->eam_fast, !initmb). */
      void compute_2b_fast(int eflag, int vflag);
      /** Resolve the eam_fast gate + build tables; called from coeff(). */
      void setup_eam_fastpath();

      /** dimer-dimer: */
      void compute_dimers(int eflag, int vflag);
      std::vector<std::pair<int,int>> midx;

      /* shared_ptr so a driver (e.g. tadah hpo) can build ONE LammpsTadah
       * per candidate potential and hand it to every PairTadah recreated by
       * the scripts' `clear` cycles — see load(shared_ptr). The object holds
       * per-simulation scratch (aeds/rhos/eam_*), so sharing is only valid
       * across sims that run sequentially (per-thread). The kernels size
       * that scratch by its own size, for the atoms LAMMPS holds: a
       * pair_coeff can give the pair style another object at any time. */
      std::shared_ptr<tadah::LammpsTadah> lt;

      void allocate();

      /* e.what() of the last failed load(): coeff() puts it in its
       * error->all message (a file that fails to load, and an EXTERN line
       * with no potential to adopt), so the reason (e.g. the DM_REAM RHOFORM
       * migration text) reaches log.lammps and the exception a library
       * driver catches instead of stderr only. Empty after a successful
       * load. Declared last: the offset of `lt` (read inline by drivers
       * through lammps_tadah()) is unchanged. */
      std::string last_load_error_;

      /* The notices of the potential's keys that have no effect (a
       * deprecated key such as SIGMA or MBLOCK, EMBF0/RHOFORM without a
       * DM_REAM block: core::report_ignored_key_once), captured while
       * load() builds the potential, in place of standard error: the
       * capture gets each key once, also one the process reported before,
       * so every load has them. coeff() writes them as LAMMPS warnings on
       * rank 0 (the screen and log.lammps of this LAMMPS instance; every
       * rank wrote them to standard error, once per process) and clears
       * them. Declared after last_load_error_ for the same reason. */
      std::vector<std::string> load_notices_;

      /* Writes load_notices_ as LAMMPS warnings on rank 0, naming the
       * potential @p pot, and clears them on every rank. */
      void emit_load_notices(const std::string &pot);

      /* Where lt came from. `pair_coeff * * <file>` always loads its file
       * (File); `pair_coeff * * EXTERN` adopts the potential staged for
       * this thread (Staged) or one a driver gave the pair style through a
       * load() overload (DriverLoad), and refuses any other: one an earlier
       * pair_coeff loaded from a file, or one an earlier EXTERN adopted from
       * the staged slot, which the driver has cleared since. Declared after
       * load_notices_ for the same reason as last_load_error_. */
      enum class PotentialSource { None, File, DriverLoad, Staged };
      PotentialSource lt_source_ = PotentialSource::None;
      std::string lt_file_;   // the file of a File potential

      /* Leaves the pair style without a potential (lt, lt_source_,
       * lt_file_): done by a pair_coeff that fails, so that a library
       * driver that catches its error cannot run with the potential the
       * line did not give (init_style() refuses a pair style without one). */
      void release_potential();

      double single(int /*i*/, int /*j*/, int itype, int jtype, double rsq,
                double /*factor_coul*/, double /*factor_lj*/, double &fforce);

    public:
      void say_hello() const {
        std::cout << "Hello from PairTadah!" << std::endl;
      }

      /* Each returns false on failure and keeps the reason in
       * last_load_error(); none prints it: coeff() reports it through
       * error->all (the CLI overload directly, the Context overloads at
       * pair_coeff * * EXTERN). A failed load leaves the pair style without
       * a potential. A potential a driver gives here is adopted by
       * `pair_coeff * * EXTERN` (when nothing is staged), by every EXTERN
       * line until a pair_coeff loads a file or adopts a staged potential;
       * `pair_coeff * * <file>` replaces it. */
      bool load(int argc, char **argv);                          // CLI
      bool load(const tadah::core::Context& ctx);   // copy from l-value
      bool load(tadah::core::Context&& ctx);        // move from r-value
      bool load(std::shared_ptr<tadah::LammpsTadah> shared_lt); // adopt shared
      const std::string& last_load_error() const { return last_load_error_; }

      /* EXTERN staging. A driver that executes a whole script through
       * LAMMPS's input->file() cannot interleave load() calls with the
       * script's own pair_coeff lines, so it stages the shared potential
       * here (per thread) before the run; every `pair_coeff * * EXTERN ...`
       * executed on that thread then adopts it in coeff(). The slot lives
       * outside the Pair object, so it survives `clear` (which destroys
       * the Pair but not the staged potential); the driver clears it once
       * the run finishes. Per-thread because drivers run one LAMMPS
       * instance per worker thread/process with a strict
       * stage -> run -> clear sequence. Once the slot is cleared, an EXTERN
       * line no longer adopts the potential an earlier one took from it
       * (an error, unless the driver stages one again or gives one with
       * load()); a pair_coeff with a file replaces it in the pair style and
       * the slot keeps it alive for the driver. */
      static void stage_extern(std::shared_ptr<tadah::LammpsTadah> pot);
      static void clear_staged_extern();

      /* IFT-adjoint host accessor.
       *
       * HPO host needs to (a) toggle lt->capture_adjoint_state around the
       * post-relax pair->compute call, and (b) read lt->adjoint_snapshot and
       * lt->aeds to assemble dO_k/dW. Exposed as a single accessor instead
       * of one method per field; production MD never touches it. */
      tadah::LammpsTadah*       lammps_tadah()       { return lt.get(); }
      const tadah::LammpsTadah* lammps_tadah() const { return lt.get(); }

      PairTadah(class LAMMPS *);
      ~PairTadah() override;

      // REQUIRED
      void compute(int, int) override;
      void settings(int, char **) override;
      void coeff(int, char **) override;

      // OPTIONAL
      void init_style() override;
      double init_one(int i, int j) override;
      int pack_forward_comm(int, int *, double *, int, int *) override;
      void unpack_forward_comm(int, int, double *) override;
      int pack_reverse_comm(int, int, double *) override;
      void unpack_reverse_comm(int, int *, double *) override;
  };
}
#endif
#endif
