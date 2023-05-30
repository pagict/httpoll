#include "hpl_websocket_utils.h"

namespace hpl {
std::pair<char[16], size_t> MakeWebsocketHeader(size_t message_length,
                                                WsFrameType type) {
  std::pair<char[16], size_t> ret;
  char *header = ret.first;
  auto &header_size = ret.second;

  header[0] = 0x80 | type;
  if (message_length < 126) {
    header[1] = message_length;
    header_size = 2;
  } else if (message_length < 0xffff) {
    header[1] = 126;
    header[2] = (message_length >> 8) & 0xff;
    header[3] = message_length & 0xff;
    header_size = 4;
  } else {
    header[1] = 127;
    header[2] = (message_length >> 56) & 0xff;
    header[3] = (message_length >> 48) & 0xff;
    header[4] = (message_length >> 40) & 0xff;
    header_size = 6;
  }
  return ret;
}
} // namespace hpl