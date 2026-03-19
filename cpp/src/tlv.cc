// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PuJunhan

#include "spz_gatekeeper/tlv.h"

namespace spz_gatekeeper {

static std::uint32_t ReadU32LE(const std::vector<std::uint8_t>& data, std::size_t off) {
  return static_cast<std::uint32_t>(data[off]) |
         (static_cast<std::uint32_t>(data[off + 1]) << 8) |
         (static_cast<std::uint32_t>(data[off + 2]) << 16) |
         (static_cast<std::uint32_t>(data[off + 3]) << 24);
}

TlvParseResult ParseTlvTrailer(const std::vector<std::uint8_t>& data, std::size_t offset) {
  TlvParseResult r;
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
      r.error = "truncated TLV header";
      return r;
    }
    std::uint32_t type = ReadU32LE(data, off);
    std::uint32_t len = ReadU32LE(data, off + 4);
    std::size_t value_off = off + 8;
    if (static_cast<std::size_t>(len) > data.size() - value_off) {
      r.ok = false;
      r.error = "truncated TLV value - insufficient data";
      return r;
    }

    TlvRecord rec;
    rec.type = type;
    rec.length = len;
    rec.offset = off;
    // 解析阶段只绑定原始 buffer，避免为每条 TLV 额外分配和 memcpy。
    rec.value_data = len == 0 ? nullptr : data.data() + value_off;

    r.records.push_back(rec);

    off = value_off + static_cast<std::size_t>(len);
  }

  r.ok = true;
  return r;
}

}  // namespace spz_gatekeeper
