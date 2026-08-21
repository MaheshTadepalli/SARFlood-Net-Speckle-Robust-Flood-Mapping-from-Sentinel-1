#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "dstore/common/result.h"

namespace dstore {

struct WireFrame {
  std::uint16_t type = 0;
  std::string correlation_id;
  std::vector<std::uint8_t> payload;
};

std::vector<std::uint8_t> encode_frame(const WireFrame& frame);
Result<WireFrame> decode_frame(const std::vector<std::uint8_t>& bytes);

}  // namespace dstore
