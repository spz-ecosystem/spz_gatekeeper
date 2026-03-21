#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
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

TEST(test_gen_fixture_valid_adobe_produces_checkable_spz) {
  TempPathGuard output_path(MakeTempPath("gen_fixture_valid_adobe"));
  const std::string path = output_path.path().string();

  const auto generate = RunCommand(std::string(CliBinaryPath()) + " gen-fixture --type 0xADBE0002 --mode valid --out \"" + path + "\"");
  ASSERT_TRUE(generate.exit_code == 0);
  ASSERT_TRUE(std::filesystem::exists(output_path.path()));
  ASSERT_TRUE(std::filesystem::file_size(output_path.path()) > 0);

  const auto check = RunCommand(std::string(CliBinaryPath()) + " check-spz \"" + path + "\" --json");
  ASSERT_TRUE(check.exit_code == 0);
  ASSERT_TRUE(check.output.find("\"type\":2914910210") != std::string::npos);
  ASSERT_TRUE(check.output.find("\"known_extension\":true") != std::string::npos);
  ASSERT_TRUE(check.output.find("\"validation_result\":true") != std::string::npos);
}

TEST(test_gen_fixture_invalid_size_triggers_validator_error) {
  TempPathGuard output_path(MakeTempPath("gen_fixture_invalid_size"));
  const std::string path = output_path.path().string();

  const auto generate = RunCommand(std::string(CliBinaryPath()) + " gen-fixture --type 0xADBE0002 --mode invalid-size --out \"" + path + "\"");
  ASSERT_TRUE(generate.exit_code == 0);
  ASSERT_TRUE(std::filesystem::exists(output_path.path()));

  const auto check = RunCommand(std::string(CliBinaryPath()) + " check-spz \"" + path + "\" --json");
  ASSERT_TRUE(check.exit_code == 1);
  ASSERT_TRUE(check.output.find("\"validation_result\":false") != std::string::npos);
  ASSERT_TRUE(check.output.find("Invalid payload size") != std::string::npos);
}

TEST(test_gen_fixture_unknown_type_marks_placeholder_payload) {
  TempPathGuard output_path(MakeTempPath("gen_fixture_unknown_type"));
  const std::string path = output_path.path().string();

  const auto generate = RunCommand(std::string(CliBinaryPath()) + " gen-fixture --type 0xCAFE0001 --out \"" + path + "\"");
  ASSERT_TRUE(generate.exit_code == 0);
  ASSERT_TRUE(generate.output.find("placeholder") != std::string::npos);
  ASSERT_TRUE(std::filesystem::exists(output_path.path()));

  const auto check = RunCommand(std::string(CliBinaryPath()) + " check-spz \"" + path + "\" --no-strict --json");
  ASSERT_TRUE(check.exit_code == 0);
  ASSERT_TRUE(check.output.find("\"type\":3405643777") != std::string::npos);
  ASSERT_TRUE(check.output.find("\"known_extension\":false") != std::string::npos);
}


}  // namespace

int main() {
  std::cout << "=== gen-fixture CLI Tests ===" << std::endl;
  std::cout << std::endl;

  RUN_TEST(test_gen_fixture_valid_adobe_produces_checkable_spz);
  RUN_TEST(test_gen_fixture_invalid_size_triggers_validator_error);
  RUN_TEST(test_gen_fixture_unknown_type_marks_placeholder_payload);

  std::cout << std::endl;
  std::cout << "=== Test Summary ===" << std::endl;
  std::cout << "Tests run: " << g_tests_run << std::endl;
  std::cout << "Tests passed: " << g_tests_passed << std::endl;
  std::cout << "Tests failed: " << (g_tests_run - g_tests_passed) << std::endl;

  if (g_tests_passed == g_tests_run) {
    std::cout << "Result: ALL " << g_tests_passed << " GEN-FIXTURE TESTS PASSED ✓" << std::endl;
    return EXIT_SUCCESS;
  }

  std::cout << "Result: " << (g_tests_run - g_tests_passed) << " GEN-FIXTURE TESTS FAILED ✗" << std::endl;
  return EXIT_FAILURE;
}
