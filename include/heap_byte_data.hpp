#pragma once

#include "types.hpp"
#include <stdlib.h>
#include <string.h>

struct HeapByteData {
  u8 *data;

  HeapByteData(u32 size) { data = static_cast<u8 *>(malloc(sizeof(u8) * size)); }
  
  HeapByteData(u32 size, u8 val) : HeapByteData(size) {
    memset(data, val, sizeof(u8) * size);
  }

  ~HeapByteData() {
    free(data);
    data = nullptr;
  }
};
