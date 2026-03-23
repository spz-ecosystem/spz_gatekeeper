// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PuJunhan

#include "spz_gatekeeper/audit_summary.h"
#include "spz_gatekeeper/extension_spec_registry.h"
#include "spz_gatekeeper/json_min.h"
#include "spz_gatekeeper/spz.h"
#include "spz_gatekeeper/validator_registry.h"

#include <cstddef>

#include <cstdint>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <zlib.h>


#include <emscripten/bind.h>
#include <emscripten/val.h>

namespace {

constexpr std::size_t kMaxSpzBytes = 512u * 1024u * 1024u;

struct FixtureBlob {
  std::vector<std::uint8_t> bytes;
  bool placeholder_payload = false;
};

emscripten::val ParseJsonObject(const std::string& json) {
  return emscripten::val::global("JSON").call<emscripten::val>("parse", json);
}

std::string StringifyJsonValue(const emscripten::val& value) {
  return emscripten::val::global("JSON").call<emscripten::val>("stringify", value)
      .as<std::string>();
}

std::vector<std::uint8_t> ToBytes(const emscripten::val& input) {

  emscripten::val uint8_array = emscripten::val::global("Uint8Array").new_(input);
  const std::size_t size = uint8_array["length"].as<std::size_t>();
  if (size > kMaxSpzBytes) {
    throw std::runtime_error("spz buffer too large");
  }

  std::vector<std::uint8_t> bytes(size);
  emscripten::val dst = emscripten::val(emscripten::typed_memory_view(bytes.size(), bytes.data()));
  dst.call<void>("set", uint8_array);
  return bytes;
}

bool TryInspect(const emscripten::val& input, bool strict, spz_gatekeeper::GateReport* rep,
                std::string* err) {
  try {
    spz_gatekeeper::SpzInspectOptions opt;
    opt.strict = strict;
    *rep = spz_gatekeeper::InspectSpzBlob(ToBytes(input), opt, "<wasm>");
    return true;
  } catch (const std::exception& ex) {
    *err = ex.what();
    return false;
  } catch (...) {
    *err = "unknown wasm exception";
    return false;
  }
}



const spz_gatekeeper::ExtensionReport* FindExtensionReport(const spz_gatekeeper::GateReport& report,
                                                           std::uint32_t type) {
  for (const auto& ext : report.extension_reports) {
    if (ext.type == type) {
      return &ext;
    }
  }
  return nullptr;
}

bool GzipCompress(const std::vector<std::uint8_t>& in, std::vector<std::uint8_t>* out,
                  std::string* err) {
  out->clear();

  z_stream strm = {};
  strm.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(in.data()));
  strm.avail_in = static_cast<uInt>(in.size());

  if (deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 16 + MAX_WBITS, 9,
                   Z_DEFAULT_STRATEGY) != Z_OK) {
    if (err != nullptr) {
      *err = "deflateInit2 failed";
    }
    return false;
  }

  std::vector<std::uint8_t> buffer(8192);
  bool ok = false;
  while (true) {
    strm.next_out = reinterpret_cast<Bytef*>(buffer.data());
    strm.avail_out = static_cast<uInt>(buffer.size());
    const int ret = deflate(&strm, Z_FINISH);
    if (ret != Z_OK && ret != Z_STREAM_END) {
      if (err != nullptr) {
        *err = "deflate failed";
      }
      break;
    }

    const std::size_t produced = buffer.size() - static_cast<std::size_t>(strm.avail_out);
    out->insert(out->end(), buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(produced));
    if (ret == Z_STREAM_END) {
      ok = true;
      break;
    }
  }

  deflateEnd(&strm);
  return ok;
}

void WriteU32Le(std::vector<std::uint8_t>* bytes, std::uint32_t value) {
  bytes->push_back(static_cast<std::uint8_t>(value & 0xFFu));
  bytes->push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFu));
  bytes->push_back(static_cast<std::uint8_t>((value >> 16) & 0xFFu));
  bytes->push_back(static_cast<std::uint8_t>((value >> 24) & 0xFFu));
}

std::vector<std::uint8_t> BuildTlvTrailer(
    const std::vector<std::pair<std::uint32_t, std::vector<std::uint8_t>>>& records) {
  std::vector<std::uint8_t> trailer;
  for (const auto& record : records) {
    WriteU32Le(&trailer, record.first);
    WriteU32Le(&trailer, static_cast<std::uint32_t>(record.second.size()));
    trailer.insert(trailer.end(), record.second.begin(), record.second.end());
  }
  return trailer;
}

std::vector<std::uint8_t> WriteFloatBytes(float value) {
  std::vector<std::uint8_t> bytes(sizeof(float));
  std::memcpy(bytes.data(), &value, sizeof(float));
  return bytes;
}

std::vector<std::uint8_t> BuildAdobeSafeOrbitPayload(bool invalid_size) {
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

std::vector<std::uint8_t> BuildDecompressedSpz(std::uint32_t version, std::uint8_t flags,
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

FixtureBlob BuildFixtureBlob(std::uint32_t type, bool invalid_size) {
  FixtureBlob fixture;

  std::vector<std::uint8_t> payload;
  if (type == 0xADBE0002u) {
    payload = BuildAdobeSafeOrbitPayload(invalid_size);
  } else {
    payload = invalid_size ? std::vector<std::uint8_t>{0x00}
                           : std::vector<std::uint8_t>{0x00, 0x00, 0x00, 0x00};
  }


  const auto trailer = BuildTlvTrailer({{type, payload}});
  const auto decompressed = BuildDecompressedSpz(3, spz_gatekeeper::kFlagHasExtensions, &trailer);
  std::string err;
  if (!GzipCompress(decompressed, &fixture.bytes, &err)) {
    throw std::runtime_error(err.empty() ? "gzip compression failed" : err);
  }
  return fixture;
}

emscripten::val BuildIssues(const spz_gatekeeper::GateReport& rep) {
  emscripten::val issues = emscripten::val::array();
  std::size_t idx = 0;
  for (const auto& it : rep.issues) {
    emscripten::val issue = emscripten::val::object();
    std::string severity = "note";
    if (it.severity == spz_gatekeeper::Severity::kError) severity = "error";
    if (it.severity == spz_gatekeeper::Severity::kWarning) severity = "warning";
    issue.set("severity", severity);
    issue.set("code", it.code);
    issue.set("message", it.message);
    issue.set("where", it.where);
    issues.set(static_cast<unsigned>(idx++), issue);
  }
  return issues;
}

std::string RegistryEntryToJson(const spz_gatekeeper::ExtensionSpec& spec) {
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

std::string BuildRegistryListJson() {
  const auto specs = spz_gatekeeper::ExtensionSpecRegistry::Instance().ListSpecs();
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
  return oss.str();
}



std::string BuildCompatibilityBoardJson() {

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
    const auto valid_fixture = BuildFixtureBlob(spec.type, false);
    const auto invalid_fixture = BuildFixtureBlob(spec.type, true);

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
    oss << ",\"wasm_quality_gate\":"
        << spz_gatekeeper::BuildWasmQualityGateJson(has_validator, !has_validator);

    oss << "}";

  }

  oss << "]}";
  return oss.str();
}

emscripten::val inspectSpz(const emscripten::val& spz_buffer, bool strict) {
  spz_gatekeeper::GateReport rep;
  std::string err;
  if (!TryInspect(spz_buffer, strict, &rep, &err)) {
    emscripten::val out = emscripten::val::object();
    out.set("ok", false);
    out.set("strict", strict);
    out.set("error", err);
    return out;
  }
  emscripten::val result = ParseJsonObject(rep.ToJson());
  result.set("ok", !rep.HasErrors());
  result.set("strict", strict);
  return result;
}

emscripten::val dumpTrailer(const emscripten::val& spz_buffer, bool strict) {
  spz_gatekeeper::GateReport rep;
  std::string err;
  if (!TryInspect(spz_buffer, strict, &rep, &err)) {
    emscripten::val out = emscripten::val::object();
    out.set("ok", false);
    out.set("strict", strict);
    out.set("error", err);
    return out;
  }

  emscripten::val result = emscripten::val::object();
  result.set("asset_path", rep.asset_path);
  result.set("strict", strict);
  result.set("ok", !rep.HasErrors());
  result.set("issues", BuildIssues(rep));

  if (!rep.spz_l2.has_value()) {
    result.set("error", std::string("no l2 info"));
    return result;
  }

  const auto& l2 = rep.spz_l2.value();
  result.set("flags", static_cast<unsigned>(l2.flags));
  result.set("trailer_size", static_cast<double>(l2.trailer_size));

  emscripten::val tlv_records = emscripten::val::array();
  std::size_t idx = 0;
  for (const auto& record : l2.tlv_records) {
    emscripten::val item = emscripten::val::object();
    item.set("type", static_cast<double>(record.type));
    item.set("length", static_cast<double>(record.length));
    item.set("offset", static_cast<double>(record.offset));
    tlv_records.set(static_cast<unsigned>(idx++), item);
  }
  result.set("tlv_records", tlv_records);
  return result;
}

std::string inspectSpzText(const emscripten::val& spz_buffer, bool strict) {
  spz_gatekeeper::GateReport rep;
  std::string err;
  if (!TryInspect(spz_buffer, strict, &rep, &err)) {
    return "error [WASM_INPUT] " + err + " (<wasm>)\n";
  }
  return rep.ToText();
}

emscripten::val inspectCompatSummary(const emscripten::val& spz_buffer) {
  spz_gatekeeper::GateReport strict_report;
  std::string strict_err;
  if (!TryInspect(spz_buffer, true, &strict_report, &strict_err)) {
    emscripten::val out = emscripten::val::object();
    out.set("audit_profile", std::string(spz_gatekeeper::kAuditProfileSpz));
    out.set("audit_mode", std::string(spz_gatekeeper::kAuditModeLocalCliSpzArtifactAudit));
    out.set("verdict", std::string("block"));
    out.set("next_action", std::string("block_artifact"));
    out.set("error", strict_err);
    return out;
  }

  spz_gatekeeper::GateReport non_strict_report;
  std::string non_strict_err;
  if (!TryInspect(spz_buffer, false, &non_strict_report, &non_strict_err)) {
    emscripten::val out = emscripten::val::object();
    out.set("audit_profile", std::string(spz_gatekeeper::kAuditProfileSpz));
    out.set("audit_mode", std::string(spz_gatekeeper::kAuditModeLocalCliSpzArtifactAudit));
    out.set("verdict", std::string("block"));
    out.set("next_action", std::string("block_artifact"));
    out.set("error", non_strict_err);
    return out;
  }

  return ParseJsonObject(
      spz_gatekeeper::BuildCompatCheckAuditJson("<wasm>", strict_report, non_strict_report));
}

emscripten::val buildBrowserAuditReport(const emscripten::val& payload) {
  const emscripten::val summary = payload["summary"];

  spz_gatekeeper::BrowserWasmAuditReport report;
  report.bundle_id = payload["bundle_id"].as<std::string>();
  report.verdict = payload["verdict"].as<std::string>();
  report.next_action = payload["next_action"].as<std::string>();
  report.audit_duration_ms = payload["audit_duration_ms"].as<double>();
  report.summary.bundle_name = summary["bundle_name"].as<std::string>();
  report.summary.file_count = static_cast<std::uint64_t>(summary["file_count"].as<double>());
  report.summary.issue_count = static_cast<std::uint64_t>(summary["issue_count"].as<double>());
  report.summary.declared_export_count =
      static_cast<std::uint64_t>(summary["declared_export_count"].as<double>());
  report.summary.loader_export_count =
      static_cast<std::uint64_t>(summary["loader_export_count"].as<double>());
  report.summary.wasm_export_count =
      static_cast<std::uint64_t>(summary["wasm_export_count"].as<double>());
  report.summary.valid_tiny_passed = summary["valid_tiny_passed"].as<bool>();
  report.summary.invalid_tiny_handled = summary["invalid_tiny_handled"].as<bool>();
  report.summary.runtime_available = summary["runtime_available"].as<bool>();
  report.manifest_summary_json = StringifyJsonValue(payload["manifest_summary"]);
  report.budgets_json = StringifyJsonValue(payload["budgets"]);
  report.issues_json = StringifyJsonValue(payload["issues"]);
  report.bundle_entries_json = StringifyJsonValue(payload["bundle_entries"]);
  report.wasm_export_summary_json = StringifyJsonValue(payload["wasm_export_summary"]);
  report.empty_shell_risk = payload["empty_shell_risk"].as<bool>();
  report.memory_budget_wired = payload["memory_budget_wired"].as<bool>();
  report.performance_budget_wired = payload["performance_budget_wired"].as<bool>();
  return ParseJsonObject(spz_gatekeeper::BuildBrowserWasmAuditJson(report));
}

emscripten::val listRegisteredExtensions() {

  return ParseJsonObject(BuildRegistryListJson());
}


emscripten::val describeExtension(double type_value) {
  const auto type = static_cast<std::uint32_t>(type_value);
  const auto spec = spz_gatekeeper::ExtensionSpecRegistry::Instance().GetSpec(type);
  if (!spec.has_value()) {
    std::ostringstream oss;
    oss << "{\"error\":\"extension not found\",\"type\":" << type << "}";
    return ParseJsonObject(oss.str());
  }
  return ParseJsonObject(RegistryEntryToJson(*spec));
}

emscripten::val getCompatibilityBoard() {
  return ParseJsonObject(BuildCompatibilityBoardJson());
}

}  // namespace

EMSCRIPTEN_BINDINGS(spz_gatekeeper_module) {
  emscripten::function("inspectSpz", &inspectSpz);
  emscripten::function("dumpTrailer", &dumpTrailer);
  emscripten::function("inspectSpzText", &inspectSpzText);
  emscripten::function("inspectCompatSummary", &inspectCompatSummary);
  emscripten::function("buildBrowserAuditReport", &buildBrowserAuditReport);

  emscripten::function("listRegisteredExtensions", &listRegisteredExtensions);

  emscripten::function("describeExtension", &describeExtension);
  emscripten::function("getCompatibilityBoard", &getCompatibilityBoard);
}
