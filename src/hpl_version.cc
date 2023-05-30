#include "hpl_version.h"
#include "hpl_logger.h"

namespace {
constexpr int IntHTTP = 'H' | 'T' << 8 | 'T' << 16 | 'P' << 24;
constexpr int IntWS = 'W' | 'S' << 8 | 'S' << 16 | '/' << 24;
constexpr int Int1_0 = '1' | '.' << 8 | '0' << 16;
constexpr int Int1_1 = '1' | '.' << 8 | '1' << 16;
constexpr int Int2_0 = '2' | '.' << 8 | '0' << 16;

hpl::HttpVersion HttpVersion(std::string_view version, size_t &end_pos) {
  int v = *(int *)(version.data()) & 0x00ffffff;
  if (v == Int1_0) {
    end_pos = 3;
    return hpl::HttpVersion::HTTP_1_0;
  } else if (v == Int1_1) {
    end_pos = 3;
    return hpl::HttpVersion::HTTP_1_1;
  } else if (v == Int2_0) {
    end_pos = 3;
    return hpl::HttpVersion::HTTP_2_0;
  } else {
    return hpl::HttpVersion::UNKNOWN;
  }
}

hpl::HttpVersion WsVersion(std::string_view version, size_t &end_pos) {
  return hpl::HttpVersion::WS;
}
} // namespace

namespace hpl {
HttpVersion ParseHttpVersion(std::string_view version, size_t &end_pos) {
  int v = *(int *)(version.data());
  if (v == IntHTTP && version.length() > 4 && version[4] == '/') {
    return ::HttpVersion(version.data() + 5, end_pos);
  }
  if (v == IntWS) {
    return ::WsVersion(version.data() + 4, end_pos);
  }
  return HttpVersion::UNKNOWN;
}

std::string HttpVersionToString(HttpVersion version) {
  switch (version) {
  case HttpVersion::HTTP_1_0:
    return "HTTP/1.0";
  case HttpVersion::HTTP_1_1:
    return "HTTP/1.1";
  case HttpVersion::HTTP_2_0:
    return "HTTP/2.0";
  case HttpVersion::WS:
    return "WS";
  default:
    return "UNKNOWN";
  }
}
} // namespace hpl