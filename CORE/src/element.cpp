#include <tadah/core/element.h>
#include <tadah/core/periodic_table.h>

namespace tadah {
namespace core {

void Element::check(const std::string &symbol, const int Z) {
  if (symbol.size() > 2)
    throw std::runtime_error("Wrong symbol: "+symbol);
  if (Z < 1)
    throw std::runtime_error("Atomic number Z < 1");
  if (Z > 118)
    throw std::runtime_error("Atomic number Z > 118");
}

Element::Element(): Z(0) {}
Element::Element(const std::string &symbol_) {
  *this = PeriodicTable::find_by_symbol(symbol_);
}
Element::Element(const int Z_) {
  *this = PeriodicTable::find_by_Z(Z_);
}
Element::Element(const std::string &symbol_, 
                 const int Z_, bool check_)
{
  if (check_) check(symbol_, Z_);
  symbol=symbol_;
  Z=Z_;
}
bool Element::operator==(const Element& other) const {
  return this->symbol == other.symbol
  && this->Z == other.Z;
}
bool Element::operator<(const Element& other) const {
  return this->Z < other.Z;
}
bool Element::operator>(const Element& other) const {
  return this->Z > other.Z;
}

} // namespace core
} // namespace tadah

