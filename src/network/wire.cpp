#include "dstore/network/wire.h"

namespace dstore {
namespace {
void put_u16(std::vector<std::uint8_t>& out, std::uint16_t v) {
  out.push_back(static_cast<std::uint8_t>(v >> 8));
  out.push_back(static_cast<std::uint8_t>(v));
}
void put_u32(std::vector<std::uint8_t>& out, std::uint32_t v) {
  out.push_back(static_cast<std::uint8_t>(v >> 24));
  out.push_back(static_cast<std::uint8_t>(v >> 16));
  out.push_back(static_cast<std::uint8_t>(v >> 8));
  out.push_back(static_cast<std::uint8_t>(v));
}
std::uint16_t get_u16(const std::vector<std::uint8_t>& b, std::size_t o) {
  return static_cast<std::uint16_t>((b[o] << 8) | b[o + 1]);
}
std::uint32_t get_u32(const std::vector<std::uint8_t>& b, std::size_t o) {
  return (static_cast<std::uint32_t>(b[o]) << 24) | (static_cast<std::uint32_t>(b[o + 1]) << 16) |
         (static_cast<std::uint32_t>(b[o + 2]) << 8) | static_cast<std::uint32_t>(b[o + 3]);
}
}  // namespace

std::vector<std::uint8_t> encode_frame(const WireFrame& frame) {
  std::vector<std::uint8_t> out;
  out.reserve(12 + frame.correlation_id.size() + frame.payload.size());
  put_u16(out, frame.type);
  put_u16(out, static_cast<std::uint16_t>(frame.correlation_id.size()));
  put_u32(out, static_cast<std::uint32_t>(frame.payload.size()));
  out.insert(out.end(), frame.correlation_id.begin(), frame.correlation_id.end());
  out.insert(out.end(), frame.payload.begin(), frame.payload.end());
  return out;
}

Result<WireFrame> decode_frame(const std::vector<std::uint8_t>& bytes) {
  if (bytes.size() < 8) return Status::Invalid("wire frame too short");
  WireFrame frame;
  frame.type = get_u16(bytes, 0);
  auto cid_len = get_u16(bytes, 2);
  auto payload_len = get_u32(bytes, 4);
  if (bytes.size() != 8u + cid_len + payload_len) return Status::Invalid("wire frame length mismatch");
  frame.correlation_id.assign(bytes.begin() + 8, bytes.begin() + 8 + cid_len);
  frame.payload.assign(bytes.begin() + 8 + cid_len, bytes.end());
  return frame;
}

}  // namespace dstore
