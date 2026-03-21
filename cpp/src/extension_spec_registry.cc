#include "spz_gatekeeper/extension_spec_registry.h"

namespace spz_gatekeeper {

ExtensionSpecRegistry& ExtensionSpecRegistry::Instance() {
  static ExtensionSpecRegistry instance;
  return instance;
}

void ExtensionSpecRegistry::RegisterSpec(const ExtensionSpec& spec) {
  std::lock_guard<std::mutex> lock(mutex_);
  specs_[spec.type] = spec;
}

std::optional<ExtensionSpec> ExtensionSpecRegistry::GetSpec(std::uint32_t type) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = specs_.find(type);
  if (it == specs_.end()) {
    return std::nullopt;
  }
  return it->second;
}

bool ExtensionSpecRegistry::HasSpec(std::uint32_t type) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return specs_.find(type) != specs_.end();
}

std::vector<ExtensionSpec> ExtensionSpecRegistry::ListSpecs() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<ExtensionSpec> specs;
  specs.reserve(specs_.size());
  for (const auto& [_, spec] : specs_) {
    specs.push_back(spec);
  }
  return specs;
}

std::size_t ExtensionSpecRegistry::SpecCount() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return specs_.size();
}

}  // namespace spz_gatekeeper
