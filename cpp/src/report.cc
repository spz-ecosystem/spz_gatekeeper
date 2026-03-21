// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PuJunhan

#include "spz_gatekeeper/report.h"

#include "spz_gatekeeper/json_min.h"

#include <sstream>

namespace spz_gatekeeper {

bool GateReport::HasErrors() const {
  for (const auto& it : issues) {
    if (it.severity == Severity::kError) return true;
  }
  return false;
}

static const char* SevToStr(Severity s) {
  switch (s) {
    case Severity::kError: return "error";
    case Severity::kWarning: return "warning";
    case Severity::kNote: return "note";
  }
  return "note";
}

std::string GateReport::ToJson() const {
  std::ostringstream oss;
  oss << "{";
  oss << "\"asset_path\":\"" << JsonEscape(asset_path) << "\"";

  oss << ",\"issues\":[";
  for (std::size_t i = 0; i < issues.size(); ++i) {
    if (i) oss << ",";
    const auto& it = issues[i];
    oss << "{";
    oss << "\"severity\":\"" << SevToStr(it.severity) << "\"";
    oss << ",\"code\":\"" << JsonEscape(it.code) << "\"";
    oss << ",\"message\":\"" << JsonEscape(it.message) << "\"";
    oss << ",\"where\":\"" << JsonEscape(it.where) << "\"";
    oss << "}";
  }
  oss << "]";

  oss << ",\"extension_reports\":[";
  for (std::size_t i = 0; i < extension_reports.size(); ++i) {
    if (i) oss << ",";
    const auto& er = extension_reports[i];
    oss << "{";
    oss << "\"type\":" << er.type;
    oss << ",\"vendor_name\":\"" << JsonEscape(er.vendor_name) << "\"";
    oss << ",\"extension_name\":\"" << JsonEscape(er.extension_name) << "\"";
    oss << ",\"known_extension\":" << (er.known_extension ? "true" : "false");
    oss << ",\"has_validator\":" << (er.has_validator ? "true" : "false");
    oss << ",\"status\":\"" << JsonEscape(er.status) << "\"";
    oss << ",\"category\":\"" << JsonEscape(er.category) << "\"";
    oss << ",\"spec_url\":\"" << JsonEscape(er.spec_url) << "\"";
    oss << ",\"short_description\":\"" << JsonEscape(er.short_description) << "\"";
    oss << ",\"validation_result\":" << (er.validation_result ? "true" : "false");
    oss << ",\"error_message\":\"" << JsonEscape(er.error_message) << "\"";
    oss << "}";

  }
  oss << "]";

  if (spz_l2.has_value()) {
    const auto& l2 = spz_l2.value();
    oss << ",\"spz_l2\":{";
    oss << "\"header_ok\":" << (l2.header_ok ? "true" : "false");
    oss << ",\"version\":" << l2.version;
    oss << ",\"num_points\":" << l2.num_points;
    oss << ",\"sh_degree\":" << static_cast<unsigned>(l2.sh_degree);
    oss << ",\"flags\":" << static_cast<unsigned>(l2.flags);
    oss << ",\"reserved\":" << static_cast<unsigned>(l2.reserved);
    oss << ",\"decompressed_size\":" << l2.decompressed_size;
    oss << ",\"base_payload_size\":" << l2.base_payload_size;
    oss << ",\"trailer_size\":" << l2.trailer_size;
    oss << ",\"tlv_records\":[";
    for (std::size_t i = 0; i < l2.tlv_records.size(); ++i) {
      if (i) oss << ",";
      const auto& r = l2.tlv_records[i];
      oss << "{";
      oss << "\"type\":" << r.type;
      oss << ",\"length\":" << r.length;
      oss << ",\"offset\":" << r.offset;
      oss << "}";
    }
    oss << "]}";
  }

  oss << "}";
  return oss.str();
}

std::string GateReport::ToText() const {
  std::ostringstream oss;
  oss << "asset: " << asset_path << "\n";
  for (const auto& it : issues) {
    oss << SevToStr(it.severity) << " [" << it.code << "] " << it.message;
    if (!it.where.empty()) oss << " (" << it.where << ")";
    oss << "\n";
  }

  for (const auto& er : extension_reports) {
    oss << "ext type=" << er.type
        << " vendor=\"" << er.vendor_name << "\""
        << " name=\"" << er.extension_name << "\""
        << " known=" << (er.known_extension ? "true" : "false")
        << " has_validator=" << (er.has_validator ? "true" : "false")
        << " status=\"" << er.status << "\""
        << " category=\"" << er.category << "\""
        << " valid=" << (er.validation_result ? "true" : "false");
    if (!er.spec_url.empty()) {
      oss << " spec_url=\"" << er.spec_url << "\"";
    }
    if (!er.short_description.empty()) {
      oss << " description=\"" << er.short_description << "\"";
    }
    if (!er.error_message.empty()) {
      oss << " error=\"" << er.error_message << "\"";
    }
    oss << "\n";
  }

  return oss.str();
}

void AddIssue(GateReport* report, Severity sev, std::string_view code, std::string_view msg,
              std::string_view where) {
  Issue it;
  it.severity = sev;
  it.code = std::string(code);
  it.message = std::string(msg);
  it.where = std::string(where);
  report->issues.push_back(std::move(it));
}

}  // namespace spz_gatekeeper
