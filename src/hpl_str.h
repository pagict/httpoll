#pragma once
#include <algorithm>
#include <iterator>
#include <string>

#ifndef _HPL_STR_H_
#define _HPL_STR_H_

namespace hpl {
inline bool icase_cmp(const std::string_view &s1, const std::string_view &s2) {
  auto distance = s1.size() < s2.size() ? s1.size() : s2.size();
  auto s1_end = std::next(s1.begin(), distance);
  auto s2_end = std::next(s2.begin(), distance);
  return std::equal(
      s1.begin(), s1_end, s2.begin(), s2_end,
      [](auto c1, auto c2) { return std::tolower(c1) == std::tolower(c2); });
}

inline auto icase_find(const std::string_view &s1,
                       std::string_view::difference_type s1_off,
                       const std::string_view &s2) {
  auto begin_iter = std::next(s1.begin(), s1_off);
  return std::search(
      begin_iter, s1.end(), s2.begin(), s2.end(),
      [](auto c1, auto c2) { return std::tolower(c1) == std::tolower(c2); });
}
} // namespace hpl

#endif // _HPL_STR_H_