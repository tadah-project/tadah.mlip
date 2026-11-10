#ifndef TADAH_CORE_ELEMENT_H
#define TADAH_CORE_ELEMENT_H
#include <iostream>
#include <functional>
#include <string>

namespace tadah {
namespace core {

struct Element {
  private:
    void check(const std::string &symbol, const int Z);

    friend std::ostream& operator<<(std::ostream& os, const Element& element)
    {
      os << "Symbol: " << element.symbol << std::endl;
      os << "Atomic Number: " << element.Z << std::endl;
      return os;
    }

  public:
    std::string symbol;
    int Z;
    Element();
    Element(const std::string &symbol_);
    Element(const int Z_);
    Element(const std::string &symbol_, const int Z_, bool check_=true);
    bool operator==(const Element& other) const;
    bool operator<(const Element& other) const;
    bool operator>(const Element& other) const;
};
} // namespace core
} // namespace tadah

// Now that Element is fully declared, define the std::hash specialization:
namespace std {
    template <>
    struct hash<tadah::core::Element>
    {
        // Implement the hash however you want:
        // Typically, hash just calls a standard hash on a unique data member.
        // Here, hashing by atomic number only:
        size_t operator()(const tadah::core::Element &e) const noexcept
        {
            return std::hash<int>()(e.Z);

        }
    };
}
#endif
