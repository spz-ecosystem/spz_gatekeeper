// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PuJunhan

#pragma once

#include "spz_gatekeeper/report.h"

#include <cstdint>
#include <string>


namespace spz_gatekeeper {

inline constexpr const char* kAuditProfileSpz = "spz";
inline constexpr const char* kAuditToolVersion = "1.0.0";
inline constexpr const char* kAuditModeBrowserLightweightWasmAudit =
    "browser_lightweight_wasm_audit";
inline constexpr const char* kAuditModeLocalCliSpzArtifactAudit =
    "local_cli_spz_artifact_audit";

bool HasWarnings(const GateReport& report);
bool HasValidatorCoverage(const GateReport& report);
bool HasEmptyShellRisk(const GateReport& report);
std::string ResolveCompatVerdict(const GateReport& strict_report,
                                 const GateReport& non_strict_report,
                                 bool* strict_ok = nullptr,
                                 bool* non_strict_ok = nullptr);
std::string ResolveCompatNextAction(const std::string& verdict);


std::string BuildIssueListJson(const GateReport& report);
std::string BuildExtensionSummaryJson(const GateReport& report);
std::string BuildRegistrySummaryJson(const GateReport& report);
std::string BuildWasmQualityGateJson(bool validator_coverage_ok, bool empty_shell_risk);

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
  std::string audit_profile;
  std::string audit_mode;
  std::string verdict;
  std::string next_action;
  std::string bundle_id;
  std::string tool_version;
};

bool ParseBrowserAuditHandoffJson(const std::string& json_text,
                                  BrowserAuditHandoff* handoff,
                                  std::string* err);

std::string BuildCompatCheckAuditJson(const std::string& path,
                                      const GateReport& strict_report,
                                      const GateReport& non_strict_report,
                                      const CompatAuditMetrics* metrics = nullptr);
std::string BuildCompatCheckAuditWithHandoffJson(const std::string& compat_json,
                                                 const std::string& artifact_verdict,
                                                 const BrowserAuditHandoff& handoff);



}  // namespace spz_gatekeeper
