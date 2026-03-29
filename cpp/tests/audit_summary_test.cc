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

void ExpectContains(const std::string& json, const std::string& needle) {
  if (json.find(needle) == std::string::npos) {
    throw std::runtime_error("Expected JSON to contain: " + needle);
  }
}

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

spz_gatekeeper::Issue MakeIssue(spz_gatekeeper::Severity severity,
                                const std::string& code,
                                const std::string& message) {
  spz_gatekeeper::Issue issue;
  issue.severity = severity;
  issue.code = code;
  issue.message = message;
  return issue;
}

spz_gatekeeper::AuditArtifactRecord MakeArtifactRecord(const std::string& artifact_id,
                                                       const std::string& artifact_kind,
                                                       const std::string& status,
                                                       const std::string& path,
                                                       std::uint64_t size_bytes = 0,
                                                       bool has_size_bytes = false) {
  spz_gatekeeper::AuditArtifactRecord artifact;
  artifact.artifact_id = artifact_id;
  artifact.artifact_kind = artifact_kind;
  artifact.source_stage = spz_gatekeeper::kAuditStageEncodeRun;
  artifact.status = status;
  artifact.path = path;
  artifact.media_type = "application/octet-stream";
  artifact.size_bytes = size_bytes;
  artifact.has_size_bytes = has_size_bytes;
  return artifact;
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

TEST(test_dual_end_report_contract_fields_are_frozen) {
  const std::string json = spz_gatekeeper::BuildDualEndReportContractSampleJson();
  ExpectContains(json, "\"artifact_id\"");
  ExpectContains(json, "\"artifact_kind\"");
  ExpectContains(json, "\"source_stage\"");
  ExpectContains(json, "\"run_meta\"");
  ExpectContains(json, "\"input_summary\"");
  ExpectContains(json, "\"stage_results\"");
  ExpectContains(json, "\"final_verdict\"");
  ExpectContains(json, "\"boards\"");
  ExpectContains(json, "\"status\":\"success\"");
  ExpectContains(json, "\"status\":\"failed\"");
  ExpectContains(json, "\"status\":\"skipped\"");
  ExpectContains(json, "\"status\":\"missing\"");
}

TEST(test_build_encode_run_json_reports_success_schema) {
  spz_gatekeeper::EncodeRunAuditReport report;
  report.artifact_id = "artifact:encode-run";
  report.run_id = "run-001";
  report.policy_mode = spz_gatekeeper::kAuditPolicyModeRelease;
  report.status = spz_gatekeeper::kStageStatusSuccess;
  report.bundle_id = "sha256:test-bundle";
  report.handoff_artifact_id = "artifact:browser-handoff";
  report.input_ply_path = "outputs/input/model.ply";
  report.output_spz_path = "outputs/encode/model.spz";
  report.output_log_path = "outputs/encode/encode.log";
  report.duration_ms = 88.5;
  report.has_duration_ms = true;
  report.artifacts.push_back(MakeArtifactRecord(
      "artifact:encode-run",
      spz_gatekeeper::kArtifactKindEncodeRunJson,
      spz_gatekeeper::kStageStatusSuccess,
      "outputs/encode/encode_run.json"));
  report.artifacts.push_back(MakeArtifactRecord(
      "artifact:spz",
      "spz_file",
      spz_gatekeeper::kStageStatusSuccess,
      "outputs/encode/model.spz",
      4096,
      true));

  const std::string json = spz_gatekeeper::BuildEncodeRunJson(report);
  ExpectContains(json, "\"artifact_id\":\"artifact:encode-run\"");
  ExpectContains(json, "\"artifact_kind\":\"encode_run_json\"");
  ExpectContains(json, "\"source_stage\":\"encode_run\"");
  ExpectContains(json, "\"run_meta\":{\"run_id\":\"run-001\",\"policy_mode\":\"release\"}");
  ExpectContains(json, "\"status\":\"success\"");
  ExpectContains(json, "\"input_summary\":{\"bundle_id\":\"sha256:test-bundle\"");
  ExpectContains(json, "\"handoff_artifact_id\":\"artifact:browser-handoff\"");
  ExpectContains(json, "\"output_summary\":{\"output_spz_path\":\"outputs/encode/model.spz\"");
  ExpectContains(json, "\"output_log_path\":\"outputs/encode/encode.log\"");
  ExpectContains(json, "\"metrics\":{\"duration_ms\":88.5}");
  ExpectContains(json, "\"artifacts\":[{");
  ExpectContains(json, "\"artifact_kind\":\"spz_file\"");
  ExpectContains(json, "\"size_bytes\":4096");
  ExpectContains(json, "\"failure\":{\"code\":null,\"message\":null}");
  ExpectContains(json, "\"issues\":[]");
}

TEST(test_build_encode_run_json_reports_failed_schema) {
  spz_gatekeeper::EncodeRunAuditReport report;
  report.artifact_id = "artifact:encode-run-failed";
  report.run_id = "run-002";
  report.policy_mode = spz_gatekeeper::kAuditPolicyModeChallenge;
  report.status = spz_gatekeeper::kStageStatusFailed;
  report.bundle_id = "sha256:test-bundle";
  report.input_ply_path = "outputs/input/model.ply";
  report.failure_code = "ENCODER_EXIT_NON_ZERO";
  report.failure_message = "encoder failed";
  report.artifacts.push_back(MakeArtifactRecord(
      "artifact:spz",
      "spz_file",
      spz_gatekeeper::kStageStatusMissing,
      ""));
  report.issues.push_back(MakeIssue(
      spz_gatekeeper::Severity::kError,
      "ENCODER_EXIT_NON_ZERO",
      "encoder failed"));

  const std::string json = spz_gatekeeper::BuildEncodeRunJson(report);
  ExpectContains(json, "\"artifact_kind\":\"encode_run_json\"");
  ExpectContains(json, "\"policy_mode\":\"challenge\"");
  ExpectContains(json, "\"status\":\"failed\"");
  ExpectContains(json, "\"output_summary\":{\"output_spz_path\":null,\"output_log_path\":null}");
  ExpectContains(json, "\"failure\":{\"code\":\"ENCODER_EXIT_NON_ZERO\",\"message\":\"encoder failed\"}");
  ExpectContains(json, "\"artifact_kind\":\"spz_file\",\"source_stage\":\"encode_run\",\"status\":\"missing\"");
  ExpectContains(json, "\"path\":null");
  ExpectContains(json, "\"issues\":[{\"severity\":\"error\",\"code\":\"ENCODER_EXIT_NON_ZERO\"");
}

TEST(test_build_artifact_index_json_reports_success_schema) {
  spz_gatekeeper::ArtifactIndexReport report;
  report.artifact_id = "artifact:index";
  report.run_id = "run-003";
  report.policy_mode = spz_gatekeeper::kAuditPolicyModeRelease;
  report.status = spz_gatekeeper::kStageStatusSuccess;
  report.artifacts.push_back(MakeArtifactRecord(
      "artifact:encode-run",
      spz_gatekeeper::kArtifactKindEncodeRunJson,
      spz_gatekeeper::kStageStatusSuccess,
      "outputs/encode/encode_run.json"));
  report.artifacts.push_back(MakeArtifactRecord(
      "artifact:spz",
      "spz_file",
      spz_gatekeeper::kStageStatusSuccess,
      "outputs/encode/model.spz",
      2048,
      true));

  const std::string json = spz_gatekeeper::BuildArtifactIndexJson(report);
  ExpectContains(json, "\"artifact_id\":\"artifact:index\"");
  ExpectContains(json, "\"artifact_kind\":\"artifact_index_json\"");
  ExpectContains(json, "\"source_stage\":\"encode_run\"");
  ExpectContains(json, "\"run_meta\":{\"run_id\":\"run-003\",\"policy_mode\":\"release\"}");
  ExpectContains(json, "\"status\":\"success\"");
  ExpectContains(json, "\"summary\":{\"total_artifacts\":2,\"success_artifacts\":2,\"failed_artifacts\":0,\"skipped_artifacts\":0,\"missing_artifacts\":0}");
  ExpectContains(json, "\"artifacts\":[{");
  ExpectContains(json, "\"artifact_kind\":\"spz_file\"");
  ExpectContains(json, "\"failure\":{\"code\":null,\"message\":null}");
  ExpectContains(json, "\"issues\":[]");
}

TEST(test_build_artifact_index_json_reports_failed_schema) {
  spz_gatekeeper::ArtifactIndexReport report;
  report.artifact_id = "artifact:index-failed";
  report.run_id = "run-004";
  report.policy_mode = spz_gatekeeper::kAuditPolicyModeChallenge;
  report.status = spz_gatekeeper::kStageStatusFailed;
  report.failure_code = "ENCODE_OUTPUT_INCOMPLETE";
  report.failure_message = "spz output missing";
  report.artifacts.push_back(MakeArtifactRecord(
      "artifact:encode-run",
      spz_gatekeeper::kArtifactKindEncodeRunJson,
      spz_gatekeeper::kStageStatusSuccess,
      "outputs/encode/encode_run.json"));
  report.artifacts.push_back(MakeArtifactRecord(
      "artifact:spz",
      "spz_file",
      spz_gatekeeper::kStageStatusMissing,
      ""));
  report.artifacts.push_back(MakeArtifactRecord(
      "artifact:log",
      "log_file",
      spz_gatekeeper::kStageStatusFailed,
      "outputs/encode/encode.log"));
  report.artifacts.push_back(MakeArtifactRecord(
      "artifact:handoff",
      "browser_handoff_json",
      spz_gatekeeper::kStageStatusSkipped,
      ""));
  report.issues.push_back(MakeIssue(
      spz_gatekeeper::Severity::kError,
      "ENCODE_OUTPUT_INCOMPLETE",
      "spz output missing"));

  const std::string json = spz_gatekeeper::BuildArtifactIndexJson(report);
  ExpectContains(json, "\"artifact_kind\":\"artifact_index_json\"");
  ExpectContains(json, "\"policy_mode\":\"challenge\"");
  ExpectContains(json, "\"status\":\"failed\"");
  ExpectContains(json, "\"summary\":{\"total_artifacts\":4,\"success_artifacts\":1,\"failed_artifacts\":1,\"skipped_artifacts\":1,\"missing_artifacts\":1}");
  ExpectContains(json, "\"failure\":{\"code\":\"ENCODE_OUTPUT_INCOMPLETE\",\"message\":\"spz output missing\"}");
  ExpectContains(json, "\"artifact_kind\":\"browser_handoff_json\",\"source_stage\":\"encode_run\",\"status\":\"skipped\"");
  ExpectContains(json, "\"artifact_kind\":\"spz_file\",\"source_stage\":\"encode_run\",\"status\":\"missing\"");
  ExpectContains(json, "\"issues\":[{\"severity\":\"error\",\"code\":\"ENCODE_OUTPUT_INCOMPLETE\"");
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
      "{\"schema_version\":\"spz_gatekeeper.browser_to_cli_handoff.v1\","
      "\"audit_profile\":\"spz\",\"audit_mode\":\"browser_lightweight_wasm_audit\","
      "\"policy_mode\":\"release\",\"bundle_id\":\"sha256:test-bundle\","
      "\"tool_version\":\"1.0.0\",\"bundle_verdict\":\"review_required\","
      "\"verdict\":\"review_required\",\"final_verdict\":\"review_required\","
      "\"release_ready\":false,\"summary\":{},\"budgets\":{},\"issues\":[],"
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
      "{\"schema_version\":\"spz_gatekeeper.browser_to_cli_handoff.v1\","
      "\"audit_profile\":\"spz\",\"audit_mode\":\"browser_lightweight_wasm_audit\","
      "\"policy_mode\":\"dev\",\"bundle_id\":\"sha256:test-bundle\","
      "\"tool_version\":\"1.0.0\",\"bundle_verdict\":\"review_required\","
      "\"verdict\":\"review_required\",\"final_verdict\":\"review_required\","
      "\"release_ready\":false,\"summary\":{},\"budgets\":{},\"issues\":[],"
      "\"next_action\":\"review_bundle_before_cli\"}";

  spz_gatekeeper::BrowserAuditHandoff handoff;
  std::string err;
  ASSERT_TRUE(spz_gatekeeper::ParseBrowserAuditHandoffJson(handoff_json, &handoff, &err));
  ASSERT_TRUE(handoff.policy_mode == spz_gatekeeper::kAuditPolicyModeDev);
}

TEST(test_parse_browser_handoff_prefers_bundle_verdict_field) {
  const std::string handoff_json =
      "{\"schema_version\":\"spz_gatekeeper.browser_to_cli_handoff.v1\","
      "\"audit_profile\":\"spz\",\"audit_mode\":\"browser_lightweight_wasm_audit\","
      "\"policy_mode\":\"challenge\",\"bundle_id\":\"sha256:test-bundle\","
      "\"tool_version\":\"1.0.0\",\"bundle_verdict\":\"block\","
      "\"verdict\":\"review_required\",\"final_verdict\":\"block\","
      "\"release_ready\":false,\"summary\":{},\"budgets\":{},\"issues\":[],"
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
  RUN_TEST(test_dual_end_report_contract_fields_are_frozen);
  RUN_TEST(test_build_encode_run_json_reports_success_schema);
  RUN_TEST(test_build_encode_run_json_reports_failed_schema);
  RUN_TEST(test_build_artifact_index_json_reports_success_schema);
  RUN_TEST(test_build_artifact_index_json_reports_failed_schema);
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
