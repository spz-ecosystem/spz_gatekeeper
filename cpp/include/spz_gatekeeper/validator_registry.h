/**
 * SPZ Extension Validator Registry
 *
 * Singleton registry for managing extension validators.
 * Provides runtime registration and lookup of validators by extension type.
 *
 * Features:
 * - Thread-safe singleton pattern
 * - Runtime validator registration
 * - Type-based validator lookup
 * - Automatic registration via static instances
 *
 * Usage:
 * @code
 * // Register a validator
 * auto validator = std::make_shared<AdobeSafeOrbitCameraValidator>();
 * ExtensionValidatorRegistry::Instance().RegisterValidator(
 *   kAdobeSafeOrbitCameraType, validator);
 *
 * // Lookup a validator
 * auto found = ExtensionValidatorRegistry::Instance().GetValidator(type);
 * if (found) {
 *   // Validate extension data
 *   std::string error;
 *   if (!found->Validate(data, size, &error)) {
 *     // Handle validation failure
 *   }
 * }
 * @endcode
 *
 * @author PuJunhan
 * @copyright Copyright (c) 2026 PuJunhan
 * @license MIT
 */

#ifndef SPZ_EXTENSION_VALIDATOR_REGISTRY_H_
#define SPZ_EXTENSION_VALIDATOR_REGISTRY_H_

#include "extension_validator.h"

#include <map>
#include <memory>
#include <vector>
#include <mutex>
#include <stdexcept>

namespace spz_gatekeeper {

/**
 * Extension Validator Registry (Singleton)
 *
 * Thread-safe singleton for managing extension validators.
 * Uses Meyers' singleton pattern for guaranteed thread-safe initialization.
 */
class ExtensionValidatorRegistry {
public:
  /**
   * Get singleton instance
   * @return Reference to singleton instance
   */
  static ExtensionValidatorRegistry& Instance() {
    static ExtensionValidatorRegistry instance;
    return instance;
  }
  
  /**
   * Register a validator for a specific extension type
   * @param type Extension type ID (e.g., 0xADBE0002)
   * @param validator Shared pointer to validator instance
   * @throws std::invalid_argument if validator is null
   */
  void RegisterValidator(uint32_t type, 
                         std::shared_ptr<SpzExtensionValidator> validator) {
    if (!validator) {
      throw std::invalid_argument("Validator cannot be null");
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    validators_[type] = validator;
  }
  
  /**
   * Get validator for a specific extension type
   * @param type Extension type ID to lookup
   * @return Shared pointer to validator, or nullptr if not found
   */
  std::shared_ptr<SpzExtensionValidator> GetValidator(uint32_t type) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = validators_.find(type);
    if (it != validators_.end()) {
      return it->second;
    }
    return nullptr;
  }
  
  /**
   * List all registered extension types
   * @return Vector of registered extension type IDs
   */
  std::vector<uint32_t> ListRegisteredValidators() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<uint32_t> types;
    types.reserve(validators_.size());
    for (const auto& pair : validators_) {
      types.push_back(pair.first);
    }
    return types;
  }
  
  /**
   * Check if a validator is registered for a type
   * @param type Extension type ID to check
   * @return true if registered, false otherwise
   */
  bool HasValidator(uint32_t type) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return validators_.find(type) != validators_.end();
  }
  
  /**
   * Get number of registered validators
   * @return Number of validators in registry
   */
  size_t ValidatorCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return validators_.size();
  }
  
  // Delete copy constructor and assignment operator
  ExtensionValidatorRegistry(const ExtensionValidatorRegistry&) = delete;
  ExtensionValidatorRegistry& operator=(const ExtensionValidatorRegistry&) = delete;
  
private:
  /**
   * Private constructor for singleton pattern
   */
  ExtensionValidatorRegistry() = default;
  
  /**
   * Destructor
   */
  ~ExtensionValidatorRegistry() = default;
  
  /// Map of extension type to validator
  mutable std::mutex mutex_;
  
  /// Validators indexed by extension type
  std::map<uint32_t, std::shared_ptr<SpzExtensionValidator>> validators_;
};

/**
 * Automatic validator registration helper
 *
 * Use this template to automatically register validators at static
 * initialization time.
 *
 * Example:
 * @code
 * static RegisterValidator<AdobeSafeOrbitCameraValidator> g_adobe_registrar;
 * @endcode
 */
template<typename T>
class RegisterValidator {
public:
  RegisterValidator() {
    auto validator = std::make_shared<T>();
    type_ = validator->GetExtensionType();
    ExtensionValidatorRegistry::Instance().RegisterValidator(type_, validator);
  }
  
  uint32_t GetExtensionType() const { return type_; }
  
private:
  uint32_t type_;
};

}  // namespace spz_gatekeeper

#endif  // SPZ_EXTENSION_VALIDATOR_REGISTRY_H_
