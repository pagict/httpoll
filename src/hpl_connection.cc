#include "hpl_connection.h"

#include <errno.h>
#include <functional>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <algorithm>
#include <iterator>
#include <optional>

#include "hpl_header_parser.h"
#include "hpl_logger.h"
#include "hpl_method.h"
#include "hpl_request_handler.h"
#include "hpl_response.h"
#include "hpl_server.h"
#include "hpl_str.h"

namespace hpl {
Connection::Connection(Server *svr, int fd) : svr_(svr), fd_(fd) {
  buffer_.reserve(8192);
}

std::optional<std::string> Connection::PopLine() {
  auto size = buffer_.size();
  if (buffer_.size() < 2) {
    return std::nullopt;
  }
  size_t idx = 0;
  static const unsigned short kIntNewLine = '\r' | ('\n' << 8);
  for (; idx < buffer_.size() - 1; ++idx) {
    if (*reinterpret_cast<unsigned short *>(buffer_.data() + idx) ==
        kIntNewLine) {
      break;
    }
  }
  if (idx >= buffer_.size() - 1) {
    LOG_DEBUG("popline no newline, idx [{}], buffer[{}]", idx, buffer_.size());
    return std::nullopt;
  }
  auto it = std::next(buffer_.begin(), idx + 2);
  std::string line(buffer_.begin(), it);
  buffer_.erase(buffer_.begin(), it);

  return line;
}

/// return -1, error or connection closed
/// return 0, the stream drained, try another time
/// return 1, success, try another read
int Connection::Read() {
  auto old_size = buffer_.size();
#ifdef SMALL_MEMORY
  const size_t kLocalBufferLen = SMALL_MEMORY;
  size_t read_len = SMALL_MEMORY - old_size;
#else
  const size_t kLocalBufferLen = 1024;
  size_t read_len = kLocalBufferLen;
#endif
  char local_buffer[kLocalBufferLen];

  int nread = read(fd_, local_buffer, read_len);
  if (nread > 0) {
    buffer_.insert(buffer_.end(), local_buffer, local_buffer + nread);
    return 1;
  }
  LOG_DEBUG("read ret: {}, err[{}][{}]", nread, errno,
            strerror_r(errno, local_buffer, kLocalBufferLen));
  if (nread == -1) {
    if (errno == EAGAIN) {
      return 0;
    } else if (errno == EINTR) {
      return 1;
    } else {
      return -1;
    }
  } else {
    // nread == 0
    return -1;
  }
}

std::string &&Connection::PopBody() { return std::move(partial_body_); }

WebsocketConnection *Connection::UpgradeToWebsocket(WsHandler ws_on_msg,
                                                    WsHook will_close_hook) {
  if (ws_conn_) {
    return ws_conn_.get();
  }
  auto key = parser.GetHeader("Sec-WebSocket-Key");
  if (key.empty()) {
    key = parser.GetHeader("Sec-WebSocket-Key1");
  }
  if (key.empty()) {
    key = parser.GetHeader("Sec-WebSocket-Key2");
  }
  if (key.empty()) {
    LOG_ERROR("websocket key not found");
    return nullptr;
  }
  ws_conn_ = std::make_unique<WebsocketConnection>(this, key, ws_on_msg,
                                                   will_close_hook);
  return ws_conn_.get();
}

/// @retval -2, error or connection closed
/// @retval -1, headers not complete
/// @retval 0, headers complete, but body not complete
/// @retval 1, a complete request
int Connection::ProcessDataIn() {
  auto state_ret = [](auto state) {
    switch (state) {
    case HttpHeaderParser::ParserState::Body:
      return 0;
    case HttpHeaderParser::ParserState::Done:
      return 1;
    default:
      return -1;
    }
  };
  do {
    int ret = Read();
    if (ret == -1) {
      return -2;
    } else if (ret == 0) {
      return state_ret(parser.GetState());
    } else {
      do {
        auto line = PopLine();
        auto line_cref = std::cref(*line);
        if (!line) {
          if (buffer_.empty()) {
            break;
          }
          if (parser.GetState() != HttpHeaderParser::ParserState::Body) {
            break;
          } else {
            auto length_limit = std::min(buffer_.size(), 1024ul);
            partial_body_ =
                std::string(buffer_.begin(), buffer_.begin() + length_limit);
            line_cref = std::cref(partial_body_);
            buffer_.erase(buffer_.begin(), buffer_.begin() + length_limit);
          }
        }

        auto stat = parser.PushLine(line_cref.get());
        if (stat == HttpHeaderParser::ParserState::Done ||
            stat == HttpHeaderParser::ParserState::Body) {
          return state_ret(stat);
        }
      } while (true); // PopLine
    }
  } while (true); // read until no more data, or error, or connection closed

  return state_ret(parser.GetState());
}

int Connection::Write(const char *data, size_t len) {
  if (fd_ == -1) {
    LOG_ERROR("invalid fd [{}]", fd_);
    return -1;
  }
  int nwrite = write(fd_, data, len);
  if (nwrite == -1) {
    if (errno == EAGAIN) {
      return 0;
    } else if (errno == EINTR) {
      return 1;
    } else {
      return -1;
    }
  } else {
    return 1;
  }
}

bool Connection::ShouldUpgradeWebsocket() const {
  if (ws_conn_) {
    // already upgraded, we don't upgrade twice
    return false;
  }
  if ((parser.GetUpgradeFlags() &
       HttpHeaderParser::UpgradeFlags::UpgradeWebSocket) &&
      (parser.GetConnectionFlags() &
       HttpHeaderParser::ConnectionFlags::ConnectionUpgrade) &&
      (parser.GetVersion() == HttpVersion::HTTP_1_1 ||
       parser.GetVersion() == HttpVersion::HTTP_1_0)) {
    return true;
  }

  return false;
}

void Connection::Close() && { svr_->CloseConn(std::move(this)); }

} // namespace hpl