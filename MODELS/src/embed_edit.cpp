#include <algorithm>
#include <cmath>
#include <tadah/core/logger.h>
#include <tadah/models/dc_selector.h>
#include <tadah/models/descriptors/dm/ream_density.h>
#include <tadah/models/eam/embed_edit.h>
#include <tadah/models/linear_regressor.h>
#include <tadah/models/nnls.h>

namespace tadah {
namespace models {
EmbedSpec embed_spec_from_ream(const core::Context &c, const std::string &id) {
  auto density = ReamDensity::from_context(c);
  EmbedSpec s;
  s.id = id;
  s.form = density.form;
  s.density.basis = embed_basis_from_name(basis_name(density.basis));
  s.embedding.basis = s.density.basis;
  s.density.centres = density.crho;
  s.density.scales = density.srho;
  s.density.amplitudes = density.arho;
  for (double d : density.rho_deg)
    s.density.degrees.push_back(static_cast<int>(d));
  D_Base::get_grid(c, "CEMBFUNC", s.embedding.centres);
  if (c.has_stored("SEMBFUNC"))
    D_Base::get_grid(c, "SEMBFUNC", s.embedding.scales);
  if (c.has_stored("KDEGEMB"))
    for (const auto &v : c("KDEGEMB"))
      s.embedding.degrees.push_back(std::stoi(v));
  if (density.basis == BasisKind::Knot5) {
    if (s.density.degrees.empty())
      s.density.degrees.assign(s.density.centres.size(), 5);
    if (s.embedding.degrees.empty())
      s.embedding.degrees.assign(s.embedding.centres.size(), 5);
  }
  DM_REAM descriptor(c);
  s.pairs = descriptor.get_init_atoms(c);
  s.reference = embf0_from_context(c);
  s.radius = c.get<double>("RCUTMB");
  s.cutoff = c.get<std::string>("RCTYPEMB");
  if (s.cutoff == "Cut_Dummy")
    s.cutoff = "NONE";
  return s;
}
void replace_ream_with_embed(core::Context &c, const std::string &id) {
  auto s = embed_spec_from_ream(c, id);
  for (const char *key :
       {"TYPEMB", "RCUTMB", "RCTYPEMB", "CGRIDMB", "SGRIDMB", "AMPGRIDMB",
        "CEMBFUNC", "SEMBFUNC", "KDEGMB", "KDEGEMB", "RHOFORM", "EMBF0"})
    c.remove(key);
  c.replace_blocks({embed_block(s)});
  resolve_embed_blocks(c);
}
core::Context edit_embed_potential(const core::Context &source, bool density,
                                   double centre, bool remove,
                                   std::optional<double> eta,
                                   std::optional<int> degree) {
  auto specs = embed_specs(source);
  if (specs.size() != 1)
    throw std::runtime_error(
        "DM_Embed edit requires one scalar descriptor block");
  auto out = source;
  DC_Selector selector(out);
  if (out.get<bool>("BIAS"))
    throw std::runtime_error("DM_Embed refit edit requires BIAS false");
  auto s = specs[0];
  const EmbedEvaluator original(s);
  auto &component = density ? s.density : s.embedding;
  const std::size_t offset = selector.d2b->size();
  std::vector<double> weights(out.size("WEIGHTS"));
  out.get("WEIGHTS", weights);
  std::vector<double> coefficients =
      density ? component.amplitudes
              : std::vector<double>(weights.begin() + offset, weights.end());
  if (coefficients.size() != component.centres.size())
    throw std::runtime_error("DM_Embed edit: coefficient count mismatch");
  if (remove) {
    if (eta || degree)
      throw std::runtime_error("DM_Embed removal accepts neither ETA nor DEG");
    if (component.centres.size() < 2)
      throw std::runtime_error(
          "DM_Embed edit cannot remove the final basis function");
    auto it =
        std::min_element(component.centres.begin(), component.centres.end(),
                         [&](double a, double b) {
                           return std::abs(a - centre) < std::abs(b - centre);
                         });
    std::size_t index = it - component.centres.begin();
    component.centres.erase(it);
    coefficients.erase(coefficients.begin() + index);
    if (!component.scales.empty())
      component.scales.erase(component.scales.begin() + index);
    if (!component.degrees.empty())
      component.degrees.erase(component.degrees.begin() + index);
    if (density)
      component.amplitudes = coefficients;
    EmbedEvaluator current(s);
    const std::size_t rows = 2048, cols = coefficients.size();
    core::phi_type phi(rows + cols, cols);
    phi.set_zero();
    core::t_type y(rows + cols);
    y.set_zero();
    std::vector<double> a(rows * cols), b(rows);
    double hi = density ? s.radius : 1.0;
    if (!density) {
      const auto &embedding = specs[0].embedding;
      hi = std::max(hi, s.envelope_cutoff);
      for (std::size_t i = 0; i < embedding.centres.size(); ++i)
        hi = std::max(hi, embedding.centres[i] +
                              (embedding.basis == EmbedBasis::Knot5 ? 1
                               : embedding.basis == EmbedBasis::Gaussian
                                   ? 3 / std::sqrt(embed_eta(embedding, i))
                                   : embed_support(embedding.basis) /
                                         embed_eta(embedding, i)));
    }
    const double lo = density ? 0.15 * s.radius : 0;
    for (std::size_t i = 0; i < rows; ++i) {
      const double q = lo + (hi - lo) * i / (rows - 1);
      double target = original.density(q).value;
      if (!density) {
        target = 0;
        for (std::size_t m = 0; m < specs[0].embedding.centres.size(); ++m)
          target += weights[offset + m] * original.embedding(m, q).value;
      }
      y(i) = b[i] = target;
      for (std::size_t j = 0; j < cols; ++j)
        phi(i, j) = a[j * rows + i] = density ? current.density(q, 'a', j).value
                                              : current.embedding(j, q).value;
    }
    if (density && s.form != RhoForm::Signed) {
      auto result = nnls(a.data(), rows, cols, b.data(), coefficients.data());
      if (!result.converged)
        throw std::runtime_error(
            "DM_Embed edit compensation NNLS did not converge");
    } else {
      core::Context config;
      config.add("OALGO", 1);
      config.add("LAMBDA", 0);
      core::t_type w;
      LinearRegressor::train(config, phi, y, w);
      for (std::size_t i = 0; i < cols; ++i)
        coefficients[i] = w(i);
    }
    double squared = 0, maximum = 0;
    for (std::size_t i = 0; i < rows; ++i) {
      double residual = -b[i];
      for (std::size_t j = 0; j < cols; ++j)
        residual += a[j * rows + i] * coefficients[j];
      squared += residual * residual;
      maximum = std::max(maximum, std::abs(residual));
    }
    core::Logger::instance().log(
        core::LogLevel::INFO, "DM_Embed edit compensation: window [", lo, ", ",
        hi, "] RMSE=", std::sqrt(squared / rows), " maxabs=", maximum);
  } else {
    component.centres.push_back(centre);
    coefficients.push_back(0);
    if (component.basis == EmbedBasis::Knot5) {
      if (eta)
        throw std::runtime_error("DM_Embed knot edit accepts no ETA");
      component.degrees.push_back(degree.value_or(5));
    } else {
      if (degree)
        throw std::runtime_error("DM_Embed bump edit accepts no DEG");
      double scale = component.scales.back();
      if (eta) {
        if (!(*eta > 0) || !std::isfinite(*eta))
          throw std::runtime_error(
              "DM_Embed edit ETA must be finite and positive");
        scale = component.coordinates == EmbedCoordinates::Eta ? *eta
                : component.basis == EmbedBasis::Gaussian ? 1 / std::sqrt(*eta)
                                                          : 1 / (*eta);
      }
      component.scales.push_back(scale);
    }
  }
  if (density)
    component.amplitudes = coefficients;
  else {
    weights.resize(offset);
    weights.insert(weights.end(), coefficients.begin(), coefficients.end());
  }
  out.replace_blocks({embed_block(s)});
  resolve_embed_blocks(out);
  out.remove("WEIGHTS");
  for (double w : weights)
    out.add("WEIGHTS", w);
  out.remove("FIXINDEX");
  out.remove("FIXWEIGHT");
  out.add("FIXINDEX", "1-" + std::to_string(weights.size()));
  for (double w : weights)
    out.add("FIXWEIGHT", w);
  DC_Selector validate(out);
  return out;
}
} // namespace models
} // namespace tadah
