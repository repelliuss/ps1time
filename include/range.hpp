#pragma once

#include "types.hpp"

struct Range {
  u32 beg;
  u32 end;

  int offset(u32 &out, u32 addr) const;
};
