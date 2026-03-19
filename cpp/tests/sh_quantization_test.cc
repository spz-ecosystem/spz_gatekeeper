#include <cmath>
#include <cstdlib>
#include <iostream>

#include "spz_gatekeeper/spz.h"

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
} while(0)

#define ASSERT_TRUE(cond) do { \
  if (!(cond)) { \
    throw std::runtime_error("Assertion failed: " #cond); \
  } \
} while(0)

#define ASSERT_NEAR(a, b, tol) do { \
  if (std::fabs((a) - (b)) > (tol)) { \
    throw std::runtime_error("Assertion failed: " #a " ~= " #b); \
  } \
} while(0)

TEST(test_epsilon_8bit) {
  const double e = spz_gatekeeper::sh_epsilon(8);
  ASSERT_NEAR(e, 2.0 / 255.0, 1e-12);
  ASSERT_NEAR(e, 0.0078431372549, 1e-10);
}

TEST(test_epsilon_5bit) {
  const double e = spz_gatekeeper::sh_epsilon(5);
  ASSERT_NEAR(e, 2.0 / 31.0, 1e-12);
  ASSERT_NEAR(e, 0.0645161290322, 1e-10);
}

TEST(test_epsilon_1bit) {
  const double e = spz_gatekeeper::sh_epsilon(1);
  ASSERT_NEAR(e, 2.0, 1e-12);
}

TEST(test_epsilon_invalid_bits) {
  const double e0 = spz_gatekeeper::sh_epsilon(0);
  const double en = spz_gatekeeper::sh_epsilon(-3);
  ASSERT_TRUE(std::isinf(e0));
  ASSERT_TRUE(std::isinf(en));
}

}  // namespace

int main() {
  std::cout << "=== SH Quantization Tests ===" << std::endl;
  std::cout << std::endl;

  RUN_TEST(test_epsilon_8bit);
  RUN_TEST(test_epsilon_5bit);
  RUN_TEST(test_epsilon_1bit);
  RUN_TEST(test_epsilon_invalid_bits);

  std::cout << std::endl;
  std::cout << "=== Test Summary ===" << std::endl;
  std::cout << "Tests run: " << g_tests_run << std::endl;
  std::cout << "Tests passed: " << g_tests_passed << std::endl;
  std::cout << "Tests failed: " << (g_tests_run - g_tests_passed) << std::endl;

  if (g_tests_passed == g_tests_run) {
    std::cout << "Result: ALL TESTS PASSED ✓" << std::endl;
    return EXIT_SUCCESS;
  }
  std::cout << "Result: SOME TESTS FAILED ✗" << std::endl;
  return EXIT_FAILURE;
}
