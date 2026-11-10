#include <tadah/models/descriptors/dm/dm_base.h>
#include <utility>
namespace tadah {
namespace models {

void DM_Base::set_rfidx(size_t rfidx_) { rfidx=rfidx_; }
size_t DM_Base::get_rfidx() const { return rfidx; }
void DM_Base::set_drfidx(size_t drfidx_) { drfidx=drfidx_; }
size_t DM_Base::get_drfidx() const { return drfidx; }

std::vector<std::string> DM_Base::get_init_atoms(const tadah::core::Context &c) {
  if (!c.contains("TYPEMB")) {
    std::cerr << "Warning: TYPEMB key was not found. "
      "Atom pairs must be initialized manually." << std::endl;
    return {};
  }
  std::vector<std::string> init_atoms(c.size("TYPEMB"));
  c.get("TYPEMB", init_atoms);
  return element_tokens("TYPEMB", std::move(init_atoms));
}

} // namespace models
} // namespace tadah

