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
  ASSERT_TRUE(result.exit_code == 0);
  ASSERT_TRUE(result.output.find("\"audit_profile\":\"spz\"") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"audit_mode\":\"local_cli_spz_artifact_audit\"") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"verdict\":\"pass\"") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"summary\":{") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"budgets\":{}") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"issues\":[") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"next_action\":\"artifact_ready\"") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"strict_ok\":true") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"non_strict_ok\":true") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"registry_summary\"") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"extension_summary\"") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"wasm_quality_gate\"") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"validator_coverage_ok\":true") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"empty_shell_risk\":false") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"release_ready\":false") != std::string::npos);
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
  ASSERT_TRUE(result.output.find("\"audit_mode\":\"local_cli_spz_artifact_audit\"") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"verdict\":\"review_required\"") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"summary\":{") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"budgets\":{}") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"issues\":[") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"next_action\":\"review_artifact\"") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"strict_ok\":false") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"non_strict_ok\":true") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"issue_summary\"") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"validator_coverage_ok\":false") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"empty_shell_risk\":true") != std::string::npos);
  ASSERT_TRUE(result.output.find("L2_EXT_UNKNOWN") != std::string::npos);

}



}  // namespace

int main() {
  std::cout << "=== compat-check CLI Tests ===" << std::endl;
  std::cout << std::endl;

  RUN_TEST(test_compat_check_reports_dual_path_for_valid_adobe_fixture);
  RUN_TEST(test_compat_check_surfaces_unknown_extension_issue_summary);

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
