#include "dstore/common/logger.h"

#include <chrono>
#include <iomanip>
#include <sstream>

#ifdef DSTORE_USE_SPDLOG
#include <spdlog/spdlog.h>
#endif

namespace dstore {

Logger& Logger::instance() {
  static Logger logger;
  return logger;
}

void Logger::log(Level level, const std::string& component, const std::string& message) {
#ifdef DSTORE_USE_SPDLOG
  auto formatted = "[" + component + "] " + message;
  if (level == Level::kInfo) spdlog::info(formatted);
  else if (level == Level::kWarn) spdlog::warn(formatted);
  else spdlog::error(formatted);
#else
  const char* label = level == Level::kInfo ? "INFO" : (level == Level::kWarn ? "WARN" : "ERROR");
  auto now = std::chrono::system_clock::now();
  auto t = std::chrono::system_clock::to_time_t(now);
  std::lock_guard lock(mu_);
  std::cerr << "{\"ts\":\"" << std::put_time(std::gmtime(&t), "%FT%TZ") << "\",\"level\":\"" << label
            << "\",\"component\":\"" << component << "\",\"msg\":\"" << message << "\"}\n";
#endif
}

}  // namespace dstore
