/**
 * Adobe Safe Orbit Camera Extension Validator
 *
 * Validates Adobe's Safe Orbit Camera extension (type 0xADBE0002).
 * This extension restricts camera orbit elevation and radius for safe viewing.
 *
 * Payload Format:
 * - minElevation: float32 (4 bytes) - Minimum elevation angle in radians [-pi/2, pi/2]
 * - maxElevation: float32 (4 bytes) - Maximum elevation angle in radians [-pi/2, pi/2]
 * - minRadius: float32 (4 bytes) - Minimum orbit radius [0, +inf)
 *
 * Total payload size: 12 bytes
 *
 * Validation Rules:
 * 1. Payload size must be exactly 12 bytes
 * 2. minElevation must be in [-pi/2, pi/2] radians
 * 3. maxElevation must be in [-pi/2, pi/2] radians
 * 4. minElevation <= maxElevation
 * 5. minRadius must be >= 0
 *
 * @author PuJunhan
 * @copyright Copyright (c) 2026 PuJunhan
 * @license MIT
 */

#ifndef SPZ_ADOBE_SAFE_ORBIT_CAMERA_VALIDATOR_H_
#define SPZ_ADOBE_SAFE_ORBIT_CAMERA_VALIDATOR_H_

#include "extension_validator.h"

#include <cmath>
#include <cstdint>
#include <cstring>

namespace spz_gatekeeper {

static constexpr uint32_t kAdobeSafeOrbitCameraType = 0xADBE0002u;
static constexpr size_t kSafeOrbitCameraPayloadSize = 12;
static constexpr float kSafeOrbitElevationLimitRadians = 1.57079632679f;

class AdobeSafeOrbitCameraValidator : public SpzExtensionValidator {
public:
  std::string GetName() const override {
    return "Adobe Safe Orbit Camera";
  }

  uint32_t GetExtensionType() const override {
    return kAdobeSafeOrbitCameraType;
  }

  bool Validate(const uint8_t* data, size_t size, std::string* error) const override {
    if (size > 0 && data == nullptr) {
      if (error) *error = "Invalid payload pointer: data is null";
      return false;
    }

    if (size != kSafeOrbitCameraPayloadSize) {
      if (error) {
        *error = "Invalid payload size: expected "
               + std::to_string(kSafeOrbitCameraPayloadSize)
               + " bytes, got " + std::to_string(size) + " bytes";
      }
      return false;
    }

    float minElevation = ReadFloatLE(data);
    float maxElevation = ReadFloatLE(data + 4);
    float minRadius = ReadFloatLE(data + 8);

    if (std::isnan(minElevation) || minElevation < -kSafeOrbitElevationLimitRadians
        || minElevation > kSafeOrbitElevationLimitRadians) {
      if (error) {
        *error = "minElevation out of range: " + std::to_string(minElevation)
               + " radians (expected [-pi/2, pi/2])";
      }
      return false;
    }

    if (std::isnan(maxElevation) || maxElevation < -kSafeOrbitElevationLimitRadians
        || maxElevation > kSafeOrbitElevationLimitRadians) {
      if (error) {
        *error = "maxElevation out of range: " + std::to_string(maxElevation)
               + " radians (expected [-pi/2, pi/2])";
      }
      return false;
    }

    if (minElevation > maxElevation) {
      if (error) {
        *error = "minElevation (" + std::to_string(minElevation)
               + ") > maxElevation (" + std::to_string(maxElevation) + ")";
      }
      return false;
    }

    if (std::isnan(minRadius) || minRadius < 0.0f) {
      if (error) {
        *error = "minRadius out of range: " + std::to_string(minRadius)
               + " (expected >= 0)";
      }
      return false;
    }

    return true;
  }

private:
  static float ReadFloatLE(const uint8_t* data) {
    float value;
    std::memcpy(&value, data, sizeof(float));
    return value;
  }
};

}  // namespace spz_gatekeeper

#endif  // SPZ_ADOBE_SAFE_ORBIT_CAMERA_VALIDATOR_H_
