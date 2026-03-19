#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <vector>

#include "spz_gatekeeper/extension_validator.h"
#include "spz_gatekeeper/tlv.h"
#include "spz_gatekeeper/validator_registry.h"

namespace {

class BenchValidator : public spz_gatekeeper::SpzExtensionValidator {
 public:
  explicit BenchValidator(uint32_t type) : type_(type) {}
  std::string GetName() const override { return "Bench"; }
  uint32_t GetExtensionType() const override { return type_; }
  bool Validate(const uint8_t*, size_t, std::string*) const override { return true; }
 private:
  uint32_t type_;
};

void write_u32_le(uint32_t value, std::vector<uint8_t>* out) {
  out->push_back(value & 0xFF);
  out->push_back((value >> 8) & 0xFF);
  out->push_back((value >> 16) & 0xFF);
  out->push_back((value >> 24) & 0xFF);
}

std::vector<uint8_t> make_tlv_100_records() {
  std::vector<uint8_t> data;
  data.reserve(100 * (8 + 4));
  for (uint32_t i = 0; i < 100; ++i) {
    write_u32_le(0x10000000u + i, &data);
    write_u32_le(4, &data);
    data.push_back(static_cast<uint8_t>(i & 0xFF));
    data.push_back(0xAA);
    data.push_back(0xBB);
    data.push_back(0xCC);
  }
  return data;
}

}  // namespace

int main() {
  using clock = std::chrono::high_resolution_clock;

  // Benchmark 1: TLV parse 100 records
  const auto tlv_data = make_tlv_100_records();
  const int parse_iters = 1000;
  auto t0 = clock::now();
  for (int i = 0; i < parse_iters; ++i) {
    auto r = spz_gatekeeper::ParseTlvTrailer(tlv_data, 0);
    if (!r.ok || r.records.size() != 100u) {
      std::cerr << "TLV parse failed" << std::endl;
      return EXIT_FAILURE;
    }
  }
  auto t1 = clock::now();
  const double parse_ms = std::chrono::duration<double, std::milli>(t1 - t0).count() / parse_iters;

  // Benchmark 2: registry lookup latency
  auto& reg = spz_gatekeeper::ExtensionValidatorRegistry::Instance();
  const int n = 2048;
  for (int i = 0; i < n; ++i) {
    const uint32_t type = 0x70000000u + static_cast<uint32_t>(i);
    reg.RegisterValidator(type, std::make_shared<BenchValidator>(type));
  }

  const int lookup_iters = 200000;
  auto q0 = clock::now();
  for (int i = 0; i < lookup_iters; ++i) {
    const uint32_t type = 0x70000000u + static_cast<uint32_t>(i % n);
    auto v = reg.GetValidator(type);
    if (!v) {
      std::cerr << "lookup failed" << std::endl;
      return EXIT_FAILURE;
    }
  }
  auto q1 = clock::now();
  const double lookup_us = std::chrono::duration<double, std::micro>(q1 - q0).count() / lookup_iters;

  std::cout << "BENCH_TLV_PARSE_100_EXT_MS=" << parse_ms << std::endl;
  std::cout << "BENCH_REGISTRY_LOOKUP_US=" << lookup_us << std::endl;

  // Gate targets from checklist.
  if (parse_ms >= 10.0) {
    std::cerr << "parse benchmark exceeds 10ms target" << std::endl;
    return EXIT_FAILURE;
  }
  if (lookup_us >= 1.0) {
    std::cerr << "lookup benchmark exceeds 1us target" << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
