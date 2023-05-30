#include <functional>
#include <string_view>

#include "hpl_connection.h"
#include "hpl_logger.h"
#include "hpl_method.h"
#include "hpl_request_handler.h"
#include "hpl_response.h"
#include "hpl_server.h"
#include "hpl_websocket_connection.h"

struct AsyncHttpContext {
  hpl::Connection *pending_get_conn = nullptr;

  int get_handler(hpl::Connection *conn, std::string_view uri,
                  std::string &&partial, bool is_final) {
    pending_get_conn = conn;
    return 0;
  }

  int websocket_ready(hpl::WebsocketConnection *ws, std::string_view uri) {
    LOG_TRACE("websocket_ready");
    if (pending_get_conn) {
      const std::string kGetTemplate = R"(
  <html>
  <head>this is my site</head>
  <body><b>hello world</b></body>
  </html>
  )";
      std::string response = MakeResponse(
          200, pending_get_conn->GetParser().GetVersion(), {}, kGetTemplate);
      pending_get_conn->Write(response.data(), response.size());
      // (std::move(pending_get_conn))->Close();
      std::move(*pending_get_conn).Close();
      pending_get_conn = nullptr;
    }
    return 0;
  }

  int ws_close(hpl::WebsocketConnection *ws, std::string_view uri) {
    LOG_TRACE("ws_close");
    return 0;
  }

  int dummy_handler(hpl::Connection *conn, std::string_view uri,
                    std::string &&partial, bool is_final) {
    LOG_TRACE("dummy_handler {} final:{}, data_len:{}", uri, is_final,
              partial.size());
    if (!is_final) {
      return 0;
    }
    const std::string kGetTemplate = R"(
  <html>
  <head>this is my site</head>
  <body><b>hello world</b></body>
  </html>
  )";
    std::string response =
        MakeResponse(200, conn->GetParser().GetVersion(), {}, kGetTemplate);
    conn->Write(response.data(), response.size());
    return -1;
  }
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

  AsyncHttpContext context;
  RequestHandler handlers{
      .ws_ready_hook = std::bind(&AsyncHttpContext::websocket_ready, &context,
                                 std::placeholders::_1, std::placeholders::_2),
      .ws_will_close_hook =
          std::bind(&AsyncHttpContext::ws_close, &context,
                    std::placeholders::_1, std::placeholders::_2),
  };

  handlers.http_handlers[static_cast<int>(HttpMethod::GET)] = std::bind(
      &AsyncHttpContext::dummy_handler, &context, std::placeholders::_1,
      std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
  handlers.http_handlers[static_cast<int>(HttpMethod::POST)] = std::bind(
      &AsyncHttpContext::dummy_handler, &context, std::placeholders::_1,
      std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
  handlers.http_handlers[static_cast<int>(HttpMethod::PUT)] = std::bind(
      &AsyncHttpContext::dummy_handler, &context, std::placeholders::_1,
      std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
  handlers.http_handlers[static_cast<int>(HttpMethod::DELETE)] = std::bind(
      &AsyncHttpContext::dummy_handler, &context, std::placeholders::_1,
      std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
  handlers.http_handlers[static_cast<int>(HttpMethod::HEAD)] = std::bind(
      &AsyncHttpContext::dummy_handler, &context, std::placeholders::_1,
      std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
  handlers.http_handlers[static_cast<int>(HttpMethod::CONNECT)] = std::bind(
      &AsyncHttpContext::dummy_handler, &context, std::placeholders::_1,
      std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);

  server.RegisterRequestHandler("/", std::move(handlers));

  RequestHandler dump_handlers;
  dump_handlers.http_handlers[static_cast<int>(HttpMethod::GET)] =
      [](auto *conn, auto uri, auto partial, auto is_final) -> int {
    const std::string kGetTemplate = R"(
<html><title>premium king</title>
<head>this is my site</head><body><b>hello world</b></body></html>)";
    std::string response =
        MakeResponse(200, conn->GetParser().GetVersion(), {}, kGetTemplate);
    conn->Write(response.data(), response.size());
    return 0;
  };
  server.RegisterRequestHandler("/abc", std::move(dump_handlers));
  LOG_DEBUG("server start at :{}", port);
  int i = 0;
  while (true) {
    server.Poll(30);
  }
  return 0;
}