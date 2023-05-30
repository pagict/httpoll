#include "hpl_header_parser.h"
#include <iterator>
#include <string_view>

#include "hpl_logger.h"
#include "hpl_str.h"

namespace {
static const std::string_view kContentLength = "content-length:";
static const std::string_view kHost = "host:";
static const std::string_view kConnection = "connection:";
static const std::string_view kUpgrade = "upgrade:";
static const std::string_view kUserAgent = "user-agent:";

static const std::string_view kUpgradeWs = "websocket";
} // namespace

namespace hpl {

HttpHeaderParser::ParserState
HttpHeaderParser::PushLine(const std::string &line) {
  if (state_ == ParserState::FirstLine) {
    std::string_view sv;
    std::tie(method_, sv, version_) = ParseFirstLine(line);
    state_ = ParserState::Headers;
    uri_ = std::string(sv);
    LOG_DEBUG("method: {}, uri: {}, version: {}",
              static_cast<unsigned>(method_), uri_,
              static_cast<unsigned>(version_));
  } else if (state_ == ParserState::Headers) {
    if (line == ("\r\n")) {
      if (content_length_) {
        state_ = ParserState::Body;
      } else {
        state_ = ParserState::Done;
      }
      return state_;
    }

    // find the first char that meets the predicate
    auto &&next_char = [](const auto &s, auto off, auto pred) {
      if (off >= s.size()) {
        return s.end();
      }
      auto iter = s.begin() + off;
      while (iter != s.end() && !pred(*iter)) {
        ++iter;
      }
      return iter;
    };

    auto iter = next_char(line, 0, [](auto c) { return c != ' '; });
    auto offset = std::distance(line.begin(), iter);
    std::string_view key(line.data() + offset);
    if (icase_cmp(key, kContentLength)) {
      std::advance(iter, kContentLength.size());
      iter = next_char(line, std::distance(line.begin(), iter),
                       [](auto c) { return c != ' '; });
      size_t idx = std::distance(line.begin(), iter);
      content_length_ = atoi(line.data() + idx);
      LOG_DEBUG("content length: {}", *content_length_);
    } else if (icase_cmp(key, kUpgrade)) {
      std::advance(iter, kUpgrade.size());
      do {
        iter =
            next_char(line, std::distance(line.begin(), iter) + 1, [](auto c) {
              return c != ' ' || c != '\r' || c != '\n';
            });
        if (iter == line.end()) {
          break;
        }

        std::string_view value_sv(line.data() +
                                  std::distance(line.begin(), iter));
        if (icase_cmp(value_sv, kUpgradeWs)) {
          upgrade_flags_ |= UpgradeWebSocket;
          iter = next_char(line, std::distance(line.begin(), iter) + 1,
                           [](auto c) { return c == ','; });
        }
      } while (iter != line.end());
      LOG_DEBUG("upgrade: {:#x}", upgrade_flags_);
    } else if (icase_cmp(key, kHost)) {
      std::advance(iter, kHost.size());
      iter = next_char(line, std::distance(line.begin(), iter),
                       [](auto c) { return c != ' '; });
      auto end_iter = next_char(line, std::distance(line.begin(), iter),
                                [](auto c) { return c == '\r' || c == '\n'; });
      host_ = std::string(iter, end_iter);
      LOG_DEBUG("host: [{}]", host_);
    } else if (icase_cmp(key, kConnection)) {
      std::advance(iter, kConnection.size() + 1);
      static const std::string kUpgrade = "upgrade";
      static const std::string kKeepAlive = "keep-alive";
      static const std::string kClose = "close";

      do {
        iter = next_char(line, std::distance(line.begin(), iter),
                         [](auto c) { return c != ' '; });
        if (iter == line.end()) {
          break;
        }
        auto offset = std::distance(line.begin(), iter);
        std::string_view value_sv(line.data() + offset);
        if (icase_cmp(value_sv, kUpgrade)) {
          connection_flags_ |= ConnectionFlags::ConnectionUpgrade;
          iter += kUpgrade.size();
          iter = next_char(line, std::distance(line.begin(), iter),
                           [](auto c) { return c == ','; });
          ++iter;
          continue;
        } else if (icase_cmp(value_sv, kKeepAlive)) {
          connection_flags_ |= ConnectionFlags::ConnectionKeepAlive;
          iter += kKeepAlive.size();
          iter = next_char(line, std::distance(line.begin(), iter),
                           [](auto c) { return c == ','; });
          ++iter;
        } else if (icase_cmp(value_sv, kClose)) {
          connection_flags_ |= ConnectionFlags::ConnectionClose;
          iter += kClose.size();
          iter = next_char(line, std::distance(line.begin(), iter),
                           [](auto c) { return c == ','; });
          ++iter;
        } else {
          // ignore
        }
      } while (true);
      LOG_DEBUG("connection: {:#x}", connection_flags_);
    } else if (icase_cmp(key, kUserAgent)) {
      std::advance(iter, kUserAgent.size());
      iter = next_char(line, std::distance(line.begin(), iter),
                       [](auto c) { return c != ' '; });
      auto end_iter = next_char(line, std::distance(line.begin(), iter),
                                [](auto c) { return c == '\r' || c == '\n'; });
      user_agent_ = std::string(iter, end_iter);
      LOG_DEBUG("user agent: [{}]", user_agent_);
    } else {
      iter = next_char(line, std::distance(line.begin(), iter),
                       [](auto c) { return c == ':'; });
      if (iter == line.end()) {
        return state_;
      }
      auto key = std::string(line.begin(), iter);
      std::advance(iter, 1);
      iter = next_char(line, std::distance(line.begin(), iter),
                       [](auto c) { return c != ' '; });
      auto end_iter = next_char(line, std::distance(line.begin(), iter),
                                [](auto c) { return c == '\r' || c == '\n'; });
      auto value = std::string(iter, end_iter);
      headers_.emplace(std::move(key), std::move(value));
    }
  } else if (state_ == ParserState::Body) {
    LOG_DEBUG("body len: {:#x}", line.size());
    body_length_ += line.size();
    if (content_length_ && body_length_ >= *content_length_) {
      state_ = ParserState::Done;
      LOG_DEBUG("done");
      return state_;
    }
  }
  return state_;
}

std::tuple<HttpMethod, std::string_view, HttpVersion>
HttpHeaderParser::ParseFirstLine(std::string_view line) {
  size_t pos = 0;
  auto method = ParseHttpMethod(line, 0, pos);
  if (method == HttpMethod::UNKNOWN) {
    return {HttpMethod::UNKNOWN, "", HttpVersion::UNKNOWN};
  }

  while (line[pos] == ' ') {
    ++pos;
  }
  auto next_pos = line.find(' ', pos);
  auto uri = line.substr(pos, next_pos - pos);
  auto version = ParseHttpVersion(line.data() + next_pos + 1, pos);
  return {method, uri, version};
}
} // namespace hpl