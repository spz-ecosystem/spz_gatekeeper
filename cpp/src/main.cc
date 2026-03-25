/**
 * SPZ Gatekeeper - SPZ Format Legality Checker
 *
 * Validates SPZ derivative algorithms for 100% backward compatibility
 * with the original SPZ packed format and vendor-extension mechanism.
 *
 * Features:
 * - L2 Validation: SPZ file format layer (header, flags, TLV trailer)
 * - Official has-extensions flag (0x02) validation
 * - TLV (Type-Length-Value) trailer format verification
 * - Development guide for vendor extensions
 *
 * Usage:
 *   spz_gatekeeper check-spz <file.spz> [--strict|--no-strict] [--json]
 *   spz_gatekeeper dump-trailer <file.spz> [--strict|--no-strict] [--json]
 *   spz_gatekeeper registry [--json]
 *   spz_gatekeeper registry show <type> [--json]
 *   spz_gatekeeper compat-check <file.spz> [--json]
 *   spz_gatekeeper compat-board [--json]
 *   spz_gatekeeper gen-fixture --type <u32> [--mode valid|invalid-size] --out <file.spz>
 *   spz_gatekeeper guide [--json]

 *   spz_gatekeeper --self-test


 *
 * @author PuJunhan
 * @copyright Copyright (c) 2026 PuJunhan
 * @license MIT
 */

#include "spz_gatekeeper/audit_summary.h"
#include "spz_gatekeeper/extension_spec_registry.h"
#include "spz_gatekeeper/json_min.h"
#include "spz_gatekeeper/spz.h"
#include "spz_gatekeeper/validator_registry.h"



#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <set>

#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include <zlib.h>

#if defined(__linux__) || defined(__APPLE__)
#include <sys/resource.h>
#endif




namespace {

static void PrintUsage();

static bool ReadAllBytes(const std::string& path, std::vector<std::uint8_t>* out) {

  std::ifstream in(path, std::ios::binary | std::ios::ate);
  if (!in.good()) return false;
  std::size_t size = static_cast<std::size_t>(in.tellg());
  in.seekg(0, std::ios::beg);
  out->resize(size);
  in.read(reinterpret_cast<char*>(out->data()), static_cast<std::streamsize>(size));
  return in.good();
}

static bool ReadAllText(const std::string& path, std::string* out) {
  std::ifstream in(path);
  if (!in.good()) return false;
  *out = std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  return true;
}

static std::optional<double> ReadPeakMemoryMb() {
#if defined(__linux__) || defined(__APPLE__)
  rusage usage{};
  if (getrusage(RUSAGE_SELF, &usage) != 0) {
    return std::nullopt;
  }
#if defined(__APPLE__)
  return static_cast<double>(usage.ru_maxrss) / (1024.0 * 1024.0);
#else
  return static_cast<double>(usage.ru_maxrss) / 1024.0;
#endif
#else
  return std::nullopt;
#endif
}

static bool GzipCompress(const std::vector<std::uint8_t>& in, std::vector<std::uint8_t>* out,

                         std::string* err) {
  out->clear();

  z_stream strm = {};
  strm.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(in.data()));
  strm.avail_in = static_cast<uInt>(in.size());

  if (deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 16 + MAX_WBITS, 9,
                   Z_DEFAULT_STRATEGY) != Z_OK) {
    if (err) *err = "deflateInit2 failed";
    return false;
  }

  std::vector<std::uint8_t> buf(8192);
  bool ok = false;
  while (true) {
    strm.next_out = reinterpret_cast<Bytef*>(buf.data());
    strm.avail_out = static_cast<uInt>(buf.size());
    int r = deflate(&strm, Z_FINISH);
    if (r != Z_OK && r != Z_STREAM_END) {
      if (err) *err = "deflate failed";
      break;
    }
    std::size_t produced = buf.size() - static_cast<std::size_t>(strm.avail_out);
    out->insert(out->end(), buf.begin(), buf.begin() + static_cast<std::ptrdiff_t>(produced));
    if (r == Z_STREAM_END) {
      ok = true;
      break;
    }
  }

  deflateEnd(&strm);
  return ok;
}

static bool HasIssueCode(const spz_gatekeeper::GateReport& rep, const char* code) {
  for (const auto& it : rep.issues) {
    if (it.code == code) return true;
  }
  return false;
}

static void WriteU32Le(std::vector<std::uint8_t>* bytes, std::uint32_t value) {

  bytes->push_back(static_cast<std::uint8_t>(value & 0xFFu));
  bytes->push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFu));
  bytes->push_back(static_cast<std::uint8_t>((value >> 16) & 0xFFu));
  bytes->push_back(static_cast<std::uint8_t>((value >> 24) & 0xFFu));
}

static bool WriteAllBytes(const std::string& path, const std::vector<std::uint8_t>& bytes) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out.good()) return false;
  out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  return out.good();
}

static std::vector<std::uint8_t> BuildTlvTrailer(
    const std::vector<std::pair<std::uint32_t, std::vector<std::uint8_t>>>& records) {
  std::vector<std::uint8_t> trailer;
  for (const auto& record : records) {
    WriteU32Le(&trailer, record.first);
    WriteU32Le(&trailer, static_cast<std::uint32_t>(record.second.size()));
    trailer.insert(trailer.end(), record.second.begin(), record.second.end());
  }
  return trailer;
}

static std::vector<std::uint8_t> WriteFloatBytes(float value) {
  std::vector<std::uint8_t> bytes(sizeof(float));
  std::memcpy(bytes.data(), &value, sizeof(float));
  return bytes;
}

static std::vector<std::uint8_t> BuildAdobeSafeOrbitPayload(bool invalid_size) {
  if (invalid_size) {
    return std::vector<std::uint8_t>(8, 0);
  }

  std::vector<std::uint8_t> payload;
  const auto min_elevation = WriteFloatBytes(-0.5f);
  const auto max_elevation = WriteFloatBytes(0.5f);
  const auto min_radius = WriteFloatBytes(1.0f);
  payload.insert(payload.end(), min_elevation.begin(), min_elevation.end());
  payload.insert(payload.end(), max_elevation.begin(), max_elevation.end());
  payload.insert(payload.end(), min_radius.begin(), min_radius.end());
  return payload;
}

static std::vector<std::uint8_t> BuildDecompressedSpz(std::uint32_t version, std::uint8_t flags,
                                                      const std::vector<std::uint8_t>* trailer) {
  std::vector<std::uint8_t> decomp;
  decomp.reserve(64);
  WriteU32Le(&decomp, 0x5053474eu);
  WriteU32Le(&decomp, version);
  WriteU32Le(&decomp, 1);
  decomp.push_back(0);
  decomp.push_back(12);
  decomp.push_back(flags);
  decomp.push_back(0);

  const bool uses_float16 = (version == 1);
  const bool uses_quat_smallest_three = (version >= 3);
  const std::size_t positions = uses_float16 ? 6 : 9;
  const std::size_t rotations = uses_quat_smallest_three ? 4 : 3;
  decomp.insert(decomp.end(), positions + 1 + 3 + 3 + rotations, 0);

  if (trailer != nullptr) {
    decomp.insert(decomp.end(), trailer->begin(), trailer->end());
  }

  return decomp;
}


static int SelfTest() {
  using namespace spz_gatekeeper;

  auto gzip = [](const std::vector<std::uint8_t>& decomp) -> std::optional<std::vector<std::uint8_t>> {
    std::vector<std::uint8_t> out;
    std::string err;
    if (!GzipCompress(decomp, &out, &err)) return std::nullopt;
    return out;
  };

  const auto sample_trailer = BuildTlvTrailer({{1u, {'a', 'b', 'c'}}});

  {
    auto gz = gzip(BuildDecompressedSpz(3, 0, nullptr));
    if (!gz.has_value()) return 10;
    SpzInspectOptions opt;
    GateReport rep = InspectSpzBlob(gz.value(), opt, "self-test:noext:notrailer");
    if (rep.HasErrors()) return 11;
    if (HasIssueCode(rep, "L2_UNDECLARED_TRAILER")) return 12;
  }

  {
    auto gz = gzip(BuildDecompressedSpz(3, 0, &sample_trailer));
    if (!gz.has_value()) return 20;
    SpzInspectOptions opt;
    GateReport rep = InspectSpzBlob(gz.value(), opt, "self-test:noext:trailer");
    if (rep.HasErrors()) return 21;
    if (!HasIssueCode(rep, "L2_UNDECLARED_TRAILER")) return 22;
  }

  {
    auto gz = gzip(BuildDecompressedSpz(3, kFlagHasExtensions, &sample_trailer));
    if (!gz.has_value()) return 30;
    SpzInspectOptions opt;
    GateReport rep = InspectSpzBlob(gz.value(), opt, "self-test:ext:trailer");
    if (rep.HasErrors()) return 31;
    if (!rep.spz_l2.has_value()) return 32;
    if (rep.spz_l2->tlv_records.size() != 1) return 33;
  }

  {
    auto gz = gzip(BuildDecompressedSpz(3, kFlagHasExtensions, nullptr));
    if (!gz.has_value()) return 40;
    SpzInspectOptions opt;
    GateReport rep = InspectSpzBlob(gz.value(), opt, "self-test:ext:notrailer");
    if (!rep.HasErrors()) return 41;
    if (!HasIssueCode(rep, "L2_EXT_DECLARED_NO_TRAILER")) return 42;
  }

  {
    auto gz = gzip(BuildDecompressedSpz(4, 0, nullptr));
    if (!gz.has_value()) return 50;
    SpzInspectOptions opt;
    GateReport rep = InspectSpzBlob(gz.value(), opt, "self-test:v4");
    if (rep.HasErrors()) return 51;
    if (HasIssueCode(rep, "L2_VERSION")) return 52;
  }


  return 0;
}

static void PrintGuide(bool json) {
  if (json) {
    std::cout << R"json({
  "title": "SPZ Vendor Extension Development Guide",
  "version": "1.0.0",
  "description": "See full guide at: https://github.com/spz-ecosystem/spz_gatekeeper/blob/main/docs/WIKI.md",

  "quick_start": {
    "goal": "Make your vendor extension pass spz_gatekeeper L2 validation",
    "steps": [
      "1. Set the official has-extensions flag: flags |= 0x02",
      "2. Append TLV records after the base payload",
      "3. Validate: spz_gatekeeper check-spz your.spz",
      "4. Test compatibility: spz_info your.spz && spz_to_ply your.spz"
    ]
  },
  "full_guide_url": "https://github.com/spz-ecosystem/spz_gatekeeper/blob/main/docs/WIKI.md",
  "resources": {
    "spz_official": "https://github.com/nianticlabs/spz",
    "spz2glb": "https://github.com/spz-ecosystem/spz2glb",
    "spz_gatekeeper": "https://github.com/spz-ecosystem/spz_gatekeeper",
    "public_guide": "docs/WIKI.md"
  }

})json" << std::endl;
    return;
  }

  std::cout << "SPZ Vendor Extension Development Guide\n";
  std::cout << "====================================\n\n";
  std::cout << "Full guide available at:\n";
  std::cout << "https://github.com/spz-ecosystem/spz_gatekeeper/blob/main/docs/WIKI.md\n\n";

  std::cout << "Quick Start:\n";
  std::cout << "  1. Set the official has-extensions flag: flags |= 0x02\n";
  std::cout << "  2. Append TLV records after the base payload\n";
  std::cout << "  3. Validate: spz_gatekeeper check-spz your.spz\n";
  std::cout << "  4. Test compatibility: spz_info your.spz && spz_to_ply your.spz\n";
  std::cout << "\nPublic docs:\n";
  std::cout << "  - docs/WIKI.md\n";
}

static std::string RegistryEntryToJson(const spz_gatekeeper::ExtensionSpec& spec) {
  std::ostringstream oss;
  oss << "{";
  oss << "\"type\":" << spec.type;
  oss << ",\"vendor_id\":" << spec.vendor_id;
  oss << ",\"extension_id\":" << spec.extension_id;
  oss << ",\"vendor_name\":\"" << spz_gatekeeper::JsonEscape(spec.vendor_name) << "\"";
  oss << ",\"extension_name\":\"" << spz_gatekeeper::JsonEscape(spec.extension_name) << "\"";
  oss << ",\"category\":\"" << spz_gatekeeper::JsonEscape(spec.category) << "\"";
  oss << ",\"status\":\"" << spz_gatekeeper::JsonEscape(spec.status) << "\"";
  oss << ",\"spec_url\":\"" << spz_gatekeeper::JsonEscape(spec.spec_url) << "\"";
  oss << ",\"short_description\":\"" << spz_gatekeeper::JsonEscape(spec.short_description) << "\"";
  oss << ",\"min_spz_version\":" << spec.min_spz_version;
  oss << ",\"requires_has_extensions_flag\":"
      << (spec.requires_has_extensions_flag ? "true" : "false");
  oss << ",\"has_validator\":"
      << (spz_gatekeeper::ExtensionValidatorRegistry::Instance().HasValidator(spec.type) ? "true" : "false");
  oss << "}";
  return oss.str();
}

static void PrintRegistryList(bool json) {
  const auto specs = spz_gatekeeper::ExtensionSpecRegistry::Instance().ListSpecs();
  if (json) {
    std::ostringstream oss;
    oss << "{";
    oss << "\"count\":" << specs.size();
    oss << ",\"extensions\":[";
    for (std::size_t i = 0; i < specs.size(); ++i) {
      if (i) {
        oss << ",";
      }
      oss << RegistryEntryToJson(specs[i]);
    }
    oss << "]}";
    std::cout << oss.str() << "\n";
    return;
  }

  std::cout << "Registered extensions: " << specs.size() << "\n";
  for (const auto& spec : specs) {
    std::cout << "- type=" << spec.type
              << " vendor=\"" << spec.vendor_name << "\""
              << " name=\"" << spec.extension_name << "\""
              << " status=\"" << spec.status << "\""
              << " category=\"" << spec.category << "\""
              << " has_validator="
              << (spz_gatekeeper::ExtensionValidatorRegistry::Instance().HasValidator(spec.type) ? "true" : "false")
              << "\n";
  }
}

static bool ParseExtensionType(const std::string& text, std::uint32_t* out) {
  try {
    std::size_t parsed = 0;
    const auto value = std::stoull(text, &parsed, 0);
    if (parsed != text.size() || value > 0xFFFFFFFFull) {
      return false;
    }
    *out = static_cast<std::uint32_t>(value);
    return true;
  } catch (...) {
    return false;
  }
}

static bool PrintRegistryShow(const std::string& type_text, bool json) {
  std::uint32_t type = 0;
  if (!ParseExtensionType(type_text, &type)) {
    if (json) {
      std::cout << "{\"error\":\"invalid extension type\",\"type_input\":\""
                << spz_gatekeeper::JsonEscape(type_text) << "\"}\n";
    } else {
      std::cerr << "invalid extension type: " << type_text << "\n";
    }
    return false;
  }

  const auto spec = spz_gatekeeper::ExtensionSpecRegistry::Instance().GetSpec(type);
  if (!spec.has_value()) {
    if (json) {
      std::cout << "{\"error\":\"extension not found\",\"type\":" << type
                << ",\"type_input\":\"" << spz_gatekeeper::JsonEscape(type_text) << "\"}\n";
    } else {
      std::cerr << "extension not found: " << type_text << "\n";
    }
    return false;
  }


  if (json) {
    std::cout << RegistryEntryToJson(*spec) << "\n";
  } else {
    std::cout << "type=" << spec->type << "\n";
    std::cout << "vendor_id=" << spec->vendor_id << "\n";
    std::cout << "extension_id=" << spec->extension_id << "\n";
    std::cout << "vendor_name=\"" << spec->vendor_name << "\"\n";
    std::cout << "extension_name=\"" << spec->extension_name << "\"\n";
    std::cout << "category=\"" << spec->category << "\"\n";
    std::cout << "status=\"" << spec->status << "\"\n";
    std::cout << "spec_url=\"" << spec->spec_url << "\"\n";
    std::cout << "short_description=\"" << spec->short_description << "\"\n";
    std::cout << "min_spz_version=" << spec->min_spz_version << "\n";
    std::cout << "requires_has_extensions_flag="
              << (spec->requires_has_extensions_flag ? "true" : "false") << "\n";
    std::cout << "has_validator="
              << (spz_gatekeeper::ExtensionValidatorRegistry::Instance().HasValidator(spec->type) ? "true" : "false")
              << "\n";
  }
  return true;
}

static int HandleRegistryCommand(int argc, char** argv) {
  bool json = false;
  bool show = false;
  std::string type_text;
  int flag_start = 2;

  if (argc >= 3) {
    const std::string arg = argv[2];
    if (arg == "list") {
      flag_start = 3;
    } else if (arg == "show") {
      if (argc < 4) {
        PrintUsage();
        return 2;
      }
      show = true;
      type_text = argv[3];
      flag_start = 4;
    } else if (arg == "--json") {
      json = true;
      flag_start = 3;
    } else {
      PrintUsage();
      return 2;
    }
  }

  for (int i = flag_start; i < argc; ++i) {
    const std::string flag = argv[i];
    if (flag == "--json") {
      json = true;
    } else {
      PrintUsage();
      return 2;
    }
  }

  if (show) {
    return PrintRegistryShow(type_text, json) ? 0 : 1;
  }

  PrintRegistryList(json);
  return 0;
}

struct FixtureBlob {
  std::vector<std::uint8_t> bytes;
  std::string mode;
  bool placeholder_payload = false;
};

static bool IsSupportedFixtureMode(const std::string& mode) {
  return mode == "valid" || mode == "invalid-size";
}

static FixtureBlob BuildFixtureBlob(std::uint32_t type, const std::string& mode) {
  FixtureBlob fixture;
  fixture.mode = mode;

  std::vector<std::uint8_t> payload;
  if (type == 0xADBE0002u) {
    payload = BuildAdobeSafeOrbitPayload(mode == "invalid-size");
  } else {
    fixture.placeholder_payload = true;
    payload = (mode == "invalid-size") ? std::vector<std::uint8_t>{0x00}
                                         : std::vector<std::uint8_t>{0x00, 0x00, 0x00, 0x00};
  }

  const auto trailer = BuildTlvTrailer({{type, payload}});
  const auto decompressed = BuildDecompressedSpz(3, spz_gatekeeper::kFlagHasExtensions, &trailer);
  std::string error;
  if (!GzipCompress(decompressed, &fixture.bytes, &error)) {
    throw std::runtime_error(error.empty() ? "gzip compression failed" : error);
  }
  return fixture;
}

static int HandleGenFixtureCommand(int argc, char** argv) {
  std::string type_text;
  std::string out_path;
  std::string mode = "valid";

  for (int i = 2; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--type" && i + 1 < argc) {
      type_text = argv[++i];
    } else if (arg == "--out" && i + 1 < argc) {
      out_path = argv[++i];
    } else if (arg == "--mode" && i + 1 < argc) {
      mode = argv[++i];
    } else {
      PrintUsage();
      return 2;
    }
  }

  std::uint32_t type = 0;
  if (type_text.empty() || out_path.empty() || !ParseExtensionType(type_text, &type) ||
      !IsSupportedFixtureMode(mode)) {
    PrintUsage();
    return 2;
  }

  try {
    const auto fixture = BuildFixtureBlob(type, mode);
    if (!WriteAllBytes(out_path, fixture.bytes)) {
      std::cerr << "failed to write fixture: " << out_path << "\n";
      return 2;
    }

    std::cout << "wrote fixture path=\"" << out_path << "\" type=" << type
              << " mode=\"" << fixture.mode << "\" placeholder="
              << (fixture.placeholder_payload ? "true" : "false") << "\n";
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << "gen-fixture failed: " << ex.what() << "\n";
    return 1;
  }
}



static const spz_gatekeeper::ExtensionReport* FindExtensionReport(

    const spz_gatekeeper::GateReport& report, std::uint32_t type) {
  for (const auto& ext : report.extension_reports) {
    if (ext.type == type) {
      return &ext;
    }
  }
  return nullptr;
}

static std::string BuildCompatibilityBoardJson() {
  const auto specs = spz_gatekeeper::ExtensionSpecRegistry::Instance().ListSpecs();
  std::ostringstream oss;
  oss << "{";
  oss << "\"count\":" << specs.size();
  oss << ",\"extensions\":[";

  for (std::size_t i = 0; i < specs.size(); ++i) {
    if (i) {
      oss << ",";
    }

    const auto& spec = specs[i];
    const auto valid_fixture = BuildFixtureBlob(spec.type, "valid");
    const auto invalid_fixture = BuildFixtureBlob(spec.type, "invalid-size");

    spz_gatekeeper::SpzInspectOptions strict_options;
    strict_options.strict = true;
    const auto strict_valid_report =
        spz_gatekeeper::InspectSpzBlob(valid_fixture.bytes, strict_options, "compat-board:valid");
    const auto strict_invalid_report =
        spz_gatekeeper::InspectSpzBlob(invalid_fixture.bytes, strict_options, "compat-board:invalid");

    spz_gatekeeper::SpzInspectOptions non_strict_options;
    non_strict_options.strict = false;
    const auto non_strict_valid_report =
        spz_gatekeeper::InspectSpzBlob(valid_fixture.bytes, non_strict_options, "compat-board:valid");

    const auto* valid_ext = FindExtensionReport(strict_valid_report, spec.type);
    const auto* invalid_ext = FindExtensionReport(strict_invalid_report, spec.type);
    const bool has_validator =
        spz_gatekeeper::ExtensionValidatorRegistry::Instance().HasValidator(spec.type);
    const bool fixture_valid_pass = valid_ext != nullptr && valid_ext->validation_result;
    const bool fixture_invalid_pass = invalid_ext != nullptr && !invalid_ext->validation_result;
    const bool strict_check_pass =
        !strict_valid_report.HasErrors() && !spz_gatekeeper::HasWarnings(strict_valid_report);
    const bool non_strict_check_pass = !non_strict_valid_report.HasErrors();
    const bool release_ready = has_validator && fixture_valid_pass && fixture_invalid_pass &&
                               strict_check_pass && non_strict_check_pass;
    const char* final_verdict = release_ready ? "pass" : "review_required";

    oss << "{";


    oss << "\"type\":" << spec.type;
    oss << ",\"vendor_name\":\"" << spz_gatekeeper::JsonEscape(spec.vendor_name) << "\"";
    oss << ",\"extension_name\":\"" << spz_gatekeeper::JsonEscape(spec.extension_name) << "\"";
    oss << ",\"status\":\"" << spz_gatekeeper::JsonEscape(spec.status) << "\"";
    oss << ",\"has_spec\":true";
    oss << ",\"has_validator\":" << (has_validator ? "true" : "false");
    oss << ",\"fixture_valid_pass\":" << (fixture_valid_pass ? "true" : "false");
    oss << ",\"fixture_invalid_pass\":" << (fixture_invalid_pass ? "true" : "false");
    oss << ",\"strict_check_pass\":" << (strict_check_pass ? "true" : "false");
    oss << ",\"non_strict_check_pass\":" << (non_strict_check_pass ? "true" : "false");
    oss << ",\"final_verdict\":\"" << final_verdict << "\"";
    oss << ",\"release_ready\":" << (release_ready ? "true" : "false");
    oss << ",\"wasm_quality_gate\":"
        << spz_gatekeeper::BuildWasmQualityGateJson(
               has_validator, !has_validator, false, release_ready);


    oss << "}";

  }

  oss << "]}";
  return oss.str();
}

static void PrintCompatibilityBoard(bool json) {
  const auto specs = spz_gatekeeper::ExtensionSpecRegistry::Instance().ListSpecs();
  if (json) {
    std::cout << BuildCompatibilityBoardJson() << "\n";
    return;
  }

  std::cout << "Compatibility board: " << specs.size() << " registered extension(s)\n";
  for (const auto& spec : specs) {
    const auto valid_fixture = BuildFixtureBlob(spec.type, "valid");
    const auto invalid_fixture = BuildFixtureBlob(spec.type, "invalid-size");

    spz_gatekeeper::SpzInspectOptions strict_options;
    strict_options.strict = true;
    const auto strict_valid_report =
        spz_gatekeeper::InspectSpzBlob(valid_fixture.bytes, strict_options, "compat-board:valid");
    const auto strict_invalid_report =
        spz_gatekeeper::InspectSpzBlob(invalid_fixture.bytes, strict_options, "compat-board:invalid");

    spz_gatekeeper::SpzInspectOptions non_strict_options;
    non_strict_options.strict = false;
    const auto non_strict_valid_report =
        spz_gatekeeper::InspectSpzBlob(valid_fixture.bytes, non_strict_options, "compat-board:valid");

    const auto* valid_ext = FindExtensionReport(strict_valid_report, spec.type);
    const auto* invalid_ext = FindExtensionReport(strict_invalid_report, spec.type);
    const bool has_validator =
        spz_gatekeeper::ExtensionValidatorRegistry::Instance().HasValidator(spec.type);
    const bool fixture_valid_pass = valid_ext != nullptr && valid_ext->validation_result;
    const bool fixture_invalid_pass = invalid_ext != nullptr && !invalid_ext->validation_result;
    const bool strict_check_pass =
        !strict_valid_report.HasErrors() && !spz_gatekeeper::HasWarnings(strict_valid_report);
    const bool non_strict_check_pass = !non_strict_valid_report.HasErrors();
    const bool release_ready = has_validator && fixture_valid_pass && fixture_invalid_pass &&
                               strict_check_pass && non_strict_check_pass;
    const char* final_verdict = release_ready ? "pass" : "review_required";

    std::cout << "- type=" << spec.type


              << " vendor=\"" << spec.vendor_name << "\""
              << " name=\"" << spec.extension_name << "\""
              << " status=\"" << spec.status << "\""
              << " has_spec=true"
              << " has_validator=" << (has_validator ? "true" : "false")
              << " fixture_valid_pass=" << (fixture_valid_pass ? "true" : "false")
              << " fixture_invalid_pass=" << (fixture_invalid_pass ? "true" : "false")
              << " strict_check_pass=" << (strict_check_pass ? "true" : "false")
              << " non_strict_check_pass=" << (non_strict_check_pass ? "true" : "false")
              << " final_verdict=" << final_verdict
              << " release_ready=" << (release_ready ? "true" : "false")
              << " wasm_validator_coverage_ok=" << (has_validator ? "true" : "false")
              << " wasm_empty_shell_risk=" << (!has_validator ? "true" : "false")
              << "\n";


  }
}


struct CompatAuditOutcome {
  std::string path;
  std::string json;
  std::string verdict;
  bool strict_ok = false;
  bool non_strict_ok = false;
  std::vector<std::string> issue_codes;
};



static bool IsSpzPath(const std::filesystem::path& path) {
  return path.has_extension() && path.extension() == ".spz";
}

static std::vector<std::string> ParseManifestSpzPaths(const std::string& manifest_path,
                                                      std::string* err) {
  std::ifstream in(manifest_path);
  if (!in.good()) {
    *err = "failed to read manifest";
    return {};
  }
  std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

  std::vector<std::string> out;
  std::set<std::string> seen;
  std::string current;
  bool in_string = false;
  bool escape = false;
  const auto base_dir = std::filesystem::path(manifest_path).parent_path();

  const auto append_if_spz = [&](const std::string& token) {
    if (token.size() < 4 || token.substr(token.size() - 4) != ".spz") {
      return;
    }
    std::filesystem::path p(token);
    if (p.is_relative()) {
      p = base_dir / p;
    }
    std::error_code ec;
    const auto normalized_path = std::filesystem::weakly_canonical(p, ec);
    const std::string normalized = (ec ? p : normalized_path).string();

    if (seen.insert(normalized).second) {
      out.push_back(normalized);
    }
  };

  for (char ch : text) {
    if (!in_string) {
      if (ch == '"') {
        in_string = true;
        current.clear();
        escape = false;
      }
      continue;
    }

    if (escape) {
      current.push_back(ch);
      escape = false;
      continue;
    }
    if (ch == '\\') {
      escape = true;
      continue;
    }
    if (ch == '"') {
      in_string = false;
      append_if_spz(current);
      continue;
    }
    current.push_back(ch);
  }

  if (out.empty()) {
    *err = "manifest does not contain any .spz path";
  }
  return out;
}

static bool RunCompatAuditForPath(const std::string& path, CompatAuditOutcome* out, std::string* err) {
  std::vector<std::uint8_t> bytes;
  if (!ReadAllBytes(path, &bytes)) {
    *err = "failed to read: " + path;
    return false;
  }

  const auto peak_memory_before_mb = ReadPeakMemoryMb();
  const auto started = std::chrono::steady_clock::now();
  spz_gatekeeper::SpzInspectOptions strict_options;

  strict_options.strict = true;
  const auto strict_report = spz_gatekeeper::InspectSpzBlob(bytes, strict_options, path);

  spz_gatekeeper::SpzInspectOptions non_strict_options;
  non_strict_options.strict = false;
  const auto non_strict_report = spz_gatekeeper::InspectSpzBlob(bytes, non_strict_options, path);
  const auto ended = std::chrono::steady_clock::now();
  const auto peak_memory_after_mb = ReadPeakMemoryMb();
  const double elapsed_ms = std::chrono::duration<double, std::milli>(ended - started).count();


  spz_gatekeeper::CompatAuditMetrics metrics;
  metrics.file_size_bytes = static_cast<std::uint64_t>(bytes.size());
  metrics.has_file_size_bytes = true;
  metrics.process_time_ms = elapsed_ms;
  metrics.has_process_time_ms = true;
  if (peak_memory_after_mb.has_value()) {
    metrics.peak_memory_mb = *peak_memory_after_mb;
    metrics.has_peak_memory_mb = true;
  }
  if (peak_memory_before_mb.has_value() && peak_memory_after_mb.has_value()) {
    metrics.memory_growth_count =
        *peak_memory_after_mb > *peak_memory_before_mb ? 1u : 0u;
    metrics.has_memory_growth_count = true;
  }

  bool strict_ok = false;

  bool non_strict_ok = false;
  const std::string verdict =
      spz_gatekeeper::ResolveCompatVerdict(strict_report, non_strict_report, &strict_ok, &non_strict_ok);


  out->path = path;
  out->json = spz_gatekeeper::BuildCompatCheckAuditJson(path, strict_report, non_strict_report, &metrics);
  out->verdict = verdict;
  out->strict_ok = strict_ok;
  out->non_strict_ok = non_strict_ok;
  out->issue_codes.clear();
  for (const auto& issue : strict_report.issues) {
    out->issue_codes.push_back(issue.code);
  }
  for (const auto& issue : non_strict_report.issues) {
    out->issue_codes.push_back(issue.code);
  }
  return true;
}

static void PrintCompatCheckSingle(const CompatAuditOutcome& outcome, bool json) {
  if (json) {
    std::cout << outcome.json << "\n";
    return;
  }
  std::cout << "asset=\"" << outcome.path << "\"\n";
  std::cout << "verdict=" << outcome.verdict << "\n";
  std::cout << "strict_ok=" << (outcome.strict_ok ? "true" : "false") << "\n";
  std::cout << "non_strict_ok=" << (outcome.non_strict_ok ? "true" : "false") << "\n";
}

static void PrintCompatCheckBatch(const std::vector<CompatAuditOutcome>& outcomes, bool json) {
  std::size_t pass_count = 0;
  std::size_t review_count = 0;
  std::size_t block_count = 0;
  std::map<std::string, std::size_t> issue_count_by_code;

  for (const auto& outcome : outcomes) {
    if (outcome.verdict == "pass") {
      ++pass_count;
    } else if (outcome.verdict == "review_required") {
      ++review_count;
    } else {
      ++block_count;
    }
    for (const auto& code : outcome.issue_codes) {
      issue_count_by_code[code] += 1;
    }
  }

  std::vector<std::pair<std::string, std::size_t>> top_issues(issue_count_by_code.begin(),
                                                              issue_count_by_code.end());
  std::sort(top_issues.begin(), top_issues.end(), [](const auto& lhs, const auto& rhs) {
    if (lhs.second != rhs.second) {
      return lhs.second > rhs.second;
    }
    return lhs.first < rhs.first;
  });

  if (json) {
    std::ostringstream oss;
    oss << "{";
    oss << "\"audit_profile\":\"" << spz_gatekeeper::kAuditProfileSpz << "\"";
    oss << ",\"audit_mode\":\"" << spz_gatekeeper::kAuditModeLocalCliSpzArtifactAudit << "\"";
    oss << ",\"summary\":{";
    oss << "\"total\":" << outcomes.size();
    oss << ",\"pass\":" << pass_count;
    oss << ",\"review_required\":" << review_count;
    oss << ",\"block\":" << block_count;
    oss << "}";
    oss << ",\"top_issues\":[";
    for (std::size_t i = 0; i < top_issues.size(); ++i) {
      if (i) {
        oss << ",";
      }
      oss << "{";
      oss << "\"code\":\"" << spz_gatekeeper::JsonEscape(top_issues[i].first) << "\"";
      oss << ",\"count\":" << top_issues[i].second;
      oss << "}";
    }
    oss << "]";
    oss << ",\"items\":[";
    for (std::size_t i = 0; i < outcomes.size(); ++i) {
      if (i) {
        oss << ",";
      }
      oss << outcomes[i].json;
    }
    oss << "]";
    oss << "}";
    std::cout << oss.str() << "\n";
    return;
  }

  std::cout << "total=" << outcomes.size() << " pass=" << pass_count << " review_required="
            << review_count << " block=" << block_count << "\n";
}

static int HandleCompatCheckCommand(int argc, char** argv) {
  if (argc < 3) {
    PrintUsage();
    return 2;
  }

  bool json = false;
  std::string single_path;
  std::string dir_path;
  std::string manifest_path;
  std::string handoff_path;


  for (int i = 2; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--json") {
      json = true;
      continue;
    }
    if (arg == "--dir" && i + 1 < argc) {
      dir_path = argv[++i];
      continue;
    }
    if (arg == "--manifest" && i + 1 < argc) {
      manifest_path = argv[++i];
      continue;
    }
    if (arg == "--handoff" && i + 1 < argc) {
      handoff_path = argv[++i];
      continue;
    }

    if (!arg.empty() && arg[0] == '-') {
      PrintUsage();
      return 2;
    }
    if (!single_path.empty()) {
      PrintUsage();
      return 2;
    }
    single_path = arg;
  }

  int mode_count = 0;
  mode_count += single_path.empty() ? 0 : 1;
  mode_count += dir_path.empty() ? 0 : 1;
  mode_count += manifest_path.empty() ? 0 : 1;
  if (mode_count != 1) {
    PrintUsage();
    return 2;
  }
  if (!handoff_path.empty() && single_path.empty()) {
    std::cerr << "--handoff requires single-file compat-check\n";
    return 2;
  }


  std::vector<std::string> targets;
  if (!single_path.empty()) {
    targets.push_back(single_path);
  } else if (!dir_path.empty()) {
    const std::filesystem::path root(dir_path);
    if (!std::filesystem::exists(root) || !std::filesystem::is_directory(root)) {
      std::cerr << "invalid directory: " << dir_path << "\n";
      return 2;
    }
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
      if (entry.is_regular_file() && IsSpzPath(entry.path())) {
        targets.push_back(entry.path().string());
      }
    }
    std::sort(targets.begin(), targets.end());
  } else {
    std::string parse_err;
    targets = ParseManifestSpzPaths(manifest_path, &parse_err);
    if (!parse_err.empty() && targets.empty()) {
      std::cerr << parse_err << "\n";
      return 2;
    }
  }

  if (targets.empty()) {
    std::cerr << "no .spz targets found\n";
    return 2;
  }

  std::vector<CompatAuditOutcome> outcomes;
  outcomes.reserve(targets.size());
  bool all_pass = true;
  for (const auto& target : targets) {
    CompatAuditOutcome outcome;
    std::string run_err;
    if (!RunCompatAuditForPath(target, &outcome, &run_err)) {
      std::cerr << run_err << "\n";
      return 2;
    }
    if (outcome.verdict != "pass") {
      all_pass = false;
    }
    outcomes.push_back(std::move(outcome));
  }

  if (outcomes.size() == 1 && !single_path.empty()) {
    if (!handoff_path.empty() && json) {
      std::string handoff_text;
      if (!ReadAllText(handoff_path, &handoff_text)) {
        std::cerr << "failed to read handoff: " << handoff_path << "\n";
        return 2;
      }
      spz_gatekeeper::BrowserAuditHandoff handoff;
      std::string handoff_err;
      if (!spz_gatekeeper::ParseBrowserAuditHandoffJson(handoff_text, &handoff, &handoff_err)) {
        std::cerr << handoff_err << "\n";
        return 2;
      }
      std::cout << spz_gatekeeper::BuildCompatCheckAuditWithHandoffJson(
                       outcomes.front().json, outcomes.front().verdict, handoff)
                << "\n";
    } else {
      PrintCompatCheckSingle(outcomes.front(), json);
    }
  } else {
    PrintCompatCheckBatch(outcomes, json);
  }

  return all_pass ? 0 : 1;
}



static int HandleCompatBoardCommand(int argc, char** argv) {
  bool json = false;
  for (int i = 2; i < argc; ++i) {
    const std::string flag = argv[i];
    if (flag == "--json") {
      json = true;
    } else {
      PrintUsage();
      return 2;
    }
  }

  try {
    PrintCompatibilityBoard(json);
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << "compat-board failed: " << ex.what() << "\n";
    return 1;
  }
}

static void PrintUsage() {



  std::cerr << "SPZ Gatekeeper - SPZ Format Legality Checker\n\n";
  std::cerr << "Usage:\n";
  std::cerr << "  spz_gatekeeper check-spz <file.spz> [--strict|--no-strict] [--json]\n";
  std::cerr << "  spz_gatekeeper dump-trailer <file.spz> [--strict|--no-strict] [--json]\n";
  std::cerr << "  spz_gatekeeper registry [--json]\n";
  std::cerr << "  spz_gatekeeper registry list [--json]\n";
  std::cerr << "  spz_gatekeeper registry show <type> [--json]\n";
  std::cerr << "  spz_gatekeeper compat-check <file.spz> [--handoff <browser_audit.json>] [--json]\n";
  std::cerr << "  spz_gatekeeper compat-check --dir <dir> [--json]\n";
  std::cerr << "  spz_gatekeeper compat-check --manifest <manifest.json> [--json]\n";


  std::cerr << "  spz_gatekeeper compat-board [--json]\n";
  std::cerr << "  spz_gatekeeper gen-fixture --type <u32> [--mode valid|invalid-size] --out <file.spz>\n";
  std::cerr << "  spz_gatekeeper guide [--json]\n";

  std::cerr << "  spz_gatekeeper --self-test\n";

  std::cerr << "  spz_gatekeeper --help\n\n";
  std::cerr << "Commands:\n";
  std::cerr << "  check-spz     Validate one SPZ file (L2 validation)\n";
  std::cerr << "  dump-trailer  Show trailer TLV records\n";
  std::cerr << "  registry      List registered extensions or inspect one entry\n";
  std::cerr << "  compat-check  Run local_cli_spz_artifact_audit for real .spz assets\n";
  std::cerr << "  compat-board  Show compatibility maturity board for registered extensions\n";
  std::cerr << "  gen-fixture   Generate a minimal SPZ fixture with one TLV record\n";
  std::cerr << "  guide         Show vendor extension development guide\n";

  std::cerr << "  --self-test   Run built-in self-tests\n";
  std::cerr << "  --help        Show this help message\n\n";

  std::cerr << "Options:\n";
  std::cerr << "  --strict      Strict mode: warnings become errors\n";
  std::cerr << "  --no-strict   Normal mode: warnings are non-fatal\n";
  std::cerr << "  --json        Output in JSON format\n\n";

  std::cerr << "Task 5 contract:\n";
  std::cerr << "  browser_lightweight_wasm_audit only gates a standard zip audit bundle in the browser.\n";
  std::cerr << "  local_cli_spz_artifact_audit audits the real .spz artifact, directory, or manifest locally.\n";
  std::cerr << "  browser_to_cli_handoff is optional, only merged into compat-check --json output.\n";
  std::cerr << "  final verdict still comes from the local CLI artifact audit.\n";
  std::cerr << "  spz_gatekeeper does not audit GLB or spz2glb.\n\n";

  std::cerr << "Examples:\n";
  std::cerr << "  spz_gatekeeper check-spz model.spz\n";
  std::cerr << "  spz_gatekeeper dump-trailer extended.spz --json\n";
  std::cerr << "  spz_gatekeeper registry --json\n";
  std::cerr << "  spz_gatekeeper registry show 0xADBE0002 --json\n";
  std::cerr << "  spz_gatekeeper compat-check model.spz --json\n";
  std::cerr << "  spz_gatekeeper compat-check model.spz --handoff browser_audit.json --json\n";
  std::cerr << "  spz_gatekeeper compat-check --dir ./fixtures --json\n";
  std::cerr << "  spz_gatekeeper compat-check --manifest ./fixtures/manifest.json --json\n";


  std::cerr << "  spz_gatekeeper compat-board --json\n";
  std::cerr << "  spz_gatekeeper gen-fixture --type 0xADBE0002 --mode valid --out fixture.spz\n";
  std::cerr << "  spz_gatekeeper guide\n";


}


}  // namespace

int main(int argc, char** argv) {
  using namespace spz_gatekeeper;

  if (argc < 2) {
    PrintUsage();
    return 2;
  }

  std::string cmd = argv[1];
  if (cmd == "--self-test") {
    return SelfTest();
  }

  if (cmd == "--help" || cmd == "-h") {
    PrintUsage();
    return 0;
  }

  if (cmd == "guide") {
    bool json = false;
    for (int i = 2; i < argc; ++i) {
      if (std::string(argv[i]) == "--json") json = true;
    }
    PrintGuide(json);
    return 0;
  }

  if (cmd == "registry") {
    return HandleRegistryCommand(argc, argv);
  }

  if (cmd == "compat-check") {
    return HandleCompatCheckCommand(argc, argv);
  }

  if (cmd == "compat-board") {
    return HandleCompatBoardCommand(argc, argv);
  }

  if (cmd == "gen-fixture") {
    return HandleGenFixtureCommand(argc, argv);
  }


  bool strict = true;


  bool json = false;
  auto parse_flags = [&](int start_idx) {
    for (int i = start_idx; i < argc; ++i) {
      std::string a = argv[i];
      if (a == "--strict") strict = true;
      else if (a == "--no-strict") strict = false;
      else if (a == "--json") json = true;
    }
  };

  if (cmd == "check-spz" || cmd == "dump-trailer") {
    if (argc < 3) {
      PrintUsage();
      return 2;
    }

    std::string path = argv[2];
    parse_flags(3);

    std::vector<std::uint8_t> b;
    if (!ReadAllBytes(path, &b)) {
      std::cerr << "failed to read: " << path << "\n";
      return 2;
    }

    SpzInspectOptions opt;
    opt.strict = strict;
    GateReport rep = InspectSpzBlob(b, opt, path);

    if (cmd == "check-spz") {
      if (json) {
        std::cout << rep.ToJson() << "\n";
      } else {
        std::cout << rep.ToText();
      }
      return rep.HasErrors() ? 1 : 0;
    }

    if (json) {
      if (rep.spz_l2.has_value()) {
        std::ostringstream oss;
        oss << "{";
        oss << "\"asset_path\":\"" << JsonEscape(path) << "\"";
        oss << ",\"flags\":" << static_cast<unsigned>(rep.spz_l2->flags);
        oss << ",\"trailer_size\":" << rep.spz_l2->trailer_size;
        oss << ",\"tlv_records\":[";
        for (std::size_t i = 0; i < rep.spz_l2->tlv_records.size(); ++i) {
          if (i) oss << ",";
          const auto& r = rep.spz_l2->tlv_records[i];
          oss << "{";
          oss << "\"type\":" << r.type;
          oss << ",\"length\":" << r.length;
          oss << ",\"offset\":" << r.offset;
          oss << "}";
        }
        oss << "]}";
        std::cout << oss.str() << "\n";
      } else {
        std::cout << "{\"asset_path\":\"" << JsonEscape(path) << "\",\"error\":\"no l2 info\"}\n";
      }
      return rep.HasErrors() ? 1 : 0;
    }

    if (!rep.spz_l2.has_value()) {
      std::cout << "no L2 info\n";
      return rep.HasErrors() ? 1 : 0;
    }

    const auto& l2 = rep.spz_l2.value();
    std::cout << "flags=" << static_cast<unsigned>(l2.flags) << " trailer_size=" << l2.trailer_size << "\n";
    if (l2.tlv_records.empty()) {
      std::cout << "(no TLV records)\n";
    } else {
      for (const auto& r : l2.tlv_records) {
        std::cout << "type=" << r.type << " len=" << r.length << " off=" << r.offset << "\n";
      }
    }

    return rep.HasErrors() ? 1 : 0;
  }

  PrintUsage();
  return 2;
}
