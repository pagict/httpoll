#include "hpl_utils.h"

#include <fcntl.h>
#include <stdio.h>

namespace hpl {

int setnonblocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1) {
    perror("fcntl get");
    return -1;
  }
  if (flags & O_NONBLOCK) {
    return 0;
  }
  if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
    perror("fcntl set");
    return -1;
  }
  return 0;
}
} // namespace hpl
