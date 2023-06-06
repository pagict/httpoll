#include "hpl_websocket_connection.h"

#include <cstdint>
#include <netinet/in.h>
#include <openssl/sha.h>
#include <sys/types.h>
#include <vector>

#include "external_helpers.h"
#include "hpl_connection.h"
#include "hpl_logger.h"
#include "hpl_request_handler.h"
#include "hpl_server.h"
#include "hpl_websocket_utils.h"

namespace hpl {
std::string WebsocketConnection::GetWsAcceptByKey(std::string_view ws_sec_key) {
  std::string ret;
  unsigned char sha1[SHA_DIGEST_LENGTH];
  auto tmp = std::string(ws_sec_key) + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  SHA1(reinterpret_cast<const unsigned char *>(tmp.c_str()), tmp.size(), sha1);
  std::string dbg_str;
  for (int i = 0; i < SHA_DIGEST_LENGTH; ++i) {
    (sha1[i] / 16) < 10 ? dbg_str.push_back((sha1[i] / 16) + '0')
                        : dbg_str.push_back((sha1[i] / 16 - 10) + 'a');
    (sha1[i] % 16) < 10 ? dbg_str.push_back((sha1[i] % 16) + '0')
                        : dbg_str.push_back((sha1[i] % 16 - 10) + 'a');
  }
  LOG_DEBUG("sha1: 0x{}", dbg_str);
  char hash[SHA_DIGEST_LENGTH * 2];
  size_t hash_len = SHA_DIGEST_LENGTH * 2;
  base64_encode(sha1, SHA_DIGEST_LENGTH, hash, &hash_len);
  ret = std::string(hash, hash_len - 1);
  return ret;
}

int WebsocketConnection::GetDescriptor() const {
  return conn_ ? conn_->GetDescriptor() : -1;
}

int WebsocketConnection::Read() {
  if (!conn_) {
    return -1;
  }
  auto ret = conn_->Read();
  if (ret < 0) {
    return ret;
  }
  std::vector<char> buf = std::move(conn_->buffer_);
#ifdef HPL_ENABLE_PING_PONG
  last_io_time_ = std::chrono::steady_clock::now();
#endif // HPL_ENABLE_PING_PONG
  struct WsFrameHeader *header =
      reinterpret_cast<struct WsFrameHeader *>(buf.data());
#pragma pack(1)
  struct WsFrameHeader {
    unsigned opcode : 4;
    unsigned rsv3 : 1;
    unsigned rsv2 : 1;
    unsigned rsv1 : 1;
    unsigned fin : 1;

    unsigned payload_prefix : 7;
    unsigned mask : 1;
  };
#pragma pack()
  off_t offset = sizeof(struct WsFrameHeader);
  uint64_t payload_len = header->payload_prefix;
  if (header->payload_prefix == 126) {
    payload_len = ntohs(*reinterpret_cast<uint16_t *>(buf.data() + offset));
    offset += 2;
  } else if (header->payload_prefix == 127) {
    auto l = ntohl(*reinterpret_cast<uint64_t *>(buf.data() + offset));
    auto h = ntohl(*reinterpret_cast<uint64_t *>(buf.data() + offset + 4));
    payload_len = (static_cast<uint64_t>(h) << 32) | l;
    offset += 8;
  }
  unsigned char mask[4];
  if (header->mask) {
    memcpy(mask, buf.data() + offset, 4);
    offset += 4;
  }
  auto i = 0;
  for (i = 0; i < payload_len && header->mask; ++i) {
    buf[offset + i] ^= mask[i & 3];
  }
  last_payload_.append(buf.data() + offset, payload_len);
  LOG_DEBUG("last_payload_: {}", last_payload_);
  if (header->opcode == 0x8) {
    // close
    return -1;
  }
#ifdef HPL_ENABLE_PING_PONG
  if (header->opcode == WsFrameType::kTypePing) {
    // ping
    Write(WsFrameType::kTypePong, buf.data() + offset, payload_len);
    LOG_TRACE("pong to {}", conn_->fd_);

    last_payload_.clear();
    return 0;
  } else if (header->opcode == WsFrameType::kTypePong) {
    // pong
    LOG_TRACE("pong from {}", conn_->fd_);
    last_payload_.clear();
    return 0;
  }
#endif // HPL_ENABLE_PING_PONG

  if (header->fin) {
    int ret = 0;
    if (on_data_) {
      ret = on_data_(this, static_cast<WsFrameType>(header->opcode),
                     last_payload_);
    }
    last_payload_.clear();
    return ret;
  } else {
    return 1;
  }
}

int WebsocketConnection::Write(WsFrameType type, const char *data, size_t len) {
  auto [header, header_len] = MakeWebsocketHeader(len, type);
  conn_->Write(header, header_len);
  conn_->Write(data, len);
#ifdef HPL_ENABLE_PING_PONG
  last_io_time_ = std::chrono::steady_clock::now();
#endif // HPL_ENABLE_PING_PONG
  return 0;
}

bool WebsocketConnection::ServerPing() {
#ifndef HPL_ENABLE_PING_PONG
  return false;
#else  // HPL_ENABLE_PING_PONG

  if (!conn_) {
    return false;
  }
  auto now = std::chrono::steady_clock::now();
  if (now - last_io_time_ > std::chrono::seconds(HPL_ENABLE_PING_PONG)) {
    Write(WsFrameType::kTypePing, nullptr, 0);
    last_io_time_ = now;
    return true;
  }
#endif // HPL_ENABLE_PING_PONG
  return false;
}
} // namespace hpl