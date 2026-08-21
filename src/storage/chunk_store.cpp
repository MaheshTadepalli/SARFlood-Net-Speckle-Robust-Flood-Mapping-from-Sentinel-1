#include "dstore/storage/chunk_store.h"

#include <fstream>

#include "dstore/common/sha256.h"

namespace dstore {

ChunkStore::ChunkStore(std::filesystem::path root, std::size_t cache_bytes)
    : root_(std::move(root)), cache_(cache_bytes / 4096 + 1) {
  std::filesystem::create_directories(root_);
}

std::filesystem::path ChunkStore::path_for(const std::string& checksum) const {
  return root_ / checksum.substr(0, 2) / checksum;
}

Result<ChunkRef> ChunkStore::put(const std::vector<std::uint8_t>& bytes) {
  auto checksum = Sha256::hex(bytes);
  auto path = path_for(checksum);
  std::lock_guard lock(mu_);
  std::filesystem::create_directories(path.parent_path());
  if (!std::filesystem::exists(path)) {
    auto tmp = path;
    tmp += ".tmp";
    {
      std::ofstream out(tmp, std::ios::binary);
      if (!out) return Status::Internal("failed to open chunk temp file");
      out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
      if (!out) return Status::Internal("failed to write chunk");
    }
    std::filesystem::rename(tmp, path);
  }
  cache_.put(checksum, bytes);
  return ChunkRef{checksum, static_cast<std::uint64_t>(bytes.size())};
}

Result<std::vector<std::uint8_t>> ChunkStore::get(const std::string& checksum) {
  if (auto cached = cache_.get(checksum)) return *cached;
  auto path = path_for(checksum);
  std::ifstream in(path, std::ios::binary);
  if (!in) return Status::NotFound("chunk not found: " + checksum);
  std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  if (Sha256::hex(bytes) != checksum) return Status::Internal("chunk checksum mismatch: " + checksum);
  cache_.put(checksum, bytes);
  return bytes;
}

Result<void> ChunkStore::remove_if_unreferenced(const std::string& checksum, int references) {
  if (references > 0) return {};
  std::lock_guard lock(mu_);
  std::error_code ec;
  std::filesystem::remove(path_for(checksum), ec);
  if (ec) return Status::Internal("failed to remove chunk: " + ec.message());
  return {};
}

bool ChunkStore::exists(const std::string& checksum) const {
  return std::filesystem::exists(path_for(checksum));
}

std::vector<std::vector<std::uint8_t>> chunk_bytes(const std::vector<std::uint8_t>& input, std::size_t chunk_size) {
  std::vector<std::vector<std::uint8_t>> chunks;
  if (chunk_size == 0) chunk_size = 4 * 1024 * 1024;
  for (std::size_t offset = 0; offset < input.size(); offset += chunk_size) {
    auto end = std::min(input.size(), offset + chunk_size);
    chunks.emplace_back(input.begin() + static_cast<std::ptrdiff_t>(offset), input.begin() + static_cast<std::ptrdiff_t>(end));
  }
  if (input.empty()) chunks.emplace_back();
  return chunks;
}

}  // namespace dstore
