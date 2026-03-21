/**
 * SPZ Extension Spec Registry
 *
 * Thread-safe singleton registry for SPZ extension specification metadata.
 */

#ifndef SPZ_EXTENSION_SPEC_REGISTRY_H_
#define SPZ_EXTENSION_SPEC_REGISTRY_H_

#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace spz_gatekeeper {

struct ExtensionSpec {
  std::uint32_t type = 0;
  std::uint16_t vendor_id = 0;
  std::uint16_t extension_id = 0;
  std::string vendor_name;
  std::string extension_name;
  std::string category;
  std::string status;
  std::string spec_url;
  std::string short_description;
  std::uint32_t min_spz_version = 0;
  bool requires_has_extensions_flag = false;
};

class ExtensionSpecRegistry {
 public:
  static ExtensionSpecRegistry& Instance();

  void RegisterSpec(const ExtensionSpec& spec);
  std::optional<ExtensionSpec> GetSpec(std::uint32_t type) const;
  bool HasSpec(std::uint32_t type) const;
  std::vector<ExtensionSpec> ListSpecs() const;
  std::size_t SpecCount() const;

  ExtensionSpecRegistry(const ExtensionSpecRegistry&) = delete;
  ExtensionSpecRegistry& operator=(const ExtensionSpecRegistry&) = delete;

 private:
  ExtensionSpecRegistry() = default;
  ~ExtensionSpecRegistry() = default;

  mutable std::mutex mutex_;
  std::map<std::uint32_t, ExtensionSpec> specs_;
};

}  // namespace spz_gatekeeper

#endif  // SPZ_EXTENSION_SPEC_REGISTRY_H_
