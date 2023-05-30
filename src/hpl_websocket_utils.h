#pragma once
#ifndef HPL_WEBSOCKET_UTILS_H
#define HPL_WEBSOCKET_UTILS_H

#include <stddef.h>

#include <utility>

#include "hpl_request_handler.h"

namespace hpl {
std::pair<char[16], size_t> MakeWebsocketHeader(size_t message_length,
                                                WsFrameType type);
} // namespace hpl
#endif // HPL_WEBSOCKET_UTILS_H