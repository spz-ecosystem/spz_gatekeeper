/**
 * SPZ Extension Validator - Unit Tests
 *
 * Tests for the extension validator base class and framework.
 * Validates proper behavior of SpzExtensionValidator interface.
 *
 * Test Coverage:
 * - Virtual destructor behavior (memory leak prevention)
 * - Pure virtual function interface
 * - Derived class implementation
 * - Error handling
 *
 * @author PuJunhan
 * @copyright Copyright (c) 2026 PuJunhan
 * @license MIT
 */

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "spz_gatekeeper/extension_validator.h"

namespace {

/// Test counter
int g_tests_run = 0;
int g_tests_passed = 0;

/// Test result reporting
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
class MockValidator : public spz_gatekeeper::SpzExtensionValidator {
public:
  std::string GetName() const override {
    return "Mock Extension";
  }
  
  uint32_t GetExtensionType() const override {
    return 0x12340001u;
  }
  
  bool Validate(const uint8_t* data, size_t size, std::string* error) const override {
    // Simple validation: size must be 4 bytes
    if (size != 4) {
      if (error) {
        *error = "Invalid size: expected 4, got " + std::to_string(size);
      }
      return false;
    }
    
    // Check value must be 0x12345678
    uint32_t value = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
    if (value != 0x12345678u) {
      if (error) {
        *error = "Invalid value: expected 0x12345678, got 0x" + 
                 to_hex(value);
      }
      return false;
    }
    
    return true;
  }
  
private:
  static std::string to_hex(uint32_t value) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%08X", value);
    return std::string(buf);
  }
};

// Test virtual destructor
TEST(test_virtual_destructor) {
  bool derived_destroyed = false;
  
  class DerivedValidator : public spz_gatekeeper::SpzExtensionValidator {
  public:
    DerivedValidator(bool* flag) : flag_(flag) {}
    
    ~DerivedValidator() override {
      *flag_ = true;
    }
    
    std::string GetName() const override { return "Test"; }
    uint32_t GetExtensionType() const override { return 0x1; }
    bool Validate(const uint8_t*, size_t, std::string*) const override { return true; }
    
  private:
    bool* flag_;
  };
  
  // Delete through base class pointer
  spz_gatekeeper::SpzExtensionValidator* ptr = new DerivedValidator(&derived_destroyed);
  delete ptr;  // Should call derived destructor
  
  ASSERT_TRUE(derived_destroyed);
}

// Test GetName
TEST(test_get_name) {
  MockValidator validator;
  ASSERT_EQ(validator.GetName(), "Mock Extension");
}

// Test GetExtensionType
TEST(test_get_extension_type) {
  MockValidator validator;
  ASSERT_EQ(validator.GetExtensionType(), 0x12340001u);
}

// Test Validate - valid case
TEST(test_validate_valid) {
  MockValidator validator;
  std::uint8_t valid_data[] = {0x78, 0x56, 0x34, 0x12};  // 0x12345678 little-endian
  std::string error;
  
  bool result = validator.Validate(valid_data, 4, &error);
  ASSERT_TRUE(result);
  ASSERT_TRUE(error.empty());
}

// Test Validate - invalid size
TEST(test_validate_invalid_size) {
  MockValidator validator;
  std::uint8_t data[] = {0x12, 0x34, 0x56};  // 3 bytes, not 4
  std::string error;
  
  bool result = validator.Validate(data, 3, &error);
  ASSERT_FALSE(result);
  ASSERT_FALSE(error.empty());
  ASSERT_TRUE(error.find("Invalid size") != std::string::npos);
}

// Test Validate - invalid value
TEST(test_validate_invalid_value) {
  MockValidator validator;
  std::uint8_t data[] = {0x00, 0x00, 0x00, 0x00};  // Wrong value
  std::string error;
  
  bool result = validator.Validate(data, 4, &error);
  ASSERT_FALSE(result);
  ASSERT_FALSE(error.empty());
  ASSERT_TRUE(error.find("Invalid value") != std::string::npos);
}

// Test Validate - null error pointer
TEST(test_validate_null_error) {
  MockValidator validator;
  std::uint8_t data[] = {0x78, 0x56, 0x34, 0x12};
  
  // Should not crash when error is nullptr
  bool result = validator.Validate(data, 4, nullptr);
  ASSERT_TRUE(result);
}

// Test Validate - empty data
TEST(test_validate_empty_data) {
  MockValidator validator;
  std::string error;
  
  bool result = validator.Validate(nullptr, 0, &error);
  ASSERT_FALSE(result);
  ASSERT_FALSE(error.empty());
}

// Test polymorphic behavior
TEST(test_polymorphism) {
  std::unique_ptr<spz_gatekeeper::SpzExtensionValidator> ptr = 
    std::make_unique<MockValidator>();
  
  ASSERT_EQ(ptr->GetName(), "Mock Extension");
  ASSERT_EQ(ptr->GetExtensionType(), 0x12340001u);
  
  std::uint8_t data[] = {0x78, 0x56, 0x34, 0x12};
  std::string error;
  ASSERT_TRUE(ptr->Validate(data, 4, &error));
}

// Test multiple instances
TEST(test_multiple_instances) {
  MockValidator v1, v2, v3;
  
  // All instances should work independently
  std::uint8_t data[] = {0x78, 0x56, 0x34, 0x12};
  
  std::string e1, e2, e3;
  ASSERT_TRUE(v1.Validate(data, 4, &e1));
  ASSERT_TRUE(v2.Validate(data, 4, &e2));
  ASSERT_TRUE(v3.Validate(data, 4, &e3));
}

}  // namespace

int main() {
  std::cout << "=== Extension Validator Unit Tests ===" << std::endl;
  std::cout << std::endl;
  
  // Run all tests
  RUN_TEST(test_virtual_destructor);
  RUN_TEST(test_get_name);
  RUN_TEST(test_get_extension_type);
  RUN_TEST(test_validate_valid);
  RUN_TEST(test_validate_invalid_size);
  RUN_TEST(test_validate_invalid_value);
  RUN_TEST(test_validate_null_error);
  RUN_TEST(test_validate_empty_data);
  RUN_TEST(test_polymorphism);
  RUN_TEST(test_multiple_instances);
  
  // Summary
  std::cout << std::endl;
  std::cout << "=== Test Summary ===" << std::endl;
  std::cout << "Tests run: " << g_tests_run << std::endl;
  std::cout << "Tests passed: " << g_tests_passed << std::endl;
  std::cout << "Tests failed: " << (g_tests_run - g_tests_passed) << std::endl;
  
  if (g_tests_passed == g_tests_run) {
    std::cout << "Result: ALL TESTS PASSED ✓" << std::endl;
    return EXIT_SUCCESS;
  } else {
    std::cout << "Result: SOME TESTS FAILED ✗" << std::endl;
    return EXIT_FAILURE;
  }
}
