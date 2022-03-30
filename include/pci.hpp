#pragma once

#include "types.hpp"
#include "range.hpp"
#include "bios.hpp"

enum PCIMatch {
  none = -1,
  bios,
  hw_regs
};

struct HWRegs {
  static constexpr u32 size = 8192;
  static constexpr Range range = {0x1f801000, 0x1f801024};
  u8 data[size];
};

// Peripheral Component Interconnect
struct PCI {
  Bios bios;
  HWRegs hw_regs;
  
  PCIMatch match(u8*& out_data, u32& offset, u32 addr);
  bool prohibited(PCIMatch match, u32 offset, u32 value);
};
