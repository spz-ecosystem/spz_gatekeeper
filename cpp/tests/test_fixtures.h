#pragma once

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <utility>
#include <vector>

#include <zlib.h>

namespace spz_gatekeeper_test {

inline void AppendU32Le(std::vector<std::uint8_t>* bytes, std::uint32_t value) {
  bytes->push_back(static_cast<std::uint8_t>(value & 0xFFu));
  bytes->push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFu));
  bytes->push_back(static_cast<std::uint8_t>((value >> 16) & 0xFFu));
  bytes->push_back(static_cast<std::uint8_t>((value >> 24) & 0xFFu));
}

inline std::vector<std::uint8_t> CreateTrailer(
    const std::vector<std::pair<std::uint32_t, std::vector<std::uint8_t>>>& records) {
  std::vector<std::uint8_t> trailer;
  for (const auto& record : records) {
    AppendU32Le(&trailer, record.first);
    AppendU32Le(&trailer, static_cast<std::uint32_t>(record.second.size()));
    trailer.insert(trailer.end(), record.second.begin(), record.second.end());
  }
  return trailer;
}

inline std::vector<std::uint8_t> GzipCompress(const std::vector<std::uint8_t>& uncompressed) {
  std::vector<std::uint8_t> compressed(1024);
  z_stream stream = {};
  if (deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 + 16, 8,
                   Z_DEFAULT_STRATEGY) != Z_OK) {
    throw std::runtime_error("deflateInit failed");
  }

  stream.next_in = const_cast<Bytef*>(uncompressed.data());
  stream.avail_in = static_cast<uInt>(uncompressed.size());
  stream.next_out = compressed.data();
  stream.avail_out = static_cast<uInt>(compressed.size());

  const int result = deflate(&stream, Z_FINISH);
  if (result != Z_STREAM_END) {
    deflateEnd(&stream);
    throw std::runtime_error("deflate failed");
  }

  compressed.resize(stream.total_out);
  deflateEnd(&stream);
  return compressed;
}

inline std::vector<std::uint8_t> CreateMinimalSpz(
    std::uint32_t num_points = 1,
    std::uint8_t version = 1,
    std::uint8_t sh_degree = 0,
    std::uint8_t fractional_bits = 8,
    std::uint8_t flags = 0x00,
    const std::vector<std::uint8_t>* trailer = nullptr) {
  std::vector<std::uint8_t> uncompressed;
  uncompressed.reserve(64);

  AppendU32Le(&uncompressed, 0x5053474Eu);
  AppendU32Le(&uncompressed, version);
  AppendU32Le(&uncompressed, num_points);
  uncompressed.push_back(sh_degree);
  uncompressed.push_back(fractional_bits);
  uncompressed.push_back(flags);
  uncompressed.push_back(0x00);

  const bool uses_float16 = (version == 1);
  const bool uses_quat_smallest_three = (version >= 3);
  const auto sh_dim_for_degree = [](std::uint8_t degree) -> int {
    switch (degree) {
      case 0: return 0;
      case 1: return 3;
      case 2: return 8;
      case 3: return 15;
      case 4: return 24;
      default: return 0;
    }
  };

  const int positions_bytes_per_point = uses_float16 ? 6 : 9;
  const int rotations_bytes_per_point = uses_quat_smallest_three ? 4 : 3;
  const int sh_bytes_per_point = sh_dim_for_degree(sh_degree) * 3;

  uncompressed.insert(uncompressed.end(), num_points * positions_bytes_per_point, 0);
  uncompressed.insert(uncompressed.end(), num_points, 255);
  uncompressed.insert(uncompressed.end(), num_points * 3, 0);
  uncompressed.insert(uncompressed.end(), num_points * 3, 0);
  uncompressed.insert(uncompressed.end(), num_points * rotations_bytes_per_point, 0);
  uncompressed.insert(uncompressed.end(), num_points * sh_bytes_per_point, 0);

  if (trailer != nullptr) {
    uncompressed.insert(uncompressed.end(), trailer->begin(), trailer->end());
  }

  return GzipCompress(uncompressed);
}

inline std::vector<std::uint8_t> WriteFloat(float value) {
  std::vector<std::uint8_t> bytes(sizeof(float));
  std::memcpy(bytes.data(), &value, sizeof(float));
  return bytes;
}

inline std::vector<std::uint8_t> CreateAdobeSafeOrbitPayload(float min_elevation = -0.5f,
                                                             float max_elevation = 0.5f,
                                                             float min_radius = 1.0f) {
  std::vector<std::uint8_t> payload;
  const auto min_bytes = WriteFloat(min_elevation);
  const auto max_bytes = WriteFloat(max_elevation);
  const auto radius_bytes = WriteFloat(min_radius);
  payload.insert(payload.end(), min_bytes.begin(), min_bytes.end());
  payload.insert(payload.end(), max_bytes.begin(), max_bytes.end());
  payload.insert(payload.end(), radius_bytes.begin(), radius_bytes.end());
  return payload;
}

}  // namespace spz_gatekeeper_test
