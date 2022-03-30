#pragma once

#include "types.hpp"
#include "range.hpp"

struct Bios {
  static constexpr u32 size = 524288;
  static constexpr Range range = {0xbfc00000, 0xbfc80000};
  u8 data[size];
};
