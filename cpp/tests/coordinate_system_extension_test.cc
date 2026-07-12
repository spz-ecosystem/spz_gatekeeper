// SPDX-License-Identifier: MulanPSL-2.0
// Copyright (c) 2026 PuJunhan
/**
 * Adobe Coordinate System Extension - Unit Tests
 *
 * Comprehensive tests for Adobe Coordinate System validator.
 * Tests all validation rules and edge cases.
 *
 * Test Coverage:
 * - Valid payload (all defined coordinate system values 0-16)
 * - Invalid payload size (not 4 bytes)
 * - Invalid coordinate system value (> 16)
 * - Boundary values (0, 16)
 * - Little-endian byte order
 * - Null data with non-zero size
 * - Null error pointer
 *
 * @author PuJunhan
 * @copyright Copyright (c) 2026 PuJunhan
 * @license MulanPSL-2.0
 */

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "spz_gatekeeper/coordinate_system_validator.h"
#include "spz_gatekeeper/extension_validator.h"

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

#define ASSERT_SUBSTRING(str, substr) do { \
  if ((str).find(substr) == std::string::npos) { \
    throw std::runtime_error("Assertion failed: '" #str "' should contain '" substr "'"); \
  } \
} while(0)

std::vector<uint8_t> write_uint32_le(uint32_t value) {
  std::vector<uint8_t> data(4);
  data[0] = static_cast<uint8_t>(value & 0xFFu);
  data[1] = static_cast<uint8_t>((value >> 8) & 0xFFu);
  data[2] = static_cast<uint8_t>((value >> 16) & 0xFFu);
  data[3] = static_cast<uint8_t>((value >> 24) & 0xFFu);
  return data;
}

TEST(test_extension_type) {
  spz_gatekeeper::AdobeCoordinateSystemValidator validator;
  ASSERT_EQ(validator.GetExtensionType(), 0xADBE0003u);
}

TEST(test_extension_name) {
  spz_gatekeeper::AdobeCoordinateSystemValidator validator;
  ASSERT_EQ(validator.GetName(), "Adobe Coordinate System");
}

TEST(test_valid_payload_unspecified) {
  spz_gatekeeper::AdobeCoordinateSystemValidator validator;
  auto data = write_uint32_le(0u);
  std::string error;

  bool result = validator.Validate(data.data(), data.size(), &error);
  ASSERT_TRUE(result);
  ASSERT_TRUE(error.empty());
}

TEST(test_valid_payload_rub) {
  spz_gatekeeper::AdobeCoordinateSystemValidator validator;
  auto data = write_uint32_le(4u);
  std::string error;

  bool result = validator.Validate(data.data(), data.size(), &error);
  ASSERT_TRUE(result);
  ASSERT_TRUE(error.empty());
}

TEST(test_valid_payload_rbu) {
  spz_gatekeeper::AdobeCoordinateSystemValidator validator;
  auto data = write_uint32_le(16u);
  std::string error;

  bool result = validator.Validate(data.data(), data.size(), &error);
  ASSERT_TRUE(result);
  ASSERT_TRUE(error.empty());
}

TEST(test_valid_payload_all_defined_values) {
  spz_gatekeeper::AdobeCoordinateSystemValidator validator;
  for (uint32_t value = 0u; value <= 16u; ++value) {
    auto data = write_uint32_le(value);
    std::string error;
    bool result = validator.Validate(data.data(), data.size(), &error);
    ASSERT_TRUE(result);
    ASSERT_TRUE(error.empty());
  }
}

TEST(test_invalid_payload_size_zero) {
  spz_gatekeeper::AdobeCoordinateSystemValidator validator;
  std::vector<uint8_t> empty_data;
  std::string error;

  bool result = validator.Validate(empty_data.data(), empty_data.size(), &error);
  ASSERT_FALSE(result);
  ASSERT_FALSE(error.empty());
  ASSERT_SUBSTRING(error, "Invalid payload size");
  ASSERT_SUBSTRING(error, "expected 4");
}

TEST(test_invalid_payload_size_small) {
  spz_gatekeeper::AdobeCoordinateSystemValidator validator;
  std::vector<uint8_t> small_data(3, 0x00);
  std::string error;

  bool result = validator.Validate(small_data.data(), small_data.size(), &error);
  ASSERT_FALSE(result);
  ASSERT_SUBSTRING(error, "Invalid payload size");
}

TEST(test_invalid_payload_size_large) {
  spz_gatekeeper::AdobeCoordinateSystemValidator validator;
  std::vector<uint8_t> large_data(8, 0x00);
  std::string error;

  bool result = validator.Validate(large_data.data(), large_data.size(), &error);
  ASSERT_FALSE(result);
  ASSERT_SUBSTRING(error, "Invalid payload size");
}

TEST(test_invalid_value_just_above_max) {
  spz_gatekeeper::AdobeCoordinateSystemValidator validator;
  auto data = write_uint32_le(17u);
  std::string error;

  bool result = validator.Validate(data.data(), data.size(), &error);
  ASSERT_FALSE(result);
  ASSERT_SUBSTRING(error, "Coordinate system value out of range");
  ASSERT_SUBSTRING(error, "17");
}

TEST(test_invalid_value_large) {
  spz_gatekeeper::AdobeCoordinateSystemValidator validator;
  auto data = write_uint32_le(0xFFFFFFFFu);
  std::string error;

  bool result = validator.Validate(data.data(), data.size(), &error);
  ASSERT_FALSE(result);
  ASSERT_SUBSTRING(error, "Coordinate system value out of range");
}

TEST(test_little_endian_byte_order) {
  spz_gatekeeper::AdobeCoordinateSystemValidator validator;

  std::vector<uint8_t> data = {0x10, 0x00, 0x00, 0x00};
  std::string error;
  bool result = validator.Validate(data.data(), data.size(), &error);
  ASSERT_TRUE(result);
  ASSERT_TRUE(error.empty());

  std::vector<uint8_t> big_endian = {0x00, 0x00, 0x00, 0x10};
  result = validator.Validate(big_endian.data(), big_endian.size(), &error);
  ASSERT_FALSE(result);
  ASSERT_SUBSTRING(error, "Coordinate system value out of range");
}

TEST(test_null_error_pointer) {
  spz_gatekeeper::AdobeCoordinateSystemValidator validator;
  auto data = write_uint32_le(4u);

  bool result = validator.Validate(data.data(), data.size(), nullptr);
  ASSERT_TRUE(result);
}

TEST(test_null_data_with_size) {
  spz_gatekeeper::AdobeCoordinateSystemValidator validator;
  std::string error;

  bool result = validator.Validate(nullptr, 4, &error);
  ASSERT_FALSE(result);
  ASSERT_SUBSTRING(error, "data is null");
}

}  // namespace

int main() {
  std::cout << "=== Adobe Coordinate System Extension Tests ===" << std::endl;
  std::cout << std::endl;

  RUN_TEST(test_extension_type);
  RUN_TEST(test_extension_name);
  RUN_TEST(test_valid_payload_unspecified);
  RUN_TEST(test_valid_payload_rub);
  RUN_TEST(test_valid_payload_rbu);
  RUN_TEST(test_valid_payload_all_defined_values);
  RUN_TEST(test_invalid_payload_size_zero);
  RUN_TEST(test_invalid_payload_size_small);
  RUN_TEST(test_invalid_payload_size_large);
  RUN_TEST(test_invalid_value_just_above_max);
  RUN_TEST(test_invalid_value_large);
  RUN_TEST(test_little_endian_byte_order);
  RUN_TEST(test_null_error_pointer);
  RUN_TEST(test_null_data_with_size);

  std::cout << std::endl;
  std::cout << "=== Test Summary ===" << std::endl;
  std::cout << "Tests run: " << g_tests_run << std::endl;
  std::cout << "Tests passed: " << g_tests_passed << std::endl;
  std::cout << "Tests failed: " << (g_tests_run - g_tests_passed) << std::endl;

  if (g_tests_passed == g_tests_run) {
    std::cout << "Result: ALL " << g_tests_passed << " TESTS PASSED ✓" << std::endl;
    return EXIT_SUCCESS;
  }

  std::cout << "Result: " << (g_tests_run - g_tests_passed) << " TESTS FAILED ✗" << std::endl;
  return EXIT_FAILURE;
}
