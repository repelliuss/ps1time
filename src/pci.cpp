#include "pci.hpp"
#include "data.hpp"

#include <cassert>
#include <cstdio>

static u32 mask_addr_to_region(u32 addr) {
  static const u32 region_mask[8] = {// KUSEG: 2048MB
                                     0xffffffff, 0xffffffff, 0xffffffff,
                                     0xffffffff,
                                     // KSEG0: 512MB
                                     0x7fffffff,
                                     // KSEG1: 512MB
                                     0x1fffffff,
                                     // KSEG2: 1024MB
                                     0xffffffff, 0xffffffff};

  return addr & region_mask[addr >> 29];
}

static constexpr void log_pci(const char *msg) {
  // printf("%s", msg);
}

PCIMatch PCI::match(u8 *&out_data, u32 &offset, u32 addr) {
  addr = mask_addr_to_region(addr);

  if (!Bios::range.offset(offset, addr)) {
    out_data = bios.data;
    return PCIMatch::bios;
  }

  if (!MemCtrl::range.offset(offset, addr)) {
    log_pci("PCI::MemCtrl matched!\n");
    out_data = hw_regs.data;
    return PCIMatch::mem_ctrl;
  }

  if (!RamSize::range.offset(offset, addr)) {
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

  if (!Timers::range.offset(offset, addr)) {
    log_pci("PCI::Timers matched!\n");
    out_data = timers.data;
    return PCIMatch::timers;
  }

  if (!DMA::range.offset(offset, addr)) {
    log_pci("PCI::DMA matched!\n");
    out_data = dma.data;
    return PCIMatch::dma;
  }

  if (!GPU::range.offset(offset, addr)) {
    log_pci("PCI::GPU matched!\n");
    out_data = gpu.data;
    return PCIMatch::gpu;
  }

  // TODO: remove printf
  printf("No PCIMatch for addr: %#8x\n", addr);

  return PCIMatch::none;
}

void PCI::store32_data(PCIMatch match, u8 *data, u32 offset, u32 val) {
  switch (match) {
    
  case PCIMatch::dma:
    switch (offset) {
    case DMA::reg::interrupt:
      dma.set_interrupt(val);
      break;
    default:
      store32(data, val, offset);
    }

  default:
    store32(data, val, offset);
    break;
  }
}
