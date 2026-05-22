// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PuJunhan

#include "spz_gatekeeper/spz.h"

#include "spz_gatekeeper/tlv.h"
#include "spz_gatekeeper/extension_spec_registry.h"
#include "spz_gatekeeper/extension_validator.h"
#include "spz_gatekeeper/validator_registry.h"
#include "spz_gatekeeper/safe_orbit_camera_validator.h"


#include <zlib.h>
#include <zstd.h>

#include <algorithm>
#include <cstring>
#if !defined(__EMSCRIPTEN__)
#include <future>
#endif
#include <limits>
#include <numeric>

namespace spz_gatekeeper {

namespace {

constexpr std::uint32_t kKnownMaxVersion = 4;

// Auto-register built-in Adobe validator for runtime check-spz paths.
static RegisterValidator<AdobeSafeOrbitCameraValidator> kAutoRegisterAdobeValidator;

static bool RegisterBuiltInSpecs() {
  ExtensionSpec adobe_spec;
  adobe_spec.type = kAdobeSafeOrbitCameraType;
  adobe_spec.vendor_id = static_cast<std::uint16_t>(kAdobeSafeOrbitCameraType >> 16);
  adobe_spec.extension_id = static_cast<std::uint16_t>(kAdobeSafeOrbitCameraType & 0xFFFFu);
  adobe_spec.vendor_name = "Adobe";
  adobe_spec.extension_name = "Adobe Safe Orbit Camera";
  adobe_spec.category = "camera";
  adobe_spec.status = "stable";
  adobe_spec.spec_url = "docs/Implementing_Custom_Extension.md";
  adobe_spec.short_description = "Constrains orbit elevation and minimum radius for safer camera control.";
  adobe_spec.min_spz_version = 1;
  adobe_spec.requires_has_extensions_flag = true;
  ExtensionSpecRegistry::Instance().RegisterSpec(adobe_spec);
  return true;
}

static const bool kAutoRegisterBuiltInSpecs = RegisterBuiltInSpecs();

/// Convert integer to hex string
static std::string ToHexString(uint32_t value) {
  char buf[16];
  std::snprintf(buf, sizeof(buf), "%04X", value);
  return std::string(buf);
}

static std::string GetFallbackVendorName(uint32_t type) {
  const auto vendor_id = static_cast<std::uint16_t>(type >> 16);
  return "Unknown (0x" + ToHexString(vendor_id) + ")";
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

struct TocEntry {
  uint64_t compressed_size;
  uint64_t uncompressed_size;
  size_t compressed_offset;
};

struct TocParseResult {
  bool ok = false;
  std::string error;
  std::vector<TocEntry> entries;
};

static TocParseResult ParseToc(const uint8_t* data, size_t size, uint32_t toc_byte_offset, uint8_t num_streams) {
  TocParseResult result;
  const size_t toc_size = static_cast<size_t>(num_streams) * 16;
  const size_t toc_end = static_cast<size_t>(toc_byte_offset) + toc_size;
  if (toc_end > size) {
    result.error = "SPZ_TOC_TRUNCATED";
    return result;
  }

  size_t compressed_offset = toc_end;
  for (uint8_t i = 0; i < num_streams; i++) {
    TocEntry e;
    const size_t pos = static_cast<size_t>(toc_byte_offset) + static_cast<size_t>(i) * 16;
    std::memcpy(&e.compressed_size, data + pos, sizeof(uint64_t));
    std::memcpy(&e.uncompressed_size, data + pos + sizeof(uint64_t), sizeof(uint64_t));
    e.compressed_offset = compressed_offset;
    compressed_offset += e.compressed_size;

    if (compressed_offset > size) {
      result.error = "SPZ_TOC_OFFSET";
      return result;
    }
    result.entries.push_back(e);
  }

  if (compressed_offset != size) {
    result.error = "SPZ_TOC_SIZE_MISMATCH";
    return result;
  }
  result.ok = true;
  return result;
}

static bool DecompressZstdStream(const uint8_t* src, size_t src_size,
                                 std::vector<uint8_t>* out, std::string* err) {
  const size_t bound = ZSTD_getFrameContentSize(src, src_size);
  if (bound == ZSTD_CONTENTSIZE_ERROR) {
    if (err) *err = "SPZ_DECOMPRESS_ZSTD";
    return false;
  }
  if (bound == ZSTD_CONTENTSIZE_UNKNOWN) {
    if (err) *err = "SPZ_DECOMPRESS_ZSTD";
    return false;
  }

  out->resize(bound);
  const size_t ret = ZSTD_decompress(out->data(), out->size(), src, src_size);
  if (ZSTD_isError(ret)) {
    if (err) *err = std::string("ZSTD_decompress: ") + ZSTD_getErrorName(ret);
    return false;
  }
  if (ret != bound) {
    if (err) *err = "SPZ_DECOMPRESS_ZSTD";
    return false;
  }
  return true;
}

static bool DecompressNgspStreams(const uint8_t* data, size_t size,
                                  const std::vector<TocEntry>& toc,
                                  std::vector<uint8_t>* decomp, std::string* err) {
  uint64_t total = 0;
  for (const auto& entry : toc) {
    total += entry.uncompressed_size;
  }
  decomp->clear();
  decomp->reserve(static_cast<size_t>(total));

#if defined(__EMSCRIPTEN__)
  for (const auto& entry : toc) {
    std::vector<uint8_t> buf;
    if (!DecompressZstdStream(data + entry.compressed_offset,
                               static_cast<size_t>(entry.compressed_size), &buf, err))
      return false;
    decomp->insert(decomp->end(), buf.begin(), buf.end());
  }
#else
  bool fallback = false;
  const bool try_parallel =
      std::thread::hardware_concurrency() >= 2 && toc.size() > 1;

  if (try_parallel) {
    std::vector<std::future<std::vector<uint8_t>>> futures;
    try {
      for (const auto& entry : toc) {
        futures.push_back(std::async(std::launch::async,
          [&entry, data]() -> std::vector<uint8_t> {
            std::vector<uint8_t> buf;
            std::string ignored;
            DecompressZstdStream(data + entry.compressed_offset,
                                  static_cast<size_t>(entry.compressed_size), &buf, &ignored);
            return buf;
          }));
      }
      for (auto& f : futures) {
        auto result = f.get();
        if (result.empty()) return false;
        decomp->insert(decomp->end(), result.begin(), result.end());
      }
    } catch (const std::system_error&) {
      fallback = true;
    }
  }
  if (!try_parallel || fallback) {
    for (const auto& entry : toc) {
      std::vector<uint8_t> buf;
      if (!DecompressZstdStream(data + entry.compressed_offset,
                                 static_cast<size_t>(entry.compressed_size), &buf, err))
        return false;
      decomp->insert(decomp->end(), buf.begin(), buf.end());
    }
  }
#endif
  return true;
}

static bool CompressZstdStream(const uint8_t* src, size_t src_size,
                               std::vector<uint8_t>* out, std::string* err) {
  const size_t bound = ZSTD_compressBound(src_size);
  out->resize(bound);
  const int compression_level = 12;
  const size_t ret = ZSTD_compress(out->data(), out->size(), src, src_size, compression_level);
  if (ZSTD_isError(ret)) {
    if (err) *err = std::string("ZSTD_compress: ") + ZSTD_getErrorName(ret);
    return false;
  }
  out->resize(ret);
  return true;
}

static bool CompressNgspStreams(
    const std::vector<std::pair<const uint8_t*, size_t>>& srcs,
    std::vector<std::vector<uint8_t>>* chunks,
    std::vector<uint64_t>* uncompressed_sizes,
    std::string* err) {
  chunks->clear();
  chunks->resize(srcs.size());
  uncompressed_sizes->clear();
  uncompressed_sizes->resize(srcs.size());

  for (size_t i = 0; i < srcs.size(); i++) {
    uncompressed_sizes->at(i) = srcs[i].second;
  }

#if defined(__EMSCRIPTEN__)
  for (size_t i = 0; i < srcs.size(); i++) {
    if (!CompressZstdStream(srcs[i].first, srcs[i].second, &chunks->at(i), err))
      return false;
  }
#else
  bool fallback = false;
  const bool try_parallel =
      std::thread::hardware_concurrency() >= 2 && srcs.size() > 1;

  if (try_parallel) {
    std::vector<std::future<bool>> futures;
    try {
      for (size_t i = 0; i < srcs.size(); i++) {
        futures.push_back(std::async(std::launch::async,
          [i, &srcs, chunks, err]() -> bool {
            return CompressZstdStream(srcs[i].first, srcs[i].second,
                                       &chunks->at(i), err);
          }));
      }
      for (auto& f : futures) {
        if (!f.get()) return false;
      }
    } catch (const std::system_error&) {
      fallback = true;
    }
  }
  if (!try_parallel || fallback) {
    for (size_t i = 0; i < srcs.size(); i++) {
      if (!CompressZstdStream(srcs[i].first, srcs[i].second, &chunks->at(i), err))
        return false;
    }
  }
#endif
  return true;
}

static bool BuildNgspBlob(uint32_t num_points, uint8_t sh_degree,
                          uint8_t fractional_bits, uint8_t flags,
                          const std::vector<uint8_t>& extension_data,
                          const std::vector<std::vector<uint8_t>>& chunks,
                          const std::vector<uint64_t>& uncompressed_sizes,
                          std::vector<uint8_t>* out) {
  const uint8_t num_streams = static_cast<uint8_t>(chunks.size());
  const uint32_t toc_byte_offset = 32 + static_cast<uint32_t>(extension_data.size());

  out->clear();
  out->reserve(toc_byte_offset + static_cast<size_t>(num_streams) * 16 +
               std::accumulate(chunks.begin(), chunks.end(), size_t(0),
                               [](size_t s, const auto& c) { return s + c.size(); }));

  const uint32_t magic = 0x5053474e;
  const uint32_t version = 4;
  out->insert(out->end(), reinterpret_cast<const uint8_t*>(&magic),
              reinterpret_cast<const uint8_t*>(&magic) + sizeof(magic));
  out->insert(out->end(), reinterpret_cast<const uint8_t*>(&version),
              reinterpret_cast<const uint8_t*>(&version) + sizeof(version));
  out->insert(out->end(), reinterpret_cast<const uint8_t*>(&num_points),
              reinterpret_cast<const uint8_t*>(&num_points) + sizeof(num_points));
  uint8_t sh_pad[4] = {sh_degree, fractional_bits, flags, num_streams};
  out->insert(out->end(), sh_pad, sh_pad + 4);
  out->insert(out->end(), reinterpret_cast<const uint8_t*>(&toc_byte_offset),
              reinterpret_cast<const uint8_t*>(&toc_byte_offset) + sizeof(toc_byte_offset));
  uint8_t reserved[12] = {};
  out->insert(out->end(), reserved, reserved + 12);

  out->insert(out->end(), extension_data.begin(), extension_data.end());

  for (size_t i = 0; i < chunks.size(); i++) {
    const uint64_t csize = chunks[i].size();
    const uint64_t usize = uncompressed_sizes[i];
    out->insert(out->end(), reinterpret_cast<const uint8_t*>(&csize),
                reinterpret_cast<const uint8_t*>(&csize) + sizeof(csize));
    out->insert(out->end(), reinterpret_cast<const uint8_t*>(&usize),
                reinterpret_cast<const uint8_t*>(&usize) + sizeof(usize));
  }

  for (const auto& chunk : chunks) {
    out->insert(out->end(), chunk.begin(), chunk.end());
  }
  return true;
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
        const auto spec = ExtensionSpecRegistry::Instance().GetSpec(record.type);
        auto validator = ExtensionValidatorRegistry::Instance().GetValidator(record.type);

        ExtensionReport ext_report;
        ext_report.type = record.type;
        ext_report.known_extension = spec.has_value();
        ext_report.has_validator = static_cast<bool>(validator);
        ext_report.vendor_name = spec.has_value() ? spec->vendor_name : GetFallbackVendorName(record.type);
        ext_report.extension_name = spec.has_value()
            ? spec->extension_name
            : (validator ? validator->GetName() : "Unknown");
        ext_report.status = spec.has_value() ? spec->status : "";
        ext_report.category = spec.has_value() ? spec->category : "";
        ext_report.spec_url = spec.has_value() ? spec->spec_url : "";
        ext_report.short_description = spec.has_value() ? spec->short_description : "";
        ext_report.validation_result = true;

        if (validator) {
          std::string error;
          const auto payload = record.ValueView();
          const bool valid = validator->Validate(payload.data(), record.length, &error);
          ext_report.validation_result = valid;
          ext_report.error_message = error;
          if (!valid) {
            AddIssue(&rep, Severity::kError, "L2_EXT_VALIDATION",
                     "Extension validation failed: " + ext_report.extension_name + " - " + error, where);
          }
        }

        rep.extension_reports.push_back(ext_report);

        if (spec.has_value() && !validator) {
          AddIssue(&rep, Severity::kWarning, "L2_EXT_REGISTERED_NO_VALIDATOR",
                   "Registered extension has no validator: " + ext_report.extension_name, where);
        } else if (!spec.has_value() && validator) {
          AddIssue(&rep, Severity::kWarning, "L2_EXT_UNREGISTERED_VALIDATOR",
                   "Validator exists for unregistered extension type: 0x" + ToHexString(record.type), where);
        } else if (!spec.has_value() && !validator) {
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