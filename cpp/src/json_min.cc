// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PuJunhan

#include "spz_gatekeeper/json_min.h"

#include <charconv>
#include <cctype>
#include <cstdio>
#include <cstdlib>


namespace spz_gatekeeper {

const JsonValue* JsonValue::Find(std::string_view key) const {
  if (type != JsonType::kObject) return nullptr;
  auto it = object_value.find(std::string(key));
  if (it == object_value.end()) return nullptr;
  return it->second.get();
}


namespace {

struct Parser {
  std::string_view s;
  std::size_t i = 0;
  JsonParseError* err = nullptr;

  void SkipWs() {
    while (i < s.size()) {
      unsigned char c = static_cast<unsigned char>(s[i]);
      if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
        i++;
      } else {
        break;
      }
    }
  }

  bool Fail(std::string msg) {
    if (err) {
      err->message = std::move(msg);
      err->offset = i;
    }
    return false;
  }

  static void AppendUtf8(std::string* out, std::uint32_t cp) {
    if (cp <= 0x7F) {
      out->push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FF) {
      out->push_back(static_cast<char>(0xC0 | (cp >> 6)));
      out->push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0xFFFF) {
      out->push_back(static_cast<char>(0xE0 | (cp >> 12)));
      out->push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
      out->push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
      out->push_back(static_cast<char>(0xF0 | (cp >> 18)));
      out->push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
      out->push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
      out->push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
  }

  bool ParseHex4(std::uint32_t* out_cp) {
    if (i + 4 > s.size()) return false;
    std::uint32_t cp = 0;
    for (int k = 0; k < 4; k++) {
      char c = s[i + k];
      cp <<= 4;
      if (c >= '0' && c <= '9') cp |= static_cast<std::uint32_t>(c - '0');
      else if (c >= 'a' && c <= 'f') cp |= static_cast<std::uint32_t>(c - 'a' + 10);
      else if (c >= 'A' && c <= 'F') cp |= static_cast<std::uint32_t>(c - 'A' + 10);
      else return false;
    }
    *out_cp = cp;
    i += 4;
    return true;
  }

  bool ParseString(std::string* out) {
    if (i >= s.size() || s[i] != '"') return Fail("expected string");
    i++;
    out->clear();
    while (i < s.size()) {
      char c = s[i++];
      if (c == '"') return true;
      if (static_cast<unsigned char>(c) < 0x20) return Fail("control char in string");
      if (c != '\\') {
        out->push_back(c);
        continue;
      }
      if (i >= s.size()) return Fail("unterminated escape");
      char e = s[i++];
      switch (e) {
        case '"': out->push_back('"'); break;
        case '\\': out->push_back('\\'); break;
        case '/': out->push_back('/'); break;
        case 'b': out->push_back('\b'); break;
        case 'f': out->push_back('\f'); break;
        case 'n': out->push_back('\n'); break;
        case 'r': out->push_back('\r'); break;
        case 't': out->push_back('\t'); break;
        case 'u': {
          std::uint32_t cp = 0;
          if (!ParseHex4(&cp)) return Fail("bad \\uXXXX escape");
          // handle surrogate pair
          if (cp >= 0xD800 && cp <= 0xDBFF) {
            if (i + 2 <= s.size() && s[i] == '\\' && s[i + 1] == 'u') {
              i += 2;
              std::uint32_t cp2 = 0;
              if (!ParseHex4(&cp2)) return Fail("bad low surrogate");
              if (cp2 >= 0xDC00 && cp2 <= 0xDFFF) {
                std::uint32_t hi = cp - 0xD800;
                std::uint32_t lo = cp2 - 0xDC00;
                std::uint32_t full = 0x10000 + ((hi << 10) | lo);
                AppendUtf8(out, full);
                break;
              }
              return Fail("invalid surrogate pair");
            }
            return Fail("missing low surrogate");
          }
          AppendUtf8(out, cp);
          break;
        }
        default:
          return Fail("unknown escape");
      }
    }
    return Fail("unterminated string");
  }

  bool ParseNumber(double* out_num) {
    std::size_t start = i;
    if (i < s.size() && (s[i] == '-' )) i++;
    if (i >= s.size()) return Fail("bad number");
    if (s[i] == '0') {
      i++;
    } else {
      if (!(s[i] >= '1' && s[i] <= '9')) return Fail("bad number");
      while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i])) != 0) i++;
    }
    if (i < s.size() && s[i] == '.') {
      i++;
      if (i >= s.size() || std::isdigit(static_cast<unsigned char>(s[i])) == 0) return Fail("bad fraction");
      while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i])) != 0) i++;
    }
    if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
      i++;
      if (i < s.size() && (s[i] == '+' || s[i] == '-')) i++;
      if (i >= s.size() || std::isdigit(static_cast<unsigned char>(s[i])) == 0) return Fail("bad exponent");
      while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i])) != 0) i++;
    }
    std::string tmp(s.substr(start, i - start));
    char* endp = nullptr;
    double v = std::strtod(tmp.c_str(), &endp);
    if (endp == tmp.c_str()) return Fail("bad number");
    *out_num = v;
    return true;
  }

  bool ParseValue(JsonValue* out) {
    SkipWs();
    if (i >= s.size()) return Fail("unexpected EOF");
    char c = s[i];
    if (c == 'n') {
      if (s.substr(i, 4) != "null") return Fail("bad literal");
      i += 4;
      out->type = JsonType::kNull;
      return true;
    }
    if (c == 't') {
      if (s.substr(i, 4) != "true") return Fail("bad literal");
      i += 4;
      out->type = JsonType::kBool;
      out->bool_value = true;
      return true;
    }
    if (c == 'f') {
      if (s.substr(i, 5) != "false") return Fail("bad literal");
      i += 5;
      out->type = JsonType::kBool;
      out->bool_value = false;
      return true;
    }
    if (c == '"') {
      out->type = JsonType::kString;
      return ParseString(&out->string_value);
    }
    if (c == '[') {
      i++;
      out->type = JsonType::kArray;
      out->array_value.clear();
      SkipWs();
      if (i < s.size() && s[i] == ']') {
        i++;
        return true;
      }
      while (true) {
        auto v = std::make_unique<JsonValue>();
        if (!ParseValue(v.get())) return false;
        out->array_value.push_back(std::move(v));
        SkipWs();
        if (i >= s.size()) return Fail("unterminated array");
        if (s[i] == ',') {
          i++;
          continue;
        }
        if (s[i] == ']') {
          i++;
          break;
        }
        return Fail("expected ',' or ']'");
      }
      return true;
    }
    if (c == '{') {
      i++;
      out->type = JsonType::kObject;
      out->object_value.clear();
      SkipWs();
      if (i < s.size() && s[i] == '}') {
        i++;
        return true;
      }
      while (true) {
        SkipWs();
        std::string key;
        if (!ParseString(&key)) return false;
        SkipWs();
        if (i >= s.size() || s[i] != ':') return Fail("expected ':'");
        i++;
        auto v = std::make_unique<JsonValue>();
        if (!ParseValue(v.get())) return false;
        out->object_value.emplace(std::move(key), std::move(v));
        SkipWs();
        if (i >= s.size()) return Fail("unterminated object");
        if (s[i] == ',') {
          i++;
          continue;
        }
        if (s[i] == '}') {
          i++;
          break;
        }
        return Fail("expected ',' or '}'");
      }
      return true;
    }


    // number
    out->type = JsonType::kNumber;
    return ParseNumber(&out->number_value);
  }
};

}  // namespace

std::optional<JsonValue> ParseJson(std::string_view text, JsonParseError* err) {
  if (err) {
    err->message.clear();
    err->offset = 0;
  }
  Parser p;
  p.s = text;
  p.i = 0;
  p.err = err;

  JsonValue root;
  if (!p.ParseValue(&root)) return std::nullopt;
  p.SkipWs();
  if (p.i != text.size()) {
    if (err) {
      err->message = "trailing characters";
      err->offset = p.i;
    }
    return std::nullopt;
  }
  return root;
}

std::string JsonEscape(std::string_view s) {
  std::string out;
  out.reserve(s.size() + 8);
  for (char c : s) {
    switch (c) {
      case '\\': out += "\\\\"; break;
      case '"': out += "\\\""; break;
      case '\b': out += "\\b"; break;
      case '\f': out += "\\f"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          char buf[7];
          std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned>(static_cast<unsigned char>(c)));
          out += buf;
        } else {
          out.push_back(c);
        }
    }
  }
  return out;
}

}  // namespace spz_gatekeeper
