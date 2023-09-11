#pragma once
#include <memory>
#include <vector>

#include "buffer_view.h"
#include "http_request.h"

namespace hpl {

class Server;

class TcpConnection {
public:
  explicit TcpConnection(Server *server, int fd);
  /// like `writev`
  int Write(const std::vector<BufferView> &bvs);
  int ConsumeRead(int ep_flags);

private:
  Server *server_;
  int fd_ = -1;
  std::vector<std::unique_ptr<HttpRequest>> reqs_;

  /// @retval -1, error or connection closed
  /// @retval 0, the stream drained, try another time
  /// @retval 1, success, try another read
  int SingleRead();

  std::optional<std::string> PopLine();

  std::vector<char> buffer_;

  friend class Server;
};
} // namespace hpl