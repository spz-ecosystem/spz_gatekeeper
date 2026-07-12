// SPDX-License-Identifier: MulanPSL-2.0
// Copyright (c) 2026 PuJunhan
/**
 * Adobe Coordinate System Extension Validator
 *
 * Validates Adobe's Coordinate System extension (type 0xADBE0003).
 * This extension records the coordinate system in which Gaussian data is physically stored.
 *
 * Payload Format:
 * - coordinateSystem: uint32_t (4 bytes, little-endian) - Coordinate system enum value
 *
 * Total payload size: 4 bytes
 *
 * Validation Rules:
 * 1. Payload size must be exactly 4 bytes
 * 2. Coordinate system value must be in the range [0, 16]
 *    (0 = UNSPECIFIED, 16 = RBU, the highest defined value in upstream splat-types.h)
 *
 * Note: A value of 0 (UNSPECIFIED) is syntactically valid per this validator, matching
 * upstream behavior where the extension may be present with UNSPECIFIED but produces a warning.
 *
 * @author PuJunhan
 * @copyright Copyright (c) 2026 PuJunhan
 * @license MulanPSL-2.0
 */

#ifndef SPZ_ADOBE_COORDINATE_SYSTEM_VALIDATOR_H_
#define SPZ_ADOBE_COORDINATE_SYSTEM_VALIDATOR_H_

#include "extension_validator.h"

#include <cstdint>
#include <string>

namespace spz_gatekeeper {

static constexpr uint32_t kAdobeCoordinateSystemType = 0xADBE0003u;
static constexpr size_t kCoordinateSystemPayloadSize = 4;
static constexpr uint32_t kMaxCoordinateSystemValue = 16u;

class AdobeCoordinateSystemValidator : public SpzExtensionValidator {
 public:
  std::string GetName() const override {
    return "Adobe Coordinate System";
  }

  uint32_t GetExtensionType() const override {
    return kAdobeCoordinateSystemType;
  }

  bool Validate(const uint8_t* data, size_t size, std::string* error) const override {
    if (size > 0 && data == nullptr) {
      if (error) *error = "Invalid payload pointer: data is null";
      return false;
    }

    if (size != kCoordinateSystemPayloadSize) {
      if (error) {
        *error = "Invalid payload size: expected "
               + std::to_string(kCoordinateSystemPayloadSize)
               + " bytes, got " + std::to_string(size) + " bytes";
      }
      return false;
    }

    const uint32_t coord_value = ReadUint32LE(data);
    if (coord_value > kMaxCoordinateSystemValue) {
      if (error) {
        *error = "Coordinate system value out of range: " + std::to_string(coord_value)
               + " (expected 0-" + std::to_string(kMaxCoordinateSystemValue) + ")";
      }
      return false;
    }

    return true;
  }

 private:
  static uint32_t ReadUint32LE(const uint8_t* data) {
    return static_cast<uint32_t>(data[0])
         | (static_cast<uint32_t>(data[1]) << 8)
         | (static_cast<uint32_t>(data[2]) << 16)
         | (static_cast<uint32_t>(data[3]) << 24);
  }
};

}  // namespace spz_gatekeeper

#endif  // SPZ_ADOBE_COORDINATE_SYSTEM_VALIDATOR_H_
