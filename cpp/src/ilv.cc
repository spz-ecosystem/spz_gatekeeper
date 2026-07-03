// SPDX-License-Identifier: MulanPSL-2.0
// Copyright (c) 2026 PuJunhan

#include "spz_gatekeeper/ilv.h"

namespace spz_gatekeeper {

static std::uint32_t ReadU32LE(const std::vector<std::uint8_t>& data, std::size_t off) {
  return static_cast<std::uint32_t>(data[off]) |
         (static_cast<std::uint32_t>(data[off + 1]) << 8) |
         (static_cast<std::uint32_t>(data[off + 2]) << 16) |
         (static_cast<std::uint32_t>(data[off + 3]) << 24);
}

// T09b (R3): ParseIlvRecords ILV byte parse core logic is reused by ParseHeaderZoneExtensions (spz.cc).
// They share the [u32 type][u32 byteLength][payload] format; only input (vector vs raw pointer)
// and boundary conditions (trailer offset vs header zone [32..tocByteOffset)) differ.
// Global TLV->ILV rename completed; this function uses the ILV naming convention.
IlvParseResult ParseIlvRecords(const std::vector<std::uint8_t>& data, std::size_t offset) {
  IlvParseResult r;
  if (offset > data.size()) {
    r.ok = false;
    r.error = "offset out of range";
    return r;
  }

  std::size_t off = offset;
  while (off < data.size()) {
    std::size_t remaining = data.size() - off;
    if (remaining < 8) {
      r.ok = false;
      r.error = "truncated ILV header";
      return r;
    }
    std::uint32_t type = ReadU32LE(data, off);
    std::uint32_t len = ReadU32LE(data, off + 4);
    std::size_t value_off = off + 8;
    if (static_cast<std::size_t>(len) > data.size() - value_off) {
      r.ok = false;
      r.error = "truncated ILV value - insufficient data";
      return r;
    }

    IlvRecord rec;
    rec.type = type;
    rec.length = len;
    rec.offset = off;
    rec.value_data = len == 0 ? nullptr : data.data() + value_off;

    r.records.push_back(rec);

    off = value_off + static_cast<std::size_t>(len);
  }

  r.ok = true;
  return r;
}

IlvParseResult ParseHeaderZoneExtensions(const std::uint8_t* ext_data, std::size_t ext_size) {
  IlvParseResult r;

  if (ext_size == 0) {
    r.ok = true;
    return r;
  }

  if (ext_size < 8) {
    r.ok = false;
    r.error = "truncated header zone extension (less than 8 bytes)";
    return r;
  }

  std::size_t off = 0;
  while (off < ext_size) {
    std::size_t remaining = ext_size - off;
    if (remaining < 8) {
      r.ok = false;
      r.error = "truncated ILV header in header zone";
      return r;
    }

    std::uint32_t type = static_cast<std::uint32_t>(ext_data[off]) |
                         (static_cast<std::uint32_t>(ext_data[off + 1]) << 8) |
                         (static_cast<std::uint32_t>(ext_data[off + 2]) << 16) |
                         (static_cast<std::uint32_t>(ext_data[off + 3]) << 24);
    std::uint32_t len = static_cast<std::uint32_t>(ext_data[off + 4]) |
                        (static_cast<std::uint32_t>(ext_data[off + 5]) << 8) |
                        (static_cast<std::uint32_t>(ext_data[off + 6]) << 16) |
                        (static_cast<std::uint32_t>(ext_data[off + 7]) << 24);
    std::size_t value_off = off + 8;

    if (static_cast<std::size_t>(len) > ext_size - value_off) {
      r.ok = false;
      r.error = "truncated ILV value in header zone";
      return r;
    }

    IlvRecord rec;
    rec.type = type;
    rec.length = len;
    rec.offset = off;
    rec.value_data = len == 0 ? nullptr : ext_data + value_off;

    r.records.push_back(rec);

    off = value_off + static_cast<std::size_t>(len);
  }

  r.ok = true;
  return r;
}

}  // namespace spz_gatekeeper
