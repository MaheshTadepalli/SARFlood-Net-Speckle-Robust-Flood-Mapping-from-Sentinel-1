#include "dstore/common/metrics.h"

#include <sstream>

namespace dstore {

void Metrics::record_upload(std::uint64_t bytes) {
  uploads_++;
  bytes_in_ += bytes;
}

void Metrics::record_download(std::uint64_t bytes) {
  downloads_++;
  bytes_out_ += bytes;
}

void Metrics::record_delete() { deletes_++; }

void Metrics::record_error() { errors_++; }

std::string Metrics::prometheus() const {
  std::ostringstream out;
  out << "dstore_uploads_total " << uploads_.load() << "\n";
  out << "dstore_downloads_total " << downloads_.load() << "\n";
  out << "dstore_deletes_total " << deletes_.load() << "\n";
  out << "dstore_errors_total " << errors_.load() << "\n";
  out << "dstore_bytes_in_total " << bytes_in_.load() << "\n";
  out << "dstore_bytes_out_total " << bytes_out_.load() << "\n";
  return out.str();
}

}  // namespace dstore
