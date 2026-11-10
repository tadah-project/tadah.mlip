#include <tadah/tadah.lammps/lammps_tadah.h>
#include <tadah/core/core_user_config_parser.h>
#include <tadah/core/intercept_layout.h>
#include <tadah/core/periodic_table.h>
#include <tadah/core/registry.h>
#include <tadah/models/descriptors/dimer_element_pairs.h>
#include <tadah/models/functions/weights_fit.h>

#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>

using namespace tadah;

/*-------------------------------------------*/
/* helper that does the heavy initialisation */
/*-------------------------------------------*/
void build_sub_objects(LammpsTadah& self)
{
    self.norm = self.c.get<bool>("NORM") &&
                !( self.c("MODEL")[1] == "BF_Linear" ||
                   self.c("MODEL")[1] == "Kern_Linear" );

    self.cutoff_max     = self.c.get<double>("RCUTMAX");
    self.cutoff_max_sq  = std::pow(self.cutoff_max, 2);

    self.linear = self.c("MODEL")[1] == "BF_Linear" ||
                  self.c("MODEL")[1] == "Kern_Linear";

    self.dimer           = self.c.get<bool  >( "DIMER", 0 );
    self.dimer_r         = self.c.get<double>( "DIMER", 1 );
    self.dimer_bond_bool = self.c.get<bool  >( "DIMER", 2 );

    self.init2b = self.c.get<bool>("INIT2B");
    self.initmb = self.c.get<bool>("INITMB");

    /* PairTadah::compute_dimers evaluates every atom as Z=1: a descriptor
     * declared for specific element pairs would apply to no pair. */
    models::require_dimer_element_pairs(self.c, *self.S.d2b, *self.S.dmb);

    /* ---- create Function_Base ---- */
    auto fb_ptr = core::factory<models::Function_Base, core::Context&>(
                      self.c.get<std::string>("MODEL", 1), self.c);
    if (!fb_ptr)
        throw std::runtime_error("cannot build Function_Base for model "
                                 + self.c.get<std::string>("MODEL", 1));
    self.fb.reset(fb_ptr);

    /* ---- create MD model ---- */
    auto model_ptr = tadah::core::factory<
                         md::M_MD_Base,
                         models::Function_Base&, core::Context&>(
                         self.c.get<std::string>("MODEL", 0),
                         *self.fb, self.c);
    if (!model_ptr)
        throw std::runtime_error("cannot build MD model "
                                 + self.c.get<std::string>("MODEL", 0));
    self.model.reset(model_ptr);
}

/*-------------------------------------------*/
/* main constructor – takes Context by move  */
/*-------------------------------------------*/
LammpsTadah::LammpsTadah(core::Context&& ctx)
    : c(std::move(ctx)),
      embf0_declared(c.has_stored("EMBF0")),   // before S materialises it
      S(c)                     // DC_Selector may need the Context early
{
    build_sub_objects(*this);
}

/*-------------------------------------------*/
/* copy-in constructor                       */
/*-------------------------------------------*/
LammpsTadah::LammpsTadah(const core::Context& ctx)
    : c(ctx),                 // one copy
      embf0_declared(c.has_stored("EMBF0")),   // before S materialises it
      S(c)
{
    build_sub_objects(*this);
}

/*-------------------------------------------*/
/* command-line constructor                  */
/*-------------------------------------------*/
LammpsTadah::LammpsTadah(int argc, char** argv)
    : LammpsTadah(
          core::CoreUserConfigParser()
              .parse(core::to_string(argv[2])) )
{
}


void LammpsTadah::initialize(int narg, char **arg)
{
  // Map LAMMPS atom types to weight factors
  // e.g. pair_coaeff * * pot.tadah Ti Nb NULL
  // watom.resize(narg-3); // this includes NULL values
  // One entry per LAMMPS type, 0 for a NULL type. Every entry is set here:
  // the same object is initialised by every pair_coeff * * EXTERN line that
  // adopts it, which may map the types differently each time (resize() kept
  // a NULL type's element of the previous mapping).
  Z.assign(narg-3, 0);
  for (int i=3; i<narg; ++i) {
    std::string symbol=arg[i];
    if (symbol=="NULL") continue;
    size_t j=0;
    for (j=0; j<c.size("ATOM"); ++j) {
      if (c.get<std::string>("ATOM", j)==symbol) {
        Z[i-3] = core::PeriodicTable::find_by_symbol(symbol).Z;
        break;
      }
    }
    if (j==c.size("ATOM"))
      throw std::runtime_error("Symbol not in potential file: "+symbol);
  }

  c.addInternal("DSIZE",S.descriptor_size());
  bias = S.bias;
  rsize = S.rsize; // size of the density array (rhoi + rhoip)

  // The one check of a potential's fit, here where the descriptor size is
  // known (DC_Selector) and on every path pair_coeff takes (a file, EXTERN):
  // the predictors' dot products run over the weights, so refuse WEIGHTS,
  // basis vectors (a kernel's: SBASIS vectors stored in BASIS; M_KRR_Core
  // also refuses WEIGHTS without a basis or with another number of vectors)
  // or NMEAN/NSTDEV (a non-linear NORM model) that do not fit this
  // descriptor, e.g. a multi-species BIAS potential trained before 1.6.0,
  // with one intercept column shared by all species, and a linear NORM true
  // potential without BIAS, instead of computing with misaligned weights.
  // pair_coeff reports it through error->all.
  models::require_weights_fit(c, S.descriptor_size(), *fb);

  // Precompute per-LAMMPS-atom-type intercept lookup tables. With per-species
  // BIAS, every atom of LAMMPS type t contributes a fixed constant
  // bias_per_atom_type[t] to its energy; the descriptor's intercept column for
  // that species is species_intercept_idx_per_type[t]. Probing the model with
  // a one-hot AED is identical to looking up w[k] in the linear case but works
  // for any model that implements epredict.
  const size_t ntypes = Z.size();
  species_intercept_idx_per_type.assign(ntypes, -1);
  bias_per_atom_type.assign(ntypes, 0.0);

  if (bias > 0) {
    const auto col = core::intercept_columns_by_Z(c);
    core::aed_type probe(S.descriptor_size());
    for (size_t t = 0; t < ntypes; ++t) {
      const int z = Z[t];
      if (z == 0) continue; // NULL slot — leave defaults
      const int k = col[z];
      if (k < 0) continue;
      species_intercept_idx_per_type[t] = k;
      probe.set_zero();
      probe(k) = 1.0;
      bias_per_atom_type[t] = model->epredict(probe);
    }
  }
}


int LammpsTadah::pack_reverse_linear(int n, int first, double *buf)
{
  int m = 0;
  int last = first + n;
  for (int i = first; i < last; i++) {
    for (size_t x = 0; x < S.dmb->rhoi_size(); x++)
      buf[m++] = rhos(x,i);
    // pack two-body only, as many-body will be computed from rho
    // which is being packed hence we do now comm mb-aeds
    for (size_t x = bias; x < S.d2b->size()+bias; x++)
      buf[m++] = aeds(x,i);
  }
  return m;
}
void LammpsTadah::unpack_reverse_linear(int n, int *list, double *buf)
{
  int m = 0;
  for (int i = 0; i < n; i++) {
    int j = list[i];
    for (size_t x = 0; x < S.dmb->rhoi_size(); x++)
      rhos(x,j) += buf[m++];
    for (size_t x = bias; x < S.d2b->size()+bias; x++)
      aeds(x,j) += buf[m++];
  }
}
// Pack the derivative of the embedding func
int LammpsTadah::pack_forward_linear(int n, int *list, double *buf)
{
  int m = 0;
  for (int i = 0; i < n; i++) {
    int j = list[i];
    for (size_t x = S.dmb->rhoi_size(); x < S.dmb->rhoi_size()+S.dmb->rhoip_size(); x++)
      buf[m++] = rhos(x,j);
  }
  return m;
}
// Unpack the derivative of the embedding func
void LammpsTadah::unpack_forward_linear(int n, int first, double *buf) 
{
  int m = 0;
  int last = first + n;
  for (int i = first; i < last; i++) {
    for (size_t x = S.dmb->rhoi_size(); x < S.dmb->rhoi_size()+S.dmb->rhoip_size(); x++)
      rhos(x,i) = buf[m++];
  }
}
// NONLINEAR
int LammpsTadah::pack_reverse_nonlinear(int n, int first, double *buf)
{
  int i,m,last;

  m = 0;
  last = first + n;
  for (i = first; i < last; i++) {
    for (size_t x = 0; x < S.dmb->rhoi_size(); x++)
      buf[m++] = rhos(x,i);
    // pack two-body only, as many-body will be computed from rho
    // which is packed above hence we do now comm mb-aeds
    for (size_t x = bias; x < S.d2b->size()+bias; x++)
      buf[m++] = aeds(x,i);
  }
  return m;
}
void LammpsTadah::unpack_reverse_nonlinear(int n, int *list, double *buf)
{
  int i,j,m;

  m = 0;
  for (i = 0; i < n; i++) {
    j = list[i];
    for (size_t x = 0; x < S.dmb->rhoi_size(); x++)
      rhos(x,j) += buf[m++];
    for (size_t x = bias; x < S.d2b->size()+bias; x++)
      aeds(x,j) += buf[m++];
  }
}
int LammpsTadah::pack_forward_nonlinear(int n, int *list, double *buf)
{
  int i,j,m;

  m = 0;
  for (i = 0; i < n; i++) {
    j = list[i];
    for (size_t x = S.dmb->rhoi_size(); x < S.dmb->rhoi_size()+S.dmb->rhoip_size(); x++)
      buf[m++] = rhos(x,j);
    for (size_t x = bias; x < S.d2b->size()+S.dmb->size()+bias; x++)
      buf[m++] = aeds(x,j);
  }
  return m;
}
void LammpsTadah::unpack_forward_nonlinear(int n, int first, double *buf) 
{
  int i,m,last;

  m = 0;
  last = first + n;
  for (i = first; i < last; i++) {
    for (size_t x = S.dmb->rhoi_size(); x < S.dmb->rhoi_size()+S.dmb->rhoip_size(); x++)
      rhos(x,i) = buf[m++];
    for (size_t x = bias; x < S.d2b->size()+S.dmb->size()+bias; x++)
      aeds(x,i) = buf[m++];
  }
}
