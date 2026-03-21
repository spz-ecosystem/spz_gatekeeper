#include <array>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

#if !defined(_WIN32)
#include <sys/wait.h>
#endif


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

struct CommandResult {
  int exit_code = -1;
  std::string output;
};

const char* CliBinaryPath() {
#if defined(_WIN32)
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


TEST(test_registry_json_lists_builtin_extension) {
  const auto result = RunCommand(std::string(CliBinaryPath()) + " registry --json");
  ASSERT_TRUE(result.exit_code == 0);
  ASSERT_TRUE(result.output.find("Adobe Safe Orbit Camera") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"status\":\"stable\"") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"has_validator\":true") != std::string::npos);
}

TEST(test_registry_list_json_alias_works) {
  const auto result = RunCommand(std::string(CliBinaryPath()) + " registry list --json");
  ASSERT_TRUE(result.exit_code == 0);
  ASSERT_TRUE(result.output.find("Adobe Safe Orbit Camera") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"category\":\"camera\"") != std::string::npos);
}

TEST(test_registry_show_json_returns_single_entry) {
  const auto result = RunCommand(std::string(CliBinaryPath()) + " registry show 0xADBE0002 --json");
  ASSERT_TRUE(result.exit_code == 0);
  ASSERT_TRUE(result.output.find("Adobe Safe Orbit Camera") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"type\":2914910210") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"spec_url\":") != std::string::npos);
}

TEST(test_registry_show_text_includes_full_contract) {
  const auto result = RunCommand(std::string(CliBinaryPath()) + " registry show 0xADBE0002");
  ASSERT_TRUE(result.exit_code == 0);
  ASSERT_TRUE(result.output.find("vendor_name=\"Adobe\"") != std::string::npos);
  ASSERT_TRUE(result.output.find("extension_name=\"Adobe Safe Orbit Camera\"") != std::string::npos);
  ASSERT_TRUE(result.output.find("has_validator=true") != std::string::npos);
}

TEST(test_registry_show_invalid_type_json_reports_original_input) {
  const auto result = RunCommand(std::string(CliBinaryPath()) + " registry show not-a-type --json");
  ASSERT_TRUE(result.exit_code == 1);
  ASSERT_TRUE(result.output.find("\"error\":\"invalid extension type\"") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"type_input\":\"not-a-type\"") != std::string::npos);
}

TEST(test_registry_show_missing_type_json_reports_original_input) {
  const auto result = RunCommand(std::string(CliBinaryPath()) + " registry show 0xBEEF0001 --json");
  ASSERT_TRUE(result.exit_code == 1);
  ASSERT_TRUE(result.output.find("\"error\":\"extension not found\"") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"type_input\":\"0xBEEF0001\"") != std::string::npos);
}

TEST(test_compat_board_json_exposes_maturity_fields) {
  const auto result = RunCommand(std::string(CliBinaryPath()) + " compat-board --json");
  ASSERT_TRUE(result.exit_code == 0);
  ASSERT_TRUE(result.output.find("\"extensions\":[") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"vendor_name\":\"Adobe\"") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"fixture_valid_pass\":true") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"fixture_invalid_pass\":true") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"strict_check_pass\":true") != std::string::npos);
  ASSERT_TRUE(result.output.find("\"non_strict_check_pass\":true") != std::string::npos);
}

TEST(test_compat_board_text_mentions_registry_role) {
  const auto result = RunCommand(std::string(CliBinaryPath()) + " compat-board");
  ASSERT_TRUE(result.exit_code == 0);
  ASSERT_TRUE(result.output.find("Compatibility board:") != std::string::npos);
  ASSERT_TRUE(result.output.find("Adobe Safe Orbit Camera") != std::string::npos);
  ASSERT_TRUE(result.output.find("fixture_valid_pass=true") != std::string::npos);
}


}  // namespace

int main() {

  std::cout << "=== Registry CLI Tests ===" << std::endl;
  std::cout << std::endl;

  RUN_TEST(test_registry_json_lists_builtin_extension);
  RUN_TEST(test_registry_list_json_alias_works);
  RUN_TEST(test_registry_show_json_returns_single_entry);
  RUN_TEST(test_registry_show_text_includes_full_contract);
  RUN_TEST(test_registry_show_invalid_type_json_reports_original_input);
  RUN_TEST(test_registry_show_missing_type_json_reports_original_input);
  RUN_TEST(test_compat_board_json_exposes_maturity_fields);
  RUN_TEST(test_compat_board_text_mentions_registry_role);

  std::cout << std::endl;

  std::cout << "=== Test Summary ===" << std::endl;
  std::cout << "Tests run: " << g_tests_run << std::endl;
  std::cout << "Tests passed: " << g_tests_passed << std::endl;
  std::cout << "Tests failed: " << (g_tests_run - g_tests_passed) << std::endl;

  if (g_tests_passed == g_tests_run) {
    std::cout << "Result: ALL " << g_tests_passed << " REGISTRY CLI TESTS PASSED ✓" << std::endl;
    return EXIT_SUCCESS;
  }

  std::cout << "Result: " << (g_tests_run - g_tests_passed) << " REGISTRY CLI TESTS FAILED ✗" << std::endl;
  return EXIT_FAILURE;
}

