/* ----------------------------------------------------------------------
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

#include "pair_tadah.h"

#include "atom.h"
#include "comm.h"
#include "domain.h"
#include "error.h"
#include "fix.h"
#include "force.h"
#include "memory.h"
#include "molecule.h"
#include "neighbor.h"
#include "neigh_list.h"
#include "neigh_request.h"

#include <tadah/core/blueprint_parser.h>
#include <tadah/core/key_notice.h>
#include <tadah/models/descriptors/dm/rhoform.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

using namespace LAMMPS_NS;

/* Per-thread staged EXTERN potential -- see stage_extern() in the header.
 * Function-local static so initialization order is a non-issue. */
static std::shared_ptr<tadah::LammpsTadah>& staged_extern_slot()
{
  static thread_local std::shared_ptr<tadah::LammpsTadah> slot;
  return slot;
}

void PairTadah::stage_extern(std::shared_ptr<tadah::LammpsTadah> pot)
{
  staged_extern_slot() = std::move(pot);
}

void PairTadah::clear_staged_extern()
{
  staged_extern_slot().reset();
}

/* Sizes the per-atom descriptor and density arrays of the potential @p lt
 * for the @p nmax atoms LAMMPS holds (local and ghost, atom->nmax). They
 * belong to the potential object, which a pair_coeff can replace (a file,
 * an EXTERN with another staged object) and a driver can share between
 * LAMMPS instances, so their own size decides: a count kept by the pair
 * style left the arrays of a new object empty, and the kernel wrote
 * through them. */
static void size_per_atom_arrays(tadah::LammpsTadah &lt, int nmax)
{
  const std::size_t n = static_cast<std::size_t>(nmax);
  if (lt.rhos.rows() != lt.S.density_size() || lt.rhos.cols() != n)
    lt.rhos.resize(lt.S.density_size(), n);
  if (lt.aeds.rows() != lt.S.descriptor_size() || lt.aeds.cols() != n)
    lt.aeds.resize(lt.S.descriptor_size(), n);
}

PairTadah::PairTadah(LAMMPS *lmp) : Pair(lmp)
{
  tadah::core::PeriodicTable::initialize();
  single_enable = 1; // 1 if single() method is implemented, 0 if missing
  restartinfo = 0; // 1 if pair style writes its settings to a restart
  one_coeff = 1; // 1 if only a pair_coeff * * command is allowed

  map = nullptr;
}

PairTadah::~PairTadah()
{
  if (allocated) {
    memory->destroy(setflag);
    memory->destroy(cutsq);
    //  delete[] map; // deleted elsewhere
  }
}
void PairTadah::compute_2b(int eflag, int vflag)
{
  // Works only for linear regression with two-body descriptor.
  // Assumes weights are "normalised" so we
  // do not need to normalise descriptors.
  // Works for both newton on or off.
  int i,j,ii,jj,jnum;
  double xtmp,ytmp,ztmp,fpair,ei;
  double rij_sq;
  int *ilist,*jlist,*numneigh,**firstneigh;

  ev_init(eflag,vflag);

  double **x = atom->x;
  double **f = atom->f;

  int *type = atom->type;
  int nlocal = atom->nlocal;
  tadah::core::Vec3d delij;

  tadah::core::aed_type aed(lt->S.descriptor_size());
  tadah::core::fd_type fd(lt->S.descriptor_size());

  int inum = list->inum;
  ilist = list->ilist;
  numneigh = list->numneigh;
  firstneigh = list->firstneigh;
  int newton_pair = force->newton_pair;

  // IFT-adjoint snapshot setup. Default false ⇒ zero MD cost.
  const std::size_t n_desc = lt->S.descriptor_size();
  if (lt->capture_adjoint_state) {
    if (lt->aeds.cols() < static_cast<std::size_t>(nlocal)
        || lt->aeds.rows() < n_desc) {
      lt->aeds.resize(n_desc, nlocal);
    }
    lt->aeds.set_zero();
    if (lt->bias > 0) {
      for (int n = 0; n < nlocal; ++n) {
        const int k_b = lt->species_intercept_idx_per_type[type[n]-1];
        if (k_b >= 0) lt->aeds(k_b, n) = 1.0;
      }
    }
    lt->adjoint_snapshot.reset(static_cast<std::size_t>(nlocal), n_desc);
  }

  for (ii = 0; ii < inum; ii++) {
    i = ilist[ii];
    xtmp = x[i][0];
    ytmp = x[i][1];
    ztmp = x[i][2];
    jlist = firstneigh[i];
    jnum = numneigh[i];

    int Zi = lt->Z[type[i]-1];
    for (jj = 0; jj < jnum; jj++) {
      j = jlist[jj];
      j &= NEIGHMASK;
      delij[0] = xtmp - x[j][0];
      delij[1] = ytmp - x[j][1];
      delij[2] = ztmp - x[j][2];
      rij_sq = delij[0]*delij[0] + delij[1]*delij[1] + delij[2]*delij[2];
      if (rij_sq == 0.0) refuse_coincident(i, j);

      if (rij_sq > lt->cutoff_max_sq) continue;

      aed.set_zero();
      fd.set_zero();  // Needed b/c _mJoin may skip parts of the vector

      int Zj = lt->Z[type[j]-1];
      double rij = sqrt(rij_sq);
      lt->S.d2b->calc_all(Zi,Zj,rij,rij_sq,aed,fd);

      fpair = lt->model->fpredict(fd,aed,0)/rij;
      ei = lt->model->epredict(aed);

      f[i][0] += fpair*delij[0];
      f[i][1] += fpair*delij[1];
      f[i][2] += fpair*delij[2];

      if (newton_pair || j < nlocal) {
        f[j][0] -= fpair*delij[0];
        f[j][1] -= fpair*delij[1];
        f[j][2] -= fpair*delij[2];
      }

      if (evflag) ev_tally(i,j,nlocal,newton_pair,ei,
                           0.0,fpair,delij[0],delij[1],delij[2]);

      if (lt->capture_adjoint_state) {
        // Accumulate per-pair AED contribution into per-atom column.
        for (std::size_t c = 0; c < n_desc; ++c) {
          lt->aeds(c, i) += aed(c);
        }
        // F_i[k] = fpair*delij[k] with fpair = -(fd(:,0)*W)/rij ⇒
        // dF_i[k]/dW[c] = -fd(c,0)*delij[k]/rij. Mirrored for atom j with
        // opposite sign when j is local (matches the f[j] -= update guard).
        const double inv_rij = 1.0 / rij;
        for (std::size_t k_ax = 0; k_ax < 3; ++k_ax) {
          const double scale_i = -delij[k_ax] * inv_rij;
          double* row_i = lt->adjoint_snapshot.row_ptr(
              static_cast<std::size_t>(i), k_ax);
          for (std::size_t c = 0; c < n_desc; ++c) {
            row_i[c] += scale_i * fd(c, 0);
          }
          if (j < nlocal) {
            const double scale_j = -scale_i;
            double* row_j = lt->adjoint_snapshot.row_ptr(
                static_cast<std::size_t>(j), k_ax);
            for (std::size_t c = 0; c < n_desc; ++c) {
              row_j[c] += scale_j * fd(c, 0);
            }
          }
        }
        // Per-pair box-strain contribution to dvirial_dW.
        // For 2b, E_pair depends only on rij. Under deformation F = I+ε:
        //   ∂rij/∂ε_α = (1/rij) · Σ_β Σ_γ delij[β] · (e_α)_βγ · delij[γ]
        //   ∂²E_pair/∂ε_α∂W_c = fd(c,0) · ∂rij/∂ε_α
        //   dvirial_dW[α,c] += -fd(c,0) · ∂rij/∂ε_α  per directed pair.
        //
        // box_dofs=1 (iso): F = (1+ε)·I ⇒ ∂rij/∂ε|_iso = rij. The
        // single-row accumulator of the iso-only code, preserved exactly.
        //
        // box_dofs=6 (Voigt): voigt_basis matches LammpsHessianAssembler
        // (e[α][α]=1 on diagonals; off-diagonals e[i][j]=e[j][i]=0.5
        // engineering-shear convention).
        if (lt->adjoint_snapshot.box_dofs == 1) {
          double* box_row = lt->adjoint_snapshot.box_row_ptr(0);
          for (std::size_t c = 0; c < n_desc; ++c) {
            box_row[c] += -fd(c, 0) * rij;
          }
        } else if (lt->adjoint_snapshot.box_dofs == 6) {
          const double inv_rij = 1.0 / rij;
          // ∂rij/∂ε_α for each Voigt mode (Voigt order xx,yy,zz,yz,xz,xy):
          const double dr_de[6] = {
            delij[0] * delij[0] * inv_rij,    // xx
            delij[1] * delij[1] * inv_rij,    // yy
            delij[2] * delij[2] * inv_rij,    // zz
            delij[1] * delij[2] * inv_rij,    // yz (½·(d1·d2 + d2·d1) = d1·d2)
            delij[0] * delij[2] * inv_rij,    // xz
            delij[0] * delij[1] * inv_rij,    // xy
          };
          for (int alpha = 0; alpha < 6; ++alpha) {
            const double pf = dr_de[alpha];
            double* box_row = lt->adjoint_snapshot.box_row_ptr(
                static_cast<std::size_t>(alpha));
            for (std::size_t c = 0; c < n_desc; ++c) {
              box_row[c] += -fd(c, 0) * pf;
            }
          }
        }
      }
    }

    // Per-species intercept: precomputed in LammpsTadah::initialize().
    // bias_per_atom_type[type-1] is 0 when bias=0, NULL slots, or unmapped Z.
    if (lt->bias > 0) {
      const double ei_bias = lt->bias_per_atom_type[type[i]-1];
      if (eflag_atom)   eatom[i]  += ei_bias;
      if (eflag_global) eng_vdwl  += ei_bias;
    }
  }

  if (lt->capture_adjoint_state) {
    lt->adjoint_snapshot.valid = true;
  }

  if (vflag_fdotr)  virial_fdotr_compute();
}
void PairTadah::compute_2b_mb_half(int eflag, int vflag)
{
  // Works for both D2 and DM descriptor with Linear models
  int i,j,ii,jj,jnum;
  double xtmp,ytmp,ztmp;
  double rij_sq;
  int *ilist,*jlist,*numneigh,**firstneigh;

  ev_init(eflag,vflag);

  double **x = atom->x;
  double **f = atom->f;

  int *type = atom->type;
  int nlocal = atom->nlocal;
  tadah::core::Vec3d delij;

  int inum = list->inum;
  ilist = list->ilist;
  numneigh = list->numneigh;
  firstneigh = list->firstneigh;
  int newton_pair = force->newton_pair;

  tadah::core::fd_type fd(lt->S.descriptor_size());

  size_per_atom_arrays(*lt, atom->nmax);

  lt->rhos.set_zero();
  lt->aeds.set_zero();

  // The bias component of the atoms LAMMPS has, local and ghost: aeds has
  // atom->nmax columns, and atom->type is not set past nlocal + nghost.
  if (lt->bias > 0) {
    const int nall = nlocal + atom->nghost;
    for (int n=0; n<nall; ++n) {
      const int k = lt->species_intercept_idx_per_type[type[n]-1];
      if (k >= 0) lt->aeds(k, n) = 1.0;
    }
  }

  // IFT-adjoint snapshot setup. Default false ⇒ zero MD cost.
  if (lt->capture_adjoint_state) {
    lt->adjoint_snapshot.reset(static_cast<std::size_t>(nlocal),
                                lt->S.descriptor_size());
  }

  for (ii = 0; ii < inum; ii++) {
    i = ilist[ii];
    xtmp = x[i][0];
    ytmp = x[i][1];
    ztmp = x[i][2];
    jlist = firstneigh[i];
    jnum = numneigh[i];

    int Zi = lt->Z[type[i]-1];
    for (jj = 0; jj < jnum; jj++) {
      j = jlist[jj];
      j &= NEIGHMASK;

      delij[0] = xtmp - x[j][0];
      delij[1] = ytmp - x[j][1];
      delij[2] = ztmp - x[j][2];
      rij_sq = delij[0]*delij[0] + delij[1]*delij[1] + delij[2]*delij[2];
      if (rij_sq == 0.0) refuse_coincident(i, j);

      if (rij_sq > lt->cutoff_max_sq) continue;
      double rij = sqrt(rij_sq);
      int Zj = lt->Z[type[j]-1];

      if (lt->S.d2b->get_rcut() > rij) {
        lt->S.d2b->calc_aed(Zi,Zj,rij,rij_sq,lt->aeds.col(i),0.5);

        if (newton_pair || j < nlocal) {
          lt->S.d2b->calc_aed(Zj,Zi,rij,rij_sq,lt->aeds.col(j),0.5);
        }
      }

      if (lt->S.dmb->get_rcut() > rij) {
        lt->S.dmb->calc_rho(Zi,Zj,rij,rij_sq,delij,lt->rhos.col(i));

        if (newton_pair || j < nlocal) {
          lt->S.dmb->calc_rho(Zj,Zi,rij,rij_sq,-delij,lt->rhos.col(j));
        }
      }
    }
  }

  // Communicate and sum densities for EAM.
  // Also communicate 2b aeds if newton is on.
  if (comm_reverse && newton_pair) {
    comm->reverse_comm(this);
    //comm->reverse_comm_pair(this);
  }

  // compute AEFP for all eam-types
  // for EAM the derivative of embedding energy at each atom
  // is also computed here
  for (ii = 0; ii < inum; ii++) {
    i = ilist[ii];
    // we have to scale 2b energy by 0.5
    // we can't do it in the loop above because
    // if dummy cutoff is used or d2_eam with dm_eam
    // cutoff is ignored and that is the only way now to
    // introduce factor into 2b aed.
    // Hence we scale 2b part of descriptor by 0.5
    // before MB aed is calculated
    //lt->aeds.col(i)*=0.5; // TODO what about bias?
    // TODO above can be now fixed since addition of the new scalling factor

    lt->S.dmb->calc_aed(lt->rhos.col(i), lt->aeds.col(i));

    if (lt->norm)
      lt->model->norm.normalise(lt->aeds.col(i));

    double ei = lt->model->epredict(lt->aeds.col(i));
    if (eflag_atom) eatom[i] = ei;
    if (eflag_global) eng_vdwl += ei;
  }

  // communicate the derivative of the embedding function
  if (comm_forward)
    comm->forward_comm(this);

  for (ii = 0; ii < inum; ii++) {
    i = ilist[ii];

    xtmp = x[i][0];
    ytmp = x[i][1];
    ztmp = x[i][2];

    jlist = firstneigh[i];
    jnum = numneigh[i];
    int Zi = lt->Z[type[i]-1];
    for (jj = 0; jj < jnum; jj++) {
      j = jlist[jj];
      j &= NEIGHMASK;
      delij[0] = xtmp - x[j][0];
      delij[1] = ytmp - x[j][1];
      delij[2] = ztmp - x[j][2];
      rij_sq = delij[0]*delij[0] + delij[1]*delij[1] + delij[2]*delij[2];
      if (rij_sq > lt->cutoff_max_sq) continue;
      fd.set_zero();  // Needed b/c _mJoin may skip parts of the vector
      double rij = sqrt(rij_sq);
      int Zj = lt->Z[type[j]-1];

      if (lt->S.d2b->get_rcut() > rij) {
        lt->S.d2b->calc_dXijdri(Zi,Zj,rij,rij_sq,fd);
      }

      if (lt->S.dmb->get_rcut() > rij) {
        lt->S.dmb->calc_dXijdri_dXjidri(Zi,Zj,rij,rij_sq,delij,
                                        lt->rhos.col(i),lt->rhos.col(j),fd);
      }

      // copy 2b descriptors first and
      // scale all 2b components by delij/rij
      // this is inefficient but will do for now... TODO
      for(size_t s=lt->bias; s<lt->S.d2b->size()+lt->bias; ++s) {
        fd(s,0) /= rij;
        fd(s,1) = fd(s,0);
        fd(s,2) = fd(s,0);
        fd(s,0) *= delij[0];
        fd(s,1) *= delij[1];
        fd(s,2) *= delij[2];
      }
      if (lt->norm)
        lt->model->norm.normalise(fd);

      tadah::core::Vec3d fij = lt->model->fpredict(fd,lt->aeds.col(i));

      f[i][0] += fij(0);
      f[i][1] += fij(1);
      f[i][2] += fij(2);

      if (newton_pair || j < nlocal) {
        f[j][0] -= fij(0);
        f[j][1] -= fij(1);
        f[j][2] -= fij(2);
      }

      if (evflag) ev_tally_xyz(i,j,nlocal,newton_pair,
                               0.0,0.0,fij(0),fij(1),fij(2),delij[0],delij[1],delij[2]);

      if (lt->capture_adjoint_state) {
        // fd is the already-rotated descriptor (lines 292-299 above): for the
        // 2b columns fd(s, k) = (fd_unrotated(s, 0) / rij) * delij[k]; the MB
        // columns store the analytical dD/dx_k from calc_dXijdri_dXjidri.
        // fij = -(fd * W) ⇒ dF_i[k]/dW[c] = -fd(c, k). Atom j picks up the
        // opposite sign, mirroring the f[j] -= fij update.
        const std::size_t n_desc_l = lt->S.descriptor_size();
        for (std::size_t k_ax = 0; k_ax < 3; ++k_ax) {
          double* row_i = lt->adjoint_snapshot.row_ptr(
              static_cast<std::size_t>(i), k_ax);
          for (std::size_t c = 0; c < n_desc_l; ++c) {
            row_i[c] += -fd(c, k_ax);
          }
          if (j < nlocal) {
            double* row_j = lt->adjoint_snapshot.row_ptr(
                static_cast<std::size_t>(j), k_ax);
            for (std::size_t c = 0; c < n_desc_l; ++c) {
              row_j[c] += +fd(c, k_ax);
            }
          }
        }
        // Per-pair box-strain contribution to dvirial_dW.
        // For mb under deformation F = I + ε on positions x → F·x:
        //   Δdelij[k] = Σ_β (e_α)_kβ · delij[β]
        //   ∂E_pair/∂ε_α = -F_i_pair · Δdelij = (fd·W) · Δdelij
        //   ∂²E_pair/∂ε_α∂W_c = Σ_k fd(c, k) · Σ_β (e_α)_kβ · delij[β].
        // dvirial_dW = -∂²E/∂ε∂W (force-side sign).
        //
        // box_dofs=1 (iso): F = (1+ε)·I ⇒ Δdelij = delij ⇒ accumulator
        // collapses to -Σ_k fd(c,k)·delij[k]. Preserved exactly.
        //
        // box_dofs=6 (Voigt): voigt_basis matches LammpsHessianAssembler.
        // Pre-compute vec_e[α][k] = Σ_β (e_α)_kβ · delij[β] once per
        // pair, then loop α × c × k.
        if (lt->adjoint_snapshot.box_dofs == 1) {
          double* box_row = lt->adjoint_snapshot.box_row_ptr(0);
          for (std::size_t c = 0; c < n_desc_l; ++c) {
            double dot = 0.0;
            for (std::size_t k = 0; k < 3; ++k) dot += fd(c, k) * delij[k];
            box_row[c] += -dot;
          }
        } else if (lt->adjoint_snapshot.box_dofs == 6) {
          // vec_e[α][k] (Voigt order xx,yy,zz,yz,xz,xy with engineering shear ½):
          const double vec_e[6][3] = {
            { delij[0],          0.0,               0.0              },  // xx
            { 0.0,               delij[1],          0.0              },  // yy
            { 0.0,               0.0,               delij[2]         },  // zz
            { 0.0,               0.5 * delij[2],    0.5 * delij[1]   },  // yz
            { 0.5 * delij[2],    0.0,               0.5 * delij[0]   },  // xz
            { 0.5 * delij[1],    0.5 * delij[0],    0.0              },  // xy
          };
          for (int alpha = 0; alpha < 6; ++alpha) {
            double* box_row = lt->adjoint_snapshot.box_row_ptr(
                static_cast<std::size_t>(alpha));
            for (std::size_t c = 0; c < n_desc_l; ++c) {
              double sum = 0.0;
              for (std::size_t k = 0; k < 3; ++k)
                sum += fd(c, k) * vec_e[alpha][k];
              box_row[c] += -sum;
            }
          }
        }
      }
    }
  }

  if (lt->capture_adjoint_state) {
    lt->adjoint_snapshot.valid = true;
  }

  if (vflag_fdotr)  virial_fdotr_compute();

}

/* ---------------------------------------------------------------------- */
/* EAM-collapsed fast path.
 *
 * For a linear model E_i = w . aed_i, the radial 2b block collapses to a
 * scalar pair function phi(r) and an EAM-form mb block to F(rho_bar_i),
 * rho_bar_i = sum_j rho(r_ij). phi/rho are cubic-Hermite tables built in
 * setup_eam_fastpath() by sampling the descriptors' own calc routines and
 * used inside their certified ranges; below a table's range the pair term
 * is evaluated analytically (tadah::collapse_phi / collapse_rho). F, F'
 * are evaluated analytically per atom (DM_Base::eam_embed, exactly as the
 * descriptor defines them, including the DM_REAM EMBF0 reference) so no
 * embedding table range needs guessing. Structure mirrors pair_eam: pass 1
 * accumulates rho_bar (reverse comm, 1 double), embedding per local atom
 * (forward comm of F', 1 double), pass 2 central pair forces.            */
void PairTadah::compute_2b_mb_half_eamfast(int eflag, int vflag)
{
  int i,j,ii,jj,jnum;
  double xtmp,ytmp,ztmp,delx,dely,delz;
  double rij_sq;
  int *ilist,*jlist,*numneigh,**firstneigh;

  ev_init(eflag,vflag);

  double **x = atom->x;
  double **f = atom->f;
  int nlocal = atom->nlocal;
  int newton_pair = force->newton_pair;

  int inum = list->inum;
  ilist = list->ilist;
  numneigh = list->numneigh;
  firstneigh = list->firstneigh;

  const tadah::EamCollapseTables &T = lt->eam_tables;
  tadah::CollapseScratch &scratch = lt->eam_scratch;
  const double rc_phi = T.phi.rmax;
  const double rc_rho = T.rho.rmax;

  const int ntotal = nlocal + atom->nghost;
  if ((int)lt->eam_rho_bar.size() < atom->nmax) {
    lt->eam_rho_bar.resize(atom->nmax);
    lt->eam_fp.resize(atom->nmax);
  }
  std::fill_n(lt->eam_rho_bar.begin(), ntotal, 0.0);

  // pass 1: accumulate rho_bar
  for (ii = 0; ii < inum; ii++) {
    i = ilist[ii];
    xtmp = x[i][0]; ytmp = x[i][1]; ztmp = x[i][2];
    jlist = firstneigh[i];
    jnum = numneigh[i];
    for (jj = 0; jj < jnum; jj++) {
      j = jlist[jj];
      j &= NEIGHMASK;
      delx = xtmp - x[j][0];
      dely = ytmp - x[j][1];
      delz = ztmp - x[j][2];
      rij_sq = delx*delx + dely*dely + delz*delz;
      if (rij_sq == 0.0) refuse_coincident(i, j);
      if (rij_sq >= rc_rho*rc_rho) continue;
      const double rij = sqrt(rij_sq);
      double rho_r, rhop_unused;
      tadah::collapse_rho(T, lt->S, rij, rho_r, rhop_unused);
      lt->eam_rho_bar[i] += rho_r;
      if (newton_pair || j < nlocal) lt->eam_rho_bar[j] += rho_r;
    }
  }

  if (newton_pair) comm->reverse_comm(this);

  // embedding: exact analytic F, F' per local atom
  for (ii = 0; ii < inum; ii++) {
    i = ilist[ii];
    double F, Fp;
    lt->S.dmb->eam_embed(T.w, lt->eam_rho_bar[i], F, Fp);
    lt->eam_fp[i] = Fp;
    if (eflag_global) eng_vdwl += F;
    if (eflag_atom) eatom[i] += F;
  }

  comm->forward_comm(this);

  // pass 2: central pair forces (table segments inside the certified
  // ranges, analytic below them)
  for (ii = 0; ii < inum; ii++) {
    i = ilist[ii];
    xtmp = x[i][0]; ytmp = x[i][1]; ztmp = x[i][2];
    jlist = firstneigh[i];
    jnum = numneigh[i];
    for (jj = 0; jj < jnum; jj++) {
      j = jlist[jj];
      j &= NEIGHMASK;
      delx = xtmp - x[j][0];
      dely = ytmp - x[j][1];
      delz = ztmp - x[j][2];
      rij_sq = delx*delx + dely*dely + delz*delz;
      if (rij_sq >= lt->cutoff_max_sq) continue;
      const double rij = sqrt(rij_sq);

      double phi = 0.0, phip = 0.0;
      if (rij < rc_phi) tadah::collapse_phi(T, lt->S, rij, scratch, phi, phip);
      double rhop = 0.0;
      if (rij < rc_rho) {
        double rv;
        tadah::collapse_rho(T, lt->S, rij, rv, rhop);
      }

      const double psip = phip + (lt->eam_fp[i] + lt->eam_fp[j]) * rhop;
      const double fpair = -psip / rij;

      f[i][0] += delx*fpair;
      f[i][1] += dely*fpair;
      f[i][2] += delz*fpair;
      if (newton_pair || j < nlocal) {
        f[j][0] -= delx*fpair;
        f[j][1] -= dely*fpair;
        f[j][2] -= delz*fpair;
      }

      if (evflag) ev_tally(i,j,nlocal,newton_pair,
                           phi,0.0,fpair,delx,dely,delz);
    }
  }

  if (vflag_fdotr) virial_fdotr_compute();
}

/* Pair-collapsed fast path (pure 2b, INITMB false): compute_2b with the
 * per-pair descriptor recompute + weight dot products replaced by a single
 * Hermite lookup, phi(r) and phi'(r) from the same segment, inside the
 * table's certified range; below it the pair term is evaluated
 * analytically (tadah::collapse_phi). The degenerate F==0, rho==0 EAM
 * collapse: one half-list pass, no ghost comm. Gate excludes bias, so no
 * per-species intercept block. Adjoint capture is routed to compute_2b in
 * compute() (safe: neither kernel does pair comm). */
void PairTadah::compute_2b_fast(int eflag, int vflag)
{
  int i,j,ii,jj,jnum;
  double xtmp,ytmp,ztmp,delx,dely,delz;
  double rij_sq;
  int *ilist,*jlist,*numneigh,**firstneigh;

  ev_init(eflag,vflag);

  double **x = atom->x;
  double **f = atom->f;
  int nlocal = atom->nlocal;
  int newton_pair = force->newton_pair;

  int inum = list->inum;
  ilist = list->ilist;
  numneigh = list->numneigh;
  firstneigh = list->firstneigh;

  const tadah::EamCollapseTables &T = lt->eam_tables;
  tadah::CollapseScratch &scratch = lt->eam_scratch;
  /* The table stands in for r < rc (its top node is the limit from below);
   * at r == rc a cutoff such as Cut_Dummy is already 0, so skip it. */
  const double rc_phi_sq = T.phi.rmax * T.phi.rmax;

  for (ii = 0; ii < inum; ii++) {
    i = ilist[ii];
    xtmp = x[i][0]; ytmp = x[i][1]; ztmp = x[i][2];
    jlist = firstneigh[i];
    jnum = numneigh[i];
    for (jj = 0; jj < jnum; jj++) {
      j = jlist[jj];
      j &= NEIGHMASK;
      delx = xtmp - x[j][0];
      dely = ytmp - x[j][1];
      delz = ztmp - x[j][2];
      rij_sq = delx*delx + dely*dely + delz*delz;
      if (rij_sq == 0.0) refuse_coincident(i, j);
      if (rij_sq > lt->cutoff_max_sq || rij_sq >= rc_phi_sq) continue;
      const double rij = sqrt(rij_sq);

      double phi, phip;
      tadah::collapse_phi(T, lt->S, rij, scratch, phi, phip);
      const double fpair = -phip / rij;

      f[i][0] += delx*fpair;
      f[i][1] += dely*fpair;
      f[i][2] += delz*fpair;
      if (newton_pair || j < nlocal) {
        f[j][0] -= delx*fpair;
        f[j][1] -= dely*fpair;
        f[j][2] -= delz*fpair;
      }

      if (evflag) ev_tally(i,j,nlocal,newton_pair,
                           phi,0.0,fpair,delx,dely,delz);
    }
  }

  if (vflag_fdotr) virial_fdotr_compute();
}

void PairTadah::compute_2b_mb_full(int eflag, int vflag)
{
  int i,j,ii,jj,jnum;
  double xtmp,ytmp,ztmp;
  double rij_sq;
  int *ilist,*jlist,*numneigh,**firstneigh;

  ev_init(eflag,vflag);

  double **x = atom->x;
  double **f = atom->f;

  int *type = atom->type;
  int nlocal = atom->nlocal;
  tadah::core::Vec3d delij;

  int inum = list->inum;
  ilist = list->ilist;
  numneigh = list->numneigh;
  firstneigh = list->firstneigh;
  int newton_pair = force->newton_pair;

  // fpsize x 3-directions
  tadah::core::fd_type fd(lt->S.descriptor_size());

  size_per_atom_arrays(*lt, atom->nmax);

  lt->rhos.set_zero();
  lt->aeds.set_zero();
  // The bias component of the atoms LAMMPS has, local and ghost: aeds has
  // atom->nmax columns, and atom->type is not set past nlocal + nghost.
  if (lt->bias > 0) {
    const int nall = nlocal + atom->nghost;
    for (int n=0; n<nall; ++n) {
      const int k = lt->species_intercept_idx_per_type[type[n]-1];
      if (k >= 0) lt->aeds(k, n) = 1.0;
    }
  }

  // IFT-adjoint snapshot setup. Default false ⇒ zero MD cost.
  if (lt->capture_adjoint_state) {
    lt->adjoint_snapshot.reset(static_cast<std::size_t>(nlocal),
                                lt->S.descriptor_size());
  }

  for (ii = 0; ii < inum; ii++) {
    i = ilist[ii];
    xtmp = x[i][0];
    ytmp = x[i][1];
    ztmp = x[i][2];
    jlist = firstneigh[i];
    jnum = numneigh[i];

    double Zi = lt->Z[type[i]-1];
    for (jj = 0; jj < jnum; jj++) {
      j = jlist[jj];
      j &= NEIGHMASK;
      delij[0] = xtmp - x[j][0];
      delij[1] = ytmp - x[j][1];
      delij[2] = ztmp - x[j][2];
      rij_sq = delij[0]*delij[0] + delij[1]*delij[1] + delij[2]*delij[2];
      if (rij_sq == 0.0) refuse_coincident(i, j);

      if (rij_sq > lt->cutoff_max_sq) continue;
      double rij = sqrt(rij_sq);
      double Zj = lt->Z[type[j]-1];

      if (lt->S.d2b->get_rcut() > rij) {
        lt->S.d2b->calc_aed(Zi,Zj,rij,rij_sq,lt->aeds.col(i),0.5);
      }

      if (lt->S.dmb->get_rcut() > rij) {
        lt->S.dmb->calc_rho(Zi,Zj,rij,rij_sq,delij,lt->rhos.col(i));
      }
    }

    // Compute B_i
    // EAM computes the derivative of the embedding energy at each atom.
    // ML descriptors compute equivalent dEi/dBi
    lt->S.dmb->calc_aed(lt->rhos.col(i), lt->aeds.col(i));

    if (lt->norm)
      lt->model->norm.normalise(lt->aeds.col(i));

    double ei = lt->model->epredict(lt->aeds.col(i));
    if (eflag_atom) eatom[i] = ei;
    if (eflag_global) eng_vdwl += ei;
  }

  // communicate the derivative of the embedding function
  // if (comm_forward)
  //   comm->forward_comm(this);

  for (ii = 0; ii < inum; ii++) {
    i = ilist[ii];

    xtmp = x[i][0];
    ytmp = x[i][1];
    ztmp = x[i][2];

    jlist = firstneigh[i];
    jnum = numneigh[i];
    int Zi = lt->Z[type[i]-1];
    for (jj = 0; jj < jnum; jj++) {
      j = jlist[jj];
      j &= NEIGHMASK;
      delij[0] = xtmp - x[j][0];
      delij[1] = ytmp - x[j][1];
      delij[2] = ztmp - x[j][2];
      rij_sq = delij[0]*delij[0] + delij[1]*delij[1] + delij[2]*delij[2];
      if (rij_sq > lt->cutoff_max_sq) continue;
      fd.set_zero();  // Needed b/c _mJoin may skip parts of the vector
      double rij = sqrt(rij_sq);
      int Zj = lt->Z[type[j]-1];

      if (lt->S.d2b->get_rcut() > rij) {
        lt->S.d2b->calc_dXijdri(Zi,Zj,rij,rij_sq,fd,0.5);
      }

      if (lt->S.dmb->get_rcut() > rij) {
        lt->S.dmb->calc_dXijdri(Zi,Zj,rij,rij_sq,delij,lt->rhos.col(i),fd);
      }

      // copy 2b descriptors first and
      // scale all 2b components by delij/rij
      // this is inefficient but will do for now... TODO
      for(size_t s=lt->bias; s<lt->S.d2b->size()+lt->bias; ++s) {
        fd(s,0) /= rij;
        fd(s,1) = fd(s,0);
        fd(s,2) = fd(s,0);
        fd(s,0) *= delij[0];
        fd(s,1) *= delij[1];
        fd(s,2) *= delij[2];
      }
      if (lt->norm)
        lt->model->norm.normalise(fd);

      tadah::core::Vec3d fij = lt->model->fpredict(fd,lt->aeds.col(i));

      f[i][0] += fij(0);
      f[i][1] += fij(1);
      f[i][2] += fij(2);

      f[j][0] -= fij(0);
      f[j][1] -= fij(1);
      f[j][2] -= fij(2);

      if (evflag) ev_tally_xyz(i,j,nlocal,newton_pair,
                               0.0,0.0,fij(0),fij(1),fij(2),delij[0],delij[1],delij[2]);

      if (lt->capture_adjoint_state) {
        // fd already rotated (lines 444-451). fij = -(fd*W) ⇒
        // dF_i[k]/dW[c] = -fd(c,k); atom j mirrors with opposite sign when
        // local (matches f[j] -= fij). Ghost-j contributions arrive via the
        // (j_local, i_image) symmetric visit in mb_full's iteration.
        const std::size_t n_desc_l = lt->S.descriptor_size();
        for (std::size_t k_ax = 0; k_ax < 3; ++k_ax) {
          double* row_i = lt->adjoint_snapshot.row_ptr(
              static_cast<std::size_t>(i), k_ax);
          for (std::size_t c = 0; c < n_desc_l; ++c) {
            row_i[c] += -fd(c, k_ax);
          }
          if (j < nlocal) {
            double* row_j = lt->adjoint_snapshot.row_ptr(
                static_cast<std::size_t>(j), k_ax);
            for (std::size_t c = 0; c < n_desc_l; ++c) {
              row_j[c] += +fd(c, k_ax);
            }
          }
        }
        // Per-pair box-strain contribution to dvirial_dW.
        // Same derivation as mb_half (see comment there). mb_full's
        // full-list double-counting cancels via the 0.5 weight already
        // applied in calc_dXijdri (line 521 above), so per-pair math
        // is identical.
        if (lt->adjoint_snapshot.box_dofs == 1) {
          double* box_row = lt->adjoint_snapshot.box_row_ptr(0);
          for (std::size_t c = 0; c < n_desc_l; ++c) {
            double dot = 0.0;
            for (std::size_t k = 0; k < 3; ++k) dot += fd(c, k) * delij[k];
            box_row[c] += -dot;
          }
        } else if (lt->adjoint_snapshot.box_dofs == 6) {
          const double vec_e[6][3] = {
            { delij[0],          0.0,               0.0              },  // xx
            { 0.0,               delij[1],          0.0              },  // yy
            { 0.0,               0.0,               delij[2]         },  // zz
            { 0.0,               0.5 * delij[2],    0.5 * delij[1]   },  // yz
            { 0.5 * delij[2],    0.0,               0.5 * delij[0]   },  // xz
            { 0.5 * delij[1],    0.5 * delij[0],    0.0              },  // xy
          };
          for (int alpha = 0; alpha < 6; ++alpha) {
            double* box_row = lt->adjoint_snapshot.box_row_ptr(
                static_cast<std::size_t>(alpha));
            for (std::size_t c = 0; c < n_desc_l; ++c) {
              double sum = 0.0;
              for (std::size_t k = 0; k < 3; ++k)
                sum += fd(c, k) * vec_e[alpha][k];
              box_row[c] += -sum;
            }
          }
        }
      }
    }
  }

  if (lt->capture_adjoint_state) {
    lt->adjoint_snapshot.valid = true;
  }

  if (vflag_fdotr)  virial_fdotr_compute();

}
void PairTadah::compute_dimers(int eflag, int vflag)
{
  int ii,jj,jnum;
  int *ilist,*jlist,*numneigh,**firstneigh;

  ev_init(eflag,vflag);

  double **x = atom->x;
  double **f = atom->f;

  int nlocal = atom->nlocal;
  int *type = atom->type;

  int inum = list->inum;
  ilist = list->ilist;
  numneigh = list->numneigh;
  firstneigh = list->firstneigh;
  int newton_pair = force->newton_pair;
  tagint *tag = atom->tag;

  tagint **special = atom->special;
  int **nspecial = atom->nspecial;

  tadah::core::fd_type fdIJ(lt->S.descriptor_size());
  tadah::core::fd_type fdJI(lt->S.descriptor_size());

  //n= 0, 1, 2, 3
  int i1,i2,j1,j2;
  tadah::core::Mat6R3C delM;
  double r_sq[6];
  double r[6];

  tadah::core::Vec3d delij_com;   // i-j CoM
  tadah::core::Vec3d i_com;   // i CoM
  tadah::core::Vec3d j_com;   // j CoM

  // if bond is not included begin summation from the third distance
  int Nstart= lt->dimer_bond_bool ? 0:2;
  double r_b = lt->dimer_r;
  double r_b_sq=r_b*r_b;

  double r_cut_com_sq =pow(lt->cutoff_max-r_b,2);

  // This is the tolerance for the bond distance
  double eps=1e-1;

  // Dimer adjoint not implemented (per-molecule lt->aeds reuse +
  // 4-atom-subunit force decomposition need per-atom remapping). Reset
  // snapshot to a known empty state with valid=false; HPO host falls back
  // to numerical FD for any dimer-using script.
  if (lt->capture_adjoint_state) {
    lt->adjoint_snapshot.reset(static_cast<std::size_t>(nlocal),
                                lt->S.descriptor_size());
  }

  std::vector<int> markflag(atom->nlocal+atom->nghost,0);
  std::vector<int> jmarkflag(atom->nlocal+atom->nghost,0);
  //markflag.set_zero();

  for (ii = 0; ii < inum; ii++) {
    i1 = ilist[ii];
    if (markflag[i1]) continue;

    i2 = atom->bond_atom[i1][0];
    i2 = atom->map(i2);
    i2 = domain->closest_image(i1,i2);

    //if (tag[i1]%2)
    //    i2 = atom->map(tag[i1] + 1);
    //else
    //    i2 = atom->map(tag[i1] - 1);
    //i2 = domain->closest_image(i1,i2);

    //int II = special[i1][0];
    //II = atom->map(II);
    //II = domain->closest_image(i1,II);
    //if (II!=i2)
    //    throw std::runtime_error("i1-II differ\n");

    //int III = atom->bond_atom[i1][0];
    //III = atom->map(III);
    //III = domain->closest_image(i1,III);
    //if (III!=i2)
    //    throw std::runtime_error("i1-III differ\n");

    if (markflag[i2]) continue;
    markflag[i2] = 1;
    markflag[i1] = 1;

    delM(0,0) = x[i1][0] - x[i2][0];
    delM(0,1) = x[i1][1] - x[i2][1];
    delM(0,2) = x[i1][2] - x[i2][2];
    r_sq[0] = delM.row(0)*delM.row(0);
    if (r_sq[0] == 0.0) refuse_coincident(i1, i2);
    r[0] = sqrt(r_sq[0]);

    if (abs(r[0]-r_b)>eps) {
      throw std::runtime_error("i1-i2 bond is: " + std::to_string(r[0]));
    }

    jlist = firstneigh[i1];
    jnum = numneigh[i1];
    i_com[0] = 0.5*(x[i1][0]+x[i2][0]);
    i_com[1] = 0.5*(x[i1][1]+x[i2][1]);
    i_com[2] = 0.5*(x[i1][2]+x[i2][2]);

    jmarkflag = markflag;   // TODO just search markflag with j1 and j2
    for (jj = 0; jj < jnum; jj++) {
      j1 = jlist[jj];
      j1 &= NEIGHMASK;
      if (jmarkflag[j1]) continue;

      delM(2,0) = x[i1][0] - x[j1][0];
      delM(2,1) = x[i1][1] - x[j1][1];
      delM(2,2) = x[i1][2] - x[j1][2];
      r_sq[2] = delM.row(2)*delM.row(2);
      if (r_sq[2] == 0.0) refuse_coincident(i1, j1);

      if (r_sq[2] > lt->cutoff_max_sq) continue;
      r[2] = sqrt(r_sq[2]);

      bool found=false;

      // but what if j1 is a ghost without a local copy?
      // we need a different approach in this case
      j2=-1;
      if (j1>=atom->nlocal) {
        // When full NN list is used than we should be able to search around i1
        // hence there is no need for ghost atoms lists
        for (int jj2 = 0; jj2 < jnum; jj2++) {
          j2 = jlist[jj2];
          j2 &= NEIGHMASK;
          delM(1,0) = x[j1][0] - x[j2][0];
          delM(1,1) = x[j1][1] - x[j2][1];
          delM(1,2) = x[j1][2] - x[j2][2];
          r_sq[1] = delM.row(1)*delM.row(1);
          // the correct atom j2 is identified  when the distance
          // between j1-j2 is approx equal to the bond length
          if (abs(r_sq[1]-r_b_sq)<eps) {
            found=true;
            r[1] = sqrt(r_sq[1]);
            break;
          }
        }
      }
      else {
        int n = nspecial[j1][0]; // number of bonded atoms either 0 or 1

        if (n==0)
          throw std::runtime_error("Tadah! No bonded atom found.\n");

        j2 = special[j1][0];    // j2 is a global index now, j1 is local
        j2 = atom->map(j2); // convert global j2 to local j2
        j2 &= NEIGHMASK;
        j2 = domain->closest_image(j1,j2);
        delM(1,0) = x[j1][0] - x[j2][0];
        delM(1,1) = x[j1][1] - x[j2][1];
        delM(1,2) = x[j1][2] - x[j2][2];
        r_sq[1] = delM.row(1)*delM.row(1);
        if (r_sq[1] == 0.0) refuse_coincident(j1, j2);
        r[1] = sqrt(r_sq[1]);

      }

      if (j2==-1)
        throw std::runtime_error("Tadah! No bonded atom found v1.\n");

      // Finally mark but only once we have j1 and j2
      if (jmarkflag[j2]) continue;
      jmarkflag[j2] = 1;
      if (jmarkflag[j1]) continue;
      jmarkflag[j1] = 1;

      if (abs(r_sq[1]-r_b_sq)<eps) {
        found=true;
      }

      if (!found) {
        throw std::runtime_error("Tadah! No bonded atom found v2.\n");
      }


      // We should have all four atoms by now.
      // Each molecule-molecule interaction is computed just once
      // So here we apply forces the same way as we would do 
      // for the full NN list.

      // Check are ij CoM distance within the max CoM cutoff distance
      j_com[0] = 0.5*(x[j1][0]+x[j2][0]);
      j_com[1] = 0.5*(x[j1][1]+x[j2][1]);
      j_com[2] = 0.5*(x[j1][2]+x[j2][2]);
      delij_com = i_com - j_com;
      double r_com_sq = delij_com * delij_com;
      if (r_com_sq > r_cut_com_sq) {
        continue;
      }

      std::vector<int> IJ = {i1,i2,j1,j2};

      if (lt->initmb) {
        lt->rhos.set_zero();
      }
      lt->aeds.set_zero();
      // Column n holds the descriptor of atom IJ[n] (i1 i2 j1 j2): its
      // intercept is that of IJ[n]'s species, not of local atom n.
      if (lt->bias > 0)
        for (size_t n=0; n<4; ++n ) {
          const int k = lt->species_intercept_idx_per_type[type[IJ[n]]-1];
          if (k >= 0) lt->aeds(k, n) = 1.0;
        }

      // Compute remaining 3 distances
      for (int n=3; n<6; ++n) {
        int I=IJ[midx[n].first];
        int J=IJ[midx[n].second];
        delM(n,0) = x[I][0] - x[J][0];
        delM(n,1) = x[I][1] - x[J][1];
        delM(n,2) = x[I][2] - x[J][2];
        r_sq[n] = delM.row(n)*delM.row(n);
        if (r_sq[n] == 0.0) refuse_coincident(I, J);
        r[n] = sqrt(r_sq[n]);
      }

      // Compute densities for every atom.
      for (int n=Nstart; n<6; ++n) {
        if (lt->initmb && lt->S.dmb->get_rcut() >= r[n]) {
          lt->S.dmb->calc_rho(1,1,r[n],r_sq[n],delM.row(n),lt->rhos.col(midx[n].first));
          lt->S.dmb->calc_rho(1,1,r[n],r_sq[n],-delM.row(n),lt->rhos.col(midx[n].second));
        }
        if (lt->init2b && lt->S.d2b->get_rcut() >= r[n]) {
          lt->S.d2b->calc_aed(1,1,r[n],r_sq[n],lt->aeds.col(midx[n].first));
          lt->S.d2b->calc_aed(1,1,r[n],r_sq[n],lt->aeds.col(midx[n].second));
        }
      }

      // Compute two-body and many-body energy. An atom belongs to the
      // cluster of every molecule pair its molecule is in: its per-atom
      // energy adds the energy of each, as eng_vdwl does.
      for (size_t n=0; n<4; ++n) {
        if (lt->initmb) {
          lt->S.dmb->calc_aed(lt->rhos.col(n),lt->aeds.col(n));
        }
        if (lt->norm)
          lt->model->norm.normalise(lt->aeds.col(n));

        if (IJ[n]<atom->nlocal) {
          int I=IJ[midx[n].first];
          int J=IJ[midx[n].second];
          double ei = lt->model->epredict(lt->aeds.col(n));
          if (eflag_atom) eatom[IJ[n]] += ei;
          if (eflag_global) eng_vdwl += ei;
        }
      }

      // Compute forces
      for (int n=Nstart; n<6; ++n) {
        fdIJ.set_zero();
        fdJI.set_zero();
        int I=IJ[midx[n].first];
        int J=IJ[midx[n].second];
        if (lt->init2b && lt->S.d2b->get_rcut() > r[n]) {
          lt->S.d2b->calc_dXijdri(1,1,r[n],r_sq[n],fdIJ);
        }

        if (lt->initmb && lt->S.dmb->get_rcut() > r[n]) {
          lt->S.dmb->calc_dXijdri(1,1,r[n],r_sq[n],delM.row(n),
                                  lt->rhos.col(midx[n].first), fdIJ);
          lt->S.dmb->calc_dXijdri(1,1,r[n],r_sq[n],-delM.row(n),
                                  lt->rhos.col(midx[n].second), fdJI);
        }

          for(size_t s=lt->bias; s<lt->S.d2b->size()+lt->bias; ++s) {
            fdIJ(s,0) /= r[n];
            fdIJ(s,1) = fdIJ(s,0);
            fdIJ(s,2) = fdIJ(s,0);
            // fdIJ=fdJI for pairwise part
            fdJI(s,0) = fdIJ(s,0);
            fdJI(s,1) = fdIJ(s,0);
            fdJI(s,2) = fdIJ(s,0);


            fdIJ(s,0) *= delM(n,0);
            fdIJ(s,1) *= delM(n,1);
            fdIJ(s,2) *= delM(n,2);

            fdJI(s,0) *= -delM(n,0);
            fdJI(s,1) *= -delM(n,1);
            fdJI(s,2) *= -delM(n,2);
          }

          if (lt->norm) {
            lt->model->norm.normalise(fdIJ);
            lt->model->norm.normalise(fdJI);
          }

          tadah::core::Vec3d f_eam = lt->model->fpredict(fdIJ,lt->aeds.col(midx[n].first));
          f_eam -= lt->model->fpredict(fdJI,lt->aeds.col(midx[n].second));

          f[I][0] += f_eam(0);
          f[I][1] += f_eam(1);
          f[I][2] += f_eam(2);

          f[J][0] -= f_eam(0);
          f[J][1] -= f_eam(1);
          f[J][2] -= f_eam(2);

          if (evflag) ev_tally_xyz(I,J,nlocal,newton_pair,0.0,0.0,
                                   f_eam(0),f_eam(1),f_eam(2),
                                   delM(n,0),delM(n,1),delM(n,2));
      }
    }
  }

  if (vflag_fdotr)  virial_fdotr_compute();

}
void PairTadah::compute(int eflag, int vflag)
  // REQUIRED
{
  if (lt->dimer) {
    compute_dimers(eflag, vflag);
  } else if (lt->eam_fast) {
    if (lt->capture_adjoint_state) {
      /* MB: falling back to the generic kernel here would silently corrupt
       * the reverse/forward comm (sizes were set to the fast path's 1 double
       * in coeff), and the collapsed path never forms the per-pair
       * descriptor vectors the adjoint needs — hard error instead.
       * Pure 2b: the generic kernel IS safe — neither kernel does pair comm
       * (widths are 0) and compute_2b forms the descriptor vectors. */
      if (lt->initmb)
        error->one(FLERR, "pair_tadah: adjoint capture is incompatible with the "
                          "EAM fast path; set EAMFAST false in the potential");
      compute_2b(eflag, vflag);
    } else if (lt->initmb) {
      compute_2b_mb_half_eamfast(eflag, vflag);
    } else {
      compute_2b_fast(eflag, vflag);
    }
  } else if (lt->linear && lt->initmb) {
    compute_2b_mb_half(eflag, vflag);
  } else if (lt->linear /*&& !lt->initmb*/) {
    compute_2b(eflag, vflag);
  } else {
    compute_2b_mb_full(eflag, vflag);
  }
}

// allocate arrays
void PairTadah::allocate()
{
  int n = atom->ntypes+1;
  map = new int[n];
  memory->create(cutsq,n,n,"pair:cutsq");
  memory->create(setflag,n,n,"pair:setflag");
  for (int i = 1; i < n; i++)
    for (int j = i; j < n; j++) setflag[i][j] = 0;

  allocated = 1;
}

// REQUIRED processes the arguments to the pair_style command
void PairTadah::settings(int narg, char **arg)
{
  // pair_style tadah
  if (narg != 0) utils::missing_cmd_args(FLERR, "pair_style tadah", error);
}

/* Resolve the EAM-collapse fast path gate and build the phi/rho tables.
 * Called from coeff() after lt->initialize(). Controlled by the EAMFAST
 * config key (BOOL, default true via the blueprint). Falls back silently to
 * the generic path when the model/descriptor combination does not collapse
 * (the gate below); when the builder refuses (e.g. a table misses the
 * collapse tolerance already in its top segment, so it has no certified
 * range) the reason is logged and the generic path kept.
 * On success the log states each table's certified range and tolerance. */
void PairTadah::setup_eam_fastpath()
{
  lt->eam_fast = false;

  /* EAMFAST is a blueprint BOOL with default_value=true: contains()
   * materialises the default on first query, so absent means enabled. The
   * guard keeps drivers with a blueprint-less Context on the (enabled)
   * default rather than throwing. */
  bool want = true;
  try {
    if (lt->c.contains("EAMFAST")) want = lt->c.get<bool>("EAMFAST");
  } catch (const std::exception&) { /* keep default */ }
  if (!want) return;

  if (!(lt->linear && !lt->norm && lt->bias == 0 && !lt->dimer &&
        lt->init2b && atom->ntypes == 1))
    return;
  /* MB present: only the EAM form collapses. Pure 2b (INITMB false) always
   * collapses -- the phi sampling is descriptor-agnostic. */
  if (lt->initmb && !(lt->S.dmb && lt->S.dmb->eam_form()))
    return;

  /* The tables of this potential object, built by an earlier pair_coeff
   * for the element of the one LAMMPS type: reused only for that element
   * (an EXTERN line can map the type to another element of the potential). */
  if (lt->eam_tables.valid &&
      lt->eam_tables.has_mb == lt->initmb &&
      lt->eam_tables.Z == lt->Z[0]) {
    lt->eam_scratch.resize(lt->S.descriptor_size());
    lt->eam_fast = true;
    return;
  }

  if (!lt->c.contains("WEIGHTS")) return;
  tadah::core::t_type w;
  w.resize(lt->c.size("WEIGHTS"));
  lt->c.get<tadah::core::t_type>("WEIGHTS", w);

  std::string why;
  const int NR = tadah::kCollapseNR;
  const bool ok = lt->initmb
      ? tadah::build_eam_collapse(lt->S, w, lt->Z[0], NR, lt->eam_tables, why)
      : tadah::build_pair2b_collapse(lt->S, w, lt->Z[0], NR, lt->eam_tables, why);
  if (!ok) {
    if (comm->me == 0)
      utils::logmesg(lmp, "  Tadah! collapse fast path unavailable: {}; "
                          "using the generic kernel\n", why);
    return;
  }
  /* Scratch for the analytic pair term below the certified range (the fast
   * kernels are serial, one buffer per potential object). */
  lt->eam_scratch.resize(lt->S.descriptor_size());
  lt->eam_fast = true;
  if (comm->me == 0)
    utils::logmesg(lmp, "  Tadah! {}\n",
                   tadah::format_collapse_report(lt->eam_tables));
}

/* REQUIRED: set coefficients for one i,j type pair, called from pair_coeff.
 *
 * A pair_coeff that pair_style tadah refuses leaves it without a potential,
 * whatever refuses it: the argument checks below, Pair::map_element2type (a
 * number of elements other than the number of types, every type NULL), the
 * load of the file, initialize() (an element the potential does not have),
 * an EXTERN line with no driver's potential to adopt, or a later step. The
 * handler of this function-try-block releases the potential and rethrows
 * the error, so a library driver that catches it cannot run the potential
 * an earlier line gave (init_style() refuses a pair style without one). A
 * line LAMMPS refuses before it calls the pair style (`pair_coeff 1 1 ...`,
 * refused by Input::pair_coeff for a one_coeff style) does not reach it. */
void PairTadah::coeff(int narg, char **arg)
try {
  if (strcmp(arg[0],"*") != 0 || strcmp(arg[1],"*") != 0)
    error->all(FLERR,"Pair style tadah requires * * for pair_coeff");

  // Print Tadah version once on rank 0 (single source of truth: blueprint TOML).
  if (comm->me == 0) {
    const auto &bp = tadah::core::BlueprintParser::getBlueprintTadahRef();
    utils::logmesg(lmp, "  Tadah! plugin version {}\n", bp.version);
  }

  if (narg < 3)
    error->all(FLERR,"Pair style tadah requires a potential file: pair_coeff * * <file> <elements...>");

  if (!allocated) allocate();

  // e.g. pair_coeff * * pot.tadah Ta Nb NULL
  map_element2type(narg - 3, arg + 3, true);

  if (strcmp(arg[2],"EXTERN") == 0) {
    /* Sentinel filename: adopt a driver-provided potential. Precedence:
     *  1. potential staged for this thread (whole-script input->file()
     *     drivers -- see stage_extern());
     *  2. a potential the driver gave this pair style through load()
     *     before this line executed (legacy per-line driver pattern), kept
     *     by every later EXTERN line until a pair_coeff loads a file or
     *     adopts a staged potential;
     *  3. hard error, also when the pair style holds a potential that is
     *     not the driver's: one an earlier `pair_coeff * * <file>` loaded,
     *     or one an earlier EXTERN adopted from the staged slot, which the
     *     driver has cleared since (both were adopted again without a
     *     word: a script ran with a potential it had not asked for). When
     *     the driver's load() of case 2 failed, its reason
     *     (last_load_error_) is in the message. */
    if (staged_extern_slot()) {
      lt = staged_extern_slot();
      lt_source_ = PotentialSource::Staged;
      lt_file_.clear();
    } else if (!(lt && lt_source_ == PotentialSource::DriverLoad)) {
      std::string why;
      if (lt && lt_source_ == PotentialSource::File)
        why = " (pair_style tadah holds the potential of '" + lt_file_ +
              "', loaded by an earlier pair_coeff; EXTERN adopts a potential "
              "the driver staged or gave this pair style with load())";
      else if (lt && lt_source_ == PotentialSource::Staged)
        why = " (pair_style tadah holds the potential an earlier pair_coeff "
              "* * EXTERN adopted from the staged slot, which the driver has "
              "cleared since; EXTERN adopts a potential the driver staged or "
              "gave this pair style with load())";
      else if (!last_load_error_.empty())
        why = " (last load() failed: " + last_load_error_ + ")";
      if (!lt) emit_load_notices("EXTERN (driver-provided)");  // of a failed load()
      error->all(FLERR,"pair_coeff * * EXTERN: no staged or pre-loaded Tadah potential{}",
                 why);
    }
  } else {
    /* A file: always read, replacing any potential the pair style holds
     * (an earlier file's, a driver's load() or an adopted staged one,
     * which its slot keeps alive for the driver), also after `pair_style
     * tadah` again, which keeps the pair style and its potential. */
    const bool loaded = PairTadah::load(narg, arg);
    emit_load_notices(arg[2]);
    if (!loaded)   // load() left the pair style without a potential
      error->all(FLERR,"pair_coeff: failed to load Tadah potential from '{}': {}",
                 arg[2], last_load_error_);
    lt_source_ = PotentialSource::File;
    lt_file_ = arg[2];
  }
  // A potential a driver loaded (load(Context)) before pair_coeff * * EXTERN.
  emit_load_notices(strcmp(arg[2], "EXTERN") == 0
                        ? std::string("EXTERN (driver-provided)")
                        : std::string(arg[2]));

  /* Map the LAMMPS types to the potential's elements. A failure (an element
   * the potential does not have: "Symbol not in potential file: W") goes
   * through error->all like a load failure, so its reason reaches
   * log.lammps and the exception a library driver catches; thrown from
   * here it left LAMMPS without a log entry. */
  try {
    lt->initialize(narg, arg);
  } catch (const std::exception& e) {
    // A potential not mapped to the line's types is not run with (the
    // handler at the end of coeff() releases it).
    error->all(FLERR, "pair_coeff: failed to initialise Tadah potential '{}': {}",
               strcmp(arg[2], "EXTERN") == 0
                   ? std::string("EXTERN (driver-provided)")
                   : std::string(arg[2]),
               e.what());
  }

  // State the DM_REAM embedding reference and density form on every load: a
  // potential without an EMBF0 line is evaluated with the default ZERO
  // (F(0) = 0), which changes the energies of a tadah <= 1.5.0 potential -- a
  // LAMMPS WARNING says so (the tadah Logger notice is silent here: level
  // ERROR, std::cout only).
  if (comm->me == 0 && lt->initmb && lt->S.dmb) {
    if (const auto mode = lt->S.dmb->embf0_mode()) {
      if (lt->embf0_declared) {
        utils::logmesg(lmp, "  Tadah! DM_REAM embedding reference EMBF0 {}{}\n",
                       tadah::models::embf0_name(*mode),
                       *mode == tadah::models::EmbF0Mode::Zero
                           ? " (F(0) = 0 by construction)"
                           : " (raw embedding of tadah <= 1.5.0)");
      } else {
        utils::logmesg(lmp, "  Tadah! DM_REAM embedding reference EMBF0 {} "
                       "(key absent: default ZERO, F(0) = 0 by construction; "
                       "add EMBF0 FREE to reproduce tadah <= 1.5.0 energies)\n",
                       tadah::models::embf0_name(*mode));
        const std::string pot = strcmp(arg[2], "EXTERN") == 0
                                    ? std::string("EXTERN (driver-provided)")
                                    : std::string(arg[2]);
        error->warning(FLERR, "pair tadah: potential '{}' has no EMBF0 line; {}",
                       pot, tadah::models::embf0_absent_consequence(lt->linear));
      }
      // The density form: mandatory on a DM_REAM potential (the load above
      // refused one without it), so it is always stored here.
      if (const auto form = tadah::models::stored_rhoform(lt->c)) {
        using tadah::models::RhoForm;
        utils::logmesg(lmp, "  Tadah! DM_REAM density form RHOFORM {}{}\n",
                       tadah::models::rhoform_name(*form),
                       *form == RhoForm::Signed
                           ? " (signed amplitudes, the density before tadah 1.6.0)"
                       : *form == RhoForm::NonNeg
                           ? " (rho >= 0 by construction)"
                           : " (rho >= 0 and non-increasing by construction)");
      }
    }
  }

  manybody_flag = lt->initmb || !lt->linear ? 1 : 0;
  single_enable = lt->linear && !lt->initmb ? 1 : 0;

  if (lt->dimer) {
    manybody_flag=0;
    /*  [0] i1-i2    (i1)-[2]-(j1)
     *  [1] j1-j2     | \    / |
     *  [2] i1-j1     | [4] /  |
     *  [3] i2-j2    [0]  \/  [1]
     *  [4] i1-j2     |   /\   |
     *  [5] i2-j1     | [5] \  |   
     *                | /    \ |   
     *               (i2)-[3]-(j2)
     *
     *  This compute only works with rigid bonds.
     *  Style shake is your friend
     *  fix fixSHAKE all shake 1e-5 20 0 b ${bondlength}
     *  For now we assume that the distance between 
     *  atoms of two interacting molecules is always
     *  greater than the bond distance.
     * 
     *  i1---i2-----------------j1---j2
     *     |-------r_com-----------|   
     *   |---------r_max-------------|
     *
     */
    midx.resize(6);
    midx[0] = std::make_pair(0,1);
    midx[1] = std::make_pair(2,3);
    midx[2] = std::make_pair(0,2);
    midx[3] = std::make_pair(1,3);
    midx[4] = std::make_pair(0,3);
    midx[5] = std::make_pair(1,2);   
    lt->aeds.resize(lt->S.descriptor_size(),4);

    if (lt->initmb) {
      size_t s = lt->S.dmb->rhoi_size()+ lt->S.dmb->rhoip_size();
      lt->rhos.resize(s,4);
    }
  }
  setup_eam_fastpath();

  if (lt->eam_fast && lt->initmb) {
    comm_reverse = 1;   // scalar rho_bar
    comm_forward = 1;   // scalar F'
  }
  else if (!lt->initmb && lt->linear) {
    comm_reverse = 0;   // compute_2b / compute_2b_fast do no pair comm
    comm_forward = 0;
  }
  else if (lt->linear) {
    comm_reverse = lt->S.dmb->rhoi_size()+lt->S.d2b->size();
    comm_forward = lt->S.dmb->rhoip_size();
  }
  else {
    comm_reverse = lt->S.dmb->rhoi_size()+lt->S.d2b->size();
    comm_forward = lt->S.dmb->rhoip_size()+lt->S.d2b->size()+lt->S.dmb->size();
  }
} catch (...) {
  release_potential();
  throw;
}

void PairTadah::init_style()
// optional: style initialization: request neighbor list(s), error checks
{
  if (atom->tag_enable == 0)
    error->all(FLERR,"Pair style tadah requires atom IDs");
  /* LAMMPS runs once every type has a pair_coeff (setflag), which a
   * pair_coeff whose load or mapping failed has set; a driver that caught
   * that error (or whose load() failed) must not run: there is no potential. */
  if (!lt)
    error->all(FLERR,"pair_style tadah has no potential: the last pair_coeff, "
                     "or a driver's PairTadah::load(), failed (see its error)");

  if (lt->dimer) {
    neighbor->add_request(this, NeighConst::REQ_FULL);
    if (force->newton_pair)
      error->all(FLERR,"Pair style tadah requires newton pair off for this potential.");
  } else if (lt->linear) {
    neighbor->add_request(this);
  } else {
    if (force->newton_pair == 0)
      error->all(FLERR,"Pair style tadah requires newton pair on for this potential.");
    // neighbor->add_request(this, NeighConst::REQ_FULL);
    neighbor->add_request(this, NeighConst::REQ_FULL | NeighConst::REQ_GHOST);
  }
}
double PairTadah::init_one(int i, int j) {
  return lt->cutoff_max;
}
/* ---------------------------------------------------------------------- */
int PairTadah::pack_reverse_comm(int n, int first, double *buf)
{
  if (lt->eam_fast) {
    for (int k = 0; k < n; ++k) buf[k] = lt->eam_rho_bar[first+k];
    return n;
  }
  if (lt->linear)
    return lt->pack_reverse_linear(n,first,buf);
  else
    return lt->pack_reverse_nonlinear(n,first,buf);
}
/* ---------------------------------------------------------------------- */
void PairTadah::unpack_reverse_comm(int n, int *list, double *buf)
{
  if (lt->eam_fast) {
    for (int k = 0; k < n; ++k) lt->eam_rho_bar[list[k]] += buf[k];
    return;
  }
  if (lt->linear)
    lt->unpack_reverse_linear(n,list,buf);
  else
    lt->unpack_reverse_nonlinear(n,list,buf);
}
/* ---------------------------------------------------------------------- */
int PairTadah::pack_forward_comm(int n, int *list, double *buf,
                                 int /*pbc_flag*/, int * /*pbc*/)
{
  if (lt->eam_fast) {
    for (int k = 0; k < n; ++k) buf[k] = lt->eam_fp[list[k]];
    return n;
  }
  if (lt->linear)
    return lt->pack_forward_linear(n,list,buf);
  else
    return lt->pack_forward_nonlinear(n,list,buf);
}
/* ---------------------------------------------------------------------- */
void PairTadah::unpack_forward_comm(int n, int first, double *buf)
{
  if (lt->eam_fast) {
    for (int k = 0; k < n; ++k) lt->eam_fp[first+k] = buf[k];
    return;
  }
  if (lt->linear)
    lt->unpack_forward_linear(n,first,buf);
  else
    lt->unpack_forward_nonlinear(n,first,buf);
}
/* Two atoms at one position: every descriptor divides by the distance, so the
 * forces would be nan. Not a physical structure; stop, naming the atom IDs. */
void PairTadah::refuse_coincident(int i, int j)
{
  const tagint a = std::min(atom->tag[i], atom->tag[j]);
  const tagint b = std::max(atom->tag[i], atom->tag[j]);
  error->one(FLERR,
             "pair_tadah: atom IDs {} and {} are at the same position: two atoms "
             "at one position are not a physical structure (every force would "
             "be nan)", a, b);
}

double PairTadah::single(int i, int j, int itype, int jtype, double rsq,
                         double /*factor_coul*/, double /*factor_lj*/, double &fforce)
{
  if (rsq == 0.0) refuse_coincident(i, j);
  tadah::core::aed_type aed(lt->S.descriptor_size());
  tadah::core::fd_type fd(lt->S.descriptor_size());
  aed.set_zero();
  // if (lt->bias)
  //   aed(0) = 1.0;
  int Zi = lt->Z[itype-1];
  int Zj = lt->Z[jtype-1];
  double rij = sqrt(rsq);
  lt->S.d2b->calc_all(Zi,Zj,rij,rsq,aed, fd);
  fforce = lt->model->fpredict(fd,aed,0)/rij;
  double ei = lt->model->epredict(aed);
  return ei;
}

using tadah::LammpsTadah;

void PairTadah::release_potential()
{
  lt.reset();
  lt_source_ = PotentialSource::None;
  lt_file_.clear();
}

void PairTadah::emit_load_notices(const std::string &pot)
{
  if (comm->me == 0)
    for (const std::string &n : load_notices_)
      error->warning(FLERR, "pair tadah: potential '{}': {}", pot, n);
  load_notices_.clear();
}

bool PairTadah::load(int argc, char **argv)
{
  last_load_error_.clear();
  load_notices_.clear();
  // The notices of the file's keys (parse and construction) go to
  // coeff(), which writes them as LAMMPS warnings, not to stderr.
  tadah::core::IgnoredKeyNoticeCapture notices;
  try
  {
    lt.reset(new tadah::LammpsTadah(argc, argv));
    // A driver's; coeff() marks the potential of its file line as File.
    lt_source_ = PotentialSource::DriverLoad;
    lt_file_.clear();
    load_notices_ = notices.notices();
    return true;
  }
  catch (const std::exception& e)
  {
    // Reported by coeff() through error->all: screen and log.lammps (rank
    // 0) and the exception every rank throws.
    last_load_error_ = e.what();
    load_notices_ = notices.notices();
    release_potential();                // ensure we own nothing
    return false;
  }
}

bool PairTadah::load(const tadah::core::Context& ctx)
{
    last_load_error_.clear();
    load_notices_.clear();
    tadah::core::IgnoredKeyNoticeCapture notices;  // see load(argc, argv)
    try {
        lt.reset(new tadah::LammpsTadah(ctx));    // one copy inside ctor
        lt_source_ = PotentialSource::DriverLoad;
        lt_file_.clear();
        load_notices_ = notices.notices();
        return true;
    }
    catch (const std::exception& e) {
        // Reported by coeff() through error->all (pair_coeff * * EXTERN):
        // screen and log.lammps, and the exception a driver catches.
        last_load_error_ = e.what();
        load_notices_ = notices.notices();
        release_potential();
        return false;
    }
}

bool PairTadah::load(std::shared_ptr<tadah::LammpsTadah> shared_lt)
{
    /* Adopt a pre-built potential (driver-owned). Sub-object construction
     * already happened in the LammpsTadah ctor; coeff() still runs
     * initialize() + setup_eam_fastpath() on it (both idempotent), so all
     * per-pair_coeff state is refreshed exactly as in the ctx path. */
    lt = std::move(shared_lt);
    lt_file_.clear();
    if (lt) {
      last_load_error_.clear();
      lt_source_ = PotentialSource::DriverLoad;
    } else {
      last_load_error_ = "no potential given (null shared LammpsTadah)";
      lt_source_ = PotentialSource::None;
    }
    return static_cast<bool>(lt);
}

bool PairTadah::load(tadah::core::Context&& ctx)
{
    last_load_error_.clear();
    load_notices_.clear();
    tadah::core::IgnoredKeyNoticeCapture notices;  // see load(argc, argv)
    try {
        lt.reset(new tadah::LammpsTadah(std::move(ctx)));   // zero copies
        lt_source_ = PotentialSource::DriverLoad;
        lt_file_.clear();
        load_notices_ = notices.notices();
        return true;
    }
    catch (const std::exception& e) {
        load_notices_ = notices.notices();
        // Reported by coeff() through error->all (pair_coeff * * EXTERN):
        // screen and log.lammps, and the exception a driver catches.
        last_load_error_ = e.what();
        release_potential();
        return false;
    }
}
