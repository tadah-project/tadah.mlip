#pragma once
#include <tadah/models/descriptors/dm/embed_spec.h>
namespace tadah {
namespace models {
EmbedSpec embed_spec_from_ream(const core::Context &,
                               const std::string &id = "eam");
void replace_ream_with_embed(core::Context &, const std::string &id = "eam");
core::Context edit_embed_potential(const core::Context &, bool density,
                                   double centre, bool remove,
                                   std::optional<double> eta = {},
                                   std::optional<int> degree = {});
} // namespace models
} // namespace tadah
