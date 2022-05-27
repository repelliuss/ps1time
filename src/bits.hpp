#pragma once

#include "types.hpp"

#include <assert.h>

constexpr unsigned bits_count_mask(unsigned beg, unsigned end) {
  assert(beg >= 0 && beg < 32);
  assert(end >= 0 && end < 32);
  return (1U << (end - beg + 1U)) - 1U;
}

constexpr u32 bits_in_range(u32 data, unsigned beg, unsigned end) {
  return (data >> beg) & bits_count_mask(beg, end);
}

constexpr void bits_copy_to_range(u32 &data, unsigned beg, unsigned end, u32 val) {
  data = (data & ~(bits_count_mask(beg, end) << beg)) | (val << beg);
}

constexpr u32 bit(u32 data, unsigned at) {
  assert(at >= 0 && at < 32);
  return (data >> at) & 1;
}

constexpr void bit_copy_to(u32 &data, unsigned at, unsigned bit) {
  assert(at >= 0 && at < 32);
  data = (data & ~(1U << at)) | (bit << at);
}

constexpr void bit_set(u32 &data, unsigned at) {
  assert(at >= 0 && at < 32);
  data |= 1U << at;
}

constexpr void bit_clear(u32 &data, unsigned at) {
  assert(at >= 0 && at < 32);
  data &= ~(1U << at);
}
