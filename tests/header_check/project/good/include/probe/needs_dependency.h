#pragma once
#include <dep/dep.h>
namespace probe {
inline int twice_dep() { return 2 * dep::value(); }
}  // namespace probe
