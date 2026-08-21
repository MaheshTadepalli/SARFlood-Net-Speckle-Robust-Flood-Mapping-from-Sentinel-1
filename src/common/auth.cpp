#include "dstore/common/auth.h"

namespace dstore {

void AuthManager::add_token(std::string token, std::unordered_set<Permission> permissions) {
  tokens_[std::move(token)] = std::move(permissions);
}

Result<void> AuthManager::authorize(const std::string& token, Permission permission) const {
  auto it = tokens_.find(token);
  if (it == tokens_.end()) return Status::Unauthenticated("missing or invalid token");
  const auto& perms = it->second;
  if (perms.contains(Permission::kAdmin) || perms.contains(permission)) return {};
  return Status::PermissionDenied("token lacks required permission");
}

}  // namespace dstore
