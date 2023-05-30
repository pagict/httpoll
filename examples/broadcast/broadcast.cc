#include <chrono>
#include <fmt/format.h>
#include <string_view>

#include "hpl_broadcast_group.h"
#include "hpl_request_handler.h"
#include "hpl_server.h"
#include "hpl_websocket_connection.h"

class BroadcastContext {
public:
  hpl::RequestHandler handlers{
      .ws_connect_hook =
          std::bind(&BroadcastContext::OnWsConnected, this,
                    std::placeholders::_1, std::placeholders::_2),
  };

  void Broadcast(std::string_view data) {
    bg.Broadcast(data, hpl::WsFrameType::kTypeText);
  }

private:
  int OnWsConnected(hpl::WebsocketConnection *conn, std::string_view uri) {
    bg.Join(conn);
    return 0;
  }

  hpl::BroadcastGroup bg;
};

int main(int argc, char **argv) {
  using namespace hpl;
  const char *addr = NULL;
  unsigned port = 2999;
  if (argc > 1) {
    port = atoi(argv[1]);
  }
  Server server;
  int ret = server.Init(addr, port, 5);
  if (ret != 0) {
    return ret;
  }

  BroadcastContext ctx;
  server.RegisterRequestHandler("/broadcast", std::move(ctx.handlers));

  auto time = std::chrono::system_clock::now();
  while (true) {
    server.Poll(1000);

    auto now = std::chrono::system_clock::now();
    if (now - time > std::chrono::seconds(10)) {
      time = now;
      auto data = fmt::format("hello world at ");
      ctx.Broadcast(data);
    }
  }
}