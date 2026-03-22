// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PuJunhan

#include "spz_gatekeeper/audit_summary.h"

#include "spz_gatekeeper/extension_spec_registry.h"
#include "spz_gatekeeper/json_min.h"

#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace spz_gatekeeper {
namespace {

std::string SeverityToString(Severity severity) {
  switch (severity) {
    case Severity::kError:
      return "error";
    case Severity::kWarning:
      return "warning";
    case Severity::kNote:
      return "note";
  }
  return "note";
}

std::string IssueToJson(const Issue& issue) {
  std::ostringstream oss;
  oss << "{";
  oss << "\"severity\":\"" << SeverityToString(issue.severity) << "\"";
  oss << ",\"code\":\"" << JsonEscape(issue.code) << "\"";
  oss << ",\"message\":\"" << JsonEscape(issue.message) << "\"";
  oss << "}";
  return oss.str();
}

std::vector<Issue> MergeIssues(const GateReport& strict_report,
                               const GateReport& non_strict_report) {
  std::vector<Issue> merged;
  std::set<std::string> seen;
  const auto append_unique = [&](const Issue& issue) {
    const std::string key = SeverityToString(issue.severity) + "|" + issue.code + "|" +
                            issue.message + "|" + issue.where;
    if (seen.insert(key).second) {
      merged.push_back(issue);
    }
  };

  for (const auto& issue : strict_report.issues) {
    append_unique(issue);
  }
  for (const auto& issue : non_strict_report.issues) {
    append_unique(issue);
  }
  return merged;
}

std::string BuildIssueListJson(const std::vector<Issue>& issues) {
  std::ostringstream oss;
  oss << "[";
  for (std::size_t i = 0; i < issues.size(); ++i) {
    if (i) {
      oss << ",";
    }
    oss << IssueToJson(issues[i]);
  }
  oss << "]";
  return oss.str();
}

std::string ResolveCompatVerdict(bool strict_ok,
                                 bool non_strict_ok,
                                 bool validator_coverage_ok,
                                 bool empty_shell_risk) {
  if (!non_strict_ok) {
    return "block";
  }
  if (!strict_ok || !validator_coverage_ok || empty_shell_risk) {
    return "review_required";
  }
  return "pass";
}

std::string ResolveCompatNextAction(const std::string& verdict) {
  if (verdict == "pass") {
    return "artifact_ready";
  }
  if (verdict == "review_required") {
    return "review_artifact";
  }
  return "block_artifact";
}

}  // namespace

bool HasWarnings(const GateReport& report) {
  for (const auto& issue : report.issues) {
    if (issue.severity == Severity::kWarning) {
      return true;
    }
  }
  return false;
}

bool HasValidatorCoverage(const GateReport& report) {
  for (const auto& ext : report.extension_reports) {
    if (!ext.has_validator) {
      return false;
    }
  }
  return true;
}

bool HasEmptyShellRisk(const GateReport& report) {
  for (const auto& ext : report.extension_reports) {
    if (!ext.has_validator) {
      return true;
    }
  }
  return false;
}

std::string BuildIssueListJson(const GateReport& report) {
  return BuildIssueListJson(report.issues);
}

std::string BuildExtensionSummaryJson(const GateReport& report) {
  std::ostringstream oss;
  oss << "[";
  for (std::size_t i = 0; i < report.extension_reports.size(); ++i) {
    if (i) {
      oss << ",";
    }
    const auto& ext = report.extension_reports[i];
    oss << "{";
    oss << "\"type\":" << ext.type;
    oss << ",\"extension_name\":\"" << JsonEscape(ext.extension_name) << "\"";
    oss << ",\"known_extension\":" << (ext.known_extension ? "true" : "false");
    oss << ",\"has_validator\":" << (ext.has_validator ? "true" : "false");
    oss << ",\"validation_result\":" << (ext.validation_result ? "true" : "false");
    oss << ",\"status\":\"" << JsonEscape(ext.status) << "\"";
    oss << "}";
  }
  oss << "]";
  return oss.str();
}

std::string BuildRegistrySummaryJson(const GateReport& report) {
  std::size_t known_extensions = 0;
  std::size_t validator_backed_extensions = 0;
  std::size_t unknown_extensions = 0;
  for (const auto& ext : report.extension_reports) {
    if (ext.known_extension) {
      ++known_extensions;
    } else {
      ++unknown_extensions;
    }
    if (ext.has_validator) {
      ++validator_backed_extensions;
    }
  }

  std::ostringstream oss;
  oss << "{";
  oss << "\"registered_extensions\":" << ExtensionSpecRegistry::Instance().SpecCount();
  oss << ",\"known_extensions\":" << known_extensions;
  oss << ",\"validator_backed_extensions\":" << validator_backed_extensions;
  oss << ",\"unknown_extensions\":" << unknown_extensions;
  oss << "}";
  return oss.str();
}

std::string BuildWasmQualityGateJson(bool validator_coverage_ok, bool empty_shell_risk) {
  std::ostringstream oss;
  oss << "{";
  oss << "\"coverage_level\":\"baseline\"";
  oss << ",\"validator_coverage_ok\":" << (validator_coverage_ok ? "true" : "false");
  oss << ",\"empty_shell_risk\":" << (empty_shell_risk ? "true" : "false");
  oss << ",\"api_surface_wired\":true";
  oss << ",\"browser_smoke_wired\":true";
  oss << ",\"empty_shell_guard_wired\":true";
  oss << ",\"warning_budget_wired\":false";
  oss << ",\"copy_budget_wired\":false";
  oss << ",\"memory_budget_wired\":false";
  oss << ",\"performance_budget_wired\":false";
  oss << ",\"artifact_audit_wired\":false";
  oss << ",\"release_ready\":false";
  oss << "}";
  return oss.str();
}

std::string BuildCompatCheckAuditJson(const std::string& path,
                                      const GateReport& strict_report,
                                      const GateReport& non_strict_report) {
  const bool strict_ok = !strict_report.HasErrors() && !HasWarnings(strict_report);
  const bool non_strict_ok = !non_strict_report.HasErrors();
  const bool validator_coverage_ok = HasValidatorCoverage(non_strict_report);
  const bool empty_shell_risk = HasEmptyShellRisk(non_strict_report);
  const std::vector<Issue> merged_issues = MergeIssues(strict_report, non_strict_report);
  const std::string verdict = ResolveCompatVerdict(
      strict_ok, non_strict_ok, validator_coverage_ok, empty_shell_risk);
  const std::string next_action = ResolveCompatNextAction(verdict);

  std::ostringstream oss;
  oss << "{";
  oss << "\"asset_path\":\"" << JsonEscape(path) << "\"";
  oss << ",\"audit_profile\":\"" << kAuditProfileSpz << "\"";
  oss << ",\"audit_mode\":\"" << kAuditModeLocalCliSpzArtifactAudit << "\"";
  oss << ",\"verdict\":\"" << verdict << "\"";
  oss << ",\"summary\":{";
  oss << "\"strict_ok\":" << (strict_ok ? "true" : "false");
  oss << ",\"non_strict_ok\":" << (non_strict_ok ? "true" : "false");
  oss << ",\"extension_count\":" << non_strict_report.extension_reports.size();
  oss << ",\"issue_count\":" << merged_issues.size();
  oss << "}";
  oss << ",\"budgets\":{}";
  oss << ",\"issues\":" << BuildIssueListJson(merged_issues);
  oss << ",\"next_action\":\"" << next_action << "\"";
  oss << ",\"strict_ok\":" << (strict_ok ? "true" : "false");
  oss << ",\"non_strict_ok\":" << (non_strict_ok ? "true" : "false");
  oss << ",\"registry_summary\":" << BuildRegistrySummaryJson(non_strict_report);
  oss << ",\"extension_summary\":" << BuildExtensionSummaryJson(non_strict_report);
  oss << ",\"wasm_quality_gate\":"
      << BuildWasmQualityGateJson(validator_coverage_ok, empty_shell_risk);
  oss << ",\"issue_summary\":{";
  oss << "\"strict\":" << BuildIssueListJson(strict_report);
  oss << ",\"non_strict\":" << BuildIssueListJson(non_strict_report);
  oss << "}";
  oss << ",\"upstream_tools\":{\"spz_info\":\"skipped\",\"spz_to_ply\":\"skipped\"}";
  oss << "}";
  return oss.str();
}

}  // namespace spz_gatekeeper
