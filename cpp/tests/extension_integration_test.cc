/**
 * SPZ Extension Integration Tests
 *
 * End-to-end integration tests for SPZ extension validation framework.
 * Tests complete workflow from SPZ parsing to extension validation.
 *
 * Test Coverage:
 * 1. SPZ without extensions (backward compatibility)
 * 2. SPZ with valid Adobe extension
 * 3. SPZ with invalid Adobe extension (payload errors)
 * 4. SPZ with unknown extension type
 * 5. SPZ with multiple extensions (Adobe + unknown)
 * 6. SPZ version 4 accepted without warning
 * 7. SPZ version > 4 emits warning but continues
 *
 * @author PuJunhan
 * @copyright Copyright (c) 2026 PuJunhan
 * @license MIT
 */

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>
#include <zlib.h>

#include "spz_gatekeeper/extension_validator.h"
#include "spz_gatekeeper/safe_orbit_camera_validator.h"
#include "spz_gatekeeper/spz.h"
#include "spz_gatekeeper/validator_registry.h"

namespace {

constexpr float kHalfPi = 1.57079632679f;
constexpr uint8_t kFlagHasExtensions = spz_gatekeeper::kFlagHasExtensions;

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

#define ASSERT_GT(a, b) do { \
  if ((a) <= (b)) { \
    throw std::runtime_error("Assertion failed: " #a " > " #b); \
  } \
} while(0)

std::vector<uint8_t> create_minimal_spz(
    uint32_t num_points = 1,
    uint8_t version = 1,
    uint8_t sh_degree = 0,
    uint8_t fractional_bits = 8,
    uint8_t flags = 0x00,
    const std::vector<uint8_t>* trailer = nullptr) {
  std::vector<uint8_t> uncompressed;

  uncompressed.push_back(0x4e);
  uncompressed.push_back(0x47);
  uncompressed.push_back(0x53);
  uncompressed.push_back(0x50);

  uncompressed.push_back(version);
  uncompressed.push_back(0x00);
  uncompressed.push_back(0x00);
  uncompressed.push_back(0x00);

  uncompressed.push_back(num_points & 0xFF);
  uncompressed.push_back((num_points >> 8) & 0xFF);
  uncompressed.push_back((num_points >> 16) & 0xFF);
  uncompressed.push_back((num_points >> 24) & 0xFF);

  uncompressed.push_back(sh_degree);
  uncompressed.push_back(fractional_bits);
  uncompressed.push_back(flags);
  uncompressed.push_back(0x00);

  const bool uses_float16 = (version == 1);
  const bool uses_quat_smallest_three = (version >= 3);
  auto sh_dim_for_degree = [](uint8_t degree) -> int {
    switch (degree) {
      case 0: return 0;
      case 1: return 3;
      case 2: return 8;
      case 3: return 15;
      case 4: return 24;
      default: return 0;
    }
  };

  const uint32_t n = num_points;
  const int positions_bytes_per_point = uses_float16 ? 6 : 9;
  const int alphas_bytes_per_point = 1;
  const int colors_bytes_per_point = 3;
  const int scales_bytes_per_point = 3;
  const int rotations_bytes_per_point = uses_quat_smallest_three ? 4 : 3;
  const int sh_bytes_per_point = sh_dim_for_degree(sh_degree) * 3;

  uncompressed.insert(uncompressed.end(), n * positions_bytes_per_point, 0);
  uncompressed.insert(uncompressed.end(), n * alphas_bytes_per_point, 255);
  uncompressed.insert(uncompressed.end(), n * colors_bytes_per_point, 0);
  uncompressed.insert(uncompressed.end(), n * scales_bytes_per_point, 0);
  uncompressed.insert(uncompressed.end(), n * rotations_bytes_per_point, 0);
  uncompressed.insert(uncompressed.end(), n * sh_bytes_per_point, 0);

  if (trailer) {
    uncompressed.insert(uncompressed.end(), trailer->begin(), trailer->end());
  }

  std::vector<uint8_t> compressed(1024);
  z_stream strm = {};
  if (deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
    throw std::runtime_error("deflateInit failed");
  }

  strm.next_in = uncompressed.data();
  strm.avail_in = static_cast<uInt>(uncompressed.size());
  strm.next_out = compressed.data();
  strm.avail_out = static_cast<uInt>(compressed.size());

  int ret = deflate(&strm, Z_FINISH);
  if (ret != Z_STREAM_END) {
    deflateEnd(&strm);
    throw std::runtime_error("deflate failed");
  }

  compressed.resize(strm.total_out);
  deflateEnd(&strm);
  return compressed;
}

std::vector<uint8_t> create_trailer(const std::vector<std::pair<uint32_t, std::vector<uint8_t>>>& records) {
  std::vector<uint8_t> trailer;
  for (const auto& rec : records) {
    trailer.push_back(rec.first & 0xFF);
    trailer.push_back((rec.first >> 8) & 0xFF);
    trailer.push_back((rec.first >> 16) & 0xFF);
    trailer.push_back((rec.first >> 24) & 0xFF);

    uint32_t len = static_cast<uint32_t>(rec.second.size());
    trailer.push_back(len & 0xFF);
    trailer.push_back((len >> 8) & 0xFF);
    trailer.push_back((len >> 16) & 0xFF);
    trailer.push_back((len >> 24) & 0xFF);

    trailer.insert(trailer.end(), rec.second.begin(), rec.second.end());
  }
  return trailer;
}

std::vector<uint8_t> write_float(float value) {
  std::vector<uint8_t> bytes(4);
  std::memcpy(bytes.data(), &value, sizeof(float));
  return bytes;
}

TEST(test_spz_no_extensions) {
  auto adobe_validator = std::make_shared<spz_gatekeeper::AdobeSafeOrbitCameraValidator>();
  spz_gatekeeper::ExtensionValidatorRegistry::Instance().RegisterValidator(0xADBE0002u, adobe_validator);

  auto spz_data = create_minimal_spz(1, 3, 0, 8, 0x00, nullptr);
  spz_gatekeeper::SpzInspectOptions opt;
  opt.strict = false;

  auto report = spz_gatekeeper::InspectSpzBlob(spz_data, opt, "test");
  ASSERT_FALSE(report.HasErrors());
  ASSERT_EQ(report.extension_reports.size(), 0u);
  ASSERT_EQ(report.spz_l2.value().num_points, 1u);
  ASSERT_EQ(report.spz_l2.value().version, 3u);
}

TEST(test_spz_valid_adobe_extension) {
  auto adobe_validator = std::make_shared<spz_gatekeeper::AdobeSafeOrbitCameraValidator>();
  spz_gatekeeper::ExtensionValidatorRegistry::Instance().RegisterValidator(0xADBE0002u, adobe_validator);

  std::vector<uint8_t> adobe_payload;
  auto min_elev = write_float(-0.5f);
  auto max_elev = write_float(0.5f);
  auto min_rad = write_float(1.0f);
  adobe_payload.insert(adobe_payload.end(), min_elev.begin(), min_elev.end());
  adobe_payload.insert(adobe_payload.end(), max_elev.begin(), max_elev.end());
  adobe_payload.insert(adobe_payload.end(), min_rad.begin(), min_rad.end());

  auto trailer = create_trailer({{0xADBE0002u, adobe_payload}});
  auto spz_data = create_minimal_spz(1, 3, 0, 8, kFlagHasExtensions, &trailer);

  spz_gatekeeper::SpzInspectOptions opt;
  opt.strict = false;

  auto report = spz_gatekeeper::InspectSpzBlob(spz_data, opt, "test");
  ASSERT_FALSE(report.HasErrors());
  ASSERT_EQ(report.extension_reports.size(), 1u);
  ASSERT_EQ(report.extension_reports[0].type, 0xADBE0002u);
  ASSERT_EQ(report.extension_reports[0].vendor_name, "Adobe");
  ASSERT_EQ(report.extension_reports[0].extension_name, "Adobe Safe Orbit Camera");
  ASSERT_TRUE(report.extension_reports[0].validation_result);
  ASSERT_TRUE(report.extension_reports[0].error_message.empty());
  ASSERT_TRUE(report.spz_l2.has_value());
  ASSERT_EQ(report.spz_l2->tlv_records.size(), 1u);
  auto payload_view = report.spz_l2->tlv_records[0].ValueView();
  ASSERT_EQ(payload_view.size(), adobe_payload.size());
  ASSERT_TRUE(std::memcmp(payload_view.data(), adobe_payload.data(), adobe_payload.size()) == 0);
  auto payload_copy = report.spz_l2->tlv_records[0].CopyValue();
  ASSERT_EQ(payload_copy.size(), adobe_payload.size());
  ASSERT_TRUE(std::memcmp(payload_copy.data(), adobe_payload.data(), adobe_payload.size()) == 0);
}

TEST(test_spz_invalid_adobe_extension_wrong_size) {
  auto adobe_validator = std::make_shared<spz_gatekeeper::AdobeSafeOrbitCameraValidator>();
  spz_gatekeeper::ExtensionValidatorRegistry::Instance().RegisterValidator(0xADBE0002u, adobe_validator);

  std::vector<uint8_t> invalid_payload(8, 0x00);
  auto trailer = create_trailer({{0xADBE0002u, invalid_payload}});
  auto spz_data = create_minimal_spz(1, 1, 0, 8, kFlagHasExtensions, &trailer);

  spz_gatekeeper::SpzInspectOptions opt;
  opt.strict = false;

  auto report = spz_gatekeeper::InspectSpzBlob(spz_data, opt, "test");
  ASSERT_TRUE(report.HasErrors());
  ASSERT_GT(report.issues.size(), 0u);
  ASSERT_EQ(report.extension_reports.size(), 1u);
  ASSERT_FALSE(report.extension_reports[0].validation_result);
  ASSERT_FALSE(report.extension_reports[0].error_message.empty());
  ASSERT_TRUE(report.extension_reports[0].error_message.find("Invalid payload size") != std::string::npos);
}

TEST(test_spz_unknown_extension_type) {
  std::vector<uint8_t> unknown_payload = {0x01, 0x02, 0x03, 0x04};
  auto trailer = create_trailer({{0xDEADBEEFu, unknown_payload}});
  auto spz_data = create_minimal_spz(1, 3, 0, 8, kFlagHasExtensions, &trailer);

  spz_gatekeeper::SpzInspectOptions opt;
  opt.strict = false;

  auto report = spz_gatekeeper::InspectSpzBlob(spz_data, opt, "test");
  ASSERT_FALSE(report.HasErrors());
  ASSERT_EQ(report.extension_reports.size(), 1u);
  ASSERT_EQ(report.extension_reports[0].type, 0xDEADBEEFu);
  ASSERT_EQ(report.extension_reports[0].extension_name, "Unknown");
  ASSERT_TRUE(report.extension_reports[0].validation_result);
  ASSERT_TRUE(report.ToJson().find("\"extension_reports\"") != std::string::npos);
  ASSERT_GT(report.issues.size(), 0u);
}

TEST(test_spz_multiple_extensions) {
  auto adobe_validator = std::make_shared<spz_gatekeeper::AdobeSafeOrbitCameraValidator>();
  spz_gatekeeper::ExtensionValidatorRegistry::Instance().RegisterValidator(0xADBE0002u, adobe_validator);

  std::vector<uint8_t> adobe_payload;
  auto v1 = write_float(-0.5f);
  auto v2 = write_float(1.0f);
  auto v3 = write_float(2.0f);
  adobe_payload.insert(adobe_payload.end(), v1.begin(), v1.end());
  adobe_payload.insert(adobe_payload.end(), v2.begin(), v2.end());
  adobe_payload.insert(adobe_payload.end(), v3.begin(), v3.end());

  std::vector<uint8_t> unknown_payload = {0xAA, 0xBB, 0xCC};
  auto trailer = create_trailer({{0xADBE0002u, adobe_payload}, {0xCAFE0001u, unknown_payload}});
  auto spz_data = create_minimal_spz(1, 3, 0, 8, kFlagHasExtensions, &trailer);

  spz_gatekeeper::SpzInspectOptions opt;
  opt.strict = false;

  auto report = spz_gatekeeper::InspectSpzBlob(spz_data, opt, "test");
  ASSERT_FALSE(report.HasErrors());
  ASSERT_EQ(report.extension_reports.size(), 2u);
  ASSERT_EQ(report.extension_reports[0].type, 0xADBE0002u);
  ASSERT_TRUE(report.extension_reports[0].validation_result);
  ASSERT_EQ(report.extension_reports[1].type, 0xCAFE0001u);
  ASSERT_EQ(report.extension_reports[1].extension_name, "Unknown");
  ASSERT_TRUE(report.extension_reports[1].validation_result);
  ASSERT_GT(report.issues.size(), 0u);
}

TEST(test_spz_version_four_ok) {
  auto spz_data = create_minimal_spz(1, 4, 0, 8, 0x00);

  spz_gatekeeper::SpzInspectOptions opt;
  opt.strict = false;

  auto report = spz_gatekeeper::InspectSpzBlob(spz_data, opt, "test");
  ASSERT_FALSE(report.HasErrors());
  ASSERT_EQ(report.issues.size(), 0u);
  ASSERT_EQ(report.spz_l2.value().version, 4u);
}

TEST(test_spz_new_version_warning) {
  auto spz_data = create_minimal_spz(1, 5, 0, 8, 0x00);

  spz_gatekeeper::SpzInspectOptions opt;
  opt.strict = false;

  auto report = spz_gatekeeper::InspectSpzBlob(spz_data, opt, "test");
  ASSERT_FALSE(report.HasErrors());
  ASSERT_GT(report.issues.size(), 0u);
  bool has_version_warning = false;
  for (const auto& issue : report.issues) {
    if (issue.message.find("newer than known max") != std::string::npos) {
      has_version_warning = true;
      break;
    }
  }
  ASSERT_TRUE(has_version_warning);
}

TEST(test_spz_very_new_version_warning) {
  auto spz_data = create_minimal_spz(1, 100, 0, 8, 0x00);

  spz_gatekeeper::SpzInspectOptions opt;
  opt.strict = false;

  auto report = spz_gatekeeper::InspectSpzBlob(spz_data, opt, "test");
  ASSERT_FALSE(report.HasErrors());
  bool has_version_warning = false;
  for (const auto& issue : report.issues) {
    if (issue.message.find("newer than known max") != std::string::npos) {
      has_version_warning = true;
      break;
    }
  }
  ASSERT_TRUE(has_version_warning);
}

TEST(test_spz_invalid_version_error) {
  auto spz_data = create_minimal_spz(1, 0, 0, 8, 0x00);

  spz_gatekeeper::SpzInspectOptions opt;
  opt.strict = false;

  auto report = spz_gatekeeper::InspectSpzBlob(spz_data, opt, "test");
  ASSERT_TRUE(report.HasErrors());
  bool has_invalid_version_error = false;
  for (const auto& issue : report.issues) {
    if (issue.message.find("invalid version") != std::string::npos) {
      has_invalid_version_error = true;
      break;
    }
  }
  ASSERT_TRUE(has_invalid_version_error);
}

TEST(test_spz_invalid_adobe_extension_range_error) {
  auto adobe_validator = std::make_shared<spz_gatekeeper::AdobeSafeOrbitCameraValidator>();
  spz_gatekeeper::ExtensionValidatorRegistry::Instance().RegisterValidator(0xADBE0002u, adobe_validator);

  std::vector<uint8_t> invalid_payload;
  auto p1 = write_float(2.0f);
  auto p2 = write_float(0.5f);
  auto p3 = write_float(1.0f);
  invalid_payload.insert(invalid_payload.end(), p1.begin(), p1.end());
  invalid_payload.insert(invalid_payload.end(), p2.begin(), p2.end());
  invalid_payload.insert(invalid_payload.end(), p3.begin(), p3.end());

  auto trailer = create_trailer({{0xADBE0002u, invalid_payload}});
  auto spz_data = create_minimal_spz(1, 3, 0, 8, kFlagHasExtensions, &trailer);

  spz_gatekeeper::SpzInspectOptions opt;
  opt.strict = false;

  auto report = spz_gatekeeper::InspectSpzBlob(spz_data, opt, "test");
  ASSERT_TRUE(report.HasErrors());
  ASSERT_GT(report.issues.size(), 0u);
  ASSERT_EQ(report.extension_reports.size(), 1u);
  ASSERT_FALSE(report.extension_reports[0].validation_result);
  ASSERT_FALSE(report.extension_reports[0].error_message.empty());
  ASSERT_TRUE(report.extension_reports[0].error_message.find("radians") != std::string::npos);
}

}  // namespace

int main() {
  std::cout << "=== SPZ Extension Integration Tests ===" << std::endl;
  std::cout << std::endl;

  RUN_TEST(test_spz_no_extensions);
  RUN_TEST(test_spz_valid_adobe_extension);
  RUN_TEST(test_spz_invalid_adobe_extension_wrong_size);
  RUN_TEST(test_spz_unknown_extension_type);
  RUN_TEST(test_spz_multiple_extensions);
  RUN_TEST(test_spz_version_four_ok);
  RUN_TEST(test_spz_new_version_warning);
  RUN_TEST(test_spz_very_new_version_warning);
  RUN_TEST(test_spz_invalid_version_error);
  RUN_TEST(test_spz_invalid_adobe_extension_range_error);

  std::cout << std::endl;
  std::cout << "=== Test Summary ===" << std::endl;
  std::cout << "Tests run: " << g_tests_run << std::endl;
  std::cout << "Tests passed: " << g_tests_passed << std::endl;
  std::cout << "Tests failed: " << (g_tests_run - g_tests_passed) << std::endl;

  if (g_tests_passed == g_tests_run) {
    std::cout << "Result: ALL " << g_tests_passed << " INTEGRATION TESTS PASSED ✓" << std::endl;
    return EXIT_SUCCESS;
  }

  std::cout << "Result: " << (g_tests_run - g_tests_passed) << " INTEGRATION TESTS FAILED ✗" << std::endl;
  return EXIT_FAILURE;
}
