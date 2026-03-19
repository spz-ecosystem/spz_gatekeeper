/**
 * Extension Validator Registry - Unit Tests
 *
 * Comprehensive tests for ExtensionValidatorRegistry singleton.
 * Tests thread safety, registration, lookup, and edge cases.
 *
 * Test Coverage:
 * - Singleton pattern (single instance)
 * - RegisterValidator operation
 * - GetValidator operation
 * - HasValidator operation
 * - ListRegisteredValidators operation
 * - ValidatorCount operation
 * - Null validator rejection
 * - Duplicate registration (overwrite)
 * - Thread safety (basic)
 * - Auto-registration template
 *
 * @author PuJunhan
 * @copyright Copyright (c) 2026 PuJunhan
 * @license MIT
 */

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <algorithm>

#include "spz_gatekeeper/extension_validator.h"
#include "spz_gatekeeper/validator_registry.h"

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

#define ASSERT_FALSE(cond) do { \
  if (cond) { \
    throw std::runtime_error("Assertion failed: NOT " #cond); \
  } \
} while(0)

#define ASSERT_EQ(a, b) do { \
  if ((a) != (b)) { \
    throw std::runtime_error("Assertion failed: " #a " == " #b); \
  } \
} while(0)

#define ASSERT_NE(a, b) do { \
  if ((a) == (b)) { \
    throw std::runtime_error("Assertion failed: " #a " != " #b); \
  } \
} while(0)

// Mock validator for testing
class TestValidator : public spz_gatekeeper::SpzExtensionValidator {
public:
  TestValidator() : type_(0), name_("DefaultTest") {}
  TestValidator(uint32_t type, const std::string& name) 
    : type_(type), name_(name) {}
  
  std::string GetName() const override { return name_; }
  uint32_t GetExtensionType() const override { return type_; }
  bool Validate(const uint8_t*, size_t, std::string*) const override { return true; }
  
private:
  uint32_t type_;
  std::string name_;
};

TEST(test_singleton_single_instance) {
  auto& inst1 = spz_gatekeeper::ExtensionValidatorRegistry::Instance();
  auto& inst2 = spz_gatekeeper::ExtensionValidatorRegistry::Instance();
  
  // Should be same instance
  ASSERT_EQ(&inst1, &inst2);
}

TEST(test_register_validator) {
  auto& registry = spz_gatekeeper::ExtensionValidatorRegistry::Instance();
  auto validator = std::make_shared<TestValidator>(0x12340001u, "Test1");
  
  // Should not throw
  registry.RegisterValidator(0x12340001u, validator);
  
  // Should be findable
  auto found = registry.GetValidator(0x12340001u);
  ASSERT_NE(found, nullptr);
  ASSERT_EQ(found->GetName(), "Test1");
}

TEST(test_get_validator_not_found) {
  auto& registry = spz_gatekeeper::ExtensionValidatorRegistry::Instance();
  
  // Non-existent type should return nullptr
  auto found = registry.GetValidator(0xFFFFFFFFu);
  ASSERT_EQ(found, nullptr);
}

TEST(test_has_validator) {
  auto& registry = spz_gatekeeper::ExtensionValidatorRegistry::Instance();
  
  // Register
  auto validator = std::make_shared<TestValidator>(0x56780002u, "Test2");
  registry.RegisterValidator(0x56780002u, validator);
  
  // Should exist
  ASSERT_TRUE(registry.HasValidator(0x56780002u));
  
  // Non-existent should not exist
  ASSERT_FALSE(registry.HasValidator(0xDEADBEEFu));
}

TEST(test_list_registered_validators) {
  auto& registry = spz_gatekeeper::ExtensionValidatorRegistry::Instance();
  
  // Clear by getting count before
  size_t count_before = registry.ValidatorCount();
  
  // Register multiple
  registry.RegisterValidator(0x00010001u, std::make_shared<TestValidator>(0x00010001u, "A"));
  registry.RegisterValidator(0x00020002u, std::make_shared<TestValidator>(0x00020002u, "B"));
  registry.RegisterValidator(0x00030003u, std::make_shared<TestValidator>(0x00030003u, "C"));
  
  auto types = registry.ListRegisteredValidators();
  
  // Should have at least 3 (may have more from other tests)
  ASSERT_TRUE(types.size() >= count_before + 3);
  
  // Check our types are there
  ASSERT_TRUE(std::find(types.begin(), types.end(), 0x00010001u) != types.end());
  ASSERT_TRUE(std::find(types.begin(), types.end(), 0x00020002u) != types.end());
  ASSERT_TRUE(std::find(types.begin(), types.end(), 0x00030003u) != types.end());
}

TEST(test_validator_count) {
  auto& registry = spz_gatekeeper::ExtensionValidatorRegistry::Instance();
  size_t count_before = registry.ValidatorCount();
  
  // Register one
  registry.RegisterValidator(0x99990001u, std::make_shared<TestValidator>(0x99990001u, "CountTest"));
  
  ASSERT_EQ(registry.ValidatorCount(), count_before + 1);
}

TEST(test_register_null_validator) {
  auto& registry = spz_gatekeeper::ExtensionValidatorRegistry::Instance();
  
  // Should throw exception
  try {
    registry.RegisterValidator(0x00000001u, nullptr);
    throw std::runtime_error("Should have thrown exception");
  } catch (const std::invalid_argument&) {
    // Expected
  }
}

TEST(test_duplicate_registration_overwrite) {
  auto& registry = spz_gatekeeper::ExtensionValidatorRegistry::Instance();
  
  // Register first validator
  auto v1 = std::make_shared<TestValidator>(0xAAAA0001u, "First");
  registry.RegisterValidator(0xAAAA0001u, v1);
  
  // Verify first
  auto found1 = registry.GetValidator(0xAAAA0001u);
  ASSERT_EQ(found1->GetName(), "First");
  
  // Register second with same type (should overwrite)
  auto v2 = std::make_shared<TestValidator>(0xAAAA0001u, "Second");
  registry.RegisterValidator(0xAAAA0001u, v2);
  
  // Verify second
  auto found2 = registry.GetValidator(0xAAAA0001u);
  ASSERT_EQ(found2->GetName(), "Second");
}

TEST(test_multiple_registrations_same_type) {
  auto& registry = spz_gatekeeper::ExtensionValidatorRegistry::Instance();
  
  // Register multiple times
  registry.RegisterValidator(0xBBBB0001u, std::make_shared<TestValidator>(0xBBBB0001u, "V1"));
  registry.RegisterValidator(0xBBBB0001u, std::make_shared<TestValidator>(0xBBBB0001u, "V2"));
  registry.RegisterValidator(0xBBBB0001u, std::make_shared<TestValidator>(0xBBBB0001u, "V3"));
  
  // Last one should win
  auto found = registry.GetValidator(0xBBBB0001u);
  ASSERT_EQ(found->GetName(), "V3");
}

TEST(test_auto_registration_template) {
  // Use the RegisterValidator template for auto-registration
  static spz_gatekeeper::RegisterValidator<TestValidator> g_auto_reg;
  
  // Get the auto-registered validator
  auto& registry = spz_gatekeeper::ExtensionValidatorRegistry::Instance();
  auto found = registry.GetValidator(g_auto_reg.GetExtensionType());
  
  // Should be registered
  ASSERT_NE(found, nullptr);
}

TEST(test_thread_safety_basic) {
  auto& registry = spz_gatekeeper::ExtensionValidatorRegistry::Instance();
  
  // Register from multiple threads
  std::vector<std::thread> threads;
  const int num_threads = 10;
  
  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([&registry, i]() {
      uint32_t type = 0xCCCC0000u + i;
      auto validator = std::make_shared<TestValidator>(type, "Thread" + std::to_string(i));
      registry.RegisterValidator(type, validator);
    });
  }
  
  // Wait for all threads
  for (auto& t : threads) {
    t.join();
  }
  
  // Verify all registered
  for (int i = 0; i < num_threads; ++i) {
    uint32_t type = 0xCCCC0000u + i;
    auto found = registry.GetValidator(type);
    ASSERT_NE(found, nullptr);
  }
}

TEST(test_concurrent_read_write) {
  auto& registry = spz_gatekeeper::ExtensionValidatorRegistry::Instance();
  
  // Pre-register some validators
  for (int i = 0; i < 5; ++i) {
    uint32_t type = 0xDDDD0000u + i;
    registry.RegisterValidator(type, std::make_shared<TestValidator>(type, "Pre" + std::to_string(i)));
  }
  
  // Concurrent reads and writes
  std::vector<std::thread> threads;
  
  // Writer threads
  for (int i = 0; i < 5; ++i) {
    threads.emplace_back([&registry, i]() {
      uint32_t type = 0xEEEE0000u + i;
      auto validator = std::make_shared<TestValidator>(type, "Writer" + std::to_string(i));
      registry.RegisterValidator(type, validator);
    });
  }
  
  // Reader threads
  for (int i = 0; i < 5; ++i) {
    threads.emplace_back([&registry, i]() {
      uint32_t type = 0xDDDD0000u + i;
      auto found = registry.GetValidator(type);
      // Should not crash, may be null or valid
    });
  }
  
  // Wait for all
  for (auto& t : threads) {
    t.join();
  }
  
  // No crashes = test passed
}

}  // namespace

int main() {
  std::cout << "=== Extension Validator Registry Tests ===" << std::endl;
  std::cout << std::endl;
  
  RUN_TEST(test_singleton_single_instance);
  RUN_TEST(test_register_validator);
  RUN_TEST(test_get_validator_not_found);
  RUN_TEST(test_has_validator);
  RUN_TEST(test_list_registered_validators);
  RUN_TEST(test_validator_count);
  RUN_TEST(test_register_null_validator);
  RUN_TEST(test_duplicate_registration_overwrite);
  RUN_TEST(test_multiple_registrations_same_type);
  RUN_TEST(test_auto_registration_template);
  RUN_TEST(test_thread_safety_basic);
  RUN_TEST(test_concurrent_read_write);
  
  std::cout << std::endl;
  std::cout << "=== Test Summary ===" << std::endl;
  std::cout << "Tests run: " << g_tests_run << std::endl;
  std::cout << "Tests passed: " << g_tests_passed << std::endl;
  std::cout << "Tests failed: " << (g_tests_run - g_tests_passed) << std::endl;
  
  if (g_tests_passed == g_tests_run) {
    std::cout << "Result: ALL " << g_tests_passed << " TESTS PASSED ✓" << std::endl;
    return EXIT_SUCCESS;
  } else {
    std::cout << "Result: " << (g_tests_run - g_tests_passed) << " TESTS FAILED ✗" << std::endl;
    return EXIT_FAILURE;
  }
}
