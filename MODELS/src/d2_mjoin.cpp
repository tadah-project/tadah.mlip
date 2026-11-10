#include <algorithm>
#include <cstddef>
#include <tadah/models/descriptors/d2/d2_mjoin.h>
namespace tadah {
namespace models {

D2_mJoin::D2_mJoin(const tadah::core::Context& c)             // new, const version
  : D2_mJoin::D2_mJoin(const_cast<tadah::core::Context&>(c)) {}      // delegates to old one

D2_mJoin::D2_mJoin() {
  init();
}
D2_mJoin::~D2_mJoin() {
  for (auto d : ds) if (d) delete d;
}

D2_mJoin::D2_mJoin(tadah::core::Context &c) : D2_Base(c) {
  init();
  contexts = parse_context(c, "TYPE2B");
  expand_grids(c, contexts, "TYPE2B");

  for (auto &c1 : contexts) {
    ds.push_back(tadah::core::factory<D2_Base, tadah::core::Context&>(c1.get<std::string>("TYPE2B"), c1));
    s += ds.back()->size();
  }
}

void D2_mJoin::calc_aed(const int Zi, const int Zj, const double rij,
                        const double rij_sq, tadah::core::aed_type &aed, const double scale) const {
  for (auto d : ds) {
    d->calc_aed(Zi, Zj, rij, rij_sq, aed, scale);
  }
}

void D2_mJoin::calc_dXijdri(const int Zi, const int Zj, const double rij,
                            const double rij_sq, tadah::core::fd_type &fd_ij, const double scale) const {
  for (auto d : ds) {
    d->calc_dXijdri(Zi, Zj, rij, rij_sq, fd_ij, scale);
  }
}

void D2_mJoin::calc_all(const int Zi, const int Zj, const double rij,
                        const double rij_sq, tadah::core::aed_type &aed, tadah::core::fd_type &fd_ij, const double scale) const {
  for (auto d : ds) {
    d->calc_all(Zi, Zj, rij, rij_sq, aed, fd_ij, scale);
  }
}

bool D2_mJoin::applies_to_every_element_pair() const {
  for (auto d : ds)
    if (!d->applies_to_every_element_pair()) return false;
  return true;
}

std::string D2_mJoin::label() const {
  return lab;
}

void D2_mJoin::set_fidx(size_t fidx_) {
  // Its own first index too: DescriptorsCalc expands the two-body force
  // rows from get_fidx().
  fidx = fidx_;
  ds[0]->set_fidx(fidx_);
  size_t s = 0;
  for (size_t i = 1; i < ds.size(); ++i) {
    s += ds[i - 1]->size();
    ds[i]->set_fidx(fidx_ + s);
  }
}

void D2_mJoin::init() {
  keys.push_back("TYPE2B");
  keys.push_back("RCTYPE2B");
  keys.push_back("RCUT2B");
  nparams=std::numeric_limits<size_t>::max();
}
bool D2_mJoin::reads_watom() const {
  for (auto d : ds)
    if (d->reads_watom()) return true;
  return false;
}
bool D2_mJoin::depends_on(const std::string &theta_name, int arg_pos) const {
  if (std::find(keys.begin(), keys.end(), theta_name) != keys.end())
    return true;
  for (auto d : ds)
    if (d->depends_on(theta_name, arg_pos)) return true;
  return false;
}
std::string D2_mJoin::cutoff_fixed_by(std::size_t i) const {
  return i < ds.size() ? ds[i]->cutoff_fixed_by(0) : std::string();
}

} // namespace models
} // namespace tadah

