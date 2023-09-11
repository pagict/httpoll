#pragma once
#ifndef _HPL_HTTP_REQUEST_H_
#define _HPL_HTTP_REQUEST_H_

#include "hpl_header_parser.h"

namespace hpl {

class TcpConnection;

class HttpRequest {
public:
  explicit HttpRequest(TcpConnection *conn);
  inline const HttpHeaderParser &GetParser() const { return parser_; }

private:
  TcpConnection *conn_ = nullptr;
  HttpHeaderParser parser_;
};
} // namespace hpl
#endif // _HPL_HTTP_REQUEST_H_