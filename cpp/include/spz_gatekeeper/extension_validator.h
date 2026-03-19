/**
 * SPZ Extension Validator - Abstract Base Class
 *
 * Provides interface for validating SPZ extension data.
 * Each extension type (e.g., Adobe Safe Orbit Camera) implements
 * this interface to validate its specific payload format.
 *
 * Extension validators are used by the SPZ Gatekeeper to verify
 * the legality of TLV (Type-Length-Value) trailer records.
 *
 * @author PuJunhan
 * @copyright Copyright (c) 2026 PuJunhan
 * @license MIT
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include <string>

namespace spz_gatekeeper {

/**
 * SPZ Extension Validator - Abstract Base Class
 * 
 * Provides interface for validating SPZ extension data.
 * Each extension type (e.g., Adobe Safe Orbit Camera) implements
 * this interface to validate its specific payload format.
 */
class SpzExtensionValidator {
public:
  /**
   * Virtual destructor for proper cleanup in derived classes
   * 
   * Ensures that derived class destructors are called correctly
   * when deleting through base class pointer, preventing memory leaks.
   */
  virtual ~SpzExtensionValidator() = default;
  
  /**
   * Get human-readable extension name
   * 
   * @return Extension name (e.g., "Adobe Safe Orbit Camera")
   */
  virtual std::string GetName() const = 0;
  
  /**
   * Get extension type ID
   * 
   * @return 32-bit type ID (e.g., 0xADBE0002)
   */
  virtual uint32_t GetExtensionType() const = 0;
  
  /**
   * Validate extension payload
   * 
   * Checks if the provided payload data conforms to the extension's
   * specific format requirements. Derived classes implement validation
   * logic for their respective extension types.
   * 
   * @param data Pointer to payload data (must not be nullptr if size > 0)
   * @param size Payload size in bytes
   * @param error Output error message (if validation fails, may be nullptr)
   * @return true if valid, false otherwise
   */
  virtual bool Validate(const uint8_t* data, size_t size, std::string* error) const = 0;
};

}  // namespace spz_gatekeeper
