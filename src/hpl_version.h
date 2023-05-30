#pragma once

#include <string>
namespace hpl {
enum HttpVersion {
  HTTP_1_0,
  HTTP_1_1,
  HTTP_2_0,
  WS,
  UNKNOWN,
};

HttpVersion ParseHttpVersion(std::string_view version, size_t &end_pos);
std::string HttpVersionToString(HttpVersion version);
} // namespace hpl