#pragma once

#include <iterator>
#include <map>
#include <string>
#include <vector>

#include <fmt/core.h>

#include "hpl_version.h"

namespace hpl {

static const std::vector<std::vector<std::string>> kStatusMessages = {
    {
        "Continue",            // 100
        "Switching Protocols", // 101
        "Processing",          // 102
        "Early Hints",         // 103
    },
    {
        "OK",                            // 200
        "Created",                       // 201
        "Accepted",                      // 202
        "Non-Authoritative Information", // 203
        "No Content",                    // 204
        "Reset Content",                 // 205
        "Partial Content",               // 206
        "Multi-Status",                  // 207
        "Already Reported",              // 208
    },
    {
        "Multiple Choices",   // 300
        "Moved Permanently",  // 301
        "Found",              // 302
        "See Other",          // 303
        "Not Modified",       // 304
        "Use Proxy",          // 305
        "(Unused)",           // 306
        "Temporary Redirect", // 307
        "Permanent Redirect", // 308
    },
    {
        "Bad Request",                   // 400
        "Unauthorized",                  // 401
        "Payment Required",              // 402
        "Forbidden",                     // 403
        "Not Found",                     // 404
        "Method Not Allowed",            // 405
        "Not Acceptable",                // 406
        "Proxy Authentication Required", // 407
        "Request Timeout",               // 408
        "Conflict",                      // 409
        "Gone",                          // 410
        "Length Required",               // 411
        "Precondition Failed",           // 412
    },
    {
        "Internal Server Error",           // 500
        "Not Implemented",                 // 501
        "Bad Gateway",                     // 502
        "Service Unavailable",             // 503
        "Gateway Timeout",                 // 504
        "HTTP Version Not Supported",      // 505
        "Variant Also Neogtiates",         // 506
        "Insufficient Storage",            // 507
        "Loop Detected",                   // 508
        "(Unused)",                        // 509
        "Not Extended",                    // 510
        "Network Authentication Required", // 511
    }};

inline std::string
MakeResponse(int status_code, HttpVersion version,
             const std::map<std::string_view, std::string_view> &headers,
             const std::string &body = "") {
  std::string ret;
  if (status_code < 100 || status_code > 599) {
    status_code = 500;
  }
  if (kStatusMessages.at(status_code / 100 - 1).size() <= status_code % 100) {
    status_code = 500;
  }

  fmt::format_to(
      std::back_inserter(ret), "{} {} {}\r\n", HttpVersionToString(version),
      status_code,
      kStatusMessages.at(status_code / 100 - 1).at(status_code % 100));
  for (const auto &[key, value] : headers) {
    fmt::format_to(std::back_inserter(ret), "{}: {}\r\n", key, value);
  }
  if (!body.empty()) {
    fmt::format_to(std::back_inserter(ret), "Content-Length: {}\r\n\r\n",
                   body.size());
    fmt::format_to(std::back_inserter(ret), "{}", body);
  } else {
    fmt::format_to(std::back_inserter(ret), "\r\n");
  }
  return ret;
}

} // namespace hpl