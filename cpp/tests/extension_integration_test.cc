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
#include <string>
#include <utility>
#include <vector>
#include <zlib.h>


#include "spz_gatekeeper/extension_spec_registry.h"
#include "spz_gatekeeper/extension_validator.h"
#include "spz_gatekeeper/safe_orbit_camera_validator.h"
#include "spz_gatekeeper/spz.h"
#include "spz_gatekeeper/validator_registry.h"
#include "test_fixtures.h"

namespace {


constexpr float kHalfPi = 1.57079632679f;
constexpr uint8_t kFlagHasExtensions = spz_gatekeeper::kFlagHasExtensions;

int g_tests_run = 0;
int g_tests_passed = 0;

bool has_issue_code(const spz_gatekeeper::GateReport& report, const char* code) {
  for (const auto& issue : report.issues) {
    if (issue.code == code) {
      return true;
    }
  }
  return false;
}

spz_gatekeeper::ExtensionSpec make_spec(uint32_t type,
                                        std::string vendor_name,
                                        std::string extension_name) {
  spz_gatekeeper::ExtensionSpec spec;
  spec.type = type;
  spec.vendor_id = static_cast<uint16_t>(type >> 16);
  spec.extension_id = static_cast<uint16_t>(type & 0xFFFFu);
  spec.vendor_name = std::move(vendor_name);
  spec.extension_name = std::move(extension_name);
  spec.category = "algorithm";
  spec.status = "draft";
  spec.spec_url = "docs/specs/test.md";
  spec.short_description = "integration test spec";
  spec.min_spz_version = 1;
  spec.requires_has_extensions_flag = true;
  return spec;
}

class AcceptAllTestValidator : public spz_gatekeeper::SpzExtensionValidator {
 public:
  explicit AcceptAllTestValidator(uint32_t type, std::string name)
      : type_(type), name_(std::move(name)) {}

  std::string GetName() const override {
    return name_;
  }

  uint32_t GetExtensionType() const override {
    return type_;
  }

  bool Validate(const uint8_t* data, size_t size, std::string* error) const override {
    if (size > 0 && data == nullptr) {
      if (error) {
        *error = "payload pointer must not be null";
      }
      return false;
    }
    if (error) {
      error->clear();
    }
    return true;
  }

 private:
  uint32_t type_;
  std::string name_;
};

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
  return spz_gatekeeper_test::CreateMinimalSpz(num_points, version, sh_degree,
                                               fractional_bits, flags, trailer);
}

std::vector<uint8_t> create_trailer(
    const std::vector<std::pair<uint32_t, std::vector<uint8_t>>>& records) {
  return spz_gatekeeper_test::CreateTrailer(records);
}

std::vector<uint8_t> write_float(float value) {
  return spz_gatekeeper_test::WriteFloat(value);
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
  ASSERT_TRUE(report.extension_reports[0].known_extension);
  ASSERT_TRUE(report.extension_reports[0].has_validator);
  ASSERT_EQ(report.extension_reports[0].status, "stable");
  ASSERT_EQ(report.extension_reports[0].category, "camera");
  ASSERT_FALSE(report.extension_reports[0].spec_url.empty());
  ASSERT_FALSE(report.extension_reports[0].short_description.empty());
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
  ASSERT_FALSE(report.extension_reports[0].known_extension);
  ASSERT_FALSE(report.extension_reports[0].has_validator);
  ASSERT_EQ(report.extension_reports[0].extension_name, "Unknown");
  ASSERT_TRUE(report.extension_reports[0].validation_result);
  ASSERT_TRUE(report.ToJson().find("\"extension_reports\"") != std::string::npos);
  ASSERT_TRUE(has_issue_code(report, "L2_EXT_UNKNOWN"));
  ASSERT_GT(report.issues.size(), 0u);
}

TEST(test_spz_registered_extension_without_validator) {
  const uint32_t type = 0xACDC0001u;
  auto spec = make_spec(type, "ACDC Labs", "Registered Without Validator");
  spz_gatekeeper::ExtensionSpecRegistry::Instance().RegisterSpec(spec);

  std::vector<uint8_t> payload = {0x10, 0x20, 0x30};
  auto trailer = create_trailer({{type, payload}});
  auto spz_data = create_minimal_spz(1, 3, 0, 8, kFlagHasExtensions, &trailer);

  spz_gatekeeper::SpzInspectOptions opt;
  opt.strict = false;

  auto report = spz_gatekeeper::InspectSpzBlob(spz_data, opt, "test");
  ASSERT_FALSE(report.HasErrors());
  ASSERT_EQ(report.extension_reports.size(), 1u);
  ASSERT_TRUE(report.extension_reports[0].known_extension);
  ASSERT_FALSE(report.extension_reports[0].has_validator);
  ASSERT_EQ(report.extension_reports[0].vendor_name, "ACDC Labs");
  ASSERT_EQ(report.extension_reports[0].extension_name, "Registered Without Validator");
  ASSERT_EQ(report.extension_reports[0].status, "draft");
  ASSERT_EQ(report.extension_reports[0].category, "algorithm");
  ASSERT_TRUE(report.extension_reports[0].validation_result);
  ASSERT_TRUE(has_issue_code(report, "L2_EXT_REGISTERED_NO_VALIDATOR"));
}

TEST(test_spz_unregistered_extension_with_validator) {
  const uint32_t type = 0xBEEF0002u;
  auto validator = std::make_shared<AcceptAllTestValidator>(type, "Unregistered Validator");
  spz_gatekeeper::ExtensionValidatorRegistry::Instance().RegisterValidator(type, validator);

  std::vector<uint8_t> payload = {0x01, 0x02};
  auto trailer = create_trailer({{type, payload}});
  auto spz_data = create_minimal_spz(1, 3, 0, 8, kFlagHasExtensions, &trailer);

  spz_gatekeeper::SpzInspectOptions opt;
  opt.strict = false;

  auto report = spz_gatekeeper::InspectSpzBlob(spz_data, opt, "test");
  ASSERT_FALSE(report.HasErrors());
  ASSERT_EQ(report.extension_reports.size(), 1u);
  ASSERT_FALSE(report.extension_reports[0].known_extension);
  ASSERT_TRUE(report.extension_reports[0].has_validator);
  ASSERT_EQ(report.extension_reports[0].extension_name, "Unregistered Validator");
  ASSERT_TRUE(report.extension_reports[0].validation_result);
  ASSERT_TRUE(has_issue_code(report, "L2_EXT_UNREGISTERED_VALIDATOR"));
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
  ASSERT_TRUE(report.extension_reports[0].known_extension);
  ASSERT_TRUE(report.extension_reports[0].has_validator);
  ASSERT_TRUE(report.extension_reports[0].validation_result);
  ASSERT_EQ(report.extension_reports[1].type, 0xCAFE0001u);
  ASSERT_FALSE(report.extension_reports[1].known_extension);
  ASSERT_FALSE(report.extension_reports[1].has_validator);
  ASSERT_EQ(report.extension_reports[1].extension_name, "Unknown");
  ASSERT_TRUE(report.extension_reports[1].validation_result);
  ASSERT_TRUE(has_issue_code(report, "L2_EXT_UNKNOWN"));
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
  RUN_TEST(test_spz_registered_extension_without_validator);
  RUN_TEST(test_spz_unregistered_extension_with_validator);
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
