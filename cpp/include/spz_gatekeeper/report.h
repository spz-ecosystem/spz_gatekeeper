// SPDX-License-Identifier: MIT
// Copyright (c) 2026 PuJunhan
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace spz_gatekeeper {

enum class Severity {
  kError,
  kWarning,
  kNote
};

struct Issue {
  Severity severity;
  std::string code;
  std::string message;
  std::string where;
};

struct ByteView {
  const std::uint8_t* data_ptr = nullptr;
  std::size_t size_bytes = 0;

  const std::uint8_t* data() const { return data_ptr; }
  std::size_t size() const { return size_bytes; }
  bool empty() const { return size_bytes == 0; }
  const std::uint8_t& operator[](std::size_t idx) const { return data_ptr[idx]; }
  const std::uint8_t* begin() const { return data_ptr; }
  const std::uint8_t* end() const { return data_ptr == nullptr ? nullptr : data_ptr + size_bytes; }
};

struct TlvRecord {
  std::uint32_t type = 0;
  std::uint32_t length = 0;
  std::size_t offset = 0;
  const std::uint8_t* value_data = nullptr;

  // TLV payload 默认只保留零拷贝视图；只有调用方显式需要独立字节时才复制。
  ByteView ValueView() const { return ByteView{value_data, static_cast<std::size_t>(length)}; }
  std::vector<std::uint8_t> CopyValue() const {
    if (value_data == nullptr || length == 0) {
      return {};
    }
    return std::vector<std::uint8_t>(value_data, value_data + length);
  }
};

struct SpzL2Info {
  bool header_ok = false;
  std::uint32_t version = 0;
  std::uint32_t num_points = 0;
  std::uint8_t sh_degree = 0;
  std::uint8_t flags = 0;
  std::uint8_t reserved = 0;
  std::size_t decompressed_size = 0;
  std::size_t base_payload_size = 0;
  std::size_t trailer_size = 0;
  std::vector<TlvRecord> tlv_records;
  // 返回报告后需要继续读取 TLV payload 时，由这份 trailer backing storage 保证视图有效。
  std::vector<std::uint8_t> tlv_storage;
};

struct ExtensionReport {
  std::uint32_t type = 0;
  std::string vendor_name;
  std::string extension_name;
  bool validation_result = false;
  std::string error_message;
};

struct GateReport {
  std::string asset_path;
  std::vector<Issue> issues;
  std::optional<SpzL2Info> spz_l2;
  std::vector<ExtensionReport> extension_reports;

  bool HasErrors() const;
  std::string ToJson() const;
  std::string ToText() const;
};

void AddIssue(GateReport* report, Severity sev, std::string_view code, std::string_view msg,
              std::string_view where = "");

}  // namespace spz_gatekeeper
