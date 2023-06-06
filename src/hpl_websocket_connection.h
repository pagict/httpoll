#pragma once

#ifdef HPL_ENABLE_PING_PONG
#include <chrono>
#endif // HPL_ENABLE_PING_PONG
#include <string>
#include <string_view>

#include "hpl_broadcast_group.h"
#include "hpl_request_handler.h"

namespace hpl {
class Server;
class WebsocketConnection {
public:
  explicit WebsocketConnection(Connection *const conn, std::string_view ws_key,
                               WsHandler on_data, WsHook will_close_hook)
      : ws_key_(ws_key), conn_(conn), on_data_(on_data),
        will_close_hook_(will_close_hook) {}

  inline std::string_view GetWsAccept() {
    if (ws_accept_.empty()) {
      ws_accept_ = GetWsAcceptByKey(ws_key_);
    }
    return ws_accept_;
  }
  inline std::string_view GetWsKey() const { return ws_key_; }
  int Write(WsFrameType type, const char *data, size_t len);

  /// try to send a ping frame to client if it is idle for a long time
  /// @retval true, ping frame is sent
  bool ServerPing();

  int GetDescriptor() const;

private:
  const std::string_view ws_key_;
  std::string ws_accept_;
  const WsHandler on_data_;
  const WsHook will_close_hook_;
  static std::string GetWsAcceptByKey(std::string_view ws_sec_key);

  std::string last_payload_;

#ifdef HPL_ENABLE_PING_PONG
  std::chrono::steady_clock::time_point last_io_time_ =
      std::chrono::steady_clock::now();
#endif // HPL_ENABLE_PING_PONG

  /// @retval 0, a complete frame is read
  /// @retval 1, a frame is read but not complete
  /// @retval -1, error
  int Read();

  Connection *const conn_;
  friend class Server;
  friend class BroadcastGroup;
};
} // namespace hpl