#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

#include "spz_gatekeeper/audit_summary.h"
#include "spz_gatekeeper/report.h"

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
} while (0)

#define ASSERT_TRUE(cond) do { \
  if (!(cond)) { \
    throw std::runtime_error("Assertion failed: " #cond); \
  } \
} while (0)

spz_gatekeeper::ExtensionReport MakeKnownExtension() {
  spz_gatekeeper::ExtensionReport ext;
  ext.type = 0xADBE0002u;
  ext.extension_name = "Adobe Safe Orbit Camera";
  ext.known_extension = true;
  ext.has_validator = true;
  ext.validation_result = true;
  ext.status = "stable";
  return ext;
}

TEST(test_audit_summary_freezes_public_mode_constants) {
  ASSERT_TRUE(std::string(spz_gatekeeper::kAuditProfileSpz) == "spz");
  ASSERT_TRUE(std::string(spz_gatekeeper::kAuditModeBrowserLightweightWasmAudit) ==
              "browser_lightweight_wasm_audit");
  ASSERT_TRUE(std::string(spz_gatekeeper::kAuditModeLocalCliSpzArtifactAudit) ==
              "local_cli_spz_artifact_audit");
}

TEST(test_build_compat_check_audit_json_reports_pass_schema) {
  spz_gatekeeper::GateReport strict_report;
  strict_report.asset_path = "fixture_valid.spz";
  strict_report.extension_reports.push_back(MakeKnownExtension());

  spz_gatekeeper::GateReport non_strict_report = strict_report;

  const std::string json =
      spz_gatekeeper::BuildCompatCheckAuditJson("fixture_valid.spz", strict_report, non_strict_report);
  ASSERT_TRUE(json.find("\"audit_profile\":\"spz\"") != std::string::npos);
  ASSERT_TRUE(json.find("\"audit_mode\":\"local_cli_spz_artifact_audit\"") != std::string::npos);
  ASSERT_TRUE(json.find("\"verdict\":\"pass\"") != std::string::npos);
  ASSERT_TRUE(json.find("\"summary\":{") != std::string::npos);
  ASSERT_TRUE(json.find("\"artifact_summary\":{") != std::string::npos);
  ASSERT_TRUE(json.find("\"budgets\":{") != std::string::npos);
  ASSERT_TRUE(json.find("\"issues\":[") != std::string::npos);

  ASSERT_TRUE(json.find("\"next_action\":\"artifact_ready\"") != std::string::npos);
}

TEST(test_build_compat_check_audit_json_reports_review_required_for_unknown_extension) {
  spz_gatekeeper::GateReport strict_report;
  strict_report.asset_path = "fixture_unknown.spz";
  strict_report.issues.push_back({spz_gatekeeper::Severity::kWarning,
                                  "L2_EXT_UNKNOWN",
                                  "unknown extension encountered",
                                  "fixture_unknown.spz"});

  spz_gatekeeper::ExtensionReport unknown_ext;
  unknown_ext.type = 0xCAFE0001u;
  unknown_ext.extension_name = "Unknown (0xCAFE0001)";
  unknown_ext.known_extension = false;
  unknown_ext.has_validator = false;
  unknown_ext.validation_result = false;
  unknown_ext.status = "unknown";
  strict_report.extension_reports.push_back(unknown_ext);

  spz_gatekeeper::GateReport non_strict_report = strict_report;
  non_strict_report.issues.clear();

  const std::string json =
      spz_gatekeeper::BuildCompatCheckAuditJson("fixture_unknown.spz", strict_report, non_strict_report);
  ASSERT_TRUE(json.find("\"verdict\":\"review_required\"") != std::string::npos);
  ASSERT_TRUE(json.find("\"artifact_summary\":{") != std::string::npos);
  ASSERT_TRUE(json.find("\"budgets\":{") != std::string::npos);
  ASSERT_TRUE(json.find("\"next_action\":\"review_artifact\"") != std::string::npos);
  ASSERT_TRUE(json.find("L2_EXT_UNKNOWN") != std::string::npos);

}

}  // namespace

int main() {
  std::cout << "=== audit_summary Tests ===" << std::endl;
  std::cout << std::endl;

  RUN_TEST(test_audit_summary_freezes_public_mode_constants);
  RUN_TEST(test_build_compat_check_audit_json_reports_pass_schema);
  RUN_TEST(test_build_compat_check_audit_json_reports_review_required_for_unknown_extension);

  std::cout << std::endl;
  std::cout << "=== Test Summary ===" << std::endl;
  std::cout << "Tests run: " << g_tests_run << std::endl;
  std::cout << "Tests passed: " << g_tests_passed << std::endl;
  std::cout << "Tests failed: " << (g_tests_run - g_tests_passed) << std::endl;

  if (g_tests_passed == g_tests_run) {
    std::cout << "Result: ALL " << g_tests_passed << " AUDIT SUMMARY TESTS PASSED ✓" << std::endl;
    return EXIT_SUCCESS;
  }

  std::cout << "Result: " << (g_tests_run - g_tests_passed) << " AUDIT SUMMARY TESTS FAILED ✗"
            << std::endl;
  return EXIT_FAILURE;
}
