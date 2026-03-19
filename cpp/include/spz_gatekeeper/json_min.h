#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace spz_gatekeeper {

enum class JsonType { kNull, kBool, kNumber, kString, kArray, kObject };

struct JsonValue;
using JsonPtr = std::unique_ptr<JsonValue>;

struct JsonValue {
  JsonType type = JsonType::kNull;
  bool bool_value = false;
  double number_value = 0.0;
  std::string string_value;
  std::vector<JsonPtr> array_value;
  std::unordered_map<std::string, JsonPtr> object_value;

  const JsonValue* Find(std::string_view key) const;
};

struct JsonParseError {
  std::string message;
  std::size_t offset = 0;
};

std::optional<JsonValue> ParseJson(std::string_view text, JsonParseError* err);

// Minimal JSON string escaper for report output.
std::string JsonEscape(std::string_view s);

}  // namespace spz_gatekeeper

