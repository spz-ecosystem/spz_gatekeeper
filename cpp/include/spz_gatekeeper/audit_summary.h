// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PuJunhan

#pragma once

#include "spz_gatekeeper/report.h"

#include <cstdint>
#include <string>


namespace spz_gatekeeper {

inline constexpr const char* kAuditProfileSpz = "spz";
inline constexpr const char* kAuditModeBrowserLightweightWasmAudit =
    "browser_lightweight_wasm_audit";
inline constexpr const char* kAuditModeLocalCliSpzArtifactAudit =
    "local_cli_spz_artifact_audit";

bool HasWarnings(const GateReport& report);
bool HasValidatorCoverage(const GateReport& report);
bool HasEmptyShellRisk(const GateReport& report);

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

std::string BuildCompatCheckAuditJson(const std::string& path,
                                      const GateReport& strict_report,
                                      const GateReport& non_strict_report,
                                      const CompatAuditMetrics* metrics = nullptr);


}  // namespace spz_gatekeeper
