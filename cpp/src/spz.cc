// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PuJunhan

#include "spz_gatekeeper/spz.h"

#include "spz_gatekeeper/tlv.h"
#include "spz_gatekeeper/extension_validator.h"
#include "spz_gatekeeper/validator_registry.h"
#include "spz_gatekeeper/safe_orbit_camera_validator.h"

#include <zlib.h>

#include <algorithm>
#include <cstring>
#include <limits>

namespace spz_gatekeeper {

namespace {

constexpr std::uint32_t kKnownMaxVersion = 4;

// Auto-register built-in Adobe validator for runtime check-spz paths.
static RegisterValidator<AdobeSafeOrbitCameraValidator> kAutoRegisterAdobeValidator;



/// Convert integer to hex string
static std::string ToHexString(uint32_t value) {
  char buf[16];
  std::snprintf(buf, sizeof(buf), "%04X", value);
  return std::string(buf);
}

/// Extract vendor name from extension type (high 16 bits)
static std::string GetVendorName(uint32_t type) {
  uint16_t vendor_id = static_cast<uint16_t>(type >> 16);
  switch (vendor_id) {
    case 0xADBE: return "Adobe";
    case 0x4E41: return "Niantic";
    default: return "Unknown (0x" + ToHexString(vendor_id) + ")";
  }
}

static std::uint32_t ReadU32LE(const std::vector<std::uint8_t>& b, std::size_t off) {
  return static_cast<std::uint32_t>(b[off]) |
         (static_cast<std::uint32_t>(b[off + 1]) << 8) |
         (static_cast<std::uint32_t>(b[off + 2]) << 16) |
         (static_cast<std::uint32_t>(b[off + 3]) << 24);
}

struct SpzHeader {
  std::uint32_t magic = 0;
  std::uint32_t version = 0;
  std::uint32_t num_points = 0;
  std::uint8_t sh_degree = 0;
  std::uint8_t fractional_bits = 0;
  std::uint8_t flags = 0;
  std::uint8_t reserved = 0;
};

static bool DecompressGzip(const std::vector<std::uint8_t>& in, std::vector<std::uint8_t>* out,
                           std::string* err) {
  out->clear();
  if (in.empty()) {
    if (err) *err = "empty input";
    return false;
  }

  z_stream strm = {};
  strm.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(in.data()));
  strm.avail_in = static_cast<uInt>(in.size());

  // 16+MAX_WBITS enables gzip header detection (matches spz-main).
  int rc = inflateInit2(&strm, 16 | MAX_WBITS);
  if (rc != Z_OK) {
    if (err) *err = "inflateInit2 failed";
    return false;
  }

  std::vector<std::uint8_t> buf(8192);
  bool ok = false;
  while (true) {
    strm.next_out = reinterpret_cast<Bytef*>(buf.data());
    strm.avail_out = static_cast<uInt>(buf.size());
    int r = inflate(&strm, Z_NO_FLUSH);
    if (r != Z_OK && r != Z_STREAM_END) {
      if (err) *err = "inflate failed";
      break;
    }
    std::size_t produced = buf.size() - static_cast<std::size_t>(strm.avail_out);
    out->insert(out->end(), buf.begin(), buf.begin() + static_cast<std::ptrdiff_t>(produced));
    if (r == Z_STREAM_END) {
      ok = true;
      break;
    }
  }

  inflateEnd(&strm);
  return ok;
}

static int32_t DimForDegree(int32_t degree) {
  switch (degree) {
    case 0: return 0;
    case 1: return 3;
    case 2: return 8;
    case 3: return 15;
    case 4: return 24;
    default: return 0;
  }
}

static void RebindTlvRecordViews(std::vector<TlvRecord>* records,
                                 const std::vector<std::uint8_t>& trailer_storage,
                                 std::size_t trailer_offset) {
  for (auto& record : *records) {
    if (record.length == 0) {
      record.value_data = nullptr;
      continue;
    }
    const std::size_t local_value_offset = (record.offset - trailer_offset) + 8;
    record.value_data = trailer_storage.data() + local_value_offset;
  }
}

static bool ParseHeader(const std::vector<std::uint8_t>& decomp, SpzHeader* h, std::string* err) {
  if (decomp.size() < 16) {
    if (err) *err = "decompressed data too small for header";
    return false;
  }
  h->magic = ReadU32LE(decomp, 0);
  h->version = ReadU32LE(decomp, 4);
  h->num_points = ReadU32LE(decomp, 8);
  h->sh_degree = decomp[12];
  h->fractional_bits = decomp[13];
  h->flags = decomp[14];
  h->reserved = decomp[15];
  return true;
}

static std::size_t ComputeBasePayloadSize(const SpzHeader& h, bool* ok) {
  *ok = false;
  if (h.version < 1) return 0;
  if (h.sh_degree > 4) return 0;

  const bool uses_float16 = (h.version == 1);
  const bool uses_quat_smallest_three = (h.version >= 3);
  const std::size_t n = static_cast<std::size_t>(h.num_points);
  const std::size_t sh_dim = static_cast<std::size_t>(DimForDegree(h.sh_degree));

  std::size_t positions = n * 3 * (uses_float16 ? 2 : 3);
  std::size_t alphas = n;
  std::size_t colors = n * 3;
  std::size_t scales = n * 3;
  std::size_t rotations = n * (uses_quat_smallest_three ? 4 : 3);
  std::size_t sh = n * sh_dim * 3;

  *ok = true;
  return 16 + positions + alphas + colors + scales + rotations + sh;
}

[[maybe_unused]] static bool CompressGzip(const std::vector<std::uint8_t>& in, std::vector<std::uint8_t>* out) {
  z_stream strm = {};
  if (deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 16 + MAX_WBITS, 9, Z_DEFAULT_STRATEGY) != Z_OK) {
    return false;
  }

  std::vector<std::uint8_t> buf(8192);
  strm.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(in.data()));
  strm.avail_in = static_cast<uInt>(in.size());

  out->clear();
  bool ok = false;
  while (true) {
    strm.next_out = reinterpret_cast<Bytef*>(buf.data());
    strm.avail_out = static_cast<uInt>(buf.size());
    int r = deflate(&strm, Z_FINISH);
    if (r != Z_OK && r != Z_STREAM_END) break;
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

}  // namespace

GateReport InspectSpzBlob(const std::vector<std::uint8_t>& gz_spz, const SpzInspectOptions& opt,
                          const std::string& where) {
  GateReport rep;
  rep.asset_path = where;

  std::vector<std::uint8_t> decomp;
  std::string derr;
  if (!DecompressGzip(gz_spz, &decomp, &derr)) {
    AddIssue(&rep, Severity::kError, "L2_GZIP_DECOMPRESS", "failed to gunzip SPZ blob", where);
    return rep;
  }

  SpzL2Info info;
  info.decompressed_size = decomp.size();

  SpzHeader h;
  std::string herr;
  if (!ParseHeader(decomp, &h, &herr)) {
    AddIssue(&rep, Severity::kError, "L2_HEADER", "failed to parse SPZ header", where);
    rep.spz_l2 = info;
    return rep;
  }

  info.header_ok = true;
  info.version = h.version;
  info.num_points = h.num_points;
  info.sh_degree = h.sh_degree;
  info.flags = h.flags;
  info.reserved = h.reserved;

  if (h.magic != 0x5053474e) {
    AddIssue(&rep, Severity::kError, "L2_MAGIC", "SPZ magic mismatch", where);
  }
  if (h.version < 1) {
    AddIssue(&rep, Severity::kError, "L2_VERSION", "invalid version", where);
  } else if (h.version > kKnownMaxVersion) {
    AddIssue(&rep, Severity::kWarning, "L2_VERSION", "version newer than known max", where);
  }
  if (h.sh_degree > 4) {
    AddIssue(&rep, Severity::kError, "L2_SH_DEGREE", "unsupported SH degree", where);
  }
  if (h.reserved != 0) {
    AddIssue(&rep, Severity::kError, "L2_RESERVED", "reserved must be 0", where);
  }

  bool base_ok = false;
  std::size_t base = ComputeBasePayloadSize(h, &base_ok);
  info.base_payload_size = base;
  if (!base_ok) {
    AddIssue(&rep, Severity::kError, "L2_BASE_SIZE", "failed to compute base payload size", where);
    rep.spz_l2 = info;
    return rep;
  }

  if (decomp.size() < base) {
    AddIssue(&rep, Severity::kError, "L2_TRUNCATED", "decompressed data shorter than base payload", where);
    rep.spz_l2 = info;
    return rep;
  }

  std::size_t trailer_size = decomp.size() - base;
  info.trailer_size = trailer_size;

  const bool declared = (h.flags & kFlagHasExtensions) != 0;

  if (declared) {
    if (trailer_size == 0) {
      AddIssue(&rep, Severity::kError, "L2_EXT_DECLARED_NO_TRAILER", "has-extensions flag is set but no trailer bytes exist", where);

      rep.spz_l2 = info;
      return rep;
    }
    auto tlv = ParseTlvTrailer(decomp, base);
    if (!tlv.ok) {
      if (opt.strict) {
        AddIssue(&rep, Severity::kError, "L2_TLV_PARSE", "trailer TLV parse failed: " + tlv.error, where);
        rep.spz_l2 = info;
        return rep;
      }
      AddIssue(&rep, Severity::kWarning, "L2_TLV_PARSE", "trailer TLV parse failed (ignored in non-strict): " + tlv.error, where);
    } else {
      // 只保留一份 trailer backing storage，避免旧实现那种逐条 TLV payload 拷贝。
      info.tlv_storage.assign(decomp.begin() + static_cast<std::ptrdiff_t>(base), decomp.end());
      RebindTlvRecordViews(&tlv.records, info.tlv_storage, base);
      info.tlv_records = std::move(tlv.records);

      // 逐条校验扩展；这里直接复用 record 视图，不再回到原始 buffer 重新切片。
      for (const auto& record : info.tlv_records) {
        auto validator = ExtensionValidatorRegistry::Instance().GetValidator(record.type);
        if (validator) {
          std::string error;
          const auto payload = record.ValueView();
          bool valid = validator->Validate(payload.data(), record.length, &error);

          ExtensionReport ext_report;
          ext_report.type = record.type;
          ext_report.vendor_name = GetVendorName(record.type);
          ext_report.extension_name = validator->GetName();
          ext_report.validation_result = valid;
          ext_report.error_message = error;
          rep.extension_reports.push_back(ext_report);

          if (!valid) {
            AddIssue(&rep, Severity::kError, "L2_EXT_VALIDATION",
                     "Extension validation failed: " + validator->GetName() + " - " + error, where);
          }
        } else {
          ExtensionReport ext_report;
          ext_report.type = record.type;
          ext_report.vendor_name = GetVendorName(record.type);
          ext_report.extension_name = "Unknown";
          ext_report.validation_result = true;
          ext_report.error_message = "";
          rep.extension_reports.push_back(ext_report);

          AddIssue(&rep, Severity::kWarning, "L2_EXT_UNKNOWN",
                   "Unknown extension type: 0x" + ToHexString(record.type) +
                   " (" + std::to_string(record.length) + " bytes)", where);
        }
      }
    }

  } else {
    if (trailer_size > 0) {
      AddIssue(&rep, Severity::kWarning, "L2_UNDECLARED_TRAILER", "trailing bytes exist but has-extensions flag is not set", where);

    }
  }

  rep.spz_l2.emplace(std::move(info));
  if (!rep.spz_l2->tlv_storage.empty()) {
    RebindTlvRecordViews(&rep.spz_l2->tlv_records, rep.spz_l2->tlv_storage,
                         rep.spz_l2->base_payload_size);
  }
  return rep;
}

double sh_epsilon(int bits) {
  if (bits <= 0) {
    return std::numeric_limits<double>::infinity();
  }
  const int denom = (1 << bits) - 1;
  if (denom <= 0) {
    return std::numeric_limits<double>::infinity();
  }
  return 2.0 / static_cast<double>(denom);
}

}  // namespace spz_ga