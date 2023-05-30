#pragma once

#include <map>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "hpl_method.h"
#include "hpl_version.h"
namespace hpl {
class HttpHeaderParser {
public:
  using ParserState = enum ParserState { FirstLine, Headers, Body, Done };

  using ConnectionFlags = enum ConnectionFlags {
    ConnectionClose = 1,
    ConnectionKeepAlive = 2,
    ConnectionUpgrade = 4
  };

  using UpgradeFlags = enum UpgradeFlags {
    UpgradeWebSocket = 1,
  };
  ParserState PushLine(const std::string &line);

  inline ParserState GetState() const { return state_; }
  inline HttpMethod GetMethod() const { return method_; }
  inline std::string_view GetUri() const { return uri_; }
  inline HttpVersion GetVersion() const { return version_; }
  inline std::string_view GetHost() const { return host_; }
  inline std::string_view GetUserAgent() const { return user_agent_; }
  inline const std::map<std::string, std::string> &GetHeaders() const {
    return headers_;
  }
  inline std::string_view GetHeader(const std::string &key) const {
    auto it = headers_.find(key);
    if (it == headers_.end()) {
      return std::string_view();
    }
    // return std::string_view(it->second.data(), it->second.size());
    return it->second;
  }
  inline unsigned GetConnectionFlags() const { return connection_flags_; }
  inline const std::optional<unsigned> &GetContentLength() const {
    return content_length_;
  }
  inline unsigned GetUpgradeFlags() const { return upgrade_flags_; }

private:
  // parse results
  HttpMethod method_ = HttpMethod::UNKNOWN;
  std::string uri_;
  HttpVersion version_ = HttpVersion::UNKNOWN;
  std::map<std::string, std::string> headers_;

  unsigned connection_flags_ = 0;
  unsigned upgrade_flags_ = 0;

  // special headers
  std::string host_;
  std::string connection_;
  std::string user_agent_;
  std::optional<unsigned> content_length_;

  unsigned body_length_ = 0;

private:
  ParserState state_ = ParserState::FirstLine;
  std::tuple<HttpMethod, std::string_view, HttpVersion>
  ParseFirstLine(std::string_view line);
};
} // namespace hpl