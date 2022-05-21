#include "pci.hpp"
#include "data.hpp"
#include "log.hpp"

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

template <typename LoadType>
constexpr int ignore_load_with(LoadType &val, const char *fn, u32 addr,
                               u32 index, const char *msg, LoadType with) {
  LOG_DEBUG("[FN:%s ADDR:0x%08x IND:%d] %s", fn, addr, index, msg);
  val = with;
  return 0;
}

int PCI::load32(u32 &val, u32 addr) {
  static const char *fn = "PCI::load32";
  u32 index;
  
  addr = mask_addr_to_region(addr);

  if (!Bios::range.offset(index, addr)) {
    val = memory::load32(bios.data, index);
    return 0;
  }

  if (!MemCtrl::range.offset(index, addr)) {
    LOG_ERROR("[FN:%s ADDR:0x%08x MASKED_ADDR:0x%08x IND:%d] %s", fn, addr,
              addr, index, "MemCtrl");
    return -1;
  }

  if (!RamSize::range.offset(index, addr)) {
    LOG_ERROR("[FN:%s ADDR:0x%08x MASKED_ADDR:0x%08x IND:%d] %s", fn, addr,
              addr, index, "RamSize");
    return -1;
  }

  if (!CacheCtrl::range.offset(index, addr)) {
    LOG_ERROR("[FN:%s ADDR:0x%08x MASKED_ADDR:0x%08x IND:%d] %s", fn, addr,
              addr, index, "CacheCtrl");
    return -1;
  }

  if (!RAM::range.offset(index, addr)) {
    val = memory::load32(ram.data, index);
    return 0;
  }

  if (!SPU::range.offset(index, addr)) {
    LOG_ERROR("[FN:%s ADDR:0x%08x MASKED_ADDR:0x%08x IND:%d] %s", fn, addr,
              addr, index, "SPU");
    return -1;
  }

  if (!Expansion1::range.offset(index, addr)) {
    LOG_ERROR("[FN:%s ADDR:0x%08x MASKED_ADDR:0x%08x IND:%d] %s", fn, addr,
              addr, index, "Expansion1");
    return -1;
  }

  if (!Expansion2::range.offset(index, addr)) {
    LOG_ERROR("[FN:%s ADDR:0x%08x MASKED_ADDR:0x%08x IND:%d] %s", fn, addr,
              addr, index, "Expansion2");
    return -1;
  }

  if (!IRQ::range.offset(index, addr)) {
    return ignore_load_with<u32>(val, fn, addr, index, "IRQ", 0);
  }

  if (!Timers::range.offset(index, addr)) {
    return ignore_load_with<u32>(val, fn, addr, index, "Timers", 0);
  }
  {}
  if (!DMA::range.offset(index, addr)) {
    return dma.load32(val, index);
  }

  if (!GPU::range.offset(index, addr)) {
    return gpu.load32(val, index);
  }

  LOG_ERROR("[FN:%s ADDR:0x%08x MASKED_ADDR:0x%08x] Unhandled", fn, addr,
            addr);
  return -1;
}

