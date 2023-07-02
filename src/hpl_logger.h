#pragma once

#ifndef _HPL_LOGGER_H_
#define _HPL_LOGGER_H_
#include <fmt/chrono.h>
#include <fmt/core.h>
#include <fmt/format.h>

#define __FILENAME__                                                           \
  (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define LOG_RAW(prefix, format, ...)                                           \
  do {                                                                         \
    struct timespec ts;                                                        \
    clock_gettime(CLOCK_REALTIME, &ts);                                        \
    fmt::print(stderr, prefix "|{:%T}.{}|{}:{}|" format "\n",                  \
               fmt::localtime(ts.tv_sec), ts.tv_nsec / 100000, __FILENAME__,   \
               __LINE__, ##__VA_ARGS__);                                       \
  } while (0)

#define LOG_TRACE(fmt, ...) LOG_RAW("\033[1;30m[T]\033[0m", fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) LOG_RAW("\033[1;34m[D]\033[0m", fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) LOG_RAW("\033[1;32m[I]\033[0m", fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) LOG_RAW("\033[1;33m[W]\033[0m", fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) LOG_RAW("\033[1;31m[E]\033[0m", fmt, ##__VA_ARGS__)
#define LOG_FATAL(fmt, ...) LOG_RAW("\033[1;35m[F]\033[0m", fmt, ##__VA_ARGS__)

#endif // _HPL_LOGGER_H_