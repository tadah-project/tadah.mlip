#pragma once
#include <memory>
#include <tadah/core/core_types.h>
#include <tadah/models/cutoffs.h>
#include <tadah/models/descriptors/dm/embed_spec.h>

namespace tadah {
namespace models {

/** Base values and first two q derivatives. Parameter evaluations expose
 * value and deriv; their second field is unused and zero. */
struct EmbedJet {
  double value = 0, deriv = 0, second = 0;
  double centre = 0, scale = 0, deriv_centre = 0, deriv_scale = 0;
};

class EmbedEvaluator {
  struct Atom {
    double centre = 0, eta = 1, sqrt_eta = 1, jacobian = 1;
    int degree = 5;
  };
  using Kernel = EmbedJet (*)(const Atom &, double);
  EmbedSpec spec_;
  std::vector<Atom> density_, embedding_;
  Kernel density_kernel_ = nullptr, embedding_kernel_ = nullptr;
  Kernel embedding_parameter_kernel_ = nullptr;
  using DensityKernel = EmbedJet (EmbedEvaluator::*)(double, char,
                                                     std::size_t) const;
  using EmbeddingSumKernel = void (EmbedEvaluator::*)(const core::t_type &,
                                                      std::size_t, double,
                                                      double &, double &) const;
  DensityKernel density_component_kernel_ = nullptr;
  EmbeddingSumKernel embedding_sum_kernel_ = nullptr;
  template <EmbedBasis K, bool Monotone>
  EmbedJet density_component(double, char, std::size_t) const;
  template <EmbedBasis K>
  void embedding_sum_component(const core::t_type &, std::size_t, double,
                               double &, double &) const;
  static DensityKernel select_density(EmbedBasis, bool);
  static EmbeddingSumKernel select_embedding_sum(EmbedBasis);
  std::unique_ptr<Cut_Base> cutoff_;
  std::vector<double> zero_;
  EmbedJet envelope(double) const;
  template <EmbedBasis K, bool Monotone, bool Left, bool Parameters>
  static EmbedJet kernel(const Atom &, double);
  static Kernel select(EmbedBasis, bool, bool, bool);

public:
  explicit EmbedEvaluator(const EmbedSpec &);
  const EmbedSpec &spec() const { return spec_; }
  /** density, optionally differentiated with respect to one atom's centre,
   * scale or amplitude, or the spatial radius (field 'r'). */
  EmbedJet density(double q, char field = ' ', std::size_t index = 0) const;
  EmbedJet density_atom(std::size_t index, double q) const {
    return density_kernel_(density_.at(index), q);
  }
  /** The spatial cutoff the density is tapered with: f_c(q) and f_c'(q)
   * (CUTOFF TYPE at RADIUS; NONE is 1 inside RADIUS), 0 from RADIUS on. */
  EmbedJet spatial_cutoff(double q) const;
  EmbedJet embedding(std::size_t, double q, char field = ' ',
                     std::size_t index = 0) const;
  /** Weight-collapsed embedding value and first derivative. The family is
   * dispatched once for the complete sum, including one envelope evaluation. */
  void embedding_sum(const core::t_type &, std::size_t, double, double &,
                     double &) const;
  double raw_zero(std::size_t i) const { return zero_.at(i); }
};
} // namespace models
} // namespace tadah
