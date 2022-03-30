#pragma once

#include "types.hpp"
#include "bios.hpp"
#include "pci.hpp"
#include "instruction.hpp"

struct CPU {
  u32 pc = 0xbfc00000;
  u32 regs[32] = {0};
  PCI pci;

  int next();
  u32 fetch(u32 addr);
  int decode_execute(Instruction instruction);

  void lui(Instruction instruction);
  void ori(Instruction instruction);
  int sw(Instruction instruction);
};

