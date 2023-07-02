#pragma once

#include "hpl_method.h"
#include <functional>
#include <string>
#include <string_view>

namespace hpl {
class Connection;
class WebsocketConnection;

enum WsFrameType {
  kTypeContinuation = 0x0,
  kTypeText,
  kTypeBinary,
  kTypeClose = 0x8,
  kTypePing = 0x9,
  kTypePong = 0xa,
};

/// @brief the handler of the request
/// @retval -1, close the connection
/// @retval 0, keep the connection
typedef std::function<int(Connection *, const std::string_view &uri,
                          std::string &&partial, bool is_final)>
    Handler;
typedef std::function<int(WebsocketConnection *, std::string_view uri)> WsHook;
typedef std::function<int(WebsocketConnection *, WsFrameType type,
                          std::string_view data)>
    WsHandler;

struct RequestHandler {
  Handler http_handlers[static_cast<int>(HttpMethod::UNKNOWN)] = {};
  Handler http_will_colse = nullptr;
  WsHook ws_connect_hook = nullptr;
  WsHook ws_ready_hook = nullptr;
  WsHook ws_will_close_hook = nullptr;
  WsHandler ws_message_handler = nullptr;
};
} // namespace hpl