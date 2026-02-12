#ifndef BACNET_MINMAX_FIX_H
#define BACNET_MINMAX_FIX_H

#ifdef __cplusplus
#include <algorithm>
#include <limits>

// The "Brilliant No-Op Macro" Trick:
// 1. We include C++ headers first so they see the real min/max functions.
// 2. We undefine any existing macros.
// 3. We bring std::min/max into the global namespace.
// 4. We define min/max as macros that replace themselves.
// This prevents bacnet/config.h from defining "min(a,b)" macros
// which break C++ headers, but still allows "min(a,b)" in BACnet
// C files (compiled as C++) to work as function calls.

#undef min
#undef max

namespace bacnet_fix {
template <typename T, typename U>
auto min(T a, U b) -> decltype(a < b ? a : b) {
  return (a < b) ? a : b;
}
template <typename T, typename U>
auto max(T a, U b) -> decltype(a > b ? a : b) {
  return (a > b) ? a : b;
}
} // namespace bacnet_fix

using bacnet_fix::max;
using bacnet_fix::min;

#define min min
#define max max

#endif

#endif // BACNET_MINMAX_FIX_H
