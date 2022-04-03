#include "pci.hpp"

#include <cassert>
#include <cstdio>

static u32 mask_addr_to_region(u32 addr) {
  static const u32 region_mask[8] = {
    // KUSEG: 2048MB
    0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
    // KSEG0: 512MB
    0x7fffffff,
    // KSEG1: 512MB
    0x1fffffff,
    // KSEG2: 1024MB
    0xffffffff, 0xffffffff};
  
  return addr & region_mask[addr >> 29];
}

static constexpr void log_pci(const char* msg) {
  //printf("%s", msg);
}

PCIMatch PCI::match(u8*& out_data, u32& offset, u32 addr) {
  addr = mask_addr_to_region(addr);
  
  if(!Bios::range.offset(offset, addr)) {
    out_data = bios.data;
    return PCIMatch::bios;
  }

  if(!MemCtrl::range.offset(offset, addr)) {
    log_pci("PCI::MemCtrl matched!\n");
    out_data = hw_regs.data;
    return PCIMatch::mem_ctrl;
  }

  if(!RamSize::range.offset(offset, addr)) {
    log_pci("PCI::RamSize matched!\n");
    out_data = ram_size.data;
    return PCIMatch::ram_size;
  }

  if (!CacheCtrl::range.offset(offset, addr)) {
    log_pci("PCI::CacheCtrl matched!\n");
    out_data = cache_ctrl.data;
    return PCIMatch::cache_ctrl;
  }

  if (!RAM::range.offset(offset, addr)) {
    log_pci("PCI::RAM matched!\n");
    out_data = ram.data;
    return PCIMatch::ram;
  }

  if (!SPU::range.offset(offset, addr)) {
    log_pci("PCI::SPU matched!\n");
    out_data = spu.data;
    return PCIMatch::spu;
  }

  if (!Expansion1::range.offset(offset, addr)) {
    log_pci("PCI::Expansion1 matched!\n");
    out_data = expansion1.data;
    return PCIMatch::expansion1;
  }

  if (!Expansion2::range.offset(offset, addr)) {
    log_pci("PCI::Expansion2 matched!\n");
    out_data = expansion2.data;
    return PCIMatch::expansion2;
  }

  if (!IRQ::range.offset(offset, addr)) {
    log_pci("PCI::IRQ matched!\n");
    out_data = irq.data;
    return PCIMatch::irq;
  }

  // TODO: remove printf
  printf("No PCIMatch for addr: %#8x\n", addr);
  
  return PCIMatch::none;
}

// TODO: remove this usage design
int PCI::prohibited(PCIMatch match, u32 offset, u32 value) {
  assert(match != PCIMatch::none);

  // REVIEW: simias doc says no other than this values are written so can this be removed?
  if(match == PCIMatch::mem_ctrl) {
    if ((offset == 0 && value != 0x1f000000) ||
	(offset == 4 && value != 0x1f802000)) {
      printf("Prohibited writing to mem_ctrl! addr: %#8x, val: %#8x\n",
	     MemCtrl::range.beg + offset, value);
      return -1;
    }
  }

  if(match == PCIMatch::spu) {
    printf("Prohibited writing to spu! addr: %#8x, val: %#8x\n",
	   SPU::range.beg + offset, value);
    return -1;
  }

  if(match == PCIMatch::expansion2) {
    printf("Prohibited writing to expansion2! addr: %#8x, val: %#8x\n",
	   Expansion2::range.beg + offset, value);
    return -1;
  }

  if(match == PCIMatch::irq) {
    printf("Prohibited writing to IRQ! addr: %#8x, val: %#8x\n",
	   IRQ::range.beg + offset, value);
    return -1;
  }

  return 0;
}
