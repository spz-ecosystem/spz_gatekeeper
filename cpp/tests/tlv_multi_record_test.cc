/**
 * TLV Multi-Record Parser - Unit Tests
 *
 * Comprehensive tests for TLV parser supporting multiple records.
 * Validates correct parsing of 0, 1, and multiple TLV records.
 *
 * Test Coverage:
 * - Empty trailer (0 records)
 * - Single record parsing
 * - Multiple records parsing
 * - Backward compatibility
 * - Boundary cases
 * - Little-endian byte order
 * - Skip unknown types via length field
 *
 * @author PuJunhan
 * @copyright Copyright (c) 2026 PuJunhan
 * @license MIT
 */

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>

#include "spz_gatekeeper/tlv.h"

namespace {

int g_tests_run = 0;
int g_tests_passed = 0;

#define TEST(name) void name()
#define RUN_TEST(name) do { \
  std::cout << "Running " << #name << "... "; \
  g_tests_run++; \
  try { \
    name(); \
    std::cout << "PASS ✓" << std::endl; \
    g_tests_passed++; \
  } catch (const std::exception& e) { \
    std::cout << "FAIL ✗: " << e.what() << std::endl; \
  } catch (...) { \
    std::cout << "FAIL ✗: unknown exception" << std::endl; \
  } \
} while(0)

#define ASSERT_TRUE(cond) do { \
  if (!(cond)) { \
    throw std::runtime_error("Assertion failed: " #cond); \
  } \
} while(0)

#define ASSERT_FALSE(cond) do { \
  if (cond) { \
    throw std::runtime_error("Assertion failed: NOT " #cond); \
  } \
} while(0)

#define ASSERT_EQ(a, b) do { \
  if ((a) != (b)) { \
    throw std::runtime_error("Assertion failed: " #a " == " #b); \
  } \
} while(0)

#define ASSERT_NE(a, b) do { \
  if ((a) == (b)) { \
    throw std::runtime_error("Assertion failed: " #a " != " #b); \
  } \
} while(0)

// Helper: Write little-endian uint32
void write_u32_le(uint32_t value, std::vector<uint8_t>& out) {
  out.push_back(value & 0xFF);
  out.push_back((value >> 8) & 0xFF);
  out.push_back((value >> 16) & 0xFF);
  out.push_back((value >> 24) & 0xFF);
}

// Helper: Create TLV record
std::vector<uint8_t> create_tlv_record(uint32_t type, const std::vector<uint8_t>& value) {
  std::vector<uint8_t> record;
  write_u32_le(type, record);
  write_u32_le(static_cast<uint32_t>(value.size()), record);
  record.insert(record.end(), value.begin(), value.end());
  return record;
}

TEST(test_empty_trailer) {
  std::vector<uint8_t> empty_data;
  auto result = spz_gatekeeper::ParseTlvTrailer(empty_data, 0);
  
  ASSERT_TRUE(result.ok);
  ASSERT_EQ(result.records.size(), 0u);
}

TEST(test_single_record) {
  std::vector<uint8_t> data = create_tlv_record(0x12340001u, {0x01, 0x02, 0x03});
  
  auto result = spz_gatekeeper::ParseTlvTrailer(data, 0);
  
  ASSERT_TRUE(result.ok);
  ASSERT_EQ(result.records.size(), 1u);
  ASSERT_EQ(result.records[0].type, 0x12340001u);
  ASSERT_EQ(result.records[0].length, 3u);
  ASSERT_EQ(result.records[0].offset, 0u);
  ASSERT_EQ(result.records[0].ValueView().size(), 3u);
}

TEST(test_multiple_records) {
  std::vector<uint8_t> data;
  
  // Record 1: type=0x0001, value={0xAA}
  auto rec1 = create_tlv_record(0x00010001u, {0xAA});
  data.insert(data.end(), rec1.begin(), rec1.end());
  
  // Record 2: type=0x0002, value={0xBB, 0xCC}
  auto rec2 = create_tlv_record(0x00020002u, {0xBB, 0xCC});
  data.insert(data.end(), rec2.begin(), rec2.end());
  
  // Record 3: type=0x0003, value={0xDD, 0xEE, 0xFF}
  auto rec3 = create_tlv_record(0x00030003u, {0xDD, 0xEE, 0xFF});
  data.insert(data.end(), rec3.begin(), rec3.end());
  
  auto result = spz_gatekeeper::ParseTlvTrailer(data, 0);
  
  ASSERT_TRUE(result.ok);
  ASSERT_EQ(result.records.size(), 3u);
  
  // Verify record 1
  ASSERT_EQ(result.records[0].type, 0x00010001u);
  ASSERT_EQ(result.records[0].length, 1u);
  ASSERT_EQ(result.records[0].offset, 0u);
  
  // Verify record 2
  ASSERT_EQ(result.records[1].type, 0x00020002u);
  ASSERT_EQ(result.records[1].length, 2u);
  ASSERT_EQ(result.records[1].offset, 9u);  // 4 (type) + 4 (length) + 1 (value) = 9
  
  // Verify record 3
  ASSERT_EQ(result.records[2].type, 0x00030003u);
  ASSERT_EQ(result.records[2].length, 3u);
  ASSERT_EQ(result.records[2].offset, 19u);  // 9 + 4 + 4 + 2 = 19
}

TEST(test_large_number_of_records) {
  std::vector<uint8_t> data;
  
  // Create 100 records
  for (uint32_t i = 0; i < 100; ++i) {
    auto rec = create_tlv_record(0x00010000u + i, {static_cast<uint8_t>(i)});
    data.insert(data.end(), rec.begin(), rec.end());
  }
  
  auto result = spz_gatekeeper::ParseTlvTrailer(data, 0);
  
  ASSERT_TRUE(result.ok);
  ASSERT_EQ(result.records.size(), 100u);
  
  // Verify first and last
  ASSERT_EQ(result.records[0].type, 0x00010000u);
  ASSERT_EQ(result.records[99].type, 0x00010063u);  // 0x00010000 + 99
}

TEST(test_zero_length_record) {
  std::vector<uint8_t> data = create_tlv_record(0x12340001u, {});
  
  auto result = spz_gatekeeper::ParseTlvTrailer(data, 0);
  
  ASSERT_TRUE(result.ok);
  ASSERT_EQ(result.records.size(), 1u);
  ASSERT_EQ(result.records[0].type, 0x12340001u);
  ASSERT_EQ(result.records[0].length, 0u);
  ASSERT_EQ(result.records[0].ValueView().size(), 0u);
}

TEST(test_large_payload_record) {
  std::vector<uint8_t> large_value(1000, 0xAB);  // 1000 bytes of 0xAB
  std::vector<uint8_t> data = create_tlv_record(0x12340001u, large_value);

  auto result = spz_gatekeeper::ParseTlvTrailer(data, 0);

  ASSERT_TRUE(result.ok);
  ASSERT_EQ(result.records.size(), 1u);
  ASSERT_EQ(result.records[0].type, 0x12340001u);
  ASSERT_EQ(result.records[0].length, 1000u);
  auto value_view = result.records[0].ValueView();
  ASSERT_EQ(value_view.size(), 1000u);
  // Verify all bytes are 0xAB
  for (size_t i = 0; i < 1000; ++i) {
    ASSERT_EQ(value_view[i], 0xAB);
  }
}

TEST(test_copy_value_on_demand) {
  std::vector<uint8_t> data = create_tlv_record(0x12340001u, {0x10, 0x20, 0x30});

  auto result = spz_gatekeeper::ParseTlvTrailer(data, 0);

  ASSERT_TRUE(result.ok);
  ASSERT_EQ(result.records.size(), 1u);
  auto copied = result.records[0].CopyValue();
  ASSERT_EQ(copied.size(), 3u);
  ASSERT_EQ(copied[0], 0x10);
  ASSERT_EQ(copied[1], 0x20);
  ASSERT_EQ(copied[2], 0x30);
}

TEST(test_little_endian_byte_order) {
  // Manually create little-endian TLV
  std::vector<uint8_t> data = {
    // Type: 0x12345678 (little-endian)
    0x78, 0x56, 0x34, 0x12,
    // Length: 0x00000003 (little-endian)
    0x03, 0x00, 0x00, 0x00,
    // Value: 3 bytes
    0xAA, 0xBB, 0xCC
  };
  
  auto result = spz_gatekeeper::ParseTlvTrailer(data, 0);
  
  ASSERT_TRUE(result.ok);
  ASSERT_EQ(result.records.size(), 1u);
  ASSERT_EQ(result.records[0].type, 0x12345678u);  // Should be parsed as 0x12345678
  ASSERT_EQ(result.records[0].length, 3u);
}

TEST(test_skip_unknown_type_via_length) {
  std::vector<uint8_t> data;
  
  // Known type (we'll check this)
  auto rec1 = create_tlv_record(0x00010001u, {0xAA});
  data.insert(data.end(), rec1.begin(), rec1.end());
  
  // Unknown type with large payload (should be skipped)
  std::vector<uint8_t> unknown_payload(100, 0xFF);
  auto rec2 = create_tlv_record(0xFFFF0001u, unknown_payload);
  data.insert(data.end(), rec2.begin(), rec2.end());
  
  // Another known type
  auto rec3 = create_tlv_record(0x00010002u, {0xBB});
  data.insert(data.end(), rec3.begin(), rec3.end());
  
  auto result = spz_gatekeeper::ParseTlvTrailer(data, 0);
  
  ASSERT_TRUE(result.ok);
  ASSERT_EQ(result.records.size(), 3u);  // All 3 records parsed

  // Verify we can still access the third record after skipping unknown
  ASSERT_EQ(result.records[2].type, 0x00010002u);
  auto third_view = result.records[2].ValueView();
  ASSERT_EQ(third_view.size(), 1u);
  ASSERT_EQ(third_view[0], 0xBB);
}

TEST(test_truncated_record_insufficient_header) {
  // Only type field (4 bytes), missing length
  std::vector<uint8_t> data = {0x01, 0x00, 0x00, 0x00};
  
  auto result = spz_gatekeeper::ParseTlvTrailer(data, 0);
  
  // Should fail or return partial result
  ASSERT_FALSE(result.ok);
  ASSERT_TRUE(result.error.find("insufficient") != std::string::npos ||
              result.error.find("truncated") != std::string::npos);
}

TEST(test_truncated_record_insufficient_value) {
  std::vector<uint8_t> data = {
    0x01, 0x00, 0x00, 0x00,  // Type
    0x10, 0x00, 0x00, 0x00,  // Length: 16 bytes
    0xAA, 0xBB               // Only 2 bytes instead of 16
  };
  
  auto result = spz_gatekeeper::ParseTlvTrailer(data, 0);
  
  ASSERT_FALSE(result.ok);
  ASSERT_TRUE(result.error.find("insufficient") != std::string::npos ||
              result.error.find("truncated") != std::string::npos);
}

TEST(test_backward_compatibility_single_record) {
  // Single record should work as before
  std::vector<uint8_t> data = create_tlv_record(0x12340001u, {0x01, 0x02, 0x03, 0x04});
  
  auto result = spz_gatekeeper::ParseTlvTrailer(data, 0);
  
  ASSERT_TRUE(result.ok);
  ASSERT_EQ(result.records.size(), 1u);
  ASSERT_EQ(result.records[0].type, 0x12340001u);
  ASSERT_EQ(result.records[0].length, 4u);
}

TEST(test_offset_calculation_accuracy) {
  std::vector<uint8_t> data;
  
  // Record 1: 4 + 4 + 1 = 9 bytes
  data.insert(data.end(), {0x01, 0x00, 0x00, 0x00});  // type
  data.insert(data.end(), {0x01, 0x00, 0x00, 0x00});  // length
  data.push_back(0xAA);  // value
  
  // Record 2: 4 + 4 + 2 = 10 bytes
  data.insert(data.end(), {0x02, 0x00, 0x00, 0x00});
  data.insert(data.end(), {0x02, 0x00, 0x00, 0x00});
  data.push_back(0xBB);
  data.push_back(0xCC);
  
  // Record 3: 4 + 4 + 3 = 11 bytes
  data.insert(data.end(), {0x03, 0x00, 0x00, 0x00});
  data.insert(data.end(), {0x03, 0x00, 0x00, 0x00});
  data.push_back(0xDD);
  data.push_back(0xEE);
  data.push_back(0xFF);
  
  auto result = spz_gatekeeper::ParseTlvTrailer(data, 0);
  
  ASSERT_TRUE(result.ok);
  ASSERT_EQ(result.records.size(), 3u);
  
  // Verify exact offsets
  ASSERT_EQ(result.records[0].offset, 0u);    // Start at 0
  ASSERT_EQ(result.records[1].offset, 9u);    // After record 1 (9 bytes)
  ASSERT_EQ(result.records[2].offset, 19u);   // After record 1+2 (9+10 bytes)
}

}  // namespace

int main() {
  std::cout << "=== TLV Multi-Record Parser Tests ===" << std::endl;
  std::cout << std::endl;
  
  RUN_TEST(test_empty_trailer);
  RUN_TEST(test_single_record);
  RUN_TEST(test_multiple_records);
  RUN_TEST(test_large_number_of_records);
  RUN_TEST(test_zero_length_record);
  RUN_TEST(test_large_payload_record);
  RUN_TEST(test_copy_value_on_demand);
  RUN_TEST(test_little_endian_byte_order);
  RUN_TEST(test_skip_unknown_type_via_length);
  RUN_TEST(test_truncated_record_insufficient_header);
  RUN_TEST(test_truncated_record_insufficient_value);
  RUN_TEST(test_backward_compatibility_single_record);
  RUN_TEST(test_offset_calculation_accuracy);
  
  std::cout << std::endl;
  std::cout << "=== Test Summary ===" << std::endl;
  std::cout << "Tests run: " << g_tests_run << std::endl;
  std::cout << "Tests passed: " << g_tests_passed << std::endl;
  std::cout << "Tests failed: " << (g_tests_run - g_tests_passed) << std::endl;
  
  if (g_tests_passed == g_tests_run) {
    std::cout << "Result: ALL " << g_tests_passed << " TESTS PASSED ✓" << std::endl;
    return EXIT_SUCCESS;
  } else {
    std::cout << "Result: " << (g_tests_run - g_tests_passed) << " TESTS FAILED ✗" << std::endl;
    return EXIT_FAILURE;
  }
}
