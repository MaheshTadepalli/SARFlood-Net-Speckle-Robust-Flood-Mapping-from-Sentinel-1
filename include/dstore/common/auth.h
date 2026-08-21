#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>

#include "dstore/common/result.h"

namespace dstore {

enum class Permission { kRead, kWrite, kDelete, kAdmin };

class AuthManager {
 public:
  void add_token(std::string token, std::unordered_set<Permission> permissions);
  Result<void> authorize(const std::string& token, Permission permission) const;

 private:
  std::unordered_map<std::string, std::unordered_set<Permission>> tokens_;
};

}  // namespace dstore
