#include <chrono>
#include <iostream>
#include <random>

#include "dstore/storage/chunk_store.h"

int main() {
  std::vector<std::uint8_t> data(128 * 1024 * 1024);
  std::mt19937 rng(7);
  for (auto& b : data) b = static_cast<std::uint8_t>(rng());
  auto start = std::chrono::steady_clock::now();
  auto chunks = dstore::chunk_bytes(data, 4 * 1024 * 1024);
  auto end = std::chrono::steady_clock::now();
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
  std::cout << "chunked_bytes=" << data.size() << "\nchunks=" << chunks.size() << "\nelapsed_ms=" << ms << "\n";
  if (ms > 0) {
    double mib = static_cast<double>(data.size()) / (1024.0 * 1024.0);
    std::cout << "throughput_mib_per_sec=" << (mib * 1000.0 / static_cast<double>(ms)) << "\n";
  }
}
