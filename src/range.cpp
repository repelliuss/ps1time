#include "range.hpp"
#include "types.hpp"

int Range::offset(u32 &out, u32 addr) const {
  if (addr >= beg && addr < end) {
    out = addr - beg;
    return 0;
  }

  return -1;
}

