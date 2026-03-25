// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PuJunhan

#include "spz_gatekeeper/audit_summary.h"

#include "spz_gatekeeper/extension_spec_registry.h"
#include "spz_gatekeeper/json_min.h"

#include <cctype>
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

bool IsSupportedAuditVerdict(const std::string& verdict) {
  return verdict == "pass" || verdict == "review_required" || verdict == "block";
}

std::string TrimAsciiWhitespace(const std::string& text) {
  std::size_t start = 0;
  while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start])) != 0) {
    ++start;
  }

  std::size_t end = text.size();
  while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
    --end;
  }
  return text.substr(start, end - start);
}

bool ExtractJsonStringField(const std::string& json_text,
                            const std::string& field,
                            std::string* value,
                            std::string* err) {
  const std::string key = "\"" + field + "\"";
  const std::size_t key_pos = json_text.find(key);
  if (key_pos == std::string::npos) {
    if (err != nullptr) {
      *err = "missing field: " + field;
    }
    return false;
  }

  const std::size_t colon_pos = json_text.find(':', key_pos + key.size());
  if (colon_pos == std::string::npos) {
    if (err != nullptr) {
      *err = "invalid field separator: " + field;
    }
    return false;
  }

  std::size_t quote_pos = json_text.find('"', colon_pos + 1);
  if (quote_pos == std::string::npos) {
    if (err != nullptr) {
      *err = "missing string value: " + field;
    }
    return false;
  }

  std::string parsed;
  bool escape = false;
  for (std::size_t i = quote_pos + 1; i < json_text.size(); ++i) {
    const char ch = json_text[i];
    if (escape) {
      switch (ch) {
        case '"':
        case '\\':
        case '/':
          parsed.push_back(ch);
          break;
        case 'b':
          parsed.push_back('\b');
          break;
        case 'f':
          parsed.push_back('\f');
          break;
        case 'n':
          parsed.push_back('\n');
          break;
        case 'r':
          parsed.push_back('\r');
          break;
        case 't':
          parsed.push_back('\t');
          break;
        default:
          parsed.push_back(ch);
          break;
      }
      escape = false;
      continue;
    }

    if (ch == '\\') {
      escape = true;
      continue;
    }
    if (ch == '"') {
      *value = parsed;
      return true;
    }
    parsed.push_back(ch);
  }

  if (err != nullptr) {
    *err = "unterminated string field: " + field;
  }
  return false;
}


std::string BuildBudgetItemJson(bool has_declared,
                                double declared,
                                bool has_observed,
                                double observed) {
  std::ostringstream oss;
  oss << "{";
  if (has_declared) {
    oss << "\"declared\":" << declared;
  } else {
    oss << "\"declared\":null";
  }
  oss << ",\"observed\":";
  if (has_observed) {
    oss << observed;
  } else {
    oss << "null";
  }
  if (!has_observed) {
    oss << ",\"status\":\"not_collected\",\"within_budget\":null";
  } else if (!has_declared) {
    oss << ",\"status\":\"observed_without_budget\",\"within_budget\":null";
  } else {
    const bool within_budget = observed <= declared;
    oss << ",\"status\":\"" << (within_budget ? "within_budget" : "over_budget") << "\"";
    oss << ",\"within_budget\":" << (within_budget ? "true" : "false");
  }
  oss << "}";
  return oss.str();
}

std::string NormalizeJsonFragment(const std::string& json, const char* fallback) {
  return json.empty() ? std::string(fallback) : json;
}

const char* ResolvePolicyMode(const std::string& policy_mode) {
  if (policy_mode == kAuditPolicyModeDev || policy_mode == kAuditPolicyModeRelease ||
      policy_mode == kAuditPolicyModeChallenge) {
    return policy_mode.c_str();
  }
  return kAuditPolicyModeRelease;
}

bool IsReleaseReadyVerdict(const std::string& verdict) {
  return verdict == "pass";
}

std::string ResolveEffectiveFinalVerdict(const std::string& verdict,
                                        const std::string& final_verdict) {
  return IsSupportedAuditVerdict(final_verdict) ? final_verdict : verdict;
}

bool ResolveEffectiveReleaseReady(const std::string& final_verdict,
                                  bool has_release_ready,
                                  bool release_ready) {
  return has_release_ready ? release_ready : IsReleaseReadyVerdict(final_verdict);
}

std::string BuildBrowserWasmQualityGateJson(const BrowserWasmAuditReport& report,
                                            bool release_ready) {
  std::ostringstream oss;
  oss << "{";
  oss << "\"coverage_level\":\"browser_lightweight\"";
  oss << ",\"validator_coverage_ok\":true";
  oss << ",\"empty_shell_risk\":" << (report.empty_shell_risk ? "true" : "false");
  oss << ",\"api_surface_wired\":true";
  oss << ",\"browser_smoke_wired\":true";
  oss << ",\"empty_shell_guard_wired\":true";
  oss << ",\"warning_budget_wired\":true";
  oss << ",\"copy_budget_wired\":" << (report.copy_budget_wired ? "true" : "false");
  oss << ",\"memory_budget_wired\":" << (report.memory_budget_wired ? "true" : "false");

  oss << ",\"performance_budget_wired\":"
      << (report.performance_budget_wired ? "true" : "false");
  oss << ",\"artifact_audit_wired\":false";
  oss << ",\"release_ready\":" << (release_ready ? "true" : "false");
  oss << "}";
  return oss.str();
}

std::string BuildArtifactSummaryJson(const GateReport& non_strict_report,

                                     const CompatAuditMetrics* metrics) {
  std::ostringstream oss;
  oss << "{";
  oss << "\"file_size_bytes\":";
  if (metrics != nullptr && metrics->has_file_size_bytes) {
    oss << metrics->file_size_bytes;
  } else {
    oss << "null";
  }
  oss << ",\"decompressed_size_bytes\":";
  if (non_strict_report.spz_l2.has_value()) {
    oss << non_strict_report.spz_l2->decompressed_size;
  } else {
    oss << "null";
  }
  oss << ",\"trailer_size_bytes\":";
  if (non_strict_report.spz_l2.has_value()) {
    oss << non_strict_report.spz_l2->trailer_size;
  } else {
    oss << "null";
  }
  oss << ",\"extension_count\":" << non_strict_report.extension_reports.size();
  oss << "}";
  return oss.str();
}

std::string BuildArtifactBudgetsJson(const GateReport& non_strict_report,
                                     const CompatAuditMetrics* metrics) {
  const bool has_decompressed_size = non_strict_report.spz_l2.has_value();
  std::ostringstream oss;
  oss << "{";
  oss << "\"file_size_bytes\":"
      << BuildBudgetItemJson(false,
                             0.0,
                             metrics != nullptr && metrics->has_file_size_bytes,
                             metrics != nullptr && metrics->has_file_size_bytes
                                 ? static_cast<double>(metrics->file_size_bytes)
                                 : 0.0);
  oss << ",\"decompressed_size_bytes\":"
      << BuildBudgetItemJson(false,
                             0.0,
                             has_decompressed_size,
                             has_decompressed_size
                                 ? static_cast<double>(non_strict_report.spz_l2->decompressed_size)
                                 : 0.0);
  oss << ",\"process_time_ms\":"
      << BuildBudgetItemJson(false,
                             0.0,
                             metrics != nullptr && metrics->has_process_time_ms,
                             metrics != nullptr && metrics->has_process_time_ms
                                 ? metrics->process_time_ms
                                 : 0.0);
  oss << ",\"peak_memory_mb\":"
      << BuildBudgetItemJson(false,
                             0.0,
                             metrics != nullptr && metrics->has_peak_memory_mb,
                             metrics != nullptr && metrics->has_peak_memory_mb
                                 ? metrics->peak_memory_mb
                                 : 0.0);
  oss << ",\"memory_growth_count\":"
      << BuildBudgetItemJson(false,
                             0.0,
                             metrics != nullptr && metrics->has_memory_growth_count,
                             metrics != nullptr && metrics->has_memory_growth_count
                                 ? static_cast<double>(metrics->memory_growth_count)
                                 : 0.0);
  oss << "}";
  return oss.str();
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

std::string ResolveCompatVerdict(const GateReport& strict_report,
                                 const GateReport& non_strict_report,
                                 bool* strict_ok,
                                 bool* non_strict_ok) {
  const bool strict_pass = !strict_report.HasErrors() && !HasWarnings(strict_report);
  const bool non_strict_pass = !non_strict_report.HasErrors();
  if (strict_ok != nullptr) {
    *strict_ok = strict_pass;
  }
  if (non_strict_ok != nullptr) {
    *non_strict_ok = non_strict_pass;
  }

  const bool validator_coverage_ok = HasValidatorCoverage(non_strict_report);
  const bool empty_shell_risk = HasEmptyShellRisk(non_strict_report);
  if (!non_strict_pass) {
    return "block";
  }
  if (!strict_pass || !validator_coverage_ok || empty_shell_risk) {
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

std::string BuildWasmQualityGateJson(bool validator_coverage_ok,
                                 bool empty_shell_risk,
                                 bool memory_budget_wired,
                                 bool release_ready) {

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
  oss << ",\"memory_budget_wired\":" << (memory_budget_wired ? "true" : "false");
  oss << ",\"performance_budget_wired\":false";
  oss << ",\"artifact_audit_wired\":false";
  oss << ",\"release_ready\":" << (release_ready ? "true" : "false");

  oss << "}";
  return oss.str();
}


bool ParseBrowserAuditHandoffJson(const std::string& json_text,
                                  BrowserAuditHandoff* handoff,
                                  std::string* err) {
  if (handoff == nullptr) {
    if (err != nullptr) {
      *err = "handoff output is null";
    }
    return false;
  }

  BrowserAuditHandoff parsed;
  parsed.raw_json = TrimAsciiWhitespace(json_text);
  if (parsed.raw_json.empty()) {
    if (err != nullptr) {
      *err = "handoff json is empty";
    }
    return false;
  }
  if (parsed.raw_json.front() != '{' || parsed.raw_json.back() != '}') {
    if (err != nullptr) {
      *err = "handoff json must be an object";
    }
    return false;
  }

  if (!ExtractJsonStringField(parsed.raw_json, "audit_profile", &parsed.audit_profile, err) ||
      !ExtractJsonStringField(parsed.raw_json, "audit_mode", &parsed.audit_mode, err) ||
      !ExtractJsonStringField(parsed.raw_json, "verdict", &parsed.verdict, err) ||
      !ExtractJsonStringField(parsed.raw_json, "next_action", &parsed.next_action, err) ||
      !ExtractJsonStringField(parsed.raw_json, "bundle_id", &parsed.bundle_id, err) ||
      !ExtractJsonStringField(parsed.raw_json, "tool_version", &parsed.tool_version, err)) {
    return false;
  }
  if (parsed.raw_json.find("\"policy_mode\"") != std::string::npos) {
    std::string policy_mode;
    if (!ExtractJsonStringField(parsed.raw_json, "policy_mode", &policy_mode, err)) {
      return false;
    }
    parsed.policy_mode = ResolvePolicyMode(policy_mode);
  }

  if (parsed.audit_profile != kAuditProfileSpz) {
    if (err != nullptr) {
      *err = "unsupported handoff audit_profile: " + parsed.audit_profile;
    }
    return false;
  }
  if (parsed.audit_mode != kAuditModeBrowserLightweightWasmAudit) {
    if (err != nullptr) {
      *err = "unsupported handoff audit_mode: " + parsed.audit_mode;
    }
    return false;
  }
  if (!IsSupportedAuditVerdict(parsed.verdict)) {
    if (err != nullptr) {
      *err = "unsupported handoff verdict: " + parsed.verdict;
    }
    return false;
  }
  if (parsed.bundle_id.empty()) {
    if (err != nullptr) {
      *err = "handoff bundle_id is empty";
    }
    return false;
  }
  if (parsed.tool_version.empty()) {
    if (err != nullptr) {
      *err = "handoff tool_version is empty";
    }
    return false;
  }

  *handoff = std::move(parsed);
  return true;
}

std::string BuildBrowserWasmAuditJson(const BrowserWasmAuditReport& report) {
  const std::string final_verdict = ResolveEffectiveFinalVerdict(report.verdict, report.final_verdict);
  const bool release_ready =
      ResolveEffectiveReleaseReady(final_verdict, report.has_release_ready, report.release_ready);
  std::ostringstream oss;
  oss << "{";
  oss << "\"audit_profile\":\"" << kAuditProfileSpz << "\"";
  oss << ",\"policy_name\":\"" << kAuditPolicyName << "\"";
  oss << ",\"policy_version\":\"" << kAuditPolicyVersion << "\"";
  oss << ",\"policy_mode\":\"" << ResolvePolicyMode(report.policy_mode) << "\"";
  oss << ",\"audit_mode\":\"" << kAuditModeBrowserLightweightWasmAudit << "\"";
  oss << ",\"bundle_id\":\"" << JsonEscape(report.bundle_id) << "\"";
  oss << ",\"tool_version\":\"" << kAuditToolVersion << "\"";
  oss << ",\"bundle_verdict\":\"" << JsonEscape(report.verdict) << "\"";
  oss << ",\"verdict\":\"" << JsonEscape(report.verdict) << "\"";
  oss << ",\"final_verdict\":\"" << JsonEscape(final_verdict) << "\"";
  oss << ",\"release_ready\":" << (release_ready ? "true" : "false");
  oss << ",\"summary\":{";
  oss << "\"bundle_name\":\"" << JsonEscape(report.summary.bundle_name) << "\"";
  oss << ",\"file_count\":" << report.summary.file_count;
  oss << ",\"issue_count\":" << report.summary.issue_count;
  oss << ",\"declared_export_count\":" << report.summary.declared_export_count;
  oss << ",\"loader_export_count\":" << report.summary.loader_export_count;
  oss << ",\"wasm_export_count\":" << report.summary.wasm_export_count;
  oss << ",\"valid_tiny_passed\":"
      << (report.summary.valid_tiny_passed ? "true" : "false");
  oss << ",\"invalid_tiny_handled\":"
      << (report.summary.invalid_tiny_handled ? "true" : "false");
  oss << ",\"runtime_available\":"
      << (report.summary.runtime_available ? "true" : "false");
  oss << "}";
  oss << ",\"budgets\":" << NormalizeJsonFragment(report.budgets_json, "{}");
  oss << ",\"issues\":" << NormalizeJsonFragment(report.issues_json, "[]");
  oss << ",\"next_action\":\"" << JsonEscape(report.next_action) << "\"";
  oss << ",\"manifest_summary\":"
      << NormalizeJsonFragment(report.manifest_summary_json, "{}");
  oss << ",\"bundle_entries\":"
      << NormalizeJsonFragment(report.bundle_entries_json, "[]");
  oss << ",\"wasm_export_summary\":"
      << NormalizeJsonFragment(report.wasm_export_summary_json, "[]");
  oss << ",\"wasm_quality_gate\":"
      << BuildBrowserWasmQualityGateJson(report, release_ready);
  oss << ",\"audit_duration_ms\":" << report.audit_duration_ms;
  oss << "}";
  return oss.str();
}


std::string BuildCompatCheckAuditJson(const std::string& path,
                                      const GateReport& strict_report,
                                      const GateReport& non_strict_report,
                                      const CompatAuditMetrics* metrics,
                                      const char* policy_mode) {


  bool strict_ok = false;
  bool non_strict_ok = false;
  const bool validator_coverage_ok = HasValidatorCoverage(non_strict_report);
  const bool empty_shell_risk = HasEmptyShellRisk(non_strict_report);
  const bool memory_budget_wired = (metrics != nullptr);
  const std::vector<Issue> merged_issues = MergeIssues(strict_report, non_strict_report);

  const std::string verdict = ResolveCompatVerdict(
      strict_report, non_strict_report, &strict_ok, &non_strict_ok);
  const std::string next_action = ResolveCompatNextAction(verdict);


  const bool release_ready = IsReleaseReadyVerdict(verdict);
  std::ostringstream oss;
  oss << "{";
  oss << "\"asset_path\":\"" << JsonEscape(path) << "\"";
  oss << ",\"audit_profile\":\"" << kAuditProfileSpz << "\"";
  oss << ",\"policy_name\":\"" << kAuditPolicyName << "\"";
  oss << ",\"policy_version\":\"" << kAuditPolicyVersion << "\"";
  oss << ",\"policy_mode\":\"" << ResolvePolicyMode(policy_mode == nullptr ? "" : policy_mode) << "\"";
  oss << ",\"audit_mode\":\"" << kAuditModeLocalCliSpzArtifactAudit << "\"";
  oss << ",\"artifact_verdict\":\"" << verdict << "\"";
  oss << ",\"verdict\":\"" << verdict << "\"";
  oss << ",\"final_verdict\":\"" << verdict << "\"";
  oss << ",\"release_ready\":" << (release_ready ? "true" : "false");

  oss << ",\"summary\":{";
  oss << "\"strict_ok\":" << (strict_ok ? "true" : "false");
  oss << ",\"non_strict_ok\":" << (non_strict_ok ? "true" : "false");
  oss << ",\"extension_count\":" << non_strict_report.extension_reports.size();
  oss << ",\"issue_count\":" << merged_issues.size();
  oss << "}";
  oss << ",\"artifact_summary\":" << BuildArtifactSummaryJson(non_strict_report, metrics);
  oss << ",\"budgets\":" << BuildArtifactBudgetsJson(non_strict_report, metrics);
  oss << ",\"issues\":" << BuildIssueListJson(merged_issues);

  oss << ",\"next_action\":\"" << next_action << "\"";
  oss << ",\"strict_ok\":" << (strict_ok ? "true" : "false");
  oss << ",\"non_strict_ok\":" << (non_strict_ok ? "true" : "false");
  oss << ",\"registry_summary\":" << BuildRegistrySummaryJson(non_strict_report);
  oss << ",\"extension_summary\":" << BuildExtensionSummaryJson(non_strict_report);
  oss << ",\"wasm_quality_gate\":"
      << BuildWasmQualityGateJson(
             validator_coverage_ok, empty_shell_risk, memory_budget_wired, release_ready);


  oss << ",\"issue_summary\":{";
  oss << "\"strict\":" << BuildIssueListJson(strict_report);
  oss << ",\"non_strict\":" << BuildIssueListJson(non_strict_report);
  oss << "}";
  oss << ",\"upstream_tools\":{\"spz_info\":\"skipped\",\"spz_to_ply\":\"skipped\"}";
  oss << "}";
  return oss.str();
}

std::string BuildCompatCheckAuditWithHandoffJson(const std::string& compat_json,
                                                 const std::string& artifact_verdict,
                                                 const BrowserAuditHandoff& handoff) {
  const std::string trimmed = TrimAsciiWhitespace(compat_json);
  if (trimmed.empty() || trimmed.back() != '}') {
    return compat_json;
  }

  std::ostringstream oss;
  oss << trimmed.substr(0, trimmed.size() - 1);
  oss << ",\"handoff\":" << handoff.raw_json;
  oss << ",\"upstream_audit\":{";
  oss << "\"audit_profile\":\"" << JsonEscape(handoff.audit_profile) << "\"";
  oss << ",\"policy_name\":\"" << kAuditPolicyName << "\"";
  oss << ",\"policy_version\":\"" << kAuditPolicyVersion << "\"";
  oss << ",\"policy_mode\":\"" << ResolvePolicyMode(handoff.policy_mode) << "\"";
  oss << ",\"audit_mode\":\"" << JsonEscape(handoff.audit_mode) << "\"";
  oss << ",\"bundle_verdict\":\"" << JsonEscape(handoff.verdict) << "\"";
  oss << ",\"verdict\":\"" << JsonEscape(handoff.verdict) << "\"";
  oss << ",\"next_action\":\"" << JsonEscape(handoff.next_action) << "\"";
  oss << ",\"bundle_id\":\"" << JsonEscape(handoff.bundle_id) << "\"";
  oss << ",\"tool_version\":\"" << JsonEscape(handoff.tool_version) << "\"";
  oss << "}";

  oss << ",\"evidence_chain\":[";
  oss << "\"" << kAuditModeBrowserLightweightWasmAudit << "\",";
  oss << "\"" << kAuditModeLocalCliSpzArtifactAudit << "\"]";

  oss << ",\"final_verdict\":\"" << JsonEscape(artifact_verdict) << "\"";
  oss << ",\"release_ready\":"
      << (IsReleaseReadyVerdict(artifact_verdict) ? "true" : "false");
  oss << "}";

  return oss.str();
}

}  // namespace spz_gatekeeper

