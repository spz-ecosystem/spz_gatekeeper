// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PuJunhan

#include "spz_gatekeeper/spz.h"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>


#include <emscripten/bind.h>
#include <emscripten/val.h>

namespace {

constexpr std::size_t kMaxSpzBytes = 512u * 1024u * 1024u;

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
  emscripten::val result = emscripten::val::global("JSON").call<emscripten::val>("parse", rep.ToJson());
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
  for (const auto& r : l2.tlv_records) {
    emscripten::val item = emscripten::val::object();
    item.set("type", static_cast<double>(r.type));
    item.set("length", static_cast<double>(r.length));
    item.set("offset", static_cast<double>(r.offset));
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


}  // namespace

EMSCRIPTEN_BINDINGS(spz_gatekeeper_module) {
  emscripten::function("inspectSpz", &inspectSpz);
  emscripten::function("dumpTrailer", &dumpTrailer);
  emscripten::function("inspectSpzText", &inspectSpzText);
}
