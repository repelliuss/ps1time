#pragma once

#include "types.hpp"
#include "range.hpp"

struct DMA {
  static constexpr u32 size = 0x80;
  static constexpr Range range = {0x1f801080, 0x1f801100};
  static constexpr u32 reg_control = 0x70;
  static constexpr u32 reg_interrupt = 0x74;
  u8 data[size] = {0};

  DMA();

  bool irq_active();
  void set_interrupt(u32 val);
};
