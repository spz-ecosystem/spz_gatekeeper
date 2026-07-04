// SPDX-License-Identifier: MulanPSL-2.0
// Copyright (c) 2026 PuJunhan
/**
 * ILV (Indexed-Length-Value) Trailer Parser
 *
 * Parses the SPZ vendor extension stream records used by
 * `KHR_gaussian_splatting_compression_spz_2` payloads:
 * - Type: uint32_t (4 bytes, little-endian)
 * - Length: uint32_t (4 bytes, little-endian)
 * - Value: uint8_t[length] (variable)
 *
 * ILV format enables skippable extension data that maintains
 * backward compatibility with original SPZ tools.
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

/// ILV (Indexed-Length-Value) parse result.
///
/// ILV layout (little-endian):
///   u32 type, u32 length, then `length` bytes value.
/// Unknown `type` must be skippable for forward compatibility.
struct IlvParseResult {
  /// Parse success flag.
  bool ok = false;
  
  /// Error message (if ok == false).
  std::string error;
  
  /// Parsed ILV records (empty if parse failed).
  std::vector<IlvRecord> records;
};

/// Parse ILV trailer from SPZ decompressed data.
///
/// The trailer appears after the standard SPZ fields (positions/alphas/colors/scales/rotations/sh).
/// Each ILV record has format: [type:u32][length:u32][value:length bytes]
///
/// 说明：解析阶段不会为每条 record 复制 payload；`IlvRecord::ValueView()` 直接指向输入 buffer，
/// 如果调用方需要独立副本，可调用 `IlvRecord::CopyValue()`。
///
/// @param data Decompressed SPZ data (starting from trailer offset)
/// @param offset Byte offset where trailer begins
/// @return IlvParseResult with parsed records or error message
IlvParseResult ParseIlvRecords(const std::vector<std::uint8_t>& data, std::size_t offset);

/// Parse header-zone ILV extensions from raw SPZ bytes (v4 path).
///
/// In SPZ v4, extensions live in the header zone — `raw[32..tocByteOffset)` —
/// as plaintext ILV records.  No ZSTD decompression is needed.
/// The ILV byte format ([u32 type][u32 byteLength][payload]) is identical to
/// `ParseIlvRecords`, only the data source and boundary conditions differ.
///
/// @param ext_data Pointer to raw byte at offset 32 (past the 32-byte header).
/// @param ext_size Number of bytes in the header zone (tocByteOffset - 32).
/// @return IlvParseResult — value_data pointers reference `ext_data` (zero-copy);
///         caller must keep the backing buffer alive for the records' lifetime.
IlvParseResult ParseHeaderZoneExtensions(const std::uint8_t* ext_data, std::size_t ext_size);

}  // namespace spz_gatekeeper
