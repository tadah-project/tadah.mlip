#include <cmath>
#include <tadah/models/descriptors/dm/embed_kernel.h>

namespace tadah {
namespace models {
namespace {
constexpr double pi = 3.14159265358979323846264338327950288;
struct Shape {
  double v = 0, d = 0, dd = 0, step = 0;
};
template <EmbedBasis K, bool Monotone> Shape shape(double x) {
  const double a = std::abs(x), sign = x < 0 ? -1 : 1;
  Shape out;
  if constexpr (K == EmbedBasis::Blip) {
    if (a >= 2) {
      out.step = x < 0 ? 1 : 0;
      return out;
    }
    const double t = 2 - a, t2 = t * t;
    out.v = 0.25 * t2 * t;
    out.d = -0.75 * t2;
    out.dd = 1.5 * t;
    double tail = t2 * t2;
    if (a < 1) {
      const double u = 1 - a, u2 = u * u;
      out.v -= u2 * u;
      out.d += 3 * u2;
      out.dd -= 6 * u;
      tail -= 4 * u2 * u2;
    }
    out.d *= sign;
    if constexpr (Monotone)
      out.step = tail / 24;
  } else if constexpr (K == EmbedBasis::Blip5) {
    if (a >= 3) {
      out.step = x < 0 ? 1 : 0;
      return out;
    }
    auto term = [&](double t, double coef) {
      double t2 = t * t, t3 = t2 * t, t4 = t2 * t2, t5 = t4 * t;
      out.v += coef * t5;
      out.d -= 5 * coef * t4;
      out.dd += 20 * coef * t3;
      if constexpr (Monotone)
        out.step += coef * t5 * t;
    };
    term(3 - a, 1);
    if (a < 2)
      term(2 - a, -6);
    if (a < 1)
      term(1 - a, 15);
    out.v /= 66;
    out.d *= sign / 66;
    out.dd /= 66;
    out.step /= 720;
  } else if constexpr (K == EmbedBasis::WendlandC2) {
    if (a >= 1) {
      out.step = x < 0 ? 1 : 0;
      return out;
    }
    double t = 1 - a, t2 = t * t, t3 = t2 * t, t4 = t2 * t2;
    out.v = t4 * (1 + 4 * a);
    out.d = -20 * a * t3 * sign;
    out.dd = -20 * t2 * (1 - 4 * a);
    if constexpr (Monotone)
      out.step = t4 * t * (1.5 - t);
  } else if constexpr (K == EmbedBasis::WendlandC4) {
    if (a >= 1) {
      out.step = x < 0 ? 1 : 0;
      return out;
    }
    double t = 1 - a, t2 = t * t, t4 = t2 * t2, t5 = t4 * t, t6 = t5 * t;
    out.v = t6 * (1 + 6 * a + (35.0 / 3) * a * a);
    out.d = -(56.0 / 3) * a * (1 + 5 * a) * t5 * sign;
    out.dd = -(56.0 / 3) * t4 * (1 + 4 * a - 35 * a * a);
    if constexpr (Monotone)
      out.step = t6 * t * (72 + t * (-99 + 35 * t)) / 16;
  }
  if (x < 0)
    out.step = 1 - out.step;
  return out;
}
} // namespace

template <EmbedBasis K, bool Monotone, bool Left, bool Parameters>
EmbedJet EmbedEvaluator::kernel(const Atom &a, double q) {
  const double z = q - a.centre;
  EmbedJet out;
  if constexpr (K == EmbedBasis::Knot5) {
    const double sign = Left ? -1 : 1, t = sign * z;
    if (t <= 0)
      return out;
    double t2 = t * t;
    if (a.degree == 3) {
      out.value = t2 * t;
      out.deriv = sign * 3 * t2;
      out.second = 6 * t;
    } else {
      out.value = t2 * t2 * t;
      out.deriv = sign * 5 * t2 * t2;
      out.second = 20 * t2 * t;
    }
  } else if constexpr (K == EmbedBasis::Gaussian) {
    const double g = std::exp(-a.eta * z * z);
    if constexpr (Monotone) {
      out.value = 0.5 * std::erfc(a.sqrt_eta * z);
      out.deriv = -a.sqrt_eta / std::sqrt(pi) * g;
      out.second = -2 * a.eta * z * out.deriv;
      if constexpr (Parameters) {
        out.scale = z * out.deriv / (2 * a.eta);
        out.deriv_scale = (out.deriv + z * out.second) / (2 * a.eta);
      }
    } else {
      out.value = g;
      out.deriv = -2 * a.eta * z * g;
      out.second = (4 * a.eta * a.eta * z * z - 2 * a.eta) * g;
      if constexpr (Parameters) {
        out.scale = -z * z * g;
        out.deriv_scale = -2 * z * g - z * z * out.deriv;
      }
    }
  } else {
    const auto h = shape<K, Monotone>(a.eta * z);
    if constexpr (Monotone) {
      constexpr double inv_area = K == EmbedBasis::Blip         ? 2.0 / 3
                                  : K == EmbedBasis::Blip5      ? 11.0 / 20
                                  : K == EmbedBasis::WendlandC2 ? 1.5
                                                                : 27.0 / 16;
      out.value = h.step;
      out.deriv = -a.eta * inv_area * h.v;
      out.second = -a.eta * a.eta * inv_area * h.d;
      if constexpr (Parameters) {
        out.scale = -z * inv_area * h.v;
        out.deriv_scale = -inv_area * (h.v + a.eta * z * h.d);
      }
    } else {
      out.value = h.v;
      out.deriv = a.eta * h.d;
      out.second = a.eta * a.eta * h.dd;
      if constexpr (Parameters) {
        out.scale = z * h.d;
        out.deriv_scale = h.d + a.eta * z * h.dd;
      }
    }
  }
  if constexpr (Parameters) {
    out.centre = -out.deriv;
    out.deriv_centre = -out.second;
    out.scale *= a.jacobian;
    out.deriv_scale *= a.jacobian;
  }
  return out;
}

EmbedEvaluator::Kernel EmbedEvaluator::select(EmbedBasis k, bool mono,
                                              bool left, bool parameters) {
  // Select policies once per component; never instantiate density x embedding.
#define TADAH_EMBED_SELECT(name)                                               \
  case EmbedBasis::name:                                                       \
    if (mono)                                                                  \
      return parameters ? &kernel<EmbedBasis::name, true, true, true>          \
                        : &kernel<EmbedBasis::name, true, true, false>;        \
    if (parameters)                                                            \
      return left ? &kernel<EmbedBasis::name, false, true, true>               \
                  : &kernel<EmbedBasis::name, false, false, true>;             \
    return left ? &kernel<EmbedBasis::name, false, true, false>                \
                : &kernel<EmbedBasis::name, false, false, false>;
  switch (k) {
    TADAH_EMBED_SELECT(Blip)
    TADAH_EMBED_SELECT(Blip5)
    TADAH_EMBED_SELECT(Gaussian) TADAH_EMBED_SELECT(Knot5)
        TADAH_EMBED_SELECT(WendlandC2) TADAH_EMBED_SELECT(WendlandC4)
  }
#undef TADAH_EMBED_SELECT
  throw std::runtime_error("DM_Embed: unknown kernel");
}

EmbedEvaluator::DensityKernel EmbedEvaluator::select_density(EmbedBasis basis,
                                                             bool monotone) {
#define TADAH_DENSITY_POLICY(name)                                             \
  case EmbedBasis::name:                                                       \
    return monotone                                                            \
               ? &EmbedEvaluator::density_component<EmbedBasis::name, true>    \
               : &EmbedEvaluator::density_component<EmbedBasis::name, false>;
  switch (basis) {
    TADAH_DENSITY_POLICY(Blip)
    TADAH_DENSITY_POLICY(Blip5) TADAH_DENSITY_POLICY(Gaussian)
        TADAH_DENSITY_POLICY(Knot5) TADAH_DENSITY_POLICY(WendlandC2)
            TADAH_DENSITY_POLICY(WendlandC4)
  }
#undef TADAH_DENSITY_POLICY
  throw std::runtime_error("DM_Embed: unknown density policy");
}
EmbedEvaluator::EmbeddingSumKernel
EmbedEvaluator::select_embedding_sum(EmbedBasis basis) {
#define TADAH_EMBEDDING_POLICY(name)                                           \
  case EmbedBasis::name:                                                       \
    return &EmbedEvaluator::embedding_sum_component<EmbedBasis::name>;
  switch (basis) {
    TADAH_EMBEDDING_POLICY(Blip)
    TADAH_EMBEDDING_POLICY(Blip5) TADAH_EMBEDDING_POLICY(Gaussian)
        TADAH_EMBEDDING_POLICY(Knot5) TADAH_EMBEDDING_POLICY(WendlandC2)
            TADAH_EMBEDDING_POLICY(WendlandC4)
  }
#undef TADAH_EMBEDDING_POLICY
  throw std::runtime_error("DM_Embed: unknown embedding sum policy");
}
void EmbedEvaluator::embedding_sum(const core::t_type &weights,
                                   std::size_t offset, double q, double &v,
                                   double &d) const {
  if (!std::isfinite(q))
    throw std::runtime_error("DM_Embed: total density must be finite");
  (this->*embedding_sum_kernel_)(weights, offset, q, v, d);
}
template <EmbedBasis K>
void EmbedEvaluator::embedding_sum_component(const core::t_type &weights,
                                             std::size_t offset, double q,
                                             double &v, double &d) const {
  const auto e = envelope(q);
  v = d = 0;
  for (std::size_t i = 0; i < embedding_.size(); ++i) {
    const auto b = kernel<K, false, false, false>(embedding_[i], q);
    double value = b.value * e.value;
    if (spec_.reference == EmbF0Mode::Zero)
      value -= zero_[i];
    v += weights(offset + i) * value;
    d += weights(offset + i) * (b.deriv * e.value + b.value * e.deriv);
  }
  if (q == 0 && spec_.reference == EmbF0Mode::Zero)
    v = 0;
}

EmbedEvaluator::EmbedEvaluator(const EmbedSpec &spec) : spec_(spec) {
  // The public evaluator validates programmatically created specifications by
  // the same reader as configuration and candidate updates.
  core::Context check;
  check.append_block(embed_block(spec));
  (void)embed_specs(check);
  auto cache = [](const EmbedComponent &c) {
    std::vector<Atom> out;
    out.reserve(c.centres.size());
    for (std::size_t i = 0; i < c.centres.size(); ++i) {
      double eta = embed_eta(c, i), jac = 1;
      if (c.basis != EmbedBasis::Knot5 &&
          c.coordinates == EmbedCoordinates::Widths)
        jac = -(c.basis == EmbedBasis::Gaussian ? 2 : 1) * eta / c.scales[i];
      out.push_back({c.centres[i], eta, std::sqrt(eta), jac,
                     c.degrees.empty() ? 5 : c.degrees[i]});
    }
    return out;
  };
  density_ = cache(spec.density);
  embedding_ = cache(spec.embedding);
  density_kernel_ =
      select(spec.density.basis, spec.form == RhoForm::Monotone, true, false);
  density_component_kernel_ =
      select_density(spec.density.basis, spec.form == RhoForm::Monotone);
  embedding_sum_kernel_ = select_embedding_sum(spec.embedding.basis);
  embedding_kernel_ = select(spec.embedding.basis, false, false, false);
  embedding_parameter_kernel_ =
      select(spec.embedding.basis, false, false, true);
  double radius = spec.radius;
  cutoff_.reset(core::factory<Cut_Base, double &>(
      spec.cutoff == "NONE" ? "Cut_Dummy" : spec.cutoff, radius));
  zero_.reserve(embedding_.size());
  for (const auto &a : embedding_)
    zero_.push_back(embedding_kernel_(a, 0).value * envelope(0).value);
}

EmbedJet EmbedEvaluator::envelope(double q) const {
  EmbedJet out;
  out.value = 1;
  if (spec_.envelope == EmbedEnvelope::Off)
    return out;
  if (q < 0)
    throw std::runtime_error(
        "DM_Embed: enabled envelope requires nonnegative total density");
  double c = spec_.envelope_cutoff, t = q / c;
  if (t >= 1)
    return out;
  if (spec_.envelope == EmbedEnvelope::SinInv) {
    double p = pi / 2, s = std::sin(p * t), v = std::cos(p * t);
    out.value = s;
    out.deriv = p * v / c;
    out.second = -p * p * s / (c * c);
  } else if (spec_.envelope == EmbedEnvelope::Smoothstep2) {
    out.value = t * t * t * (10 + t * (-15 + 6 * t));
    out.deriv = 30 * t * t * (1 - t) * (1 - t) / c;
    out.second = 60 * t * (1 - t) * (1 - 2 * t) / (c * c);
  } else {
    double t2 = t * t, t4 = t2 * t2, u = 1 - t;
    out.value = t4 * t * (126 + t * (-420 + t * (540 + t * (-315 + 70 * t))));
    out.deriv = 630 * t4 * u * u * u * u / c;
    out.second = 2520 * t2 * t * u * u * u * (1 - 2 * t) / (c * c);
  }
  // Derivatives with respect to the envelope matching density.
  out.scale = -q * out.deriv / c;
  out.deriv_scale = -(out.deriv + q * out.second) / c;
  return out;
}

EmbedJet EmbedEvaluator::spatial_cutoff(double q) const {
  EmbedJet out;
  if (q >= spec_.radius)
    return out;
  out.value = cutoff_->calc(q);
  out.deriv = cutoff_->calc_prime(q);
  return out;
}
EmbedJet EmbedEvaluator::density(double q, char field,
                                 std::size_t index) const {
  return (this->*density_component_kernel_)(q, field, index);
}
template <EmbedBasis K, bool Monotone>
EmbedJet EmbedEvaluator::density_component(double q, char field,
                                           std::size_t index) const {
  EmbedJet out;
  if (q >= spec_.radius)
    return out;
  const double fc = cutoff_->calc(q), fp = cutoff_->calc_prime(q);
  double f = fc, df = fp;
  if (field == 'r') {
    f = cutoff_->calc_drcut(q);
    df = cutoff_->calc_prime_drcut(q);
  }
  const bool single = field == 'c' || field == 's' || field == 'a';
  if (single && index >= density_.size())
    throw std::out_of_range("DM_Embed density parameter index");
  const bool parameters = field == 'c' || field == 's';
  for (std::size_t i = single ? index : 0,
                   end = single ? index + 1 : density_.size();
       i < end; ++i) {
    auto j = parameters ? kernel<K, Monotone, true, true>(density_[i], q)
                        : kernel<K, Monotone, true, false>(density_[i], q);
    double v = j.value, d = j.deriv;
    if (field == 'c' || field == 's' || field == 'a') {
      if (i != index)
        continue;
      if (field == 'c') {
        v = j.centre;
        d = j.deriv_centre;
      }
      if (field == 's') {
        v = j.scale;
        d = j.deriv_scale;
      }
    }
    double a = field == 'a' ? 1 : spec_.density.amplitudes[i];
    out.value += a * v;
    out.deriv += a * d;
    out.second += a * j.second;
  }
  out.second = f * out.second + 2 * df * out.deriv +
               cutoff_->calc_double_prime(q) * out.value;
  out.deriv = f * out.deriv + df * out.value;
  out.value *= f;
  if (field != ' ')
    out.second = 0;
  return out;
}

EmbedJet EmbedEvaluator::embedding(std::size_t i, double q, char field,
                                   std::size_t index) const {
  if (!std::isfinite(q))
    throw std::runtime_error("DM_Embed: total density must be finite");
  const auto kernel = (field == 'c' || field == 's')
                          ? embedding_parameter_kernel_
                          : embedding_kernel_;
  auto b = kernel(embedding_.at(i), q);
  auto e = envelope(q);
  EmbedJet out;
  if (field == 'c' || field == 's') {
    if (i != index)
      return out;
    double v = field == 'c' ? b.centre : b.scale,
           d = field == 'c' ? b.deriv_centre : b.deriv_scale;
    out.value = v * e.value;
    out.deriv = d * e.value + v * e.deriv;
    if (spec_.reference == EmbF0Mode::Zero) {
      const auto z = embedding_parameter_kernel_(embedding_[i], 0);
      out.value -= (field == 'c' ? z.centre : z.scale) * envelope(0).value;
    }
  } else if (field == 'e') {
    out.value = b.value * e.scale;
    out.deriv = b.deriv * e.scale + b.value * e.deriv_scale;
  } else {
    out.value = b.value * e.value;
    out.deriv = b.deriv * e.value + b.value * e.deriv;
    out.second =
        b.second * e.value + 2 * b.deriv * e.deriv + b.value * e.second;
    if (spec_.reference == EmbF0Mode::Zero)
      out.value -= zero_[i];
  }
  if (q == 0 && spec_.reference == EmbF0Mode::Zero)
    out.value = 0;
  return out;
}
} // namespace models
} // namespace tadah
