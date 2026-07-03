// SPDX-License-Identifier: MulanPSL-2.0
// Copyright (c) 2026 PuJunhan
/**
 * SPZ Gatekeeper - SPZ Format Validator
 *
 * Core SPZ validation interface for L2 verification:
 * - SPZ header validation (magic, version, flags)
 * - Official has-extensions flag (0x02) checking
 * - ILV trailer parsing and validation
 * - Backward compatibility verification
 *
 * This module validates compatibility with the official SPZ packed format
 * and the post-payload vendor extension stream used by
 * `KHR_gaussian_splatting_compression_spz_2` assets.
 *
 * @author PuJunhan
 * @copyright Copyright (c) 2026 PuJunhan
 * @license MulanPSL-2.0
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "report.h"

namespace spz_gatekeeper {

// Official SPZ extension-presence flag.
// When set (0x02), indicates that extension data is present after the base payload.
static constexpr std::uint8_t kFlagAntialiased = 0x01;
static constexpr std::uint8_t kFlagHasExtensions = 0x02;

/// Options for SPZ blob inspection.
struct SpzInspectOptions {
  /// Strict mode: if true, ILV parse failures are errors; otherwise warnings.
  bool strict = true;
};

/// Inspect a raw SPZ blob.
///
/// Performs L2 validation:
/// - Decompresses gzip data
/// - Parses SPZ header (magic, version, flags, etc.)
/// - Validates base payload size
/// - Parses ILV trailer if the official has-extensions flag is set
///
/// @param raw_spz Raw SPZ bytes (gzip or ZSTD compressed)
/// @param opt Inspection options (strict/non-strict mode)
/// @param where Location description for error reporting
/// @return GateReport with validation results (errors, warnings, L2 info)
GateReport InspectSpzBlob(const std::vector<std::uint8_t>& raw_spz, const SpzInspectOptions& opt,
                          const std::string& where);

/// Compute SH quantization epsilon for given bit-width.
/// Formula: epsilon = 2.0 / ((1 << bits) - 1)
/// Returns +inf when bits <= 0.
double sh_epsilon(int bits);

}  // namespace spz_gatekeeper
