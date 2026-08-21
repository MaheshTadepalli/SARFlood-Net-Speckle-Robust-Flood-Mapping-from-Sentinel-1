#pragma once

#include <stdexcept>
#include <string>
#include <utility>
#include <variant>

namespace dstore {

class Status {
 public:
  enum class Code { kOk, kInvalidArgument, kUnauthenticated, kPermissionDenied, kNotFound, kConflict, kUnavailable, kInternal };

  Status() : code_(Code::kOk) {}
  Status(Code code, std::string message) : code_(code), message_(std::move(message)) {}

  static Status Ok() { return {}; }
  static Status Invalid(std::string message) { return {Code::kInvalidArgument, std::move(message)}; }
  static Status Unauthenticated(std::string message) { return {Code::kUnauthenticated, std::move(message)}; }
  static Status PermissionDenied(std::string message) { return {Code::kPermissionDenied, std::move(message)}; }
  static Status NotFound(std::string message) { return {Code::kNotFound, std::move(message)}; }
  static Status Conflict(std::string message) { return {Code::kConflict, std::move(message)}; }
  static Status Unavailable(std::string message) { return {Code::kUnavailable, std::move(message)}; }
  static Status Internal(std::string message) { return {Code::kInternal, std::move(message)}; }

  bool ok() const { return code_ == Code::kOk; }
  Code code() const { return code_; }
  const std::string& message() const { return message_; }

 private:
  Code code_;
  std::string message_;
};

template <typename T>
class Result {
 public:
  Result(T value) : data_(std::move(value)) {}
  Result(Status status) : data_(std::move(status)) {
    if (std::get<Status>(data_).ok()) throw std::invalid_argument("non-value Result cannot hold OK");
  }

  bool ok() const { return std::holds_alternative<T>(data_); }
  const T& value() const { return std::get<T>(data_); }
  T& value() { return std::get<T>(data_); }
  const Status& status() const {
    static const Status ok_status;
    return ok() ? ok_status : std::get<Status>(data_);
  }

 private:
  std::variant<T, Status> data_;
};

template <>
class Result<void> {
 public:
  Result() : status_(Status::Ok()) {}
  Result(Status status) : status_(std::move(status)) {}
  bool ok() const { return status_.ok(); }
  const Status& status() const { return status_; }

 private:
  Status status_;
};

}  // namespace dstore
