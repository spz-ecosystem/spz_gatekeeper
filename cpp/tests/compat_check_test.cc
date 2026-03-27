#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

#if !defined(_WIN32)
#include <sys/wait.h>
#endif


#include "spz_gatekeeper/json_min.h"
#include "test_fixtures.h"

namespace {

constexpr std::uint8_t kFlagHasExtensions = 0x02;

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

struct CommandResult {
  int exit_code = -1;
  std::string output;
};

std::string CliBinaryPath() {
#if defined(_WIN32)
  const std::array<std::string, 3> candidates = {
      "spz_gatekeeper.exe",
      ".\\spz_gatekeeper.exe",
      "Release\\spz_gatekeeper.exe",
  };
  for (const auto& candidate : candidates) {
    if (std::filesystem::exists(candidate)) {
      return candidate;
    }
  }
  return "spz_gatekeeper.exe";
#else
  return "./spz_gatekeeper";
#endif
}


int NormalizeProcessExitCode(int status) {
#if defined(_WIN32)
  return status;
#else
  return WIFEXITED(status) ? WEXITSTATUS(status) : status;
#endif
}

class TempPathGuard {
 public:
  explicit TempPathGuard(std::filesystem::path path) : path_(std::move(path)) {}
  ~TempPathGuard() {
    std::error_code ec;
    std::filesystem::remove(path_, ec);
  }

  const std::filesystem::path& path() const {
    return path_;
  }

 private:
  std::filesystem::path path_;
};

class TempDirGuard {
 public:
  explicit TempDirGuard(std::filesystem::path path) : path_(std::move(path)) {
    std::filesystem::create_directories(path_);
  }
  ~TempDirGuard() {
    std::error_code ec;
    std::filesystem::remove_all(path_, ec);
  }

  const std::filesystem::path& path() const {
    return path_;
  }

 private:
  std::filesystem::path path_;
};


CommandResult RunCommand(const std::string& command) {
  std::array<char, 256> buffer{};
  std::string output;
  const std::string redirected = command + " 2>&1";
#if defined(_WIN32)
  FILE* pipe = _popen(redirected.c_str(), "r");
#else
  FILE* pipe = popen(redirected.c_str(), "r");
#endif
  if (pipe == nullptr) {
    throw std::runtime_error("popen failed");
  }
  while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
    output += buffer.data();
  }
#if defined(_WIN32)
  const int status = _pclose(pipe);
#else
  const int status = pclose(pipe);
#endif
  if (status == -1) {
    throw std::runtime_error("pclose failed");
  }
  return {NormalizeProcessExitCode(status), output};
}


std::filesystem::path MakeTempPath(const std::string& stem) {
  return std::filesystem::temp_directory_path() /
         (stem + "_" + std::to_string(std::rand()) + ".spz");
}

std::filesystem::path MakeTempDirPath(const std::string& stem) {
  return std::filesystem::temp_directory_path() /
         (stem + "_" + std::to_string(std::rand()));
}

std::string NormalizePathForExpectedJson(const std::filesystem::path& path, bool canonicalize) {
  if (!canonicalize) {
    return path.string();
  }
  std::error_code ec;
  const auto normalized = std::filesystem::weakly_canonical(path, ec);
  return (ec ? path : normalized).string();
}

std::string BuildAssetPathJsonFragment(const std::filesystem::path& path, bool canonicalize = false) {
  return "\"asset_path\":\"" +
         spz_gatekeeper::JsonEscape(NormalizePathForExpectedJson(path, canonicalize)) + "\"";
}

bool HasAnyBudgetReviewIssue(const std::string& output) {
  return output.find("ARTIFACT_MEMORY_BUDGET_NOT_COLLECTED") != std::string::npos ||
         output.find("ARTIFACT_MEMORY_GROWTH_BUDGET_NOT_COLLECTED") != std::string::npos ||
         output.find("ARTIFACT_MEMORY_OVER_BUDGET") != std::string::npos ||
         output.find("ARTIFACT_MEMORY_GROWTH_OVER_BUDGET") != std::string::npos;
}

void WriteBinaryFile(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
  std::ofstream out(path, std::ios::binary);
  if (!out.good()) {
    throw std::runtime_error("failed to open temp file");
  }
  out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  if (!out.good()) {
    throw std::runtime_error("failed to write temp file");
  }
}

TEST(test_compat_check_reports_dual_path_for_valid_adobe_fixture) {
  TempPathGuard input_path(MakeTempPath("compat_check_valid_adobe"));
  const auto trailer = spz_gatekeeper_test::CreateTrailer(
      {{0xADBE0002u, spz_gatekeeper_test::CreateAdobeSafeOrbitPayload()}});
  WriteBinaryFile(input_path.path(),
                  spz_gatekeeper_test::CreateMinimalSpz(1, 3, 0, 8, kFlagHasExtensions, &trailer));

  const auto result = RunCommand(std::string(CliBinaryPath()) + " compat-check \"" + input_path.path().string() + "\" --json");
  ASSERT_TRUE(result.exit_code == 0 || result.exit_code == 1);
  ASSERT_TRUE(result.output.find("\"audit_profile\":\"spz\"") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"policy_name\":\"spz_gatekeeper_policy\"") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"policy_version\":\"2.0.0\"") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"policy_mode\":\"release\"") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"audit_mode\":\"local_cli_spz_artifact_audit\"") != std::string::npos);

  ASSERT_TRUE(result.output.find("\"summary\":{") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"artifact_summary\":{") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"budgets\":{") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"file_size_bytes\":{") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"decompressed_size_bytes\":{") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"peak_memory_mb\":{\"declared\":256,\"observed\":") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"memory_growth_count\":{\"declared\":1,\"observed\":") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"issues\":[") != std::string::npos);

  const bool pass_verdict = result.output.find("\"verdict\":\"pass\"") != std::string::npos;
  const bool review_verdict = result.output.find("\"verdict\":\"review_required\"") != std::string::npos;
  ASSERT_TRUE(pass_verdict || review_verdict);
  if (pass_verdict) {
    ASSERT_TRUE(result.output.find("\"next_action\":\"artifact_ready\"") != std::string::npos);
    ASSERT_TRUE(result.output.find("\"final_verdict\":\"pass\"") != std::string::npos);
    ASSERT_TRUE(result.output.find("\"release_ready\":true") != std::string::npos);
  } else {
    ASSERT_TRUE(result.output.find("\"next_action\":\"review_artifact\"") != std::string::npos);
    ASSERT_TRUE(result.output.find("\"final_verdict\":\"review_required\"") != std::string::npos);
    ASSERT_TRUE(result.output.find("\"release_ready\":false") != std::string::npos);
    ASSERT_TRUE(HasAnyBudgetReviewIssue(result.output));
  }

  ASSERT_TRUE(result.output.find("\"strict_ok\":true") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"non_strict_ok\":true") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"registry_summary\"") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"extension_summary\"") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"wasm_quality_gate\"") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"validator_coverage_ok\":true") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"empty_shell_risk\":false") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"memory_budget_wired\":true") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"upstream_tools\"") != std::string::npos);


}

TEST(test_compat_check_surfaces_unknown_extension_issue_summary) {
  TempPathGuard input_path(MakeTempPath("compat_check_unknown_extension"));
  const auto trailer = spz_gatekeeper_test::CreateTrailer(
      {{0xCAFE0001u, std::vector<std::uint8_t>{0x00, 0x01, 0x02, 0x03}}});
  WriteBinaryFile(input_path.path(),
                  spz_gatekeeper_test::CreateMinimalSpz(1, 3, 0, 8, kFlagHasExtensions, &trailer));

  const auto result = RunCommand(std::string(CliBinaryPath()) + " compat-check \"" + input_path.path().string() + "\" --json");
  ASSERT_TRUE(result.exit_code == 1);
  ASSERT_TRUE(result.output.find("\"audit_profile\":\"spz\"") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"policy_name\":\"spz_gatekeeper_policy\"") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"policy_version\":\"2.0.0\"") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"policy_mode\":\"release\"") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"audit_mode\":\"local_cli_spz_artifact_audit\"") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"verdict\":\"review_required\"") != std::string::npos);

  ASSERT_TRUE(result.output.find("\"summary\":{") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"artifact_summary\":{") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"budgets\":{") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"file_size_bytes\":{") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"decompressed_size_bytes\":{") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"issues\":[") != std::string::npos);

  ASSERT_TRUE(result.output.find("\"next_action\":\"review_artifact\"") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"strict_ok\":false") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"non_strict_ok\":true") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"issue_summary\"") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"validator_coverage_ok\":false") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"empty_shell_risk\":true") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"final_verdict\":\"review_required\"") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"release_ready\":false") != std::string::npos);
  ASSERT_TRUE(result.output.find("L2_EXT_UNKNOWN") != std::string::npos);
}


TEST(test_compat_check_dir_mode_outputs_batch_summary) {
  TempDirGuard workdir(MakeTempDirPath("compat_check_dir_mode"));
  const auto pass_trailer = spz_gatekeeper_test::CreateTrailer(
      {{0xADBE0002u, spz_gatekeeper_test::CreateAdobeSafeOrbitPayload()}});
  const auto review_trailer = spz_gatekeeper_test::CreateTrailer(
      {{0xCAFE0001u, std::vector<std::uint8_t>{0x01, 0x02, 0x03, 0x04}}});

  const auto pass_path = workdir.path() / "pass.spz";
  const auto review_path = workdir.path() / "review.spz";
  WriteBinaryFile(pass_path,
                  spz_gatekeeper_test::CreateMinimalSpz(1, 3, 0, 8, kFlagHasExtensions, &pass_trailer));
  WriteBinaryFile(review_path,
                  spz_gatekeeper_test::CreateMinimalSpz(1, 3, 0, 8, kFlagHasExtensions, &review_trailer));

  const auto result = RunCommand(std::string(CliBinaryPath()) + " compat-check --dir \"" + workdir.path().string() + "\" --json");
  ASSERT_TRUE(result.exit_code == 1);
  ASSERT_TRUE(result.output.find("\"audit_mode\":\"local_cli_spz_artifact_audit\"") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"policy_mode\":\"release\"") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"summary\":{") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"total\":2") != std::string::npos);
  const bool stable_release_summary =
      result.output.find("\"pass\":1") != std::string::npos &&
      result.output.find("\"review_required\":1") != std::string::npos;
  const bool budget_limited_summary =
      result.output.find("\"pass\":0") != std::string::npos &&
      result.output.find("\"review_required\":2") != std::string::npos;
  ASSERT_TRUE(stable_release_summary || budget_limited_summary);
  ASSERT_TRUE(result.output.find("\"items\":[") != std::string::npos);
  ASSERT_TRUE(result.output.find(BuildAssetPathJsonFragment(pass_path) + ",\"audit_profile\":\"spz\",\"policy_name\":\"spz_gatekeeper_policy\",\"policy_version\":\"2.0.0\",\"policy_mode\":\"release\"") != std::string::npos);
  ASSERT_TRUE(result.output.find(BuildAssetPathJsonFragment(review_path) + ",\"audit_profile\":\"spz\",\"policy_name\":\"spz_gatekeeper_policy\",\"policy_version\":\"2.0.0\",\"policy_mode\":\"release\"") != std::string::npos);
}

TEST(test_compat_check_manifest_mode_outputs_batch_summary) {
  TempDirGuard workdir(MakeTempDirPath("compat_check_manifest_mode"));
  const auto pass_trailer = spz_gatekeeper_test::CreateTrailer(
      {{0xADBE0002u, spz_gatekeeper_test::CreateAdobeSafeOrbitPayload()}});
  const auto block_trailer = spz_gatekeeper_test::CreateTrailer(
      {{0xADBE0002u, std::vector<std::uint8_t>{0x00, 0x00, 0x00, 0x00}}});

  const auto pass_path = workdir.path() / "manifest_pass.spz";
  const auto block_path = workdir.path() / "manifest_block.spz";
  WriteBinaryFile(pass_path,
                  spz_gatekeeper_test::CreateMinimalSpz(1, 3, 0, 8, kFlagHasExtensions, &pass_trailer));
  WriteBinaryFile(block_path,
                  spz_gatekeeper_test::CreateMinimalSpz(1, 3, 0, 8, kFlagHasExtensions, &block_trailer));

  const auto manifest_path = workdir.path() / "manifest.json";
  std::ofstream manifest(manifest_path);
  const auto pass_name = pass_path.filename().generic_string();
  const auto block_name = block_path.filename().generic_string();
  manifest << "{\"files\":[\"" << pass_name << "\",\"" << block_name << "\"]}";
  manifest.close();


  const auto result = RunCommand(std::string(CliBinaryPath()) + " compat-check --manifest \"" + manifest_path.string() + "\" --allow-legacy-text-scan --json");
  ASSERT_TRUE(result.exit_code == 1);
  ASSERT_TRUE(result.output.find("\"summary\":{") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"total\":2") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"block\":1") != std::string::npos);
  const bool stable_release_summary =
      result.output.find("\"pass\":1") != std::string::npos &&
      result.output.find("\"review_required\":0") != std::string::npos;
  const bool budget_limited_summary =
      result.output.find("\"pass\":0") != std::string::npos &&
      result.output.find("\"review_required\":1") != std::string::npos;
  ASSERT_TRUE(stable_release_summary || budget_limited_summary);
  ASSERT_TRUE(result.output.find("\"top_issues\":[") != std::string::npos);
}

TEST(test_compat_check_manifest_mode_rejects_legacy_without_flag) {
  TempDirGuard workdir(MakeTempDirPath("compat_check_manifest_legacy_reject"));
  const auto pass_trailer = spz_gatekeeper_test::CreateTrailer(
      {{0xADBE0002u, spz_gatekeeper_test::CreateAdobeSafeOrbitPayload()}});
  const auto pass_path = workdir.path() / "manifest_pass.spz";
  WriteBinaryFile(pass_path,
                  spz_gatekeeper_test::CreateMinimalSpz(1, 3, 0, 8, kFlagHasExtensions, &pass_trailer));

  const auto manifest_path = workdir.path() / "legacy_manifest.json";
  std::ofstream manifest(manifest_path);
  manifest << "{\"files\":[\"" << pass_path.filename().generic_string() << "\"]}";
  manifest.close();

  const auto result =
      RunCommand(std::string(CliBinaryPath()) + " compat-check --manifest \"" + manifest_path.string() + "\" --json");
  ASSERT_TRUE(result.exit_code == 2);
  ASSERT_TRUE(result.output.find("legacy text-scan fallback disabled") != std::string::npos);
}

TEST(test_compat_check_manifest_mode_supports_structured_items_and_grouped_summary) {
  TempDirGuard workdir(MakeTempDirPath("compat_check_manifest_structured"));
  const auto pass_trailer = spz_gatekeeper_test::CreateTrailer(
      {{0xADBE0002u, spz_gatekeeper_test::CreateAdobeSafeOrbitPayload()}});
  const auto review_trailer = spz_gatekeeper_test::CreateTrailer(
      {{0xCAFE0001u, std::vector<std::uint8_t>{0x01, 0x02, 0x03, 0x04}}});
  const auto block_trailer = spz_gatekeeper_test::CreateTrailer(
      {{0xADBE0002u, std::vector<std::uint8_t>{0x00, 0x00, 0x00, 0x00}}});

  const auto review_path = workdir.path() / "scene_review.spz";
  const auto pass_path = workdir.path() / "scene_pass.spz";
  const auto block_path = workdir.path() / "scene_block.spz";
  WriteBinaryFile(review_path,
                  spz_gatekeeper_test::CreateMinimalSpz(1, 3, 0, 8, kFlagHasExtensions, &review_trailer));
  WriteBinaryFile(pass_path,
                  spz_gatekeeper_test::CreateMinimalSpz(1, 3, 0, 8, kFlagHasExtensions, &pass_trailer));
  WriteBinaryFile(block_path,
                  spz_gatekeeper_test::CreateMinimalSpz(1, 3, 0, 8, kFlagHasExtensions, &block_trailer));

  const auto manifest_path = workdir.path() / "challenge_manifest.json";
  std::ofstream manifest(manifest_path);
  manifest << "{\"items\":["
           << "{\"path\":\"" << review_path.filename().generic_string()
           << "\",\"scene_id\":\"scene-b\",\"group\":\"community\",\"split\":\"dev\",\"difficulty\":\"medium\"},"
           << "{\"path\":\"" << pass_path.filename().generic_string()
           << "\",\"scene_id\":\"scene-a\",\"group\":\"official\",\"split\":\"challenge\",\"difficulty\":\"easy\"},"
           << "{\"path\":\"" << block_path.filename().generic_string()
           << "\",\"scene_id\":\"scene-c\",\"group\":\"official\",\"split\":\"challenge\",\"difficulty\":\"hard\"}"
           << "]}";
  manifest.close();

  const auto result = RunCommand(std::string(CliBinaryPath()) + " compat-check --manifest \"" + manifest_path.string() + "\" --json");
  ASSERT_TRUE(result.exit_code == 1);
  ASSERT_TRUE(result.output.find("\"policy_name\":\"spz_gatekeeper_policy\"") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"policy_version\":\"2.0.0\"") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"policy_mode\":\"challenge\"") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"summary\":{") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"total\":3") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"block\":1") != std::string::npos);
  const bool stable_release_summary =
      result.output.find("\"pass\":1") != std::string::npos &&
      result.output.find("\"review_required\":1") != std::string::npos;
  const bool budget_limited_summary =
      result.output.find("\"pass\":0") != std::string::npos &&
      result.output.find("\"review_required\":2") != std::string::npos;
  ASSERT_TRUE(stable_release_summary || budget_limited_summary);
  ASSERT_TRUE(result.output.find("\"grouped_summary\":{") != std::string::npos);
  ASSERT_TRUE(result.output.find(BuildAssetPathJsonFragment(review_path, true) + ",\"audit_profile\":\"spz\",\"policy_name\":\"spz_gatekeeper_policy\",\"policy_version\":\"2.0.0\",\"policy_mode\":\"challenge\"") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"manifest_labels\":{\"scene_id\":\"scene-b\",\"group\":\"community\",\"split\":\"dev\",\"difficulty\":\"medium\"}") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"value\":\"community\",\"summary\":{\"total\":1,\"pass\":0,\"review_required\":1,\"block\":0}") != std::string::npos);
  const bool official_stable = result.output.find("\"value\":\"official\",\"summary\":{\"total\":2,\"pass\":1,\"review_required\":0,\"block\":1}") != std::string::npos;
  const bool official_budget_limited = result.output.find("\"value\":\"official\",\"summary\":{\"total\":2,\"pass\":0,\"review_required\":1,\"block\":1}") != std::string::npos;
  ASSERT_TRUE(official_stable || official_budget_limited);
  const bool challenge_stable = result.output.find("\"value\":\"challenge\",\"summary\":{\"total\":2,\"pass\":1,\"review_required\":0,\"block\":1}") != std::string::npos;
  const bool challenge_budget_limited = result.output.find("\"value\":\"challenge\",\"summary\":{\"total\":2,\"pass\":0,\"review_required\":1,\"block\":1}") != std::string::npos;
  ASSERT_TRUE(challenge_stable || challenge_budget_limited);
  ASSERT_TRUE(result.output.find("\"value\":\"scene-b\",\"summary\":{\"total\":1,\"pass\":0,\"review_required\":1,\"block\":0}") != std::string::npos);

  const auto review_pos = result.output.find(BuildAssetPathJsonFragment(review_path, true));
  const auto pass_pos = result.output.find(BuildAssetPathJsonFragment(pass_path, true));
  const auto block_pos = result.output.find(BuildAssetPathJsonFragment(block_path, true));
  ASSERT_TRUE(review_pos != std::string::npos);
  ASSERT_TRUE(pass_pos != std::string::npos);
  ASSERT_TRUE(block_pos != std::string::npos);
  ASSERT_TRUE(review_pos < pass_pos);
  ASSERT_TRUE(pass_pos < block_pos);
}

TEST(test_compat_check_manifest_mode_emits_challenge_stats_and_visualization) {
  TempDirGuard workdir(MakeTempDirPath("compat_check_manifest_visualization"));
  const auto pass_trailer = spz_gatekeeper_test::CreateTrailer(
      {{0xADBE0002u, spz_gatekeeper_test::CreateAdobeSafeOrbitPayload()}});
  const auto review_trailer = spz_gatekeeper_test::CreateTrailer(
      {{0xCAFE0001u, std::vector<std::uint8_t>{0x01, 0x02, 0x03, 0x04}}});

  const auto pass_path = workdir.path() / "scene_pass.spz";
  const auto review_path = workdir.path() / "scene_review.spz";
  WriteBinaryFile(pass_path,
                  spz_gatekeeper_test::CreateMinimalSpz(1, 3, 0, 8, kFlagHasExtensions, &pass_trailer));
  WriteBinaryFile(review_path,
                  spz_gatekeeper_test::CreateMinimalSpz(1, 3, 0, 8, kFlagHasExtensions, &review_trailer));

  const auto manifest_path = workdir.path() / "challenge_manifest_visualization.json";
  std::ofstream manifest(manifest_path);
  manifest << "{\"items\":["
           << "{\"path\":\"" << pass_path.filename().generic_string()
           << "\",\"scene_id\":\"scene-a\",\"group\":\"official\",\"split\":\"challenge\",\"difficulty\":\"easy\"},"
           << "{\"path\":\"" << review_path.filename().generic_string()
           << "\",\"scene_id\":\"scene-b\",\"group\":\"community\",\"split\":\"dev\",\"difficulty\":\"medium\"}"
           << "]}";
  manifest.close();

  const auto result = RunCommand(std::string(CliBinaryPath()) + " compat-check --manifest \"" + manifest_path.string() + "\" --json");
  ASSERT_TRUE(result.exit_code == 1);
  ASSERT_TRUE(result.output.find("\"challenge_stats\":{") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"labeled_items\":2") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"unlabeled_items\":0") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"dimensions\":[\"scene_id\",\"group\",\"split\",\"difficulty\"]") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"stable_item_order\":true") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"visualization\":{") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"table_columns\":[\"asset_path\",\"verdict\",\"scene_id\",\"group\",\"split\",\"difficulty\"]") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"grouped_dimensions\":[\"scene_id\",\"group\",\"split\",\"difficulty\"]") != std::string::npos);
}

TEST(test_compat_check_merges_browser_handoff_without_skipping_artifact_audit) {
  TempDirGuard workdir(MakeTempDirPath("compat_check_handoff_mode"));
  const auto trailer = spz_gatekeeper_test::CreateTrailer(
      {{0xADBE0002u, spz_gatekeeper_test::CreateAdobeSafeOrbitPayload()}});

  const auto input_path = workdir.path() / "artifact_pass.spz";
  WriteBinaryFile(input_path,
                  spz_gatekeeper_test::CreateMinimalSpz(1, 3, 0, 8, kFlagHasExtensions, &trailer));

  const auto handoff_path = workdir.path() / "browser_handoff.json";
  std::ofstream handoff(handoff_path);
  handoff << "{\"schema_version\":\"spz_gatekeeper.browser_to_cli_handoff.v1\","
             "\"audit_profile\":\"spz\",\"audit_mode\":\"browser_lightweight_wasm_audit\","
             "\"policy_mode\":\"dev\",\"bundle_id\":\"sha256:test-bundle\",\"tool_version\":\"1.0.0\","
             "\"bundle_verdict\":\"block\",\"verdict\":\"block\",\"final_verdict\":\"block\","
             "\"release_ready\":false,\"summary\":{},\"budgets\":{},\"issues\":[],"
             "\"next_action\":\"block_bundle\"}";
  handoff.close();

  const auto result = RunCommand(std::string(CliBinaryPath()) + " compat-check \"" + input_path.string() + "\" --handoff \"" + handoff_path.string() + "\" --json");
  ASSERT_TRUE(result.exit_code == 0 || result.exit_code == 1);
  const bool pass_verdict = result.output.find("\"verdict\":\"pass\"") != std::string::npos;
  const bool review_verdict = result.output.find("\"verdict\":\"review_required\"") != std::string::npos;
  ASSERT_TRUE(pass_verdict || review_verdict);
  ASSERT_TRUE(result.output.find("\"handoff\":{") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"upstream_audit\":{") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"policy_mode\":\"dev\"") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"bundle_id\":\"sha256:test-bundle\"") != std::string::npos);
  if (pass_verdict) {
    ASSERT_TRUE(result.output.find("\"final_verdict\":\"pass\"") != std::string::npos);
  } else {
    ASSERT_TRUE(result.output.find("\"final_verdict\":\"review_required\"") != std::string::npos);
    ASSERT_TRUE(HasAnyBudgetReviewIssue(result.output));
  }
  ASSERT_TRUE(result.output.find("\"evidence_chain\":[\"browser_lightweight_wasm_audit\",\"local_cli_spz_artifact_audit\"]") != std::string::npos);
}

TEST(test_help_describes_task5_boundaries_and_handoff_contract) {
  const auto result = RunCommand(std::string(CliBinaryPath()) + " --help");
  ASSERT_TRUE(result.exit_code == 0);
  ASSERT_TRUE(result.output.find("local_cli_spz_artifact_audit") != std::string::npos);
  ASSERT_TRUE(result.output.find("browser_to_cli_handoff") != std::string::npos);
  ASSERT_TRUE(result.output.find("does not audit GLB or spz2glb") != std::string::npos);
  ASSERT_TRUE(result.output.find("final verdict still comes from the local CLI artifact audit") != std::string::npos);
  ASSERT_TRUE(result.output.find("--allow-legacy-text-scan") != std::string::npos);
  ASSERT_TRUE(result.output.find("legacy path text-scan fallback is disabled by default") != std::string::npos);
}


}  // namespace



int main() {
  std::cout << "=== compat-check CLI Tests ===" << std::endl;
  std::cout << std::endl;

  RUN_TEST(test_compat_check_reports_dual_path_for_valid_adobe_fixture);
  RUN_TEST(test_compat_check_surfaces_unknown_extension_issue_summary);
  RUN_TEST(test_compat_check_dir_mode_outputs_batch_summary);
  RUN_TEST(test_compat_check_manifest_mode_outputs_batch_summary);
  RUN_TEST(test_compat_check_manifest_mode_rejects_legacy_without_flag);
  RUN_TEST(test_compat_check_manifest_mode_supports_structured_items_and_grouped_summary);
  RUN_TEST(test_compat_check_manifest_mode_emits_challenge_stats_and_visualization);
  RUN_TEST(test_compat_check_merges_browser_handoff_without_skipping_artifact_audit);
  RUN_TEST(test_help_describes_task5_boundaries_and_handoff_contract);

  std::cout << std::endl;

  std::cout << "=== Test Summary ===" << std::endl;
  std::cout << "Tests run: " << g_tests_run << std::endl;
  std::cout << "Tests passed: " << g_tests_passed << std::endl;
  std::cout << "Tests failed: " << (g_tests_run - g_tests_passed) << std::endl;

  if (g_tests_passed == g_tests_run) {
    std::cout << "Result: ALL " << g_tests_passed << " COMPAT-CHECK TESTS PASSED ✓" << std::endl;
    return EXIT_SUCCESS;
  }

  std::cout << "Result: " << (g_tests_run - g_tests_passed) << " COMPAT-CHECK TESTS FAILED ✗" << std::endl;
  return EXIT_FAILURE;
}
