/**
 * Adobe Safe Orbit Camera Extension - Unit Tests
 *
 * Comprehensive tests for Adobe Safe Orbit Camera validator.
 * Tests all validation rules and edge cases.
 *
 * Test Coverage:
 * - Valid payload (all fields in range)
 * - Invalid payload size (not 12 bytes)
 * - Invalid minElevation (out of [-pi/2, pi/2] range)
 * - Invalid maxElevation (out of [-pi/2, pi/2] range)
 * - Invalid minElevation > maxElevation
 * - Invalid minRadius (< 0)
 * - NaN values
 * - Boundary values (exactly -pi/2, pi/2, 0)
 * - Endianness (little-endian byte order)
 *
 * @author PuJunhan
 * @copyright Copyright (c) 2026 PuJunhan
 * @license MIT
 */

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include "spz_gatekeeper/extension_validator.h"
#include "spz_gatekeeper/safe_orbit_camera_validator.h"

namespace {

constexpr float kHalfPi = 1.57079632679f;

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

void write_float_le(float value, uint8_t* out) {
  std::memcpy(out, &value, sizeof(float));
}

std::vector<uint8_t> create_valid_payload(float minElev = -0.5f,
                                          float maxElev = 0.5f,
                                          float minRad = 1.0f) {
  std::vector<uint8_t> data(12);
  write_float_le(minElev, data.data());
  write_float_le(maxElev, data.data() + 4);
  write_float_le(minRad, data.data() + 8);
  return data;
}

TEST(test_extension_type) {
  spz_gatekeeper::AdobeSafeOrbitCameraValidator validator;
  ASSERT_EQ(validator.GetExtensionType(), 0xADBE0002u);
}

TEST(test_extension_name) {
  spz_gatekeeper::AdobeSafeOrbitCameraValidator validator;
  ASSERT_EQ(validator.GetName(), "Adobe Safe Orbit Camera");
}

TEST(test_valid_payload_typical) {
  spz_gatekeeper::AdobeSafeOrbitCameraValidator validator;
  auto data = create_valid_payload(-0.5f, 0.5f, 1.0f);
  std::string error;

  bool result = validator.Validate(data.data(), data.size(), &error);
  ASSERT_TRUE(result);
  ASSERT_TRUE(error.empty());
}

TEST(test_valid_payload_boundary_min) {
  spz_gatekeeper::AdobeSafeOrbitCameraValidator validator;
  auto data = create_valid_payload(-kHalfPi, kHalfPi, 0.0f);
  std::string error;

  bool result = validator.Validate(data.data(), data.size(), &error);
  ASSERT_TRUE(result);
  ASSERT_TRUE(error.empty());
}

TEST(test_valid_payload_same_elevation) {
  spz_gatekeeper::AdobeSafeOrbitCameraValidator validator;
  auto data = create_valid_payload(0.25f, 0.25f, 5.0f);
  std::string error;

  bool result = validator.Validate(data.data(), data.size(), &error);
  ASSERT_TRUE(result);
  ASSERT_TRUE(error.empty());
}

TEST(test_invalid_payload_size_zero) {
  spz_gatekeeper::AdobeSafeOrbitCameraValidator validator;
  std::vector<uint8_t> empty_data;
  std::string error;

  bool result = validator.Validate(empty_data.data(), empty_data.size(), &error);
  ASSERT_FALSE(result);
  ASSERT_FALSE(error.empty());
  ASSERT_SUBSTRING(error, "Invalid payload size");
  ASSERT_SUBSTRING(error, "expected 12");
}

TEST(test_invalid_payload_size_small) {
  spz_gatekeeper::AdobeSafeOrbitCameraValidator validator;
  std::vector<uint8_t> small_data(8);
  std::string error;

  bool result = validator.Validate(small_data.data(), small_data.size(), &error);
  ASSERT_FALSE(result);
  ASSERT_SUBSTRING(error, "Invalid payload size");
}

TEST(test_invalid_payload_size_large) {
  spz_gatekeeper::AdobeSafeOrbitCameraValidator validator;
  std::vector<uint8_t> large_data(16);
  std::string error;

  bool result = validator.Validate(large_data.data(), large_data.size(), &error);
  ASSERT_FALSE(result);
  ASSERT_SUBSTRING(error, "Invalid payload size");
}

TEST(test_invalid_payload_size_11) {
  spz_gatekeeper::AdobeSafeOrbitCameraValidator validator;
  std::vector<uint8_t> data(11);
  std::string error;

  bool result = validator.Validate(data.data(), data.size(), &error);
  ASSERT_FALSE(result);
  ASSERT_SUBSTRING(error, "expected 12");
}

TEST(test_invalid_min_elevation_too_low) {
  spz_gatekeeper::AdobeSafeOrbitCameraValidator validator;
  auto data = create_valid_payload(-1.7f, 0.5f, 1.0f);
  std::string error;

  bool result = validator.Validate(data.data(), data.size(), &error);
  ASSERT_FALSE(result);
  ASSERT_SUBSTRING(error, "minElevation out of range");
  ASSERT_SUBSTRING(error, "radians");
}

TEST(test_invalid_min_elevation_too_high) {
  spz_gatekeeper::AdobeSafeOrbitCameraValidator validator;
  auto data = create_valid_payload(1.7f, 0.5f, 1.0f);
  std::string error;

  bool result = validator.Validate(data.data(), data.size(), &error);
  ASSERT_FALSE(result);
  ASSERT_SUBSTRING(error, "minElevation out of range");
  ASSERT_SUBSTRING(error, "radians");
}

TEST(test_invalid_max_elevation_too_low) {
  spz_gatekeeper::AdobeSafeOrbitCameraValidator validator;
  auto data = create_valid_payload(-0.5f, -1.7f, 1.0f);
  std::string error;

  bool result = validator.Validate(data.data(), data.size(), &error);
  ASSERT_FALSE(result);
  ASSERT_SUBSTRING(error, "maxElevation out of range");
  ASSERT_SUBSTRING(error, "radians");
}

TEST(test_invalid_max_elevation_too_high) {
  spz_gatekeeper::AdobeSafeOrbitCameraValidator validator;
  auto data = create_valid_payload(-0.5f, 1.7f, 1.0f);
  std::string error;

  bool result = validator.Validate(data.data(), data.size(), &error);
  ASSERT_FALSE(result);
  ASSERT_SUBSTRING(error, "maxElevation out of range");
  ASSERT_SUBSTRING(error, "radians");
}

TEST(test_invalid_min_greater_than_max) {
  spz_gatekeeper::AdobeSafeOrbitCameraValidator validator;
  auto data = create_valid_payload(1.0f, -1.0f, 1.0f);
  std::string error;

  bool result = validator.Validate(data.data(), data.size(), &error);
  ASSERT_FALSE(result);
  ASSERT_SUBSTRING(error, "minElevation");
  ASSERT_SUBSTRING(error, "maxElevation");
}

TEST(test_invalid_min_radius_negative) {
  spz_gatekeeper::AdobeSafeOrbitCameraValidator validator;
  auto data = create_valid_payload(-0.5f, 0.5f, -0.1f);
  std::string error;

  bool result = validator.Validate(data.data(), data.size(), &error);
  ASSERT_FALSE(result);
  ASSERT_SUBSTRING(error, "minRadius out of range");
}

TEST(test_valid_min_radius_zero) {
  spz_gatekeeper::AdobeSafeOrbitCameraValidator validator;
  auto data = create_valid_payload(-0.5f, 0.5f, 0.0f);
  std::string error;

  bool result = validator.Validate(data.data(), data.size(), &error);
  ASSERT_TRUE(result);
  ASSERT_TRUE(error.empty());
}

TEST(test_valid_min_radius_large) {
  spz_gatekeeper::AdobeSafeOrbitCameraValidator validator;
  auto data = create_valid_payload(-0.5f, 0.5f, 1000.0f);
  std::string error;

  bool result = validator.Validate(data.data(), data.size(), &error);
  ASSERT_TRUE(result);
  ASSERT_TRUE(error.empty());
}

TEST(test_nan_min_elevation) {
  spz_gatekeeper::AdobeSafeOrbitCameraValidator validator;
  std::vector<uint8_t> data(12);
  write_float_le(std::numeric_limits<float>::quiet_NaN(), data.data());
  write_float_le(0.5f, data.data() + 4);
  write_float_le(1.0f, data.data() + 8);
  std::string error;

  bool result = validator.Validate(data.data(), data.size(), &error);
  ASSERT_FALSE(result);
  ASSERT_SUBSTRING(error, "minElevation");
}

TEST(test_nan_max_elevation) {
  spz_gatekeeper::AdobeSafeOrbitCameraValidator validator;
  std::vector<uint8_t> data(12);
  write_float_le(-0.5f, data.data());
  write_float_le(std::numeric_limits<float>::quiet_NaN(), data.data() + 4);
  write_float_le(1.0f, data.data() + 8);
  std::string error;

  bool result = validator.Validate(data.data(), data.size(), &error);
  ASSERT_FALSE(result);
  ASSERT_SUBSTRING(error, "maxElevation");
}

TEST(test_nan_min_radius) {
  spz_gatekeeper::AdobeSafeOrbitCameraValidator validator;
  std::vector<uint8_t> data(12);
  write_float_le(-0.5f, data.data());
  write_float_le(0.5f, data.data() + 4);
  write_float_le(std::numeric_limits<float>::quiet_NaN(), data.data() + 8);
  std::string error;

  bool result = validator.Validate(data.data(), data.size(), &error);
  ASSERT_FALSE(result);
  ASSERT_SUBSTRING(error, "minRadius");
}

TEST(test_infinity_values) {
  spz_gatekeeper::AdobeSafeOrbitCameraValidator validator;
  std::vector<uint8_t> data(12);
  write_float_le(std::numeric_limits<float>::infinity(), data.data());
  write_float_le(0.5f, data.data() + 4);
  write_float_le(1.0f, data.data() + 8);
  std::string error;

  bool result = validator.Validate(data.data(), data.size(), &error);
  ASSERT_FALSE(result);
  ASSERT_SUBSTRING(error, "minElevation out of range");
}

TEST(test_null_error_pointer) {
  spz_gatekeeper::AdobeSafeOrbitCameraValidator validator;
  auto data = create_valid_payload();

  bool result = validator.Validate(data.data(), data.size(), nullptr);
  ASSERT_TRUE(result);
}

TEST(test_null_data_with_size) {
  spz_gatekeeper::AdobeSafeOrbitCameraValidator validator;
  std::string error;

  bool result = validator.Validate(nullptr, 12, &error);
  ASSERT_FALSE(result);
  ASSERT_SUBSTRING(error, "data is null");
}

TEST(test_little_endian_byte_order) {
  spz_gatekeeper::AdobeSafeOrbitCameraValidator validator;

  std::vector<uint8_t> data = {
    0x00, 0x00, 0x00, 0xBF,
    0x00, 0x00, 0x80, 0x3F,
    0x00, 0x00, 0x80, 0x3F
  };

  std::string error;
  bool result = validator.Validate(data.data(), data.size(), &error);
  ASSERT_TRUE(result);
  ASSERT_TRUE(error.empty());
}

TEST(test_error_message_detail) {
  spz_gatekeeper::AdobeSafeOrbitCameraValidator validator;
  auto data = create_valid_payload(-2.0f, 0.5f, 1.0f);
  std::string error;

  bool result = validator.Validate(data.data(), data.size(), &error);
  ASSERT_FALSE(result);
  ASSERT_FALSE(error.empty());
  ASSERT_SUBSTRING(error, "minElevation");
}

TEST(test_boundary_exact_values) {
  spz_gatekeeper::AdobeSafeOrbitCameraValidator validator;

  auto data1 = create_valid_payload(-kHalfPi, 0.0f, 0.0f);
  std::string e1;
  ASSERT_TRUE(validator.Validate(data1.data(), data1.size(), &e1));

  auto data2 = create_valid_payload(0.0f, kHalfPi, 0.0f);
  std::string e2;
  ASSERT_TRUE(validator.Validate(data2.data(), data2.size(), &e2));

  auto data3 = create_valid_payload(-0.5f, 0.5f, 0.0f);
  std::string e3;
  ASSERT_TRUE(validator.Validate(data3.data(), data3.size(), &e3));
}

}  // namespace

int main() {
  std::cout << "=== Adobe Safe Orbit Camera Extension Tests ===" << std::endl;
  std::cout << std::endl;

  RUN_TEST(test_extension_type);
  RUN_TEST(test_extension_name);
  RUN_TEST(test_valid_payload_typical);
  RUN_TEST(test_valid_payload_boundary_min);
  RUN_TEST(test_valid_payload_same_elevation);
  RUN_TEST(test_invalid_payload_size_zero);
  RUN_TEST(test_invalid_payload_size_small);
  RUN_TEST(test_invalid_payload_size_large);
  RUN_TEST(test_invalid_payload_size_11);
  RUN_TEST(test_invalid_min_elevation_too_low);
  RUN_TEST(test_invalid_min_elevation_too_high);
  RUN_TEST(test_invalid_max_elevation_too_low);
  RUN_TEST(test_invalid_max_elevation_too_high);
  RUN_TEST(test_invalid_min_greater_than_max);
  RUN_TEST(test_invalid_min_radius_negative);
  RUN_TEST(test_valid_min_radius_zero);
  RUN_TEST(test_valid_min_radius_large);
  RUN_TEST(test_nan_min_elevation);
  RUN_TEST(test_nan_max_elevation);
  RUN_TEST(test_nan_min_radius);
  RUN_TEST(test_infinity_values);
  RUN_TEST(test_null_error_pointer);
  RUN_TEST(test_null_data_with_size);
  RUN_TEST(test_little_endian_byte_order);
  RUN_TEST(test_error_message_detail);
  RUN_TEST(test_boundary_exact_values);

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
