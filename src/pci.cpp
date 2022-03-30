#include "pci.hpp"

#include <cassert>
#include <cstdio>

PCIMatch PCI::match(u8*& out_data, u32& offset, u32 addr) {
  if(!Bios::range.offset(offset, addr)) {
    out_data = bios.data;
    return PCIMatch::bios;
  }

  if(!HWRegs::range.offset(offset, addr)) {
    out_data = hw_regs.data;
    return PCIMatch::hw_regs;
  }

  // TODO: remove printf
  printf("No PCIMatch for addr: %#8x\n", addr);
  
  return PCIMatch::none;
}

// TODO: remove this usage design
bool PCI::prohibited(PCIMatch match, u32 offset, u32 value) {
  assert(match != PCIMatch::none);

  // REVIEW: simias doc says no other than this values are written so can this be removed?
  if(match == PCIMatch::hw_regs) {
    if ((offset == 0 && value != 0x1f000000) ||
	(offset == 4 && value != 0x1f802000)) {
      printf("Prohibited writing to hw_regs! addr: %#8x, val: %#8x\n",
	     HWRegs::range.beg + offset, value);
      return false;
    }
  }

  return true;
}
