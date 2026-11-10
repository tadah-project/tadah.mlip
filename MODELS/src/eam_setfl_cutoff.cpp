#include <tadah/models/descriptors/eam_setfl_cutoff.h>

#include <tadah/core/number_format.h>

#include <cstddef>
#include <stdexcept>

namespace tadah {
namespace models {

void require_setfl_cutoff(const tadah::core::Context &c,
                          const std::string &rcut_key,
                          const std::string &label, const EAMSetflIO &eam) {
  using tadah::core::format_roundtrip;
  const double rc = eam.rcut();
  for (std::size_t i = 0; i < c.size(rcut_key); ++i) {
    const double v = c.get<double>(rcut_key, i);
    if (v == rc) continue;
    throw std::runtime_error(
        label + ": " + rcut_key + " " + format_roundtrip(v) +
        " differs from the cutoff " + format_roundtrip(rc) +
        " of the setfl file '" + eam.path() + "' (SETFL); the " + rcut_key +
        " of " + label + " is the file's cutoff, " + format_roundtrip(rc) +
        ".");
  }
}

std::string setfl_cutoff_clause(const std::string &label,
                                const EAMSetflIO &eam) {
  return label + " takes the cutoff " +
         tadah::core::format_roundtrip(eam.rcut()) + " of its setfl file '" +
         eam.path() + "' (SETFL)";
}

}  // namespace models
}  // namespace tadah
