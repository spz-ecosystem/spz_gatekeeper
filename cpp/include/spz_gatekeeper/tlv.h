/**
 * TLV (Type-Length-Value) Trailer Parser
 *
 * Parses the SPZ vendor extension stream records used by
 * `KHR_gaussian_splatting_compression_spz_2` payloads:
 * - Type: uint32_t (4 bytes, little-endian)
 * - Length: uint32_t (4 bytes, little-endian)
 * - Value: uint8_t[length] (variable)
 *
 * TLV format enables skippable extension data that maintains
 * backward compatibility with original SPZ tools.
 *
 * @author PuJunhan
 * @copyright Copyright (c) 2026 PuJunhan
 * @license MIT
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "report.h"

namespace spz_gatekeeper {

/// TLV (Type-Length-Value) parse result.
///
/// TLV layout (little-endian):
///   u32 type, u32 length, then `length` bytes value.
/// Unknown `type` must be skippable for forward compatibility.
struct TlvParseResult {
  /// Parse success flag.
  bool ok = false;
  
  /// Error message (if ok == false).
  std::string error;
  
  /// Parsed TLV records (empty if parse failed).
  std::vector<TlvRecord> records;
};

/// Parse TLV trailer from SPZ decompressed data.
///
/// The trailer appears after the standard SPZ fields (positions/alphas/colors/scales/rotations/sh).
/// Each TLV record has format: [type:u32][length:u32][value:length bytes]
///
/// 说明：解析阶段不会为每条 record 复制 payload；`TlvRecord::ValueView()` 直接指向输入 buffer，
/// 如果调用方需要独立副本，可调用 `TlvRecord::CopyValue()`。
///
/// @param data Decompressed SPZ data (starting from trailer offset)
/// @param offset Byte offset where trailer begins
/// @return TlvParseResult with parsed records or error message
TlvParseResult ParseTlvTrailer(const std::vector<std::uint8_t>& data, std::size_t offset);

}  // namespace spz_gatekeeper
