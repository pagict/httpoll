#include "hpl_server.h"

#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <functional>
#include <memory>

#include <fmt/args.h>

#include "hpl_connection.h"
#include "hpl_header_parser.h"
#include "hpl_logger.h"
#include "hpl_method.h"
#include "hpl_response.h"
#include "hpl_utils.h"

namespace hpl {

Server::Server() : poll_fd(-1) {}

int Server::Init(const char *addr, int port, int backlog) {
  struct sigaction sa;
  sa.sa_handler = SIG_IGN;
  sigaction(SIGPIPE, &sa, nullptr);
  poll_fd = epoll_create1(0);

  const int kFmtErrorBufSize = 64;
  char fmt_error_buf[kFmtErrorBufSize];
  if (poll_fd == -1) {
    LOG_ERROR("Init error create epoll {}",
              strerror_r(errno, fmt_error_buf, kFmtErrorBufSize));
    return -1;
  }

  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd == -1) {
    LOG_FATAL("Init error create socket {}",
              strerror_r(errno, fmt_error_buf, kFmtErrorBufSize));
    return -2;
  }

  int reuse = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int)) ==
      -1) {
    LOG_FATAL("Init error setsockopt reuseaddr {}",
              strerror_r(errno, fmt_error_buf, kFmtErrorBufSize));
    return -3;
  }
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(int)) ==
      -1) {
    LOG_FATAL("Init error setsockopt reuseport {}",
              strerror_r(errno, fmt_error_buf, kFmtErrorBufSize));
    return -3;
  }

  struct sockaddr_in server_addr = {
      .sin_family = AF_INET,
      .sin_port = htons(port),
  };
  if (addr) {
    server_addr.sin_addr.s_addr = inet_addr(addr);
  } else {
    server_addr.sin_addr.s_addr = INADDR_ANY;
  }
  if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) ==
      -1) {
    LOG_FATAL("Init error bind {}",
              strerror_r(errno, fmt_error_buf, kFmtErrorBufSize));
    return -3;
  }
  if (listen(server_fd, backlog) == -1) {
    LOG_FATAL("Init error listen {}",
              strerror_r(errno, fmt_error_buf, kFmtErrorBufSize));
    return -4;
  }

  server_conn_ = std::unique_ptr<Connection>(new Connection(this, server_fd));

  struct epoll_event event = {
      .events = EPOLLIN,
      .data = {.ptr = server_conn_.get()},
  };
  if (epoll_ctl(poll_fd, EPOLL_CTL_ADD, server_fd, &event) == -1) {
    LOG_FATAL("Init error epoll_ctl {}",
              strerror_r(errno, fmt_error_buf, kFmtErrorBufSize));
    return -4;
  }
  return 0;
}

int Server::Poll(int timeout) {
  const int max_events = 10;
  const int kFmtErrorBufSize = 64;
  char fmt_error_buf[kFmtErrorBufSize];
  struct epoll_event events[max_events];

  for (auto pending_con_iter = pending_conns_.begin();
       pending_con_iter != pending_conns_.end(); ++pending_con_iter) {
    auto &pending_con = *pending_con_iter;
    if (pending_con->ws_conn_) {
      bool sent_ping = pending_con->ws_conn_->ServerPing();
      if (sent_ping) {
        LOG_TRACE("sent ping to client");
      }
    }
  }
  int nfds = epoll_wait(poll_fd, events, max_events, timeout);
  if (nfds == -1) {
    LOG_ERROR("epoll_wait err[{}][{}] nfds[{}]", errno,
              strerror_r(errno, fmt_error_buf, kFmtErrorBufSize), nfds);
    return -1;
  }

  for (int i = 0; i < nfds; ++i) {
    auto *conn = static_cast<Connection *>(events[i].data.ptr);
    if (conn == server_conn_.get()) {
      LOG_INFO("EPOLLIN for server_conn_");
      auto new_conn = AcceptNewConnection();
      if (new_conn) {
#ifdef SMALL_MEMORY
        uint32_t ep_flags = EPOLLIN;
#else
        uint32_t ep_flags = EPOLLIN | EPOLLET;
#endif
        struct epoll_event event = {
            .events = ep_flags,
            .data = {.ptr = new_conn.get()},
        };
        if (epoll_ctl(poll_fd, EPOLL_CTL_ADD, new_conn->fd_, &event) == -1) {
          LOG_ERROR("epoll_ctl err[{}], accept error",
                    strerror_r(errno, fmt_error_buf, kFmtErrorBufSize));
          return -4;
        }
        pending_conns_.push_back(std::move(new_conn));
      }
    } else {
      LOG_TRACE("epoll event[{:#x}] for conn[{} {}]",
                (unsigned)events[i].events, fmt::ptr(conn), conn->fd_);
      if (events[i].events & EPOLLIN) {
        if (conn->ws_conn_) {
          auto ret = conn->ws_conn_->Read();
          LOG_DEBUG("websocket read ret[{}]", ret);
          if (ret == -1) {
            if (conn->ws_conn_->will_close_hook_) {
              conn->ws_conn_->will_close_hook_(conn->ws_conn_.get(), "");
            }
            CloseConn(conn);
          }
        } else {
          auto ret = conn->ProcessDataIn();
          LOG_DEBUG("EPOLLIN for conn[{} {}] ret[{}]", fmt::ptr(conn),
                    conn->fd_, ret);
          switch (ret) {
          case -2: {
            LOG_DEBUG("EPOLLIN for conn[{} {}] ret[{}]", fmt::ptr(conn),
                      conn->fd_, ret);
            CloseConn(conn);
            break;
          }
          case -1: {
            continue;
          }
          case 1:
          case 0: {
            // short http request(HTTP/1.0, HTTP/1.1)
            const auto &parser = conn->GetParser();
            const auto &uri = parser.GetUri();
            const auto &method = parser.GetMethod();
            auto &&handlers_iter = request_handlers.find(uri);

            LOG_DEBUG("uri: {}, method: {}", uri,
                      static_cast<unsigned>(method));
            if (handlers_iter != request_handlers.end()) {
              Handler handler = nullptr;

              if (conn->ShouldUpgradeWebsocket()) {
                auto *ws_conn = conn->UpgradeToWebsocket(
                    handlers_iter->second.ws_message_handler,
                    handlers_iter->second.ws_will_close_hook);
                LOG_DEBUG("upgrade websocket [{}] key[{}] accept[{}]",
                          conn->fd_, ws_conn->GetWsKey(),
                          ws_conn->GetWsAccept());
                int n = 0;
                if (handlers_iter->second.ws_connect_hook) {
                  n = handlers_iter->second.ws_connect_hook(ws_conn, uri);
                }
                if (n == -1) {
                  LOG_ERROR("ws_connect_hook close conn {}", conn->fd_);
                  CloseConn(conn);
                  continue;
                }

                const auto switch_protocol_rsp = MakeResponse(
                    101, conn->GetParser().GetVersion(),
                    {{"Upgrade", "websocket"},
                     {"Connection", "Upgrade"},
                     {"Sec-WebSocket-Accept", ws_conn->GetWsAccept()}});
                conn->Write(switch_protocol_rsp.data(),
                            switch_protocol_rsp.size());
                LOG_TRACE("write switch_protocol_rsp");
                if (handlers_iter->second.ws_ready_hook) {
                  n = handlers_iter->second.ws_ready_hook(ws_conn, uri);
                }
                if (n == -1) {
                  CloseConn(conn);
                }
                continue;

              } else {
                handler = handlers_iter->second
                              .http_handlers[static_cast<int>(method)];
              }

              int handler_ret = -1;
              if (handler) {
                handler_ret = handler(conn, uri, conn->PopBody(),
                                      ret == 1 ? true : false);
              } else {
                LOG_ERROR("uri registered, method not support");
              }
              if (handler_ret == -1) {
                CloseConn(conn);
              }
            } else {
              auto rsp_404 =
                  MakeResponse(404, conn->GetParser().GetVersion(), {});
              ret = conn->Write(rsp_404.data(), rsp_404.size());
              CloseConn(conn);
            }
          }
          }
        }
      } else if (events[i].events & EPOLLRDHUP) {
        LOG_TRACE("EPOLLRDHUP for conn[{} {}]", fmt::ptr(conn), conn->fd_);
      } else if (events[i].events & EPOLLERR) {
        LOG_TRACE("EPOLLERR for conn[{} {}]", fmt::ptr(conn), conn->fd_);
      } else if (events[i].events & EPOLLHUP) {
        LOG_TRACE("EPOLLHUP for conn[{} {}]", fmt::ptr(conn), conn->fd_);
      } else {
        LOG_TRACE("unknown event[{:#x}] for conn[{} {}]",
                  (unsigned)events[i].events, fmt::ptr(conn), conn->fd_);
      }
    }
  }
  return nfds;
}

std::unique_ptr<Connection> Server::AcceptNewConnection() {
  struct sockaddr_in client_addr;
  socklen_t client_addr_len = sizeof(client_addr);
  int client_fd = accept(server_conn_->fd_, (struct sockaddr *)&client_addr,
                         &client_addr_len);
  const int kBufSize = 64;
  char buf[kBufSize];
  if (client_fd == -1) {
    LOG_ERROR("Accept error [{}], fd[{}]", strerror_r(errno, buf, kBufSize),
              server_conn_->fd_);
    return nullptr;
  }
  int on = 1;
  if (setsockopt(client_fd, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on)) == -1) {
    LOG_ERROR("setsockopt keep-alive error [{}]",
              strerror_r(errno, buf, kBufSize));
    return nullptr;
  }
  setnonblocking(client_fd);

  auto new_conn = std::unique_ptr<Connection>(new Connection(this, client_fd));
  return new_conn;
}

int Server::CloseConn(Connection *conn) {
  LOG_TRACE("close conn[{} {}]", fmt::ptr(conn), conn->fd_);
  struct epoll_event event = {
      .events = EPOLLIN,
      .data = {.ptr = conn},
  };
  const int kBufSize = 64;
  char buf[kBufSize];
  if (epoll_ctl(poll_fd, EPOLL_CTL_DEL, conn->fd_, &event) == -1) {
    LOG_ERROR("epoll_ctl, del conn[{} {}] error:{}", fmt::ptr(conn), conn->fd_,
              strerror_r(errno, buf, kBufSize));
    return -1;
  }
  LOG_DEBUG("{} {}", __FUNCTION__, conn->fd_);
  if (conn->fd_ != -1) {
    close(conn->fd_);
    conn->fd_ = -1;
  }
  pending_conns_.remove_if([conn](const auto &c) { return c.get() == conn; });
  return 0;
}
} // namespace hpl