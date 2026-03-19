// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PuJunhan

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>

#include "spz_gatekeeper/spz.h"
#include "spz_gatekeeper/tlv.h"

namespace {

// Helper to build minimal SPZ v3 decompressed data
std::vector<std::uint8_t> BuildDecompressedV3(std::uint8_t flags, bool with_trailer) {
  auto write_u32 = [](std::vector<std::uint8_t>* b, std::uint32_t v) {
    b->push_back(static_cast<std::uint8_t>(v & 0xFF));
    b->push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
    b->push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
    b->push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
  };

  std::vector<std::uint8_t> decomp;
  decomp.reserve(64);
  write_u32(&decomp, 0x5053474e);  // magic "NGSP"
  write_u32(&decomp, 3);           // version
  write_u32(&decomp, 1);           // numPoints
  decomp.push_back(0);             // shDegree
  decomp.push_back(12);            // fractionalBits
  decomp.push_back(flags);         // flags
  decomp.push_back(0);             // reserved

  // base segments for numPoints=1, v3:
  // positions: 1*3*3=9
  // alphas: 1
  // colors: 3
  // scales: 3
  // rotations: 4
  // sh: 0
  decomp.insert(decomp.end(), 9 + 1 + 3 + 3 + 4, 0);

  if (with_trailer) {
    // TLV trailer: type=1, len=3, value=abc
    write_u32(&decomp, 1);
    write_u32(&decomp, 3);
    decomp.push_back('a');
    decomp.push_back('b');
    decomp.push_back('c');
  }

  return decomp;
}

bool HasIssueCode(const spz_gatekeeper::GateReport& rep, const char* code) {
  for (const auto& it : rep.issues) {
    if (std::strcmp(it.code.c_str(), code) == 0) return true;
  }
  return false;
}

}  // namespace

int test_tlv_parsing() {
  // Test: TLV parsing
  std::cout << "Test: TLV parsing... ";
  
  std::vector<std::uint8_t> data = {
    0x01, 0x00, 0x00, 0x00,  // type = 1
    0x03, 0x00, 0x00, 0x00,  // length = 3
    'a', 'b', 'c',           // value
    0x02, 0x00, 0x00, 0x00,  // type = 2
    0x02, 0x00, 0x00, 0x00,  // length = 2
    'd', 'e'                 // value
  };
  
  auto result = spz_gatekeeper::ParseTlvTrailer(data, 0);
  
  if (!result.ok) {
    std::cout << "FAILED (parse error: " << result.error << ")" << std::endl;
    return 1;
  }
  
  if (result.records.size() != 2) {
    std::cout << "FAILED (expected 2 records, got " << result.records.size() << ")" << std::endl;
    return 1;
  }
  
  if (result.records[0].type != 1 || result.records[0].length != 3) {
    std::cout << "FAILED (first record mismatch)" << std::endl;
    return 1;
  }
  
  if (result.records[1].type != 2 || result.records[1].length != 2) {
    std::cout << "FAILED (second record mismatch)" << std::endl;
    return 1;
  }
  
  std::cout << "PASSED" << std::endl;
  return 0;
}

int test_tlv_empty() {
  // Test: Empty TLV data
  std::cout << "Test: Empty TLV data... ";
  
  std::vector<std::uint8_t> data;
  auto result = spz_gatekeeper::ParseTlvTrailer(data, 0);
  
  if (!result.ok || !result.records.empty()) {
    std::cout << "FAILED (should handle empty data)" << std::endl;
    return 1;
  }
  
  std::cout << "PASSED" << std::endl;
  return 0;
}

int test_tlv_truncated() {
  // Test: Truncated TLV
  std::cout << "Test: Truncated TLV... ";
  
  std::vector<std::uint8_t> data = {
    0x01, 0x00, 0x00, 0x00,  // type = 1
    0x03, 0x00, 0x00, 0x00   // length = 3 (but no value)
  };
  
  auto result = spz_gatekeeper::ParseTlvTrailer(data, 0);
  
  if (result.ok) {
    std::cout << "FAILED (should fail on truncated data)" << std::endl;
    return 1;
  }
  
  std::cout << "PASSED" << std::endl;
  return 0;
}

int main() {
  std::cout << "=== SPZ Gatekeeper Test Suite ===" << std::endl;
  std::cout << std::endl;
  
  int failures = 0;
  
  failures += test_tlv_parsing();
  failures += test_tlv_empty();
  failures += test_tlv_truncated();
  
  std::cout << std::endl;
  std::cout << "=== Test Summary ===" << std::endl;
  
  if (failures == 0) {
    std::cout << "All tests PASSED!" << std::endl;
    return EXIT_SUCCESS;
  } else {
    std::cout << failures << " test(s) FAILED!" << std::endl;
    return EXIT_FAILURE;
  }
}
