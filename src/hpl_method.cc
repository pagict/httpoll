#include "hpl_method.h"

#include "hpl_logger.h"
namespace hpl {

HttpMethod ParseHttpMethod(std::string_view method, size_t pos,
                           size_t &end_pos) {
  constexpr int IntGET = 'G' | 'E' << 8 | 'T' << 16;
  int v = (*(int *)(method.data() + pos));
  if ((v & 0x00ffffff) == IntGET) {
    end_pos = pos + 3;
    return HttpMethod::GET;
  }

  constexpr int IntPUT = 'P' | 'U' << 8 | 'T' << 16;
  if ((v & 0x00ffffff) == IntPUT) {
    end_pos = pos + 3;
    return HttpMethod::PUT;
  }
  constexpr int IntPOST = 'P' | 'O' << 8 | 'S' << 16 | 'T' << 24;
  if (v == IntPOST) {
    end_pos = pos + 4;
    return HttpMethod::POST;
  }

  return HttpMethod::UNKNOWN;
}
} // namespace hpl