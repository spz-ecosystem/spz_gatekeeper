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
 *   spz_gatekeeper guide [--json]
 *   spz_gatekeeper --self-test
 *
 * @author PuJunhan
 * @copyright Copyright (c) 2026 PuJunhan
 * @license MIT
 */

#include "spz_gatekeeper/json_min.h"
#include "spz_gatekeeper/spz.h"

#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>
#include <zlib.h>

namespace {

static bool ReadAllBytes(const std::string& path, std::vector<std::uint8_t>* out) {
  std::ifstream in(path, std::ios::binary | std::ios::ate);
  if (!in.good()) return false;
  std::size_t size = static_cast<std::size_t>(in.tellg());
  in.seekg(0, std::ios::beg);
  out->resize(size);
  in.read(reinterpret_cast<char*>(out->data()), static_cast<std::streamsize>(size));
  return in.good();
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

static std::vector<std::uint8_t> BuildDecompressedSpz(std::uint32_t version, std::uint8_t flags,
                                                      bool with_trailer) {
  auto write_u32 = [](std::vector<std::uint8_t>* b, std::uint32_t v) {
    b->push_back(static_cast<std::uint8_t>(v & 0xFF));
    b->push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
    b->push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
    b->push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
  };

  std::vector<std::uint8_t> decomp;
  decomp.reserve(64);
  write_u32(&decomp, 0x5053474e);
  write_u32(&decomp, version);
  write_u32(&decomp, 1);
  decomp.push_back(0);
  decomp.push_back(12);
  decomp.push_back(flags);
  decomp.push_back(0);

  const bool uses_float16 = (version == 1);
  const bool uses_quat_smallest_three = (version >= 3);
  const std::size_t positions = uses_float16 ? 6 : 9;
  const std::size_t rotations = uses_quat_smallest_three ? 4 : 3;
  decomp.insert(decomp.end(), positions + 1 + 3 + 3 + rotations, 0);

  if (with_trailer) {
    write_u32(&decomp, 1);
    write_u32(&decomp, 3);
    decomp.push_back('a');
    decomp.push_back('b');
    decomp.push_back('c');
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

  {
    auto gz = gzip(BuildDecompressedSpz(3, 0, false));
    if (!gz.has_value()) return 10;
    SpzInspectOptions opt;
    GateReport rep = InspectSpzBlob(gz.value(), opt, "self-test:noext:notrailer");
    if (rep.HasErrors()) return 11;
    if (HasIssueCode(rep, "L2_UNDECLARED_TRAILER")) return 12;
  }

  {
    auto gz = gzip(BuildDecompressedSpz(3, 0, true));
    if (!gz.has_value()) return 20;
    SpzInspectOptions opt;
    GateReport rep = InspectSpzBlob(gz.value(), opt, "self-test:noext:trailer");
    if (rep.HasErrors()) return 21;
    if (!HasIssueCode(rep, "L2_UNDECLARED_TRAILER")) return 22;
  }

  {
    auto gz = gzip(BuildDecompressedSpz(3, kFlagHasExtensions, true));
    if (!gz.has_value()) return 30;
    SpzInspectOptions opt;
    GateReport rep = InspectSpzBlob(gz.value(), opt, "self-test:ext:trailer");
    if (rep.HasErrors()) return 31;
    if (!rep.spz_l2.has_value()) return 32;
    if (rep.spz_l2->tlv_records.size() != 1) return 33;
  }

  {
    auto gz = gzip(BuildDecompressedSpz(3, kFlagHasExtensions, false));
    if (!gz.has_value()) return 40;
    SpzInspectOptions opt;
    GateReport rep = InspectSpzBlob(gz.value(), opt, "self-test:ext:notrailer");
    if (!rep.HasErrors()) return 41;
    if (!HasIssueCode(rep, "L2_EXT_DECLARED_NO_TRAILER")) return 42;
  }

  {
    auto gz = gzip(BuildDecompressedSpz(4, 0, false));
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

static void PrintUsage() {
  std::cerr << "SPZ Gatekeeper - SPZ Format Legality Checker\n\n";
  std::cerr << "Usage:\n";
  std::cerr << "  spz_gatekeeper check-spz <file.spz> [--strict|--no-strict] [--json]\n";
  std::cerr << "  spz_gatekeeper dump-trailer <file.spz> [--strict|--no-strict] [--json]\n";
  std::cerr << "  spz_gatekeeper guide [--json]\n";
  std::cerr << "  spz_gatekeeper --self-test\n";
  std::cerr << "  spz_gatekeeper --help\n\n";
  std::cerr << "Commands:\n";
  std::cerr << "  check-spz     Validate SPZ file (L2 validation)\n";
  std::cerr << "  dump-trailer  Show trailer TLV records\n";
  std::cerr << "  guide         Show vendor extension development guide\n";
  std::cerr << "  --self-test   Run built-in self-tests\n";
  std::cerr << "  --help        Show this help message\n\n";
  std::cerr << "Options:\n";
  std::cerr << "  --strict      Strict mode: warnings become errors\n";
  std::cerr << "  --no-strict   Normal mode: warnings are non-fatal\n";
  std::cerr << "  --json        Output in JSON format\n\n";
  std::cerr << "Examples:\n";
  std::cerr << "  spz_gatekeeper check-spz model.spz\n";
  std::cerr << "  spz_gatekeeper dump-trailer extended.spz --json\n";
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
