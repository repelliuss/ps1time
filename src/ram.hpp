#pragma once

#include "heap_byte_data.hpp"
#include "types.hpp"
#include "range.hpp"

struct RAM : HeapByteData {
  static constexpr u32 size = 2097152; // 2MB
  static constexpr Range range = {0x00000000, 0x00200000};
  RAM() : HeapByteData(size, 0xca) {} // initial garbage content value
};

