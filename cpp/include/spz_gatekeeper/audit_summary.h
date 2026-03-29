// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PuJunhan

#pragma once

#include "spz_gatekeeper/report.h"

#include <cstdint>
#include <string>


namespace spz_gatekeeper {

inline constexpr const char* kAuditProfileSpz = "spz";
inline constexpr const char* kAuditPolicyName = "spz_gatekeeper_policy";
inline constexpr const char* kAuditPolicyVersion = "2.0.0";
inline constexpr const char* kAuditPolicyModeDev = "dev";
inline constexpr const char* kAuditPolicyModeRelease = "release";
inline constexpr const char* kAuditPolicyModeChallenge = "challenge";
inline constexpr const char* kAuditToolVersion = "1.0.0";
inline constexpr const char* kAuditModeBrowserLightweightWasmAudit =
    "browser_lightweight_wasm_audit";
inline constexpr const char* kAuditModeLocalCliSpzArtifactAudit =
    "local_cli_spz_artifact_audit";
inline constexpr const char* kBrowserToCliHandoffSchemaVersion =
    "spz_gatekeeper.browser_to_cli_handoff.v1";
inline constexpr const char* kArtifactFieldArtifactId = "artifact_id";
inline constexpr const char* kArtifactFieldArtifactKind = "artifact_kind";
inline constexpr const char* kArtifactFieldSourceStage = "source_stage";
inline constexpr const char* kDualEndFieldRunMeta = "run_meta";
inline constexpr const char* kDualEndFieldInputSummary = "input_summary";
inline constexpr const char* kDualEndFieldStageResults = "stage_results";
inline constexpr const char* kDualEndFieldFinalVerdict = "final_verdict";
inline constexpr const char* kDualEndFieldBoards = "boards";
inline constexpr const char* kAuditStageWebAudit = "web_audit";
inline constexpr const char* kAuditStageEncodeRun = "encode_run";
inline constexpr const char* kAuditStageCliAudit = "cli_audit";
inline constexpr const char* kAuditStageChallengeReport = "challenge_report";
inline constexpr const char* kAuditStageReportFusion = "report_fusion";
inline constexpr const char* kStageStatusSuccess = "success";
inline constexpr const char* kStageStatusFailed = "failed";
inline constexpr const char* kStageStatusSkipped = "skipped";
inline constexpr const char* kStageStatusMissing = "missing";
inline constexpr const char* kAuditBudgetColdStartMs = "cold_start_ms";
inline constexpr const char* kAuditBudgetTinyCaseMs = "tiny_case_ms";
inline constexpr const char* kAuditBudgetPeakMemoryMb = "peak_memory_mb";
inline constexpr const char* kAuditBudgetCopyPassLimit = "copy_pass_limit";
inline constexpr const char* kAuditBudgetFileSizeBytes = "file_size_bytes";
inline constexpr const char* kAuditBudgetDecompressedSizeBytes = "decompressed_size_bytes";
inline constexpr const char* kAuditBudgetProcessTimeMs = "process_time_ms";
inline constexpr const char* kAuditBudgetMemoryGrowthCount = "memory_growth_count";

struct CompatAuditMetrics;

bool HasWarnings(const GateReport& report);
bool HasValidatorCoverage(const GateReport& report);
bool HasEmptyShellRisk(const GateReport& report);
std::string ResolveCompatVerdict(const GateReport& strict_report,
                                 const GateReport& non_strict_report,
                                 bool* strict_ok = nullptr,
                                 bool* non_strict_ok = nullptr);
std::string ResolveCompatAuditVerdict(const GateReport& strict_report,
                                      const GateReport& non_strict_report,
                                      const CompatAuditMetrics* metrics,
                                      const char* policy_mode = kAuditPolicyModeRelease,
                                      bool* strict_ok = nullptr,
                                      bool* non_strict_ok = nullptr);
std::string ResolveCompatNextAction(const std::string& verdict);


std::string BuildIssueListJson(const GateReport& report);
std::string BuildExtensionSummaryJson(const GateReport& report);
std::string BuildRegistrySummaryJson(const GateReport& report);
std::string BuildWasmQualityGateJson(bool validator_coverage_ok,
                                     bool empty_shell_risk,
                                     bool memory_budget_wired = false,
                                     bool release_ready = false);
std::string BuildDualEndReportContractSampleJson();


struct CompatAuditMetrics {
  std::uint64_t file_size_bytes = 0;
  bool has_file_size_bytes = false;
  double process_time_ms = 0.0;
  bool has_process_time_ms = false;
  double peak_memory_mb = 0.0;
  bool has_peak_memory_mb = false;
  std::uint64_t memory_growth_count = 0;
  bool has_memory_growth_count = false;
};

struct BrowserAuditHandoff {
  std::string raw_json;
  std::string schema_version;
  std::string audit_profile;
  std::string audit_mode;
  std::string policy_mode = kAuditPolicyModeRelease;
  std::string verdict;
  std::string final_verdict;
  bool release_ready = false;
  std::string next_action;
  std::string bundle_id;
  std::string tool_version;
};


struct BrowserWasmAuditSummary {
  std::string bundle_name;
  std::uint64_t file_count = 0;
  std::uint64_t issue_count = 0;
  std::uint64_t declared_export_count = 0;
  std::uint64_t loader_export_count = 0;
  std::uint64_t wasm_export_count = 0;
  bool valid_tiny_passed = false;
  bool invalid_tiny_handled = false;
  bool runtime_available = false;
};

struct BrowserWasmAuditReport {
  std::string bundle_id;
  std::string policy_mode = kAuditPolicyModeRelease;
  std::string verdict;
  std::string final_verdict;
  bool release_ready = false;
  bool has_release_ready = false;
  std::string next_action;
  double audit_duration_ms = 0.0;
  BrowserWasmAuditSummary summary;

  std::string manifest_summary_json = "{}";
  std::string budgets_json = "{}";
  std::string copy_breakdown_json = "{}";
  std::string issues_json = "[]";
  std::string bundle_entries_json = "[]";
  std::string wasm_export_summary_json = "[]";
  bool empty_shell_risk = false;
  bool copy_budget_wired = false;
  bool memory_budget_wired = false;
  bool performance_budget_wired = false;
};


bool ParseBrowserAuditHandoffJson(const std::string& json_text,
                                  BrowserAuditHandoff* handoff,
                                  std::string* err);
std::string BuildBrowserWasmAuditJson(const BrowserWasmAuditReport& report);

std::string BuildCompatCheckAuditJson(const std::string& path,
                                      const GateReport& strict_report,
                                      const GateReport& non_strict_report,
                                      const CompatAuditMetrics* metrics = nullptr,
                                      const char* policy_mode = kAuditPolicyModeRelease);

std::string BuildCompatCheckAuditWithHandoffJson(const std::string& compat_json,
                                                 const std::string& artifact_verdict,
                                                 const BrowserAuditHandoff& handoff);



}  // namespace spz_gatekeeper
