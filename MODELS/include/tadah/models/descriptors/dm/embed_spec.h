#pragma once
#include <map>
#include <string>
#include <tadah/core/context.h>
#include <tadah/models/descriptors/dm/embf0.h>
#include <tadah/models/descriptors/dm/rhoform.h>
#include <vector>

namespace tadah {
namespace models {

enum class EmbedBasis { Blip, Blip5, Gaussian, Knot5, WendlandC2, WendlandC4 };
enum class EmbedEnvelope { Off, SinInv, Smoothstep2, Smoothstep4 };
enum class EmbedCoordinates { Eta, Widths };

struct EmbedComponent {
  EmbedBasis basis = EmbedBasis::Blip;
  EmbedCoordinates coordinates = EmbedCoordinates::Eta;
  std::vector<double> centres, scales, amplitudes;
  std::vector<int> degrees;
};

struct EmbedSpec {
  std::string id;
  std::vector<std::string> pairs;
  EmbedComponent density, embedding;
  RhoForm form = RhoForm::Signed;
  EmbF0Mode reference = EmbF0Mode::Zero;
  std::string cutoff = "NONE";
  double radius = 0;
  EmbedEnvelope envelope = EmbedEnvelope::Off;
  double envelope_cutoff = 0;
};

std::vector<EmbedSpec> embed_specs(const core::Context &);
core::ConfigBlock embed_block(const EmbedSpec &);
const char *embed_basis_name(EmbedBasis);
EmbedBasis embed_basis_from_name(const std::string &);
double embed_support(EmbedBasis); // infinity for Gaussian; left knots: zero
double embed_eta(const EmbedComponent &, std::size_t);
std::vector<double> expand_embed_grid(const std::vector<std::string> &,
                                      const std::string &label);

/** Resolve component grids and radii before constructing consumers. New
 * declarations activate the many-body term; an explicit INITMB false is an
 * error. TYPEMB @id references retain declaration order when absent. */
void resolve_embed_blocks(core::Context &);
std::vector<core::Context> embed_contexts(const core::Context &);

/** A continuous parameter in its declared coordinates. A binding owns no
 * references into a candidate: read/write always address the supplied context.
 */
struct EmbedParameterBinding {
  std::string path, owner, units;
  bool positive = false;
  std::vector<double> values;
  double eta_jacobian(const EmbedComponent &, std::size_t) const;
  void write(core::Context &, std::size_t index, double value) const;
};
std::vector<EmbedParameterBinding> embed_bindings(const core::Context &);
EmbedParameterBinding embed_binding(const core::Context &, const std::string &);
void write_embed_coordinates(
    core::Context &, const std::map<std::string, std::vector<double>> &);

} // namespace models
} // namespace tadah
