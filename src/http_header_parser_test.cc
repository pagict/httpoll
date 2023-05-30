#include <gtest/gtest.h>

#include "hpl_header_parser.h"
TEST(http_header_parser, test) {

  auto lines = {"GET / HTTP/1.1\r\n",
                "Host: myserver.com\r\n",
                "Connection: Upgrade, keep-alive\r\n",
                "Content-Length:13\r\n",
                "Upgrade: websocket\r\n",
                "User-Agent: Mozilla/5.0\r\n",
                "Extra-Header: some data\r\n",
                "\r\n",
                "ddddsome dataoverflowed\r\n"};

  hpl::HttpHeaderParser parser;
  for (auto &line : lines) {
    parser.PushLine(std::move(line));
  }

  EXPECT_EQ(parser.GetState(), hpl::HttpHeaderParser::ParserState::Done);
  EXPECT_EQ(parser.GetMethod(), hpl::HttpMethod::GET);
  EXPECT_STREQ(parser.GetUri().c_str(), "/");
  EXPECT_STREQ(parser.GetHost().c_str(), "myserver.com");
  EXPECT_STREQ(parser.GetUserAgent().c_str(), "Mozilla/5.0");
  EXPECT_EQ(parser.GetVersion(), hpl::HttpVersion::HTTP_1_1);
  EXPECT_EQ(parser.GetContentLength(), 13);
  EXPECT_EQ(parser.GetUpgradeFlags(),
            hpl::HttpHeaderParser::UpgradeFlags::UpgradeWebSocket);
  EXPECT_EQ(parser.GetConnectionFlags(),
            hpl::HttpHeaderParser::ConnectionFlags::ConnectionUpgrade |
                hpl::HttpHeaderParser::ConnectionFlags::ConnectionKeepAlive);
  const auto &headers = parser.GetHeaders();
  EXPECT_EQ(headers.size(), 1);
  EXPECT_STREQ(headers.at("Extra-Header").c_str(), "some data");
  const auto &body = parser.GetBody();
  EXPECT_EQ(body.size(), 13);
}