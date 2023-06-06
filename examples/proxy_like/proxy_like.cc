#include <stddef.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <curl/curl.h>
#include <curl/easy.h>
#include <curl/multi.h>

#include <chrono>
#include <memory>

#include "hpl_connection.h"
#include "hpl_logger.h"
#include "hpl_method.h"
#include "hpl_request_handler.h"
#include "hpl_server.h"

size_t curl_write_body_func(char *ptr, size_t size, size_t nmemb,
                            void *userdata);
size_t curl_write_headers_func(char *ptr, size_t size, size_t nmemb,
                               void *userdata);

class request_pipe_t {
public:
  request_pipe_t(CURLM *curlm, hpl::Connection *conn, const std::string &uri)
      : curlm_(curlm), conn_(conn), curl_(curl_easy_init()) {
    curl_easy_setopt(curl_, CURLOPT_URL,
                     (std::string("https://qq.com") + uri).c_str());
    curl_easy_setopt(curl_, CURLOPT_WRITEDATA, this);
    curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, curl_write_body_func);
    curl_easy_setopt(curl_, CURLOPT_HEADERFUNCTION, curl_write_headers_func);
    curl_easy_setopt(curl_, CURLOPT_HEADERDATA, this);
    curl_multi_add_handle(curlm_, curl_);
    LOG_TRACE("curl_multi_add_handle {}", fmt::ptr(curl_));
  }

  size_t curl_write_body(char *ptr, size_t size, size_t nmemb) {
    LOG_TRACE("curl_write_body: [{}]", ptr);
    conn_->Write(ptr, size * nmemb);
    std::move(*conn_).Close();
    return size * nmemb;
  }

  size_t curl_write_headers(char *ptr, size_t size, size_t nmemb) {
    LOG_TRACE("curl_write_headers: [{}]", ptr);
    conn_->Write(ptr, size * nmemb);
    return size * nmemb;
  }
  ~request_pipe_t() {
    if (curl_) {
      curl_multi_remove_handle(curlm_, curl_);
      LOG_TRACE("curl_multi_remove_handle {}", fmt::ptr(curl_));
      curl_easy_cleanup(curl_);
    }
  }
  CURLM *curlm_ = nullptr;
  hpl::Connection *conn_ = nullptr;
  CURL *curl_ = nullptr;
};

struct ServerContext {
  hpl::Handler getter = [this](hpl::Connection *conn,
                               const std::string_view &uri,
                               std::string &&partial, bool is_final) {
    req_pipes_.push_back(std::unique_ptr<request_pipe_t>(
        new request_pipe_t(curlm, conn, uri.data())));

    return 0;
  };
  hpl::Handler poster = [](hpl::Connection *conn, const std::string_view &uri,
                           std::string &&partial, bool is_final) {
    printf("poster: %s\n", uri.data());
    return 0;
  };

  hpl::RequestHandler request_handler;

  std::vector<std::unique_ptr<request_pipe_t>> req_pipes_;
  CURLM *curlm = nullptr;

  size_t curl_pipe_to_conn_body(char *ptr, size_t size, size_t nmemb,
                                request_pipe_t *req_pipe) {
    auto iter = std::find_if(req_pipes_.begin(), req_pipes_.end(),
                             [&](auto &p) { return p.get() == req_pipe; });
    if (iter == req_pipes_.end()) {
      LOG_ERROR("request_pipe_map.find failed");
      return size * nmemb;
    }
    auto ret = req_pipe->curl_write_body(ptr, size, nmemb);
    LOG_INFO("remove request_pipe_t {}, curl:{}", fmt::ptr(req_pipe),
             fmt::ptr(req_pipe->curl_));
    req_pipes_.erase(iter);
    return ret;
  }

  size_t curl_pipe_to_conn_headers(char *ptr, size_t size, size_t nmemb,
                                   request_pipe_t *req_pipe) {
    auto iter = std::find_if(req_pipes_.begin(), req_pipes_.end(),
                             [&](auto &p) { return p.get() == req_pipe; });
    if (iter == req_pipes_.end()) {
      LOG_ERROR("request_pipe_map.find failed");
      return size * nmemb;
    }
    auto ret = req_pipe->curl_write_headers(ptr, size, nmemb);
    return ret;
  }

  ServerContext() {
    curlm = curl_multi_init();
    request_handler.http_handlers[static_cast<int>(hpl::HttpMethod::GET)] =
        getter;
    request_handler.http_handlers[static_cast<int>(hpl::HttpMethod::POST)] =
        poster;
  }
  ~ServerContext() {
    if (curlm) {
      curl_multi_cleanup(curlm);
    }
  }
};

ServerContext *g_context = nullptr;

size_t curl_write_body_func(char *ptr, size_t size, size_t nmemb,
                            void *userdata) {
  request_pipe_t *request_pipe = (request_pipe_t *)userdata;
  return g_context->curl_pipe_to_conn_body(ptr, size, nmemb, request_pipe);
}
size_t curl_write_headers_func(char *ptr, size_t size, size_t nmemb,
                               void *userdata) {
  request_pipe_t *request_pipe = (request_pipe_t *)userdata;
  return g_context->curl_pipe_to_conn_headers(ptr, size, nmemb, request_pipe);
}

int main(int argc, char **argv) {
  hpl::Server server;
  server.Init("192.168.64.9", 2999, 50);

  g_context = new ServerContext();
  server.RegisterRequestHandler("*", std::move(g_context->request_handler));

  int running_handles = 0;
  while (true) {
    server.Poll(50);
    curl_multi_perform(g_context->curlm, &running_handles);
  }

  return 0;
}