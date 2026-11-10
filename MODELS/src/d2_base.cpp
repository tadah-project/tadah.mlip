#include <tadah/models/descriptors/d2/d2_base.h>
#include <utility>
namespace tadah {
namespace models {


std::vector<std::string> D2_Base::get_init_atoms(const tadah::core::Context &c) {
  if (!c.contains("TYPE2B")) {
    std::cerr << "Warning: TYPE2B key was not found. "
      "Atom pairs must be initialized manually." << std::endl;
    return {};
  }
  std::vector<std::string> init_atoms(c.size("TYPE2B"));
  c.get("TYPE2B", init_atoms);
  return element_tokens("TYPE2B", std::move(init_atoms));
}

} // namespace models
} // namespace tadah

