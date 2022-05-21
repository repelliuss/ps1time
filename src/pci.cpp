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

PCIType PCI::match(u8 *&out_data, u32 &offset, u32 addr) {
  addr = mask_addr_to_region(addr);

  if (!Bios::range.offset(offset, addr)) {
    out_data = bios.data;
    return PCIType::bios;
  }

  if (!MemCtrl::range.offset(offset, addr)) {
    log_pci("PCI::MemCtrl matched!\n");
    out_data = hw_regs.data;
    return PCIType::mem_ctrl;
  }

  if (!RamSize::range.offset(offset, addr)) {
    log_pci("PCI::RamSize matched!\n");
    out_data = ram_size.data;
    return PCIType::ram_size;
  }

  if (!CacheCtrl::range.offset(offset, addr)) {
    log_pci("PCI::CacheCtrl matched!\n");
    out_data = cache_ctrl.data;
    return PCIType::cache_ctrl;
  }

  if (!RAM::range.offset(offset, addr)) {
    log_pci("PCI::RAM matched!\n");
    out_data = ram.data;
    return PCIType::ram;
  }

  if (!SPU::range.offset(offset, addr)) {
    log_pci("PCI::SPU matched!\n");
    out_data = spu.data;
    return PCIType::spu;
  }

  if (!Expansion1::range.offset(offset, addr)) {
    log_pci("PCI::Expansion1 matched!\n");
    out_data = expansion1.data;
    return PCIType::expansion1;
  }

  if (!Expansion2::range.offset(offset, addr)) {
    log_pci("PCI::Expansion2 matched!\n");
    out_data = expansion2.data;
    return PCIType::expansion2;
  }

  if (!IRQ::range.offset(offset, addr)) {
    log_pci("PCI::IRQ matched!\n");
    out_data = irq.data;
    return PCIType::irq;
  }

  if (!Timers::range.offset(offset, addr)) {
    log_pci("PCI::Timers matched!\n");
    out_data = timers.data;
    return PCIType::timers;
  }

  if (!DMA::range.offset(offset, addr)) {
    log_pci("PCI::DMA matched!\n");
    out_data = dma.data;
    return PCIType::dma;
  }

  if (!GPU::range.offset(offset, addr)) {
    log_pci("PCI::GPU matched!\n");
    out_data = gpu.data;
    return PCIType::gpu;
  }

  // TODO: remove printf
  printf("No PCIMatch for addr: %#8x\n", addr);

  return PCIType::none;
}

int PCI::store32_data(PCIType match, u8 *data, u32 offset, u32 val) {
  switch (match) {
  case PCIType::gpu:
    switch (offset) {
    case 0:
      return gpu.gp0(val);
    case 4:
      return gpu.gp1(val);
    }
    assert(false);
    return -1;


    // TODO: logic here can be reduced with constraints to 0xZ0 where Z is
    // [0,6]?
  case PCIType::dma:
    switch (offset) {
    case DMA::reg::interrupt:
      dma.set_interrupt(val);
      break;

    case DMA::reg::mdecin_base_address:
    case DMA::reg::mdecout_base_address:
    case DMA::reg::gpu_base_address:
    case DMA::reg::cdrom_base_address:
    case DMA::reg::spu_base_address:
    case DMA::reg::pio_base_address:
    case DMA::reg::otc_base_address:
      dma.set_base_addr(offset, val);
      break;

    case DMA::reg::mdecin_channel_control:
    case DMA::reg::mdecout_channel_control:
    case DMA::reg::gpu_channel_control:
    case DMA::reg::cdrom_channel_control:
    case DMA::reg::spu_channel_control:
    case DMA::reg::pio_channel_control:
    case DMA::reg::otc_channel_control:
      memory::store32(data, val, offset);
      {
        DMA::ChannelView channel = dma.make_channel_view(offset);
        return dma.try_transfer(channel);
      }

      break;

    default:
      memory::store32(data, val, offset);
      break;
    }
    break;


  default:
    memory::store32(data, val, offset);
    break;
  }

  return 0;
}

// REVIEW: shouldn't return error value, use prohibitions
int PCI::load32_data(PCIType match, u8 *data, u32 offset) {
  switch (match) {
  case PCIType::irq:
  case PCIType::timers:
    return 0;

  case PCIType::gpu:
    switch(offset) {
    case 0:
      return gpu.read();
    case 4:
      return gpu.status();
    }
    assert(false);
    return -1;

  default:
    return memory::load32(data, offset);
  }
}
