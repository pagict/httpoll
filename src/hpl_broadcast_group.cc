#include "hpl_broadcast_group.h"

#include <array>
#include <fcntl.h>
#include <stddef.h>
#include <string.h>
#include <strings.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>

#include <utility>

#include "hpl_connection.h"
#include "hpl_logger.h"
#include "hpl_request_handler.h"
#include "hpl_websocket_connection.h"
#include "hpl_websocket_utils.h"

namespace hpl {
BroadcastGroup::BroadcastGroup() {
  int ret = pipe(pipefd_);

  if (ret != 0) {
    const unsigned int kBufSize = 64;
    char fmtErrorBuf[kBufSize];
    LOG_ERROR("pipe failed: {}", strerror_r(errno, fmtErrorBuf, kBufSize));
  }
  fcntl(pipefd_[0], F_SETFL, O_NONBLOCK);
  fcntl(pipefd_[1], F_SETFL, O_NONBLOCK);
}

BroadcastGroup::~BroadcastGroup() {
  if (pipefd_[0] != -1) {
    close(pipefd_[0]);
  }
  if (pipefd_[1] != -1) {
    close(pipefd_[1]);
  }

  for (auto &pair : connections_) {
    close(pair.first[0]);
    close(pair.first[1]);
  }
  connections_.clear();
}

int BroadcastGroup::Join(WebsocketConnection *connection) {
  if (connection == nullptr) {
    return -1;
  }

  int enable = 1;
  int i = setsockopt(connection->conn_->GetDescriptor(), SOL_SOCKET,
                     SO_ZEROCOPY, &enable, sizeof(enable));
  const unsigned int kBufSize = 64;
  char fmtErrorBuf[kBufSize];

  if (i < 0) {
    LOG_ERROR("setsockopt failed: {}",
              strerror_r(errno, fmtErrorBuf, kBufSize));
  }
  int pipes[2] = {-1, -1};
  int ret = pipe(pipes);
  if (ret != 0) {
    LOG_ERROR("pipe failed: {}", strerror_r(errno, fmtErrorBuf, kBufSize));
    return -1;
  }
  fcntl(pipes[0], F_SETFL, O_NONBLOCK);
  fcntl(pipes[1], F_SETFL, O_NONBLOCK);

  connections_.emplace_back(
      std::make_pair(std::array<int, 2>{pipes[0], pipes[1]}, connection));
  return 0;
}

int BroadcastGroup::Leave(const WebsocketConnection *connection) {
  auto iter = std::find_if(
      connections_.begin(), connections_.end(),
      [connection](const auto &pair) { return pair.second == connection; });
  if (iter == connections_.end()) {
    return -1;
  }
  close(iter->first[0]);
  close(iter->first[1]);
  connections_.erase(iter);
  return 0;
}

int BroadcastGroup::Broadcast(const std::string_view &data,
                              WsFrameType msg_type) {
  if (pipefd_[0] == -1 || pipefd_[1] == -1) {
    return -1;
  }
  if (connections_.empty()) {
    return 0;
  }
  const unsigned int kBufSize = 64;
  char fmtErrorBuf[kBufSize];

  auto header = MakeWebsocketHeader(data.size(), msg_type);
  int ret = write(pipefd_[1], header.first, header.second);
  if (ret == -1) {
    LOG_ERROR("write failed: {}", strerror_r(errno, fmtErrorBuf, kBufSize));
    return -1;
  }
  ret = write(pipefd_[1], data.data(), data.size());
  if (ret == -1) {
    LOG_ERROR("write failed: {}", strerror_r(errno, fmtErrorBuf, kBufSize));
    return -1;
  }
  size_t n_conns = connections_.size();
  for (auto i = 0; i < n_conns - 1; ++i) {
    tee(pipefd_[0], connections_[i].first[1], header.second + data.length(), 0);
  }
  splice(pipefd_[0], nullptr, connections_[n_conns - 1].first[1], nullptr,
         header.second + data.length(), SPLICE_F_MOVE);

  for (auto i = 0; i < n_conns; ++i) {
    splice(connections_[i].first[0], nullptr,
           connections_[i].second->GetDescriptor(), nullptr,
           header.second + data.length(), SPLICE_F_MOVE);
  }

  LOG_TRACE("broadcast done");
  return 0;
}

int BroadcastGroup::Broadcast_vmsplice(const std::string_view &data,
                                       WsFrameType msg_type) {
  if (pipefd_[0] == -1 || pipefd_[1] == -1) {
    return -1;
  }
  if (connections_.empty()) {
    return 0;
  }
  const unsigned int kBufSize = 64;
  char fmtErrorBuf[kBufSize];

  auto header = MakeWebsocketHeader(data.size(), msg_type);
  struct iovec iov[2];
  iov[0].iov_base = header.first;
  iov[0].iov_len = header.second;
  iov[1].iov_base = const_cast<char *>(data.data());
  iov[1].iov_len = data.size();
  for (int i = 0; i < connections_.size(); ++i) {
    vmsplice(connections_[i].first[1], iov, 2, SPLICE_F_GIFT);
    splice(connections_[i].first[0], nullptr,
           connections_[i].second->GetDescriptor(), nullptr,
           header.second + data.length(), SPLICE_F_MOVE);
  }
  return 0;
}

} // namespace hpl