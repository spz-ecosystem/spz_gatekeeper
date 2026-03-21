/**
 * SPZ Extension Spec Registry Tests
 */

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "spz_gatekeeper/extension_spec_registry.h"

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

#define ASSERT_FALSE(cond) do { \
  if (cond) { \
    throw std::runtime_error("Assertion failed: NOT " #cond); \
  } \
} while (0)

#define ASSERT_EQ(a, b) do { \
  if ((a) != (b)) { \
    throw std::runtime_error("Assertion failed: " #a " == " #b); \
  } \
} while (0)

spz_gatekeeper::ExtensionSpec MakeSpec(uint32_t type,
                                       std::string vendor_name,
                                       std::string extension_name) {
  spz_gatekeeper::ExtensionSpec spec;
  spec.type = type;
  spec.vendor_id = static_cast<uint16_t>(type >> 16);
  spec.extension_id = static_cast<uint16_t>(type & 0xFFFFu);
  spec.vendor_name = std::move(vendor_name);
  spec.extension_name = std::move(extension_name);
  spec.category = "algorithm";
  spec.status = "draft";
  spec.spec_url = "https://example.invalid/spec";
  spec.short_description = "test spec";
  spec.min_spz_version = 1;
  spec.requires_has_extensions_flag = true;
  return spec;
}

TEST(test_register_and_lookup_spec) {
  auto& reg = spz_gatekeeper::ExtensionSpecRegistry::Instance();
  const auto base_count = reg.SpecCount();

  const auto spec = MakeSpec(0xADBE1001u, "Adobe", "Safe Orbit Camera Draft");
  reg.RegisterSpec(spec);

  const auto got = reg.GetSpec(spec.type);
  ASSERT_TRUE(got.has_value());
  ASSERT_EQ(got->type, spec.type);
  ASSERT_EQ(got->vendor_name, "Adobe");
  ASSERT_EQ(got->extension_name, "Safe Orbit Camera Draft");
  ASSERT_EQ(reg.SpecCount(), base_count + 1u);
}

TEST(test_has_spec_and_list_specs) {
  auto& reg = spz_gatekeeper::ExtensionSpecRegistry::Instance();
  const auto spec = MakeSpec(0x4E410001u, "Niantic", "Spatial Metadata");
  reg.RegisterSpec(spec);

  ASSERT_TRUE(reg.HasSpec(spec.type));
  const auto listed = reg.ListSpecs();

  bool found = false;
  for (const auto& item : listed) {
    if (item.type == spec.type) {
      found = true;
      ASSERT_EQ(item.vendor_name, "Niantic");
      ASSERT_EQ(item.extension_name, "Spatial Metadata");
    }
  }
  ASSERT_TRUE(found);
}

TEST(test_register_spec_overwrites_existing_entry) {
  auto& reg = spz_gatekeeper::ExtensionSpecRegistry::Instance();
  const auto type = 0xBEEF0001u;
  const auto base_count = reg.SpecCount();

  auto first = MakeSpec(type, "Vendor A", "First Name");
  first.status = "planned";
  reg.RegisterSpec(first);

  auto second = MakeSpec(type, "Vendor B", "Second Name");
  second.status = "stable";
  reg.RegisterSpec(second);

  const auto got = reg.GetSpec(type);
  ASSERT_TRUE(got.has_value());
  ASSERT_EQ(got->vendor_name, "Vendor B");
  ASSERT_EQ(got->extension_name, "Second Name");
  ASSERT_EQ(got->status, "stable");
  ASSERT_EQ(reg.SpecCount(), base_count + 1u);
}

TEST(test_missing_spec_returns_empty_optional) {
  auto& reg = spz_gatekeeper::ExtensionSpecRegistry::Instance();
  ASSERT_FALSE(reg.HasSpec(0xFFFFFFFFu));
  ASSERT_FALSE(reg.GetSpec(0xFFFFFFFFu).has_value());
}

}  // namespace

int main() {
  std::cout << "=== SPZ Extension Spec Registry Tests ===" << std::endl;
  std::cout << std::endl;

  RUN_TEST(test_register_and_lookup_spec);
  RUN_TEST(test_has_spec_and_list_specs);
  RUN_TEST(test_register_spec_overwrites_existing_entry);
  RUN_TEST(test_missing_spec_returns_empty_optional);

  std::cout << std::endl;
  std::cout << "=== Test Summary ===" << std::endl;
  std::cout << "Tests run: " << g_tests_run << std::endl;
  std::cout << "Tests passed: " << g_tests_passed << std::endl;
  std::cout << "Tests failed: " << (g_tests_run - g_tests_passed) << std::endl;

  if (g_tests_passed == g_tests_run) {
    std::cout << "Result: ALL " << g_tests_passed << " REGISTRY TESTS PASSED ✓" << std::endl;
    return EXIT_SUCCESS;
  }

  std::cout << "Result: " << (g_tests_run - g_tests_passed) << " REGISTRY TESTS FAILED ✗" << std::endl;
  return EXIT_FAILURE;
}
