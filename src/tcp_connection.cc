#include "tcp_connection.h"
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/uio.h>
#include <unistd.h>
#include <sys/epoll.h>

#include <algorithm>

#include "hpl_logger.h"

namespace hpl {

// implement TcpConnection::Write by `writev`
int TcpConnection::Write(const std::vector<BufferView> &bvs) {
  std::vector<iovec> iovs;
  iovs.reserve(bvs.size());
  std::transform(bvs.begin(), bvs.end(), std::back_inserter(iovs),
                 [](const auto &bv) -> iovec {
                   return {const_cast<char *>(bv.first), bv.second};
                 });
  return writev(fd_, iovs.data(), iovs.size());
}

int TcpConnection::SingleRead() {

  auto old_size = buffer_.size();
#ifdef SMALL_MEMORY
  const size_t kLocalBufferLen = SMALL_MEMORY;
  size_t read_len = SMALL_MEMORY - old_size;
#else
  const size_t kLocalBufferLen = 1024;
  size_t read_len = kLocalBufferLen;
#endif
  char local_buffer[kLocalBufferLen];

  assert(read_len > 0);
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

/// @retval -2, error or connection closed
/// @retval -1, headers not complete
/// @retval 0, headers complete, but body not complete
/// @retval 1, a complete request
int TcpConnection::ConsumeRead(int ep_flags) {
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
    int ret = SingleRead();
    if (ret == -1) {
      if (ep_flags & EPOLLRDHUP || ep_flags & EPOLLHUP) {
        LOG_DEBUG("connection closed by peer, we can still write");
        return 1;
      }
      return -2;
    } else if (ret == 0) {
      return state_ret(parser.GetState());
    } else {
      do {
        auto line = PopLine();
        auto line_cref = std::cref(*line);
        if (!line) {
          if (buffer_.empty() || reqs_.empty()) {
            break;
          }
          auto& last_req = reqs_.back();
          auto& parser = last_req->parser_;
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

        if (reqs_.empty() || reqs_.back()->parser_.GetState() ==
                                HttpHeaderParser::ParserState::Done) {
          reqs_.emplace_back(std::make_unique<HttpRequest>());
        }
        auto &parser = reqs_.back()->parser_;
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

std::optional<std::string> TcpConnection::PopLine() {
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

} // namespace hpl