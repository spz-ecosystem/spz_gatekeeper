// SPDX-License-Identifier: MulanPSL-2.0
// Copyright (c) 2026 PuJunhan
//
// Specification reference for SPZ extension 0xADBE0003 (Adobe Coordinate System).
// Based on the Adobe extension proposal tracked in Niantic splat-extensions.
//
// Payload format:
//   - 4 bytes little-endian uint32_t
//   - Value range: 0 .. 16 (matches CoordinateSystem enum)
//   - Value 0 (UNSPECIFIED) is technically valid but likely a writer bug;
//     upstream consumers fall back to RUB and emit a warning.
//
// This file exists so that ExtensionSpec::spec_url in the gatekeeper registry
// points to a real on-disk document (L4 cross-review requirement).

#ifndef SPZ_GATEKEEPER_EXTENSIONS_CC_COORDINATE_SYSTEM_ADOBE_H_
#define SPZ_GATEKEEPER_EXTENSIONS_CC_COORDINATE_SYSTEM_ADOBE_H_

namespace spz_gatekeeper {
namespace ext {

// Placeholder enum matching upstream CoordinateSystem values used by the
// validator. The validator only checks that the payload is a uint32_t <= 16.
enum class CoordinateSystemAdobe : uint32_t {
  UNSPECIFIED = 0,
  // Values 1..16 are defined by the upstream Adobe/Niantic specification.
  MAX_VALID = 16
};

}  // namespace ext
}  // namespace spz_gatekeeper

#endif  // SPZ_GATEKEEPER_EXTENSIONS_CC_COORDINATE_SYSTEM_ADOBE_H_
