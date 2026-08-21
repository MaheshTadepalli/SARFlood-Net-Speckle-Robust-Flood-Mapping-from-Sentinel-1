#pragma once

#include <iostream>
#include <mutex>
#include <string>

namespace dstore {

class Logger {
 public:
  enum class Level { kInfo, kWarn, kError };
  static Logger& instance();
  void log(Level level, const std::string& component, const std::string& message);

 private:
  std::mutex mu_;
};

#define DSTORE_LOG_INFO(component, message) ::dstore::Logger::instance().log(::dstore::Logger::Level::kInfo, component, message)
#define DSTORE_LOG_WARN(component, message) ::dstore::Logger::instance().log(::dstore::Logger::Level::kWarn, component, message)
#define DSTORE_LOG_ERROR(component, message) ::dstore::Logger::instance().log(::dstore::Logger::Level::kError, component, message)

}  // namespace dstore
