#pragma once
#include <cstddef>
#include <string>
namespace hpl {
enum class HttpMethod : unsigned {
  GET = 0,
  POST,
  PUT,
  DELETE,
  HEAD,
  CONNECT,
  OPTIONS,
  TRACE,
  PATCH,
  UNKNOWN, // make sure this is the last one
};

HttpMethod ParseHttpMethod(std::string_view method, size_t pos,
                           size_t &end_pos);
} // namespace hpl