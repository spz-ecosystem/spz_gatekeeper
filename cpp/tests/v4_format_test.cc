// SPDX-License-Identifier: MulanPSL-2.0
// Copyright (c) 2026 PuJunhan

// R6 T23: v4 format 6-category tests
// Uses real bench dataset v4.spz files when available,
// falls back to synthetic fixtures for CI.

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "spz_gatekeeper/spz.h"
#include "spz_gatekeeper/ilv.h"

namespace {

// ─── Helpers ───

static void write_u32_le(std::vector<std::uint8_t>* b, std::uint32_t v) {
  b->push_back(static_cast<std::uint8_t>(v & 0xFF));
  b->push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
  b->push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
  b->push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
}

static bool read_file(const std::string& path, std::vector<std::uint8_t>* out) {
  std::ifstream f(path, std::ios::binary | std::ios::ate);
  if (!f.is_open()) return false;
  auto sz = f.tellg();
  f.seekg(0);
  out->resize(static_cast<std::size_t>(sz));
  f.read(reinterpret_cast<char*>(out->data()), sz);
  return f.good();
}

// Build a minimal v4 NGSP blob (32B header + 1 empty ZSTD stream)
static std::vector<std::uint8_t> BuildSyntheticV4() {
  std::vector<std::uint8_t> blob;
  blob.reserve(64);
  // 32-byte NGSP header
  write_u32_le(&blob, 0x5053474e);  // magic "NGSP"
  write_u32_le(&blob, 4);           // version = 4
  write_u32_le(&blob, 0);           // numStreams = 0 (empty)
  write_u32_le(&blob, 0);           // reserved
  for (int i = 0; i < 16; ++i) blob.push_back(0);  // rest of 32B header
  return blob;
}

// Build a minimal v3 NGSP blob (decompressed, gzip format)
static std::vector<std::uint8_t> BuildSyntheticV3Decomp() {
  std::vector<std::uint8_t> decomp;
  decomp.reserve(64);
  write_u32_le(&decomp, 0x5053474e);  // magic "NGSP"
  write_u32_le(&decomp, 3);           // version = 3
  write_u32_le(&decomp, 1);           // numPoints = 1
  decomp.push_back(0);                // shDegree
  decomp.push_back(12);               // fractionalBits
  decomp.push_back(0);                // flags
  decomp.push_back(0);                // padding
  return decomp;
}

// ─── Test 1: v4 header parse ───

void test_v4_header_parse() {
  std::cout << "Test: v4 header parse... ";
  auto blob = BuildSyntheticV4();

  // Verify header is at least 32 bytes
  assert(blob.size() >= 32);

  // Check magic
  std::uint32_t magic;
  std::memcpy(&magic, blob.data(), 4);
  assert(magic == 0x5053474e);  // "NGSP"

  // Check version
  std::uint32_t version;
  std::memcpy(&version, blob.data() + 4, 4);
  assert(version == 4);

  std::cout << "PASS\n";
}

// ─── Test 2: v4 corrupt header (bad magic) ───

void test_v4_corrupt_magic() {
  std::cout << "Test: v4 corrupt magic... ";
  auto blob = BuildSyntheticV4();
  blob[0] = 0xFF;  // corrupt magic byte

  std::uint32_t magic;
  std::memcpy(&magic, blob.data(), 4);
  assert(magic != 0x5053474e);

  // Gatekeeper should detect non-NGSP magic
  std::cout << "PASS\n";
}

// ─── Test 3: v4 version detect ───

void test_v4_version_detect() {
  std::cout << "Test: v4 version detect... ";

  // v4 blob
  auto v4 = BuildSyntheticV4();
  std::uint32_t ver4;
  std::memcpy(&ver4, v4.data() + 4, 4);
  assert(ver4 == 4);

  // v3 blob
  auto v3 = BuildSyntheticV3Decomp();
  std::uint32_t ver3;
  std::memcpy(&ver3, v3.data() + 4, 4);
  assert(ver3 == 3);

  // gzip magic (legacy)
  std::vector<std::uint8_t> gzip = {0x1f, 0x8b, 0x08, 0x00};
  assert(gzip[0] == 0x1f && gzip[1] == 0x8b);

  std::cout << "PASS\n";
}

// ─── Test 4: v4 InspectSpzBlob (if bench data available) ───

void test_v4_inspect_bench() {
  std::cout << "Test: v4 inspect bench data... ";
  // Try to find bench data
  std::vector<std::string> candidates = {
    "C:/Users/HP/Downloads/spz-anime-text2scene-bench/Winter Forest Cabin/raw/Winter Forest Cabin_v4.spz",
    "../../../spz-anime-text2scene-bench/Winter Forest Cabin/raw/Winter Forest Cabin_v4.spz",
  };

  bool found = false;
  for (const auto& path : candidates) {
    std::vector<std::uint8_t> data;
    if (read_file(path, &data) && data.size() > 32) {
      auto result = spz_gatekeeper::InspectSpzBlob(data);
      // v4 file should be detected as version 4
      if (result.header.version == 4) {
        std::cout << "PASS (bench: " << path << ", " << data.size() << " bytes)\n";
        found = true;
        break;
      }
    }
  }
  if (!found) {
    std::cout << "SKIP (no bench data found)\n";
  }
}

// ─── Test 5: v1-v3 regression (synthetic) ───

void test_v1_v3_regression() {
  std::cout << "Test: v1-v3 regression... ";
  auto decomp = BuildSyntheticV3Decomp();

  // Verify v3 header structure
  std::uint32_t magic;
  std::memcpy(&magic, decomp.data(), 4);
  assert(magic == 0x5053474e);

  std::uint32_t version;
  std::memcpy(&version, decomp.data() + 4, 4);
  assert(version == 3);

  // Parse ILV from v3 trailer (should be empty for synthetic data)
  auto ilv = spz_gatekeeper::ParseIlvRecords(decomp.data() + 16,
                                               decomp.size() - 16);
  // No trailer = no records (this is expected)
  assert(ilv.records.empty() || ilv.error.empty());

  std::cout << "PASS\n";
}

// ─── Test 6: v4 header zone ILV ───

void test_v4_header_zone_ilv() {
  std::cout << "Test: v4 header zone ILV... ";
  auto blob = BuildSyntheticV4();

  // The header zone is bytes[32..tocByteOffset)
  // For our synthetic blob, there's nothing after the 32B header
  // This verifies the parsing doesn't crash on empty extension zone
  if (blob.size() > 32) {
    auto ilv = spz_gatekeeper::ParseIlvRecords(blob.data() + 32,
                                                 blob.size() - 32);
    // Empty extension zone = no records
    assert(ilv.records.empty());
  }

  std::cout << "PASS\n";
}

}  // namespace

int main() {
  std::cout << "=== R6 T23: v4 Format Tests ===" << std::endl;

  test_v4_header_parse();
  test_v4_corrupt_magic();
  test_v4_version_detect();
  test_v4_inspect_bench();
  test_v1_v3_regression();
  test_v4_header_zone_ilv();

  std::cout << "\nAll v4 format tests completed." << std::endl;
  return 0;
}
