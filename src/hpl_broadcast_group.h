#pragma once

#include <string_view>
#include <vector>

#include "hpl_request_handler.h"
#include "hpl_websocket_connection.h"
#ifndef HPL_BROADCAST_GROUP_H
#define HPL_BROADCAST_GROUP_H

namespace hpl {
class BroadcastGroup {
public:
  BroadcastGroup();
  ~BroadcastGroup();

  int Join(WebsocketConnection *connection);
  int Leave(const WebsocketConnection *connection);
  int Broadcast(const std::string_view &data, WsFrameType msg_type);

private:
  int pipefd_[2] = {-1, -1};
  std::vector<std::pair<std::array<int, 2>, WebsocketConnection *>>
      connections_;
};
} // namespace hpl

#endif // HPL_BROADCAST_GROUP_H