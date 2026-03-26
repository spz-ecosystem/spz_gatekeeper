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
  ASSERT_TRUE(std::string(spz_gatekeeper::kAuditPolicyName) == "spz_gatekeeper_policy");
  ASSERT_TRUE(std::string(spz_gatekeeper::kAuditPolicyVersion) == "2.0.0");
  ASSERT_TRUE(std::string(spz_gatekeeper::kAuditPolicyModeDev) == "dev");
  ASSERT_TRUE(std::string(spz_gatekeeper::kAuditPolicyModeRelease) == "release");
  ASSERT_TRUE(std::string(spz_gatekeeper::kAuditPolicyModeChallenge) == "challenge");
  ASSERT_TRUE(std::string(spz_gatekeeper::kAuditModeBrowserLightweightWasmAudit) ==
              "browser_lightweight_wasm_audit");
  ASSERT_TRUE(std::string(spz_gatekeeper::kAuditModeLocalCliSpzArtifactAudit) ==
              "local_cli_spz_artifact_audit");
}


TEST(test_build_browser_wasm_audit_json_reports_pass_schema) {
  spz_gatekeeper::BrowserWasmAuditReport report;
  report.bundle_id = "sha256:test-bundle";
  report.verdict = "pass";
  report.next_action = "allow_local_cli_audit";
  report.audit_duration_ms = 12.5;
  report.summary.bundle_name = "bundle.zip";
  report.summary.file_count = 5;
  report.summary.issue_count = 0;
  report.summary.declared_export_count = 3;
  report.summary.loader_export_count = 3;
  report.summary.wasm_export_count = 1;
  report.summary.valid_tiny_passed = true;
  report.summary.invalid_tiny_handled = true;
  report.summary.runtime_available = true;
  report.manifest_summary_json = "{\"profile\":\"spz\"}";
  report.budgets_json = "{\"cold_start_ms\":{\"status\":\"within_budget\"}}";
  report.issues_json = "[]";
  report.bundle_entries_json = "[]";
  report.wasm_export_summary_json = "[{\"name\":\"coreCheck\",\"kind\":\"function\"}]";
  report.empty_shell_risk = false;
  report.copy_budget_wired = true;
  report.memory_budget_wired = true;
  report.performance_budget_wired = true;


  const std::string json = spz_gatekeeper::BuildBrowserWasmAuditJson(report);
  ASSERT_TRUE(json.find("\"audit_profile\":\"spz\"") != std::string::npos);
  ASSERT_TRUE(json.find("\"policy_name\":\"spz_gatekeeper_policy\"") != std::string::npos);
  ASSERT_TRUE(json.find("\"policy_version\":\"2.0.0\"") != std::string::npos);
  ASSERT_TRUE(json.find("\"policy_mode\":\"release\"") != std::string::npos);
  ASSERT_TRUE(json.find("\"audit_mode\":\"browser_lightweight_wasm_audit\"") !=
              std::string::npos);

  ASSERT_TRUE(json.find("\"bundle_id\":\"sha256:test-bundle\"") != std::string::npos);
  ASSERT_TRUE(json.find("\"summary\":{") != std::string::npos);
  ASSERT_TRUE(json.find("\"manifest_summary\":{\"profile\":\"spz\"}") !=
              std::string::npos);
  ASSERT_TRUE(json.find("\"bundle_entries\":[]") != std::string::npos);
  ASSERT_TRUE(json.find("\"wasm_quality_gate\":{") != std::string::npos);
  ASSERT_TRUE(json.find("\"warning_budget_wired\":true") != std::string::npos);
  ASSERT_TRUE(json.find("\"copy_budget_wired\":true") != std::string::npos);
  ASSERT_TRUE(json.find("\"memory_budget_wired\":true") != std::string::npos);

  ASSERT_TRUE(json.find("\"performance_budget_wired\":true") != std::string::npos);
  ASSERT_TRUE(json.find("\"final_verdict\":\"pass\"") != std::string::npos);
  ASSERT_TRUE(json.find("\"release_ready\":true") != std::string::npos);
}


TEST(test_build_browser_wasm_audit_json_derives_release_ready_from_final_verdict) {
  spz_gatekeeper::BrowserWasmAuditReport report;
  report.bundle_id = "sha256:test-bundle";
  report.policy_mode = spz_gatekeeper::kAuditPolicyModeDev;
  report.verdict = "pass";
  report.final_verdict = "review_required";
  report.has_release_ready = true;
  report.release_ready = true;
  report.next_action = "review_bundle_before_cli";
  report.audit_duration_ms = 8.0;
  report.summary.bundle_name = "bundle.zip";
  report.summary.file_count = 5;
  report.summary.issue_count = 1;
  report.summary.declared_export_count = 3;
  report.summary.loader_export_count = 3;
  report.summary.wasm_export_count = 1;
  report.summary.valid_tiny_passed = true;
  report.summary.invalid_tiny_handled = true;
  report.summary.runtime_available = true;
  report.manifest_summary_json = "{\"profile\":\"spz\"}";
  report.budgets_json = "{\"cold_start_ms\":{\"status\":\"within_budget\"}}";
  report.issues_json = "[{\"severity\":\"warning\",\"code\":\"BUNDLE_REVIEW\"}]";
  report.bundle_entries_json = "[]";
  report.wasm_export_summary_json = "[]";
  report.empty_shell_risk = false;
  report.copy_budget_wired = true;
  report.memory_budget_wired = true;
  report.performance_budget_wired = true;

  const std::string json = spz_gatekeeper::BuildBrowserWasmAuditJson(report);
  ASSERT_TRUE(json.find("\"policy_mode\":\"dev\"") != std::string::npos);
  ASSERT_TRUE(json.find("\"bundle_verdict\":\"pass\"") != std::string::npos);
  ASSERT_TRUE(json.find("\"final_verdict\":\"review_required\"") != std::string::npos);
  ASSERT_TRUE(json.find("\"release_ready\":false") != std::string::npos);
  ASSERT_TRUE(json.find("\"artifact_audit_wired\":false,\"release_ready\":false") !=
              std::string::npos);
}

TEST(test_build_browser_wasm_audit_json_includes_copy_breakdown) {
  spz_gatekeeper::BrowserWasmAuditReport report;
  report.bundle_id = "sha256:test-bundle";
  report.verdict = "pass";
  report.next_action = "allow_local_cli_audit";
  report.audit_duration_ms = 4.0;
  report.summary.bundle_name = "bundle.zip";
  report.summary.file_count = 3;
  report.summary.issue_count = 0;
  report.summary.declared_export_count = 3;
  report.summary.loader_export_count = 3;
  report.summary.wasm_export_count = 1;
  report.summary.valid_tiny_passed = true;
  report.summary.invalid_tiny_handled = true;
  report.summary.runtime_available = true;
  report.manifest_summary_json = "{\"profile\":\"spz\"}";
  report.budgets_json = "{\"copy_pass_limit\":{\"declared\":2,\"observed\":2,\"status\":\"within_budget\",\"within_budget\":true}}";
  report.copy_breakdown_json =
      "{\"total_passes\":2,\"stages\":[{\"name\":\"zip_inflate\",\"count\":1},{\"name\":\"module_clone\",\"count\":1}]}";
  report.issues_json = "[]";
  report.bundle_entries_json = "[]";
  report.wasm_export_summary_json = "[]";
  report.copy_budget_wired = true;

  const std::string json = spz_gatekeeper::BuildBrowserWasmAuditJson(report);
  ASSERT_TRUE(json.find("\"copy_breakdown\":{\"total_passes\":2") != std::string::npos);
  ASSERT_TRUE(json.find("\"name\":\"zip_inflate\"") != std::string::npos);
  ASSERT_TRUE(json.find("\"name\":\"module_clone\"") != std::string::npos);
}

TEST(test_build_compat_check_audit_json_reports_pass_schema) {
  spz_gatekeeper::GateReport strict_report;
  strict_report.asset_path = "fixture_valid.spz";
  strict_report.extension_reports.push_back(MakeKnownExtension());

  spz_gatekeeper::GateReport non_strict_report = strict_report;
  spz_gatekeeper::CompatAuditMetrics metrics;
  metrics.peak_memory_mb = 42.0;
  metrics.has_peak_memory_mb = true;
  metrics.memory_growth_count = 1;
  metrics.has_memory_growth_count = true;

  const std::string json = spz_gatekeeper::BuildCompatCheckAuditJson(
      "fixture_valid.spz", strict_report, non_strict_report, &metrics);

  ASSERT_TRUE(json.find("\"audit_profile\":\"spz\"") != std::string::npos);
  ASSERT_TRUE(json.find("\"policy_name\":\"spz_gatekeeper_policy\"") != std::string::npos);
  ASSERT_TRUE(json.find("\"policy_version\":\"2.0.0\"") != std::string::npos);
  ASSERT_TRUE(json.find("\"policy_mode\":\"release\"") != std::string::npos);
  ASSERT_TRUE(json.find("\"audit_mode\":\"local_cli_spz_artifact_audit\"") != std::string::npos);
  ASSERT_TRUE(json.find("\"verdict\":\"pass\"") != std::string::npos);

  ASSERT_TRUE(json.find("\"summary\":{") != std::string::npos);
  ASSERT_TRUE(json.find("\"artifact_summary\":{") != std::string::npos);
  ASSERT_TRUE(json.find("\"budgets\":{") != std::string::npos);
  ASSERT_TRUE(json.find("\"peak_memory_mb\":{\"declared\":256,\"observed\":42,\"status\":\"within_budget\",\"within_budget\":true}") != std::string::npos);
  ASSERT_TRUE(json.find("\"memory_growth_count\":{\"declared\":1,\"observed\":1,\"status\":\"within_budget\",\"within_budget\":true}") != std::string::npos);
  ASSERT_TRUE(json.find("\"issues\":[") != std::string::npos);

  ASSERT_TRUE(json.find("\"next_action\":\"artifact_ready\"") != std::string::npos);
  ASSERT_TRUE(json.find("\"memory_budget_wired\":true") != std::string::npos);
  ASSERT_TRUE(json.find("\"wasm_quality_gate\":{\"coverage_level\":\"baseline\"") != std::string::npos);
  ASSERT_TRUE(json.find("\"final_verdict\":\"pass\"") != std::string::npos);
  ASSERT_TRUE(json.find("\"release_ready\":true") != std::string::npos);
}

TEST(test_build_compat_check_audit_json_escalates_release_when_memory_budget_not_collected) {
  spz_gatekeeper::GateReport strict_report;
  strict_report.asset_path = "fixture_valid.spz";
  strict_report.extension_reports.push_back(MakeKnownExtension());

  spz_gatekeeper::GateReport non_strict_report = strict_report;
  spz_gatekeeper::CompatAuditMetrics metrics;

  const std::string json = spz_gatekeeper::BuildCompatCheckAuditJson(
      "fixture_valid.spz",
      strict_report,
      non_strict_report,
      &metrics,
      spz_gatekeeper::kAuditPolicyModeRelease);

  ASSERT_TRUE(json.find("\"policy_mode\":\"release\"") != std::string::npos);
  ASSERT_TRUE(json.find("\"peak_memory_mb\":{\"declared\":256,\"observed\":null,\"status\":\"not_collected\",\"within_budget\":null}") != std::string::npos);
  ASSERT_TRUE(json.find("\"memory_growth_count\":{\"declared\":1,\"observed\":null,\"status\":\"not_collected\",\"within_budget\":null}") != std::string::npos);
  ASSERT_TRUE(json.find("ARTIFACT_MEMORY_BUDGET_NOT_COLLECTED") != std::string::npos);
  ASSERT_TRUE(json.find("ARTIFACT_MEMORY_GROWTH_BUDGET_NOT_COLLECTED") != std::string::npos);
  ASSERT_TRUE(json.find("\"final_verdict\":\"review_required\"") != std::string::npos);
  ASSERT_TRUE(json.find("\"release_ready\":false") != std::string::npos);
  ASSERT_TRUE(json.find("\"next_action\":\"review_artifact\"") != std::string::npos);
}

TEST(test_build_compat_check_audit_json_keeps_dev_mode_observed_without_budget) {
  spz_gatekeeper::GateReport strict_report;
  strict_report.asset_path = "fixture_valid.spz";
  strict_report.extension_reports.push_back(MakeKnownExtension());

  spz_gatekeeper::GateReport non_strict_report = strict_report;
  spz_gatekeeper::CompatAuditMetrics metrics;
  metrics.peak_memory_mb = 42.0;
  metrics.has_peak_memory_mb = true;
  metrics.memory_growth_count = 2;
  metrics.has_memory_growth_count = true;

  const std::string json = spz_gatekeeper::BuildCompatCheckAuditJson(
      "fixture_valid.spz",
      strict_report,
      non_strict_report,
      &metrics,
      spz_gatekeeper::kAuditPolicyModeDev);

  ASSERT_TRUE(json.find("\"policy_mode\":\"dev\"") != std::string::npos);
  ASSERT_TRUE(json.find("\"peak_memory_mb\":{\"declared\":null,\"observed\":42,\"status\":\"observed_without_budget\",\"within_budget\":null}") != std::string::npos);
  ASSERT_TRUE(json.find("\"memory_growth_count\":{\"declared\":null,\"observed\":2,\"status\":\"observed_without_budget\",\"within_budget\":null}") != std::string::npos);
  ASSERT_TRUE(json.find("ARTIFACT_MEMORY_BUDGET_NOT_COLLECTED") == std::string::npos);
  ASSERT_TRUE(json.find("ARTIFACT_MEMORY_OVER_BUDGET") == std::string::npos);
  ASSERT_TRUE(json.find("\"final_verdict\":\"pass\"") != std::string::npos);
  ASSERT_TRUE(json.find("\"release_ready\":true") != std::string::npos);
}

TEST(test_build_compat_check_audit_json_escalates_challenge_when_memory_over_budget) {
  spz_gatekeeper::GateReport strict_report;
  strict_report.asset_path = "fixture_valid.spz";
  strict_report.extension_reports.push_back(MakeKnownExtension());

  spz_gatekeeper::GateReport non_strict_report = strict_report;
  spz_gatekeeper::CompatAuditMetrics metrics;
  metrics.peak_memory_mb = 300.0;
  metrics.has_peak_memory_mb = true;
  metrics.memory_growth_count = 2;
  metrics.has_memory_growth_count = true;

  const std::string json = spz_gatekeeper::BuildCompatCheckAuditJson(
      "fixture_valid.spz",
      strict_report,
      non_strict_report,
      &metrics,
      spz_gatekeeper::kAuditPolicyModeChallenge);

  ASSERT_TRUE(json.find("\"policy_mode\":\"challenge\"") != std::string::npos);
  ASSERT_TRUE(json.find("\"peak_memory_mb\":{\"declared\":256,\"observed\":300,\"status\":\"over_budget\",\"within_budget\":false}") != std::string::npos);
  ASSERT_TRUE(json.find("\"memory_growth_count\":{\"declared\":1,\"observed\":2,\"status\":\"over_budget\",\"within_budget\":false}") != std::string::npos);
  ASSERT_TRUE(json.find("ARTIFACT_MEMORY_OVER_BUDGET") != std::string::npos);
  ASSERT_TRUE(json.find("ARTIFACT_MEMORY_GROWTH_OVER_BUDGET") != std::string::npos);
  ASSERT_TRUE(json.find("\"final_verdict\":\"review_required\"") != std::string::npos);
  ASSERT_TRUE(json.find("\"release_ready\":false") != std::string::npos);
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

TEST(test_parse_browser_handoff_and_merge_into_compat_audit_json) {
  spz_gatekeeper::GateReport strict_report;
  strict_report.asset_path = "fixture_valid.spz";
  strict_report.extension_reports.push_back(MakeKnownExtension());

  spz_gatekeeper::GateReport non_strict_report = strict_report;
  const std::string compat_json =
      spz_gatekeeper::BuildCompatCheckAuditJson("fixture_valid.spz", strict_report, non_strict_report);

  const std::string handoff_json =
      "{\"audit_profile\":\"spz\",\"audit_mode\":\"browser_lightweight_wasm_audit\","
      "\"bundle_id\":\"sha256:test-bundle\",\"tool_version\":\"1.0.0\","
      "\"verdict\":\"review_required\",\"summary\":{},\"budgets\":{},\"issues\":[],"
      "\"next_action\":\"review_bundle_before_cli\"}";

  spz_gatekeeper::BrowserAuditHandoff handoff;
  std::string err;
  ASSERT_TRUE(spz_gatekeeper::ParseBrowserAuditHandoffJson(handoff_json, &handoff, &err));

  const std::string merged =
      spz_gatekeeper::BuildCompatCheckAuditWithHandoffJson(compat_json, "pass", handoff);
  ASSERT_TRUE(merged.find("\"handoff\":{") != std::string::npos);
  ASSERT_TRUE(merged.find("\"upstream_audit\":{") != std::string::npos);
  ASSERT_TRUE(merged.find("\"policy_name\":\"spz_gatekeeper_policy\"") != std::string::npos);
  ASSERT_TRUE(merged.find("\"policy_version\":\"2.0.0\"") != std::string::npos);
  ASSERT_TRUE(merged.find("\"policy_mode\":\"release\"") != std::string::npos);
  ASSERT_TRUE(merged.find("\"bundle_id\":\"sha256:test-bundle\"") != std::string::npos);
  ASSERT_TRUE(merged.find("\"tool_version\":\"1.0.0\"") != std::string::npos);
  ASSERT_TRUE(merged.find("\"evidence_chain\":[\"browser_lightweight_wasm_audit\",\"local_cli_spz_artifact_audit\"]") != std::string::npos);
  ASSERT_TRUE(merged.find("\"final_verdict\":\"pass\"") != std::string::npos);
  ASSERT_TRUE(merged.find("\"release_ready\":true") != std::string::npos);
}

TEST(test_parse_browser_handoff_preserves_explicit_policy_mode) {
  const std::string handoff_json =
      "{\"audit_profile\":\"spz\",\"audit_mode\":\"browser_lightweight_wasm_audit\","
      "\"policy_mode\":\"dev\",\"bundle_id\":\"sha256:test-bundle\","
      "\"tool_version\":\"1.0.0\",\"verdict\":\"review_required\","
      "\"summary\":{},\"budgets\":{},\"issues\":[],"
      "\"next_action\":\"review_bundle_before_cli\"}";

  spz_gatekeeper::BrowserAuditHandoff handoff;
  std::string err;
  ASSERT_TRUE(spz_gatekeeper::ParseBrowserAuditHandoffJson(handoff_json, &handoff, &err));
  ASSERT_TRUE(handoff.policy_mode == spz_gatekeeper::kAuditPolicyModeDev);
}

TEST(test_parse_browser_handoff_prefers_bundle_verdict_field) {
  const std::string handoff_json =
      "{\"audit_profile\":\"spz\",\"audit_mode\":\"browser_lightweight_wasm_audit\","
      "\"policy_mode\":\"challenge\",\"bundle_id\":\"sha256:test-bundle\","
      "\"tool_version\":\"1.0.0\",\"bundle_verdict\":\"block\","
      "\"verdict\":\"review_required\",\"summary\":{},\"budgets\":{},\"issues\":[],"
      "\"next_action\":\"block_bundle\"}";

  spz_gatekeeper::BrowserAuditHandoff handoff;
  std::string err;
  ASSERT_TRUE(spz_gatekeeper::ParseBrowserAuditHandoffJson(handoff_json, &handoff, &err));
  ASSERT_TRUE(handoff.policy_mode == spz_gatekeeper::kAuditPolicyModeChallenge);
  ASSERT_TRUE(handoff.verdict == "block");
}

}  // namespace

int main() {
  std::cout << "=== audit_summary Tests ===" << std::endl;
  std::cout << std::endl;

  RUN_TEST(test_audit_summary_freezes_public_mode_constants);
  RUN_TEST(test_build_browser_wasm_audit_json_reports_pass_schema);
  RUN_TEST(test_build_browser_wasm_audit_json_derives_release_ready_from_final_verdict);
  RUN_TEST(test_build_browser_wasm_audit_json_includes_copy_breakdown);
  RUN_TEST(test_build_compat_check_audit_json_reports_pass_schema);
  RUN_TEST(test_build_compat_check_audit_json_escalates_release_when_memory_budget_not_collected);
  RUN_TEST(test_build_compat_check_audit_json_keeps_dev_mode_observed_without_budget);
  RUN_TEST(test_build_compat_check_audit_json_escalates_challenge_when_memory_over_budget);
  RUN_TEST(test_build_compat_check_audit_json_reports_review_required_for_unknown_extension);
  RUN_TEST(test_parse_browser_handoff_and_merge_into_compat_audit_json);
  RUN_TEST(test_parse_browser_handoff_preserves_explicit_policy_mode);
  RUN_TEST(test_parse_browser_handoff_prefers_bundle_verdict_field);

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
