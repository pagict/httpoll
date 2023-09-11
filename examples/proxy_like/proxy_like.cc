#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <curl/curl.h>
#include <curl/easy.h>
#include <curl/multi.h>
#include <fmt/core.h>

#include <chrono>
#include <memory>
#include <string_view>

#include "hpl_connection.h"
#include "hpl_logger.h"
#include "hpl_method.h"
#include "hpl_request_handler.h"
#include "hpl_server.h"

size_t curl_write_body_func(char *ptr, size_t size, size_t nmemb,
                            void *userdata);

class request_pipe_t {
public:
  request_pipe_t(CURLM *curlm, hpl::Connection *conn, const std::string &uri)
      : curlm_(curlm), conn_(conn), curl_(curl_easy_init()) {
    LOG_DEBUG("request_pipe_t construct: {}, conn[{}], curl[{}]",
              fmt::ptr(this), fmt::ptr(conn), fmt::ptr(curl_));
    curl_easy_setopt(curl_, CURLOPT_URL,
                     (std::string("http://175.178.48.71:3000") + uri).c_str());
    curl_easy_setopt(curl_, CURLOPT_WRITEDATA, this);
    curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, curl_write_body_func);
    const auto &conn_headers = conn_->GetParser().GetHeaders();
    for (const auto &header : conn_headers) {
      headers_ = curl_slist_append(
          headers_, (header.first + ": " + header.second).c_str());
    }
    if (!conn_headers.empty()) {
      curl_easy_setopt(curl_, CURLOPT_HEADER, 1L);
    }
    curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, headers_);
    curl_multi_add_handle(curlm_, curl_);
    LOG_TRACE("curl_multi_add_handle {}", fmt::ptr(curl_));
  }

  size_t curl_write_body(char *ptr, size_t size, size_t nmemb) {
    LOG_TRACE("curl_write_body: [{}]", ptr);
    conn_->Write(ptr, size * nmemb);
    return size * nmemb;
  }

  CURLM *curlm_ = nullptr;
  hpl::Connection *conn_ = nullptr;
  CURL *curl_ = nullptr;
  curl_slist *headers_ = nullptr;

  ~request_pipe_t() {
    curl_free(headers_);
  }
};

struct ServerContext {
  static hpl::Connection *last_conn;
  hpl::Handler getter = [this](hpl::Connection *conn,
                               const std::string_view &uri,
                               std::string &&partial, bool is_final) {
    if (!is_final) {
      return 0;
    }
    req_pipes_.push_back(std::unique_ptr<request_pipe_t>(
        new request_pipe_t(curlm, conn, uri.data())));
    last_conn = conn;
    return 0;
  };
  hpl::Handler poster = [](hpl::Connection *conn, const std::string_view &uri,
                           std::string &&partial, bool is_final) {
    LOG_TRACE("poster: {}", uri.data());
    return 0;
  };
  hpl::Handler will_close_hook = [this](hpl::Connection *conn,
                                        const std::string_view &uri,
                                        std::string &&partial, bool is_final) {
    LOG_TRACE("will_close_hook {}", fmt::ptr(conn));
    auto iter = std::find_if(req_pipes_.begin(), req_pipes_.end(),
                             [&](auto &p) { return p->conn_ == conn; });
    if (iter == req_pipes_.end()) {
      LOG_ERROR("request_pipe_map.find failed");
      return 0;
    }
    CURL *curl = (*iter)->curl_;
    RemovePipe(curl);
    return 0;
  };

  hpl::RequestHandler request_handler;

  std::vector<std::unique_ptr<request_pipe_t>> req_pipes_;
  CURLM *curlm = nullptr;

  void RemovePipe(CURL *curl) {
    auto iter = std::find_if(req_pipes_.begin(), req_pipes_.end(),
                             [&](auto &p) { return p->curl_ == curl; });
    if (iter == req_pipes_.end()) {
      LOG_ERROR("request_pipe_map.find failed");
      return;
    }
    LOG_DEBUG("RemovePipe: {}, curl[{}] conn[{}]", fmt::ptr(iter->get()),
              fmt::ptr(curl), fmt::ptr((*iter)->conn_));
    LOG_TRACE("curl_multi_remove_handle {}", fmt::ptr(curl));
    curl_multi_remove_handle(curlm, curl);
    curl_easy_cleanup(curl);
    req_pipes_.erase(iter);
  }

  size_t curl_pipe_to_conn_body(char *ptr, size_t size, size_t nmemb,
                                request_pipe_t *req_pipe) {
    auto iter = std::find_if(req_pipes_.begin(), req_pipes_.end(),
                             [&](auto &p) { return p.get() == req_pipe; });
    if (iter == req_pipes_.end()) {
      LOG_ERROR("request_pipe_map.find failed");
      LOG_DEBUG("curl_pipe_to_conn_body: [{}]", ptr);
      return size * nmemb;
    }
    return req_pipe->curl_write_body(ptr, size, nmemb);
  }

  ServerContext() {
    curlm = curl_multi_init();
    request_handler.http_handlers[static_cast<int>(hpl::HttpMethod::GET)] =
        getter;
    request_handler.http_handlers[static_cast<int>(hpl::HttpMethod::POST)] =
        poster;
    request_handler.http_will_colse = will_close_hook;
  }
  ~ServerContext() {
    if (curlm) {
      LOG_TRACE("curl_multi_cleanup {}", fmt::ptr(curlm));
      curl_multi_cleanup(curlm);
    }
  }
};

hpl::Connection *ServerContext::last_conn = nullptr;

ServerContext *g_context = nullptr;

size_t curl_write_body_func(char *ptr, size_t size, size_t nmemb,
                            void *userdata) {
  request_pipe_t *request_pipe = (request_pipe_t *)userdata;
  return g_context->curl_pipe_to_conn_body(ptr, size, nmemb, request_pipe);
}

int main(int argc, char **argv) {
  signal(SIGPIPE, SIG_IGN);

  hpl::Server server;
  server.Init("192.168.64.9", 2999, 50);

  g_context = new ServerContext();
  server.RegisterRequestHandler("*", std::move(g_context->request_handler));

  int running_handles = 0;
  while (true) {
    server.Poll(50);
    curl_multi_perform(g_context->curlm, &running_handles);
    struct CURLMsg *m = nullptr;
    while ((m = curl_multi_info_read(g_context->curlm, &running_handles))) {
      if (m->msg == CURLMSG_DONE) {
        CURL *e = m->easy_handle;
        g_context->RemovePipe(e);
      }
    }
  }

  return 0;
}