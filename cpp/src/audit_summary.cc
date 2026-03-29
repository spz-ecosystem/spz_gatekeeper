// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PuJunhan

#include "spz_gatekeeper/audit_summary.h"

#include "spz_gatekeeper/extension_spec_registry.h"
#include "spz_gatekeeper/json_min.h"

#include <array>
#include <cctype>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
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
  if (policy_mode == kAuditPolicyModeDev) {
    return kAuditPolicyModeDev;
  }
  if (policy_mode == kAuditPolicyModeChallenge) {
    return kAuditPolicyModeChallenge;
  }
  return kAuditPolicyModeRelease;
}

const char* ResolveStageStatus(const std::string& status, const char* fallback) {
  if (status == kStageStatusSuccess) {
    return kStageStatusSuccess;
  }
  if (status == kStageStatusFailed) {
    return kStageStatusFailed;
  }
  if (status == kStageStatusSkipped) {
    return kStageStatusSkipped;
  }
  if (status == kStageStatusMissing) {
    return kStageStatusMissing;
  }
  return fallback;
}

const char* ResolveArtifactSourceStage(const std::string& source_stage) {
  if (source_stage == kAuditStageWebAudit) {
    return kAuditStageWebAudit;
  }
  if (source_stage == kAuditStageCliAudit) {
    return kAuditStageCliAudit;
  }
  if (source_stage == kAuditStageChallengeReport) {
    return kAuditStageChallengeReport;
  }
  if (source_stage == kAuditStageReportFusion) {
    return kAuditStageReportFusion;
  }
  return kAuditStageEncodeRun;
}

std::string OptionalJsonString(const std::string& value) {
  if (value.empty()) {
    return "null";
  }
  return "\"" + JsonEscape(value) + "\"";
}

std::string BuildFailureJson(const std::string& code, const std::string& message) {
  std::ostringstream oss;
  oss << "{";
  oss << "\"code\":" << OptionalJsonString(code);
  oss << ",\"message\":" << OptionalJsonString(message);
  oss << "}";
  return oss.str();
}

std::string BuildArtifactRecordJson(const AuditArtifactRecord& artifact) {
  std::ostringstream oss;
  oss << "{";
  oss << "\"" << kArtifactFieldArtifactId << "\":\"" << JsonEscape(artifact.artifact_id)
      << "\"";
  oss << ",\"" << kArtifactFieldArtifactKind << "\":\""
      << JsonEscape(artifact.artifact_kind) << "\"";
  oss << ",\"" << kArtifactFieldSourceStage << "\":\""
      << ResolveArtifactSourceStage(artifact.source_stage) << "\"";
  oss << ",\"status\":\"" << ResolveStageStatus(artifact.status, kStageStatusSuccess) << "\"";
  oss << ",\"path\":" << OptionalJsonString(artifact.path);
  oss << ",\"media_type\":" << OptionalJsonString(artifact.media_type);
  oss << ",\"sha256\":" << OptionalJsonString(artifact.sha256);
  oss << ",\"size_bytes\":";
  if (artifact.has_size_bytes) {
    oss << artifact.size_bytes;
  } else {
    oss << "null";
  }
  oss << "}";
  return oss.str();
}

std::string BuildArtifactRecordListJson(const std::vector<AuditArtifactRecord>& artifacts) {
  std::ostringstream oss;
  oss << "[";
  for (std::size_t i = 0; i < artifacts.size(); ++i) {
    if (i) {
      oss << ",";
    }
    oss << BuildArtifactRecordJson(artifacts[i]);
  }
  oss << "]";
  return oss.str();
}

struct ArtifactStatusCounts {
  std::size_t total = 0;
  std::size_t success = 0;
  std::size_t failed = 0;
  std::size_t skipped = 0;
  std::size_t missing = 0;
};

ArtifactStatusCounts CountArtifactStatuses(const std::vector<AuditArtifactRecord>& artifacts) {
  ArtifactStatusCounts counts;
  for (const auto& artifact : artifacts) {
    ++counts.total;
    const char* status = ResolveStageStatus(artifact.status, kStageStatusSuccess);
    if (status == kStageStatusFailed) {
      ++counts.failed;
    } else if (status == kStageStatusSkipped) {
      ++counts.skipped;
    } else if (status == kStageStatusMissing) {
      ++counts.missing;
    } else {
      ++counts.success;
    }
  }
  return counts;
}

std::string BuildArtifactIndexSummaryJson(const std::vector<AuditArtifactRecord>& artifacts) {
  const ArtifactStatusCounts counts = CountArtifactStatuses(artifacts);
  std::ostringstream oss;
  oss << "{";
  oss << "\"total_artifacts\":" << counts.total;
  oss << ",\"success_artifacts\":" << counts.success;
  oss << ",\"failed_artifacts\":" << counts.failed;
  oss << ",\"skipped_artifacts\":" << counts.skipped;
  oss << ",\"missing_artifacts\":" << counts.missing;
  oss << "}";
  return oss.str();
}

struct BudgetThreshold {
  bool has_declared = false;
  double declared = 0.0;
};

struct BudgetPolicyEntry {
  std::string_view metric_name;
  BudgetThreshold dev;
  BudgetThreshold release;
  BudgetThreshold challenge;
};

constexpr BudgetThreshold NoDeclaredBudget() {
  return {false, 0.0};
}

constexpr BudgetThreshold DeclaredBudget(double declared) {
  return {true, declared};
}

constexpr std::array<BudgetPolicyEntry, 8> kUnifiedBudgetPolicyTable = {{
    {kAuditBudgetColdStartMs, NoDeclaredBudget(), DeclaredBudget(1500.0), DeclaredBudget(1500.0)},
    {kAuditBudgetTinyCaseMs, NoDeclaredBudget(), DeclaredBudget(500.0), DeclaredBudget(500.0)},
    {kAuditBudgetPeakMemoryMb, NoDeclaredBudget(), DeclaredBudget(256.0), DeclaredBudget(256.0)},
    {kAuditBudgetCopyPassLimit, NoDeclaredBudget(), DeclaredBudget(2.0), DeclaredBudget(2.0)},
    {kAuditBudgetFileSizeBytes, NoDeclaredBudget(), NoDeclaredBudget(), NoDeclaredBudget()},
    {kAuditBudgetDecompressedSizeBytes, NoDeclaredBudget(), NoDeclaredBudget(), NoDeclaredBudget()},
    {kAuditBudgetProcessTimeMs, NoDeclaredBudget(), NoDeclaredBudget(), NoDeclaredBudget()},
    {kAuditBudgetMemoryGrowthCount, NoDeclaredBudget(), DeclaredBudget(1.0), DeclaredBudget(1.0)},
}};

BudgetThreshold ResolveBudgetThreshold(const char* policy_mode, std::string_view metric_name) {
  const std::string resolved_policy_mode =
      ResolvePolicyMode(policy_mode == nullptr ? "" : policy_mode);
  for (const auto& entry : kUnifiedBudgetPolicyTable) {
    if (entry.metric_name != metric_name) {
      continue;
    }
    if (resolved_policy_mode == kAuditPolicyModeDev) {
      return entry.dev;
    }
    if (resolved_policy_mode == kAuditPolicyModeChallenge) {
      return entry.challenge;
    }
    return entry.release;
  }
  return {};
}

void AppendIssue(std::vector<Issue>* issues,
                 Severity severity,
                 std::string_view code,
                 std::string_view message) {
  issues->push_back(Issue{severity, std::string(code), std::string(message), ""});
}

bool AppendArtifactBudgetIssues(const char* policy_mode,
                                const CompatAuditMetrics* metrics,
                                std::vector<Issue>* issues) {
  bool requires_review = false;

  const auto peak_memory_budget = ResolveBudgetThreshold(policy_mode, kAuditBudgetPeakMemoryMb);
  if (peak_memory_budget.has_declared) {
    if (metrics == nullptr || !metrics->has_peak_memory_mb) {
      AppendIssue(issues,
                  Severity::kWarning,
                  "ARTIFACT_MEMORY_BUDGET_NOT_COLLECTED",
                  "peak_memory_mb 未完成采集，不能作为 release/challenge 通过依据");
      requires_review = true;
    } else if (metrics->peak_memory_mb > peak_memory_budget.declared) {
      AppendIssue(issues,
                  Severity::kWarning,
                  "ARTIFACT_MEMORY_OVER_BUDGET",
                  "peak_memory_mb 超出 policy/manifest 预算");
      requires_review = true;
    }
  }

  const auto memory_growth_budget =
      ResolveBudgetThreshold(policy_mode, kAuditBudgetMemoryGrowthCount);
  if (memory_growth_budget.has_declared) {
    if (metrics == nullptr || !metrics->has_memory_growth_count) {
      AppendIssue(issues,
                  Severity::kWarning,
                  "ARTIFACT_MEMORY_GROWTH_BUDGET_NOT_COLLECTED",
                  "memory_growth_count 未完成采集，不能作为 release/challenge 通过依据");
      requires_review = true;
    } else if (static_cast<double>(metrics->memory_growth_count) > memory_growth_budget.declared) {
      AppendIssue(issues,
                  Severity::kWarning,
                  "ARTIFACT_MEMORY_GROWTH_OVER_BUDGET",
                  "memory_growth_count 超出 policy/manifest 预算");
      requires_review = true;
    }
  }

  return requires_review;
}

bool IsReleaseReadyVerdict(const std::string& verdict) {
  return verdict == "pass";
}

std::string ResolveEffectiveFinalVerdict(const std::string& verdict,
                                        const std::string& final_verdict) {
  return IsSupportedAuditVerdict(final_verdict) ? final_verdict : verdict;
}

bool ResolveEffectiveReleaseReady(const std::string& final_verdict,
                                  bool /*has_release_ready*/,
                                  bool /*release_ready*/) {
  return IsReleaseReadyVerdict(final_verdict);
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
                                     const CompatAuditMetrics* metrics,
                                     const char* policy_mode) {
  const bool has_decompressed_size = non_strict_report.spz_l2.has_value();
  const auto file_size_budget = ResolveBudgetThreshold(policy_mode, kAuditBudgetFileSizeBytes);
  const auto decompressed_size_budget =
      ResolveBudgetThreshold(policy_mode, kAuditBudgetDecompressedSizeBytes);
  const auto process_time_budget = ResolveBudgetThreshold(policy_mode, kAuditBudgetProcessTimeMs);
  const auto peak_memory_budget = ResolveBudgetThreshold(policy_mode, kAuditBudgetPeakMemoryMb);
  const auto memory_growth_budget =
      ResolveBudgetThreshold(policy_mode, kAuditBudgetMemoryGrowthCount);
  std::ostringstream oss;
  oss << "{";
  oss << "\"file_size_bytes\":"
      << BuildBudgetItemJson(file_size_budget.has_declared,
                             file_size_budget.declared,
                             metrics != nullptr && metrics->has_file_size_bytes,
                             metrics != nullptr && metrics->has_file_size_bytes
                                 ? static_cast<double>(metrics->file_size_bytes)
                                 : 0.0);
  oss << ",\"decompressed_size_bytes\":"
      << BuildBudgetItemJson(decompressed_size_budget.has_declared,
                             decompressed_size_budget.declared,
                             has_decompressed_size,
                             has_decompressed_size
                                 ? static_cast<double>(non_strict_report.spz_l2->decompressed_size)
                                 : 0.0);
  oss << ",\"process_time_ms\":"
      << BuildBudgetItemJson(process_time_budget.has_declared,
                             process_time_budget.declared,
                             metrics != nullptr && metrics->has_process_time_ms,
                             metrics != nullptr && metrics->has_process_time_ms
                                 ? metrics->process_time_ms
                                 : 0.0);
  oss << ",\"peak_memory_mb\":"
      << BuildBudgetItemJson(peak_memory_budget.has_declared,
                             peak_memory_budget.declared,
                             metrics != nullptr && metrics->has_peak_memory_mb,
                             metrics != nullptr && metrics->has_peak_memory_mb
                                 ? metrics->peak_memory_mb
                                 : 0.0);
  oss << ",\"memory_growth_count\":"
      << BuildBudgetItemJson(memory_growth_budget.has_declared,
                             memory_growth_budget.declared,
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

std::string ResolveCompatAuditVerdict(const GateReport& strict_report,
                                      const GateReport& non_strict_report,
                                      const CompatAuditMetrics* metrics,
                                      const char* policy_mode,
                                      bool* strict_ok,
                                      bool* non_strict_ok) {
  std::string verdict = ResolveCompatVerdict(strict_report, non_strict_report, strict_ok, non_strict_ok);
  std::vector<Issue> budget_issues;
  if (AppendArtifactBudgetIssues(policy_mode, metrics, &budget_issues) && verdict == "pass") {
    return "review_required";
  }
  return verdict;
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

std::string BuildEncodeRunJson(const EncodeRunAuditReport& report) {
  std::ostringstream oss;
  oss << "{";
  oss << "\"" << kArtifactFieldArtifactId << "\":\"" << JsonEscape(report.artifact_id) << "\"";
  oss << ",\"" << kArtifactFieldArtifactKind << "\":\"" << kArtifactKindEncodeRunJson << "\"";
  oss << ",\"" << kArtifactFieldSourceStage << "\":\"" << kAuditStageEncodeRun << "\"";
  oss << ",\"" << kDualEndFieldRunMeta << "\":{";
  oss << "\"run_id\":\"" << JsonEscape(report.run_id) << "\"";
  oss << ",\"policy_mode\":\"" << ResolvePolicyMode(report.policy_mode) << "\"";
  oss << "}";
  oss << ",\"status\":\"" << ResolveStageStatus(report.status, kStageStatusSuccess) << "\"";
  oss << ",\"input_summary\":{";
  oss << "\"bundle_id\":" << OptionalJsonString(report.bundle_id);
  oss << ",\"handoff_artifact_id\":" << OptionalJsonString(report.handoff_artifact_id);
  oss << ",\"input_ply_path\":" << OptionalJsonString(report.input_ply_path);
  oss << "}";
  oss << ",\"output_summary\":{";
  oss << "\"output_spz_path\":" << OptionalJsonString(report.output_spz_path);
  oss << ",\"output_log_path\":" << OptionalJsonString(report.output_log_path);
  oss << "}";
  oss << ",\"metrics\":{";
  oss << "\"duration_ms\":";
  if (report.has_duration_ms) {
    oss << report.duration_ms;
  } else {
    oss << "null";
  }
  oss << "}";
  oss << ",\"artifacts\":" << BuildArtifactRecordListJson(report.artifacts);
  oss << ",\"failure\":" << BuildFailureJson(report.failure_code, report.failure_message);
  oss << ",\"issues\":" << BuildIssueListJson(report.issues);
  oss << "}";
  return oss.str();
}

std::string BuildArtifactIndexJson(const ArtifactIndexReport& report) {
  std::ostringstream oss;
  oss << "{";
  oss << "\"" << kArtifactFieldArtifactId << "\":\"" << JsonEscape(report.artifact_id) << "\"";
  oss << ",\"" << kArtifactFieldArtifactKind << "\":\"" << kArtifactKindArtifactIndexJson << "\"";
  oss << ",\"" << kArtifactFieldSourceStage << "\":\"" << kAuditStageEncodeRun << "\"";
  oss << ",\"" << kDualEndFieldRunMeta << "\":{";
  oss << "\"run_id\":\"" << JsonEscape(report.run_id) << "\"";
  oss << ",\"policy_mode\":\"" << ResolvePolicyMode(report.policy_mode) << "\"";
  oss << "}";
  oss << ",\"status\":\"" << ResolveStageStatus(report.status, kStageStatusSuccess) << "\"";
  oss << ",\"summary\":" << BuildArtifactIndexSummaryJson(report.artifacts);
  oss << ",\"artifacts\":" << BuildArtifactRecordListJson(report.artifacts);
  oss << ",\"failure\":" << BuildFailureJson(report.failure_code, report.failure_message);
  oss << ",\"issues\":" << BuildIssueListJson(report.issues);
  oss << "}";
  return oss.str();
}

std::string BuildDualEndReportContractSampleJson() {
  std::ostringstream oss;
  oss << "{";
  oss << "\"" << kArtifactFieldArtifactId << "\":\"sample-dual-end-report\"";
  oss << ",\"" << kArtifactFieldArtifactKind << "\":\"dual_end_report_json\"";
  oss << ",\"" << kArtifactFieldSourceStage << "\":\"" << kAuditStageReportFusion << "\"";
  oss << ",\"" << kDualEndFieldRunMeta << "\":{";
  oss << "\"run_id\":\"sample-run\"";
  oss << ",\"policy_mode\":\"" << kAuditPolicyModeRelease << "\"";
  oss << ",\"execution_mode\":\"full_dual_end\"";
  oss << "}";
  oss << ",\"" << kDualEndFieldInputSummary << "\":{";
  oss << "\"bundle_present\":true";
  oss << ",\"handoff_present\":true";
  oss << ",\"ply_present\":true";
  oss << ",\"spz_present\":true";
  oss << ",\"challenge_enabled\":true";
  oss << "}";
  oss << ",\"" << kDualEndFieldStageResults << "\":{";
  oss << "\"" << kAuditStageWebAudit << "\":{\"status\":\"" << kStageStatusSuccess << "\"}";
  oss << ",\"" << kAuditStageEncodeRun << "\":{\"status\":\"" << kStageStatusFailed << "\"}";
  oss << ",\"" << kAuditStageCliAudit << "\":{\"status\":\"" << kStageStatusSkipped << "\"}";
  oss << ",\"" << kAuditStageChallengeReport << "\":{\"status\":\"" << kStageStatusMissing << "\"}";
  oss << ",\"" << kAuditStageReportFusion << "\":{\"status\":\"" << kStageStatusSuccess << "\"}";
  oss << "}";
  oss << ",\"" << kDualEndFieldFinalVerdict << "\":{";
  oss << "\"verdict\":\"review_required\"";
  oss << "}";
  oss << ",\"" << kDualEndFieldBoards << "\":{";
  oss << "\"maturity_board_snapshot\":{\"status\":\"not_enabled\"}";
  oss << ",\"quality_board_snapshot\":{\"status\":\"not_enabled\"}";
  oss << "}";
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

  JsonParseError parse_err;
  const auto root = ParseJson(parsed.raw_json, &parse_err);
  if (!root.has_value()) {
    if (err != nullptr) {
      *err = "handoff json parse failed at offset " + std::to_string(parse_err.offset) +
             ": " + parse_err.message;
    }
    return false;
  }
  if (root->type != JsonType::kObject) {
    if (err != nullptr) {
      *err = "handoff json must be an object";
    }
    return false;
  }

  const auto require_string_field = [&](std::string_view key, std::string* out) -> bool {
    const auto* value = root->Find(key);
    if (value == nullptr) {
      if (err != nullptr) {
        *err = "missing field: " + std::string(key);
      }
      return false;
    }
    if (value->type != JsonType::kString) {
      if (err != nullptr) {
        *err = "field must be string: " + std::string(key);
      }
      return false;
    }
    *out = value->string_value;
    return true;
  };

  const auto require_bool_field = [&](std::string_view key, bool* out) -> bool {
    const auto* value = root->Find(key);
    if (value == nullptr) {
      if (err != nullptr) {
        *err = "missing field: " + std::string(key);
      }
      return false;
    }
    if (value->type != JsonType::kBool) {
      if (err != nullptr) {
        *err = "field must be bool: " + std::string(key);
      }
      return false;
    }
    *out = value->bool_value;
    return true;
  };

  if (!require_string_field("schema_version", &parsed.schema_version) ||
      !require_string_field("audit_profile", &parsed.audit_profile) ||
      !require_string_field("audit_mode", &parsed.audit_mode) ||
      !require_string_field("policy_mode", &parsed.policy_mode) ||
      !require_string_field("next_action", &parsed.next_action) ||
      !require_string_field("bundle_id", &parsed.bundle_id) ||
      !require_string_field("tool_version", &parsed.tool_version) ||
      !require_string_field("final_verdict", &parsed.final_verdict) ||
      !require_bool_field("release_ready", &parsed.release_ready)) {
    return false;
  }

  const auto* issues = root->Find("issues");
  if (issues == nullptr) {
    if (err != nullptr) {
      *err = "missing field: issues";
    }
    return false;
  }
  if (issues->type != JsonType::kArray) {
    if (err != nullptr) {
      *err = "field must be array: issues";
    }
    return false;
  }

  const auto* bundle_verdict = root->Find("bundle_verdict");
  if (bundle_verdict != nullptr && bundle_verdict->type == JsonType::kString &&
      !bundle_verdict->string_value.empty()) {
    parsed.verdict = bundle_verdict->string_value;
  } else {
    const auto* verdict = root->Find("verdict");
    if (verdict != nullptr && verdict->type == JsonType::kString &&
        !verdict->string_value.empty()) {
      parsed.verdict = verdict->string_value;
    } else {
      parsed.verdict = parsed.final_verdict;
    }
  }

  parsed.policy_mode = ResolvePolicyMode(parsed.policy_mode);

  if (parsed.schema_version != kBrowserToCliHandoffSchemaVersion) {
    if (err != nullptr) {
      *err = "unsupported handoff schema_version: " + parsed.schema_version;
    }
    return false;
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
  if (!IsSupportedAuditVerdict(parsed.final_verdict)) {
    if (err != nullptr) {
      *err = "unsupported handoff final_verdict: " + parsed.final_verdict;
    }
    return false;
  }
  const bool expected_release_ready = IsReleaseReadyVerdict(parsed.final_verdict);
  if (parsed.release_ready != expected_release_ready) {
    if (err != nullptr) {
      *err = "handoff release_ready must match final_verdict";
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
  oss << ",\"copy_breakdown\":"
      << NormalizeJsonFragment(report.copy_breakdown_json, "{}");
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
  const char* effective_policy_mode =
      ResolvePolicyMode(policy_mode == nullptr ? "" : policy_mode);
  std::vector<Issue> merged_issues = MergeIssues(strict_report, non_strict_report);

  std::string verdict = ResolveCompatAuditVerdict(
      strict_report, non_strict_report, metrics, effective_policy_mode, &strict_ok, &non_strict_ok);
  AppendArtifactBudgetIssues(effective_policy_mode, metrics, &merged_issues);
  const std::string next_action = ResolveCompatNextAction(verdict);

  const bool release_ready = IsReleaseReadyVerdict(verdict);
  std::ostringstream oss;
  oss << "{";
  oss << "\"asset_path\":\"" << JsonEscape(path) << "\"";
  oss << ",\"audit_profile\":\"" << kAuditProfileSpz << "\"";
  oss << ",\"policy_name\":\"" << kAuditPolicyName << "\"";
  oss << ",\"policy_version\":\"" << kAuditPolicyVersion << "\"";
  oss << ",\"policy_mode\":\"" << effective_policy_mode << "\"";
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
  oss << ",\"budgets\":" << BuildArtifactBudgetsJson(non_strict_report, metrics, effective_policy_mode);
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

