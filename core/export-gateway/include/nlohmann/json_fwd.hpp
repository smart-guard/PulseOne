/**
 * @file json_fwd.hpp
 * @brief Forward declaration of nlohmann::json using the same inline namespace
 *        macros as json.hpp (v3.11.2), to avoid "ambiguous json" conflicts.
 *
 * Usage: #include <nlohmann/json_fwd.hpp>  (in headers instead of json.hpp)
 *        #include <nlohmann/json.hpp>       (in .cpp files only)
 */
#pragma once

// ── Reproduce the version/ABI macros from json.hpp ───────────────────────────
#ifndef NLOHMANN_JSON_VERSION_MAJOR
#define NLOHMANN_JSON_VERSION_MAJOR 3
#define NLOHMANN_JSON_VERSION_MINOR 11
#define NLOHMANN_JSON_VERSION_PATCH 2
#endif

#ifndef NLOHMANN_JSON_ABI_TAG_DIAGNOSTICS
#define NLOHMANN_JSON_ABI_TAG_DIAGNOSTICS
#endif
#ifndef NLOHMANN_JSON_ABI_TAG_LEGACY_DISCARDED_VALUE_COMPARISON
#define NLOHMANN_JSON_ABI_TAG_LEGACY_DISCARDED_VALUE_COMPARISON
#endif
#ifndef NLOHMANN_JSON_NAMESPACE_NO_VERSION
#define NLOHMANN_JSON_NAMESPACE_NO_VERSION 0
#endif

#define NLOHMANN_JSON_ABI_TAGS_CONCAT_EX(a, b) json_abi##a##b
#define NLOHMANN_JSON_ABI_TAGS_CONCAT(a, b)                                    \
  NLOHMANN_JSON_ABI_TAGS_CONCAT_EX(a, b)
#define NLOHMANN_JSON_ABI_TAGS                                                 \
  NLOHMANN_JSON_ABI_TAGS_CONCAT(                                               \
      NLOHMANN_JSON_ABI_TAG_DIAGNOSTICS,                                       \
      NLOHMANN_JSON_ABI_TAG_LEGACY_DISCARDED_VALUE_COMPARISON)

#define NLOHMANN_JSON_NAMESPACE_VERSION_CONCAT_EX(a, b, c) _v##a##_##b##_##c
#define NLOHMANN_JSON_NAMESPACE_VERSION_CONCAT(a, b, c)                        \
  NLOHMANN_JSON_NAMESPACE_VERSION_CONCAT_EX(a, b, c)
#if NLOHMANN_JSON_NAMESPACE_NO_VERSION
#define NLOHMANN_JSON_NAMESPACE_VERSION
#else
#define NLOHMANN_JSON_NAMESPACE_VERSION                                        \
  NLOHMANN_JSON_NAMESPACE_VERSION_CONCAT(NLOHMANN_JSON_VERSION_MAJOR,          \
                                         NLOHMANN_JSON_VERSION_MINOR,          \
                                         NLOHMANN_JSON_VERSION_PATCH)
#endif

#define NLOHMANN_JSON_NAMESPACE_CONCAT_EX(a, b) a##b
#define NLOHMANN_JSON_NAMESPACE_CONCAT(a, b)                                   \
  NLOHMANN_JSON_NAMESPACE_CONCAT_EX(a, b)

#ifndef NLOHMANN_JSON_NAMESPACE_BEGIN
#define NLOHMANN_JSON_NAMESPACE_BEGIN                                          \
  namespace nlohmann {                                                         \
  inline namespace NLOHMANN_JSON_NAMESPACE_CONCAT(                             \
      NLOHMANN_JSON_ABI_TAGS, NLOHMANN_JSON_NAMESPACE_VERSION) {
#endif
#ifndef NLOHMANN_JSON_NAMESPACE_END
#define NLOHMANN_JSON_NAMESPACE_END                                            \
  }                                                                            \
  } // namespace nlohmann
#endif
// ─────────────────────────────────────────────────────────────────────────────

// Forward-declare basic_json and the json alias inside the SAME inline ns
NLOHMANN_JSON_NAMESPACE_BEGIN

template <template <typename, typename, typename...> class ObjectType,
          template <typename, typename...> class ArrayType, class StringType,
          class BooleanType, class NumberIntegerType, class NumberUnsignedType,
          class NumberFloatType, template <typename> class AllocatorType,
          template <typename, typename = void> class JSONSerializer,
          class BinaryType, class CustomBaseClass>
class basic_json;

using json = basic_json<std::map, std::vector, std::string, bool, std::int64_t,
                        std::uint64_t, double, std::allocator, void,
                        std::vector<std::uint8_t>, void>;

NLOHMANN_JSON_NAMESPACE_END

// Convenience alias at the nlohmann level (mirrors what json.hpp exposes)
namespace nlohmann {
using json = NLOHMANN_JSON_NAMESPACE_CONCAT(NLOHMANN_JSON_ABI_TAGS,
                                            NLOHMANN_JSON_NAMESPACE_VERSION)::
    basic_json<std::map, std::vector, std::string, bool, std::int64_t,
               std::uint64_t, double, std::allocator, void,
               std::vector<std::uint8_t>, void>;
} // namespace nlohmann
