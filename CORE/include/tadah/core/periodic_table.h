#ifndef TADAH_CORE_PERIODIC_TABLE_H
#define TADAH_CORE_PERIODIC_TABLE_H

#include <string>
#include <map>
#include <tadah/core/element.h>

namespace tadah {
namespace core {

struct PeriodicTable {
  static std::map<std::string, Element> symbol;
  static std::map<std::string, Element> name;
  static std::map<int, Element> Z;

  static std::map<int/*Z*/, double> masses;  // separate map for atomic masses

  static void initialize();

  static const Element &find_by_name(const std::string &s);
  static const Element &find_by_symbol(const std::string &s);
  static const Element &find_by_Z(int z);

  static double get_mass(int Z);
  static double get_mass(const Element &e);

  static void add(const Element& e);
  static void add(const std::string& symbol_, const std::string& name_, int Z_);

  private:
  static void fill();

  static void init_masses();
};

} // namespace core
} // namespace tadah
#endif
