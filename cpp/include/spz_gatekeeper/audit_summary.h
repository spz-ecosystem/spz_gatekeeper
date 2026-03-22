// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PuJunhan

#pragma once

#include "spz_gatekeeper/report.h"

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
std::string BuildCompatCheckAuditJson(const std::string& path,
                                      const GateReport& strict_report,
                                      const GateReport& non_strict_report);

}  // namespace spz_gatekeeper
