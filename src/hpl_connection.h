#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "hpl_header_parser.h"
#include "hpl_method.h"
#include "hpl_request_handler.h"
#include "hpl_version.h"
#include "hpl_websocket_connection.h"

namespace hpl {
class Server;

class Connection {
public:
  int Write(const char *data, size_t len);
  inline const HttpHeaderParser &GetParser() const { return parser; }

  /// @note don't call this function in a handler, or the connection will be
  /// double closed
  void Close() &&;

  int GetDescriptor() const { return fd_; }

private:
  Connection(Server *svr, int fd);
  bool ShouldUpgradeWebsocket() const;

  Server *svr_;
  int fd_ = -1;
  std::vector<char> buffer_;
  std::unique_ptr<WebsocketConnection> ws_conn_;

  friend class Server;
  friend class WebsocketConnection;

  int ProcessDataIn();
  std::string &&PopBody();
  WebsocketConnection *UpgradeToWebsocket(WsHandler ws_on_msg,
                                          WsHook will_close_hook);

  std::string partial_body_;

  std::optional<std::string> PopLine();

  int HandleRequest();

  HttpHeaderParser parser;

  /// @retval -1, error or connection closed
  /// @retval 0, the stream drained, try another time
  /// @retval 1, success, try another read
  int Read();
};
} // namespace hpl