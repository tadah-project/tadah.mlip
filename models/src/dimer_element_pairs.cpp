#include <tadah/models/descriptors/dimer_element_pairs.h>

#include <cstddef>
#include <stdexcept>
#include <string>

namespace tadah {
namespace models {

namespace {

/* `key` and its values, as the configuration line reads. */
std::string key_line(const tadah::core::Context &c, const std::string &key) {
  std::string line = key;
  for (std::size_t i = 0; i < c.size(key); ++i)
    line += " " + c.get<std::string>(key, i);
  return line;
}

void require_every_pair(const tadah::core::Context &c, const D_Base &d,
                        const std::string &init_key,
                        const std::string &type_key) {
  if (!c.get<bool>(init_key) || d.size() == 0) return;
  if (d.applies_to_every_element_pair()) return;
  throw std::runtime_error(
      "DIMER evaluates every atom as Z=1; declare TYPE2B/TYPEMB with * * "
      "element pairs (" + key_line(c, type_key) +
      " does not apply to every element pair)");
}

} // namespace

void require_dimer_element_pairs(const tadah::core::Context &c,
                                 const D_Base &d2, const D_Base &dm) {
  if (!c.get<bool>("DIMER", 0)) return;
  require_every_pair(c, d2, "INIT2B", "TYPE2B");
  require_every_pair(c, dm, "INITMB", "TYPEMB");
}

} // namespace models
} // namespace tadah
