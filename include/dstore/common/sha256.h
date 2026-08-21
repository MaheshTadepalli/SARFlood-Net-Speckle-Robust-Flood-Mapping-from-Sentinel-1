#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace dstore {

class Sha256 {
 public:
  Sha256();
  void update(std::span<const std::uint8_t> bytes);
  std::array<std::uint8_t, 32> digest();
  static std::string hex(std::span<const std::uint8_t> bytes);
  static std::string hex(const std::string& bytes);

 private:
  void transform(const std::uint8_t* chunk);
  std::uint32_t state_[8];
  std::uint64_t bit_len_;
  std::vector<std::uint8_t> buffer_;
};

}  // namespace dstore
