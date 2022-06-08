#include "pci.hpp"
#include "data.hpp"
#include "log.hpp"

#include <cassert>
#include <cstdio>

void PCI::clock_sync(Clock &clock) {
  if (clock.alarmed(Clock::Host::gpu)) {
    gpu.clock_sync(clock, irq);
  }
}

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


int ignore_load_with(u32 &val, const char *fn, u32 addr, u32 index,
                               const char *msg, u32 with) {
  LOG_DEBUG("[FN:%s ADDR:0x%08x IND:%d] Ignored %s", fn, addr, index, msg);
  val = with;
  return 0;
}

template <typename StoreType>
constexpr int ignore_store(StoreType val, const char *fn, u32 addr, u32 index,
                           const char *msg) {
  LOG_DEBUG("[FN:%s ADDR:0x%08x IND:%d VAL:0x%08x] Ignored %s", fn, addr, index,
            val, msg);
  return 0;
}

int PCI::load32(u32 &val, u32 addr, Clock &clock) {
  static const char *fn = "PCI::load32";
  u32 index;

  addr = mask_addr_to_region(addr);

  if (!Bios::range.offset(index, addr)) {
    val = memory::load32(bios.data, index);
    return 0;
  }

  if (!CDROM::range.offset(index, addr)) {
    LOG_ERROR("[FN:%s ADDR:0x%08x IND:%d] %s", fn, addr, index, "CDROM");
    return -1;
  }

  if (!HWregs::range.offset(index, addr)) {
    LOG_ERROR("[FN:%s ADDR:0x%08x IND:%d] %s", fn, addr, index, "HWregs");
    return -1;
  }

  if (!RamSize::range.offset(index, addr)) {
    LOG_ERROR("[FN:%s ADDR:0x%08x IND:%d] %s", fn, addr, index, "RamSize");
    return -1;
  }

  if (!CacheCtrl::range.offset(index, addr)) {
    LOG_ERROR("[FN:%s ADDR:0x%08x IND:%d] %s", fn, addr, index, "CacheCtrl");
    return -1;
  }

  if (!RAM::range.offset(index, addr)) {
    val = memory::load32(ram.data, index);
    return 0;
  }

  if (!SPU::range.offset(index, addr)) {
    LOG_ERROR("[FN:%s ADDR:0x%08x IND:%d] %s", fn, addr, index, "SPU");
    return -1;
  }

  if (!Expansion1::range.offset(index, addr)) {
    LOG_ERROR("[FN:%s ADDR:0x%08x IND:%d] %s", fn, addr, index, "Expansion1");
    return -1;
  }

  if (!Expansion2::range.offset(index, addr)) {
    LOG_ERROR("[FN:%s ADDR:0x%08x IND:%d] %s", fn, addr, index, "Expansion2");
    return -1;
  }

  if (!IRQ::range.offset(index, addr)) {
    return irq.load32(val, index);
  }

  if (!Timers::range.offset(index, addr)) {
    return timers.load32(val, index, clock, irq);
  }
  
  if (!DMA::range.offset(index, addr)) {
    return dma.load32(val, index);
  }

  if (!GPU::range.offset(index, addr)) {
    return gpu.load32(val, index, clock, irq);
  }

  LOG_ERROR("[FN:%s ADDR:0x%08x] Unhandled", fn, addr);
  return -1;
}

int PCI::load16(u32 &val, u32 addr, Clock &clock) {
  static const char *fn = "PCI::load16";
  u32 index;

  addr = mask_addr_to_region(addr);

  if (!Bios::range.offset(index, addr)) {
    LOG_ERROR("[FN:%s ADDR:0x%08x IND:%d] %s", fn, addr, index, "Bios");
    return -1;
  }

  if (!CDROM::range.offset(index, addr)) {
    LOG_ERROR("[FN:%s ADDR:0x%08x IND:%d] %s", fn, addr, index, "CDROM");
    return -1;
  }

  if (!HWregs::range.offset(index, addr)) {
    LOG_ERROR("[FN:%s ADDR:0x%08x IND:%d] %s", fn, addr, index, "HWregs");
    return -1;
  }

  if (!RamSize::range.offset(index, addr)) {
    LOG_ERROR("[FN:%s ADDR:0x%08x IND:%d] %s", fn, addr, index, "RamSize");
    return -1;
  }

  if (!CacheCtrl::range.offset(index, addr)) {
    LOG_ERROR("[FN:%s ADDR:0x%08x IND:%d] %s", fn, addr, index, "CacheCtrl");
    return -1;
  }

  if (!RAM::range.offset(index, addr)) {
    val = memory::load16(ram.data, index);
    return 0;
  }

  if (!SPU::range.offset(index, addr)) {
    return ignore_load_with(val, fn, addr, index, "SPU", 0);
  }

  if (!Expansion1::range.offset(index, addr)) {
    LOG_ERROR("[FN:%s ADDR:0x%08x IND:%d] %s", fn, addr, index, "Expansion1");
    return -1;
  }

  if (!Expansion2::range.offset(index, addr)) {
    LOG_ERROR("[FN:%s ADDR:0x%08x IND:%d] %s", fn, addr, index, "Expansion2");
    return -1;
  }

  if (!IRQ::range.offset(index, addr)) {
    return irq.load16(val, index);
  }

  if (!Timers::range.offset(index, addr)) {
    return timers.load16(val, index, clock, irq);
  }

  if (!DMA::range.offset(index, addr)) {
    LOG_ERROR("[FN:%s ADDR:0x%08x IND:%d] %s", fn, addr, index, "DMA");
    return -1;
  }

  if (!GPU::range.offset(index, addr)) {
    LOG_ERROR("[FN:%s ADDR:0x%08x IND:%d] %s", fn, addr, index, "GPU");
    return -1;
  }

  LOG_ERROR("[FN:%s ADDR:0x%08x] Unhandled", fn, addr);
  return -1;
}

int PCI::load8(u32 &val, u32 addr) {
  static const char *fn = "PCI::load8";
  u32 index;

  addr = mask_addr_to_region(addr);

  if (!Bios::range.offset(index, addr)) {
    val = memory::load8(bios.data, index);
    return 0;
  }

  if (!CDROM::range.offset(index, addr)) {
    return cdrom.load8(val, index);
  }

  if (!HWregs::range.offset(index, addr)) {
    LOG_ERROR("[FN:%s ADDR:0x%08x IND:%d] %s", fn, addr, index, "HWregs");
    return -1;
  }

  if (!RamSize::range.offset(index, addr)) {
    LOG_ERROR("[FN:%s ADDR:0x%08x IND:%d] %s", fn, addr, index, "RamSize");
    return -1;
  }

  if (!CacheCtrl::range.offset(index, addr)) {
    LOG_ERROR("[FN:%s ADDR:0x%08x IND:%d] %s", fn, addr, index, "CacheCtrl");
    return -1;
  }

  if (!RAM::range.offset(index, addr)) {
    val = memory::load8(ram.data, index);
    return 0;
  }

  if (!SPU::range.offset(index, addr)) {
    LOG_ERROR("[FN:%s ADDR:0x%08x IND:%d] %s", fn, addr, index, "SPU");
    return -1;
  }

  if (!Expansion1::range.offset(index, addr)) {
    return ignore_load_with(val, fn, addr, index, "Expansion1", 0xff);
  }

  if (!Expansion2::range.offset(index, addr)) {
    LOG_ERROR("[FN:%s ADDR:0x%08x IND:%d] %s", fn, addr, index, "Expansion2");
    return -1;
  }

  if (!IRQ::range.offset(index, addr)) {
    LOG_ERROR("[FN:%s ADDR:0x%08x IND:%d] %s", fn, addr, index, "IRQ");
    return -1;
  }

  if (!Timers::range.offset(index, addr)) {
    LOG_ERROR("[FN:%s ADDR:0x%08x IND:%d] %s", fn, addr, index, "Timers");
    return -1;
  }

  if (!DMA::range.offset(index, addr)) {
    LOG_ERROR("[FN:%s ADDR:0x%08x IND:%d] %s", fn, addr, index, "DMA");
    return -1;
  }

  if (!GPU::range.offset(index, addr)) {
    LOG_ERROR("[FN:%s ADDR:0x%08x IND:%d] %s", fn, addr, index, "GPU");
    return -1;
  }

  LOG_ERROR("[FN:%s ADDR:0x%08x] Unhandled", fn, addr);
  return -1;
}

int PCI::store32(u32 val, u32 addr, Clock &clock) {
  static const char *fn = "PCI::store32";
  u32 index;
  
  addr = mask_addr_to_region(addr);

  if (!Bios::range.offset(index, addr)) {
    LOG_ERROR("[FN:%s ADDR:0x%08x IND:%d VAL:0x%08x] %s", fn, addr, index, val,
              "Bios");
    return -1;
  }

  if (!HWregs::range.offset(index, addr)) {
    return hw_regs.store32(val, index);
  }

  if (!CDROM::range.offset(index, addr)) {
    LOG_ERROR("[FN:%s ADDR:0x%08x IND:%d] %s", fn, addr, index, "CDROM");
    return -1;
  }

  if (!RamSize::range.offset(index, addr)) {
    return ignore_store(val, fn, addr, index, "RamSize");
  }

  if (!JOYmemcard::range.offset(index, addr)) {
    return ignore_store(val, fn, addr, index, "JOYmemcard");
  }

  if (!CacheCtrl::range.offset(index, addr)) {
    cache_ctrl.val = val;
    return 0;
  }

  if (!RAM::range.offset(index, addr)) {
    memory::store32(ram.data, index, val);
    return 0;
  }

  if (!SPU::range.offset(index, addr)) {
    LOG_ERROR("[FN:%s ADDR:0x%08x IND:%d VAL:0x%08x] %s", fn, addr, index, val,
              "SPU");
    return -1;
  }

  if (!Expansion1::range.offset(index, addr)) {
    LOG_ERROR("[FN:%s ADDR:0x%08x IND:%d VAL:0x%08x] %s", fn, addr, index, val,
              "Expansion1");
    return -1;
  }

  // TODO: Rustation ignores store32 to expansion2
  if (!Expansion2::range.offset(index, addr)) {
    LOG_ERROR("[FN:%s ADDR:0x%08x IND:%d VAL:0x%08x] %s", fn, addr, index, val,
              "Expansion2");
    return -1;
  }

  if (!IRQ::range.offset(index, addr)) {
    return irq.store32(val, index);
  }

  if (!Timers::range.offset(index, addr)) {
    return timers.store32(val, index, clock, irq, gpu);
  }
  
  if (!DMA::range.offset(index, addr)) {
    return dma.store32(val, index, irq);
  }

  if (!GPU::range.offset(index, addr)) {
    return gpu.store32(val, index, &timers, clock, irq);
  }

  LOG_ERROR("[FN:%s ADDR:0x%08x VAL:0x%08x] Unhandled", fn, addr, val);
  return -1;
}

int PCI::store16(u16 val, u32 addr, Clock &clock) {
  static const char *fn = "PCI::store16";
  u32 index;
  
  addr = mask_addr_to_region(addr);

  if (!Bios::range.offset(index, addr)) {
    LOG_ERROR("[FN:%s ADDR:0x%08x IND:%d VAL:0x%08x] %s", fn, addr, index, val,
              "Bios");
    return -1;
  }

  if (!CDROM::range.offset(index, addr)) {
    LOG_ERROR("[FN:%s ADDR:0x%08x IND:%d] %s", fn, addr, index, "CDROM");
    return -1;
  }

  if (!HWregs::range.offset(index, addr)) {
    LOG_ERROR("[FN:%s ADDR:0x%08x IND:%d VAL:0x%08x] %s", fn, addr, index, val,
              "HWregs");
    return -1;
  }

  if (!RamSize::range.offset(index, addr)) {
    LOG_ERROR("[FN:%s ADDR:0x%08x IND:%d VAL:0x%08x] %s", fn, addr, index, val,
              "RamSize");
    return -1;
  }

  if (!JOYmemcard::range.offset(index, addr)) {
    return ignore_store(val, fn, addr, index, "JOYmemcard");
  }

  if (!CacheCtrl::range.offset(index, addr)) {
    LOG_ERROR("[FN:%s ADDR:0x%08x IND:%d VAL:0x%08x] %s", fn, addr, index, val,
              "CacheCtrl");
    return -1;
  }

  if (!RAM::range.offset(index, addr)) {
    memory::store16(ram.data, index, val);
    return 0;
  }

  if (!SPU::range.offset(index, addr)) {
    return ignore_store(val, fn, addr, index, "SPU");
  }

  if (!Expansion1::range.offset(index, addr)) {
    LOG_ERROR("[FN:%s ADDR:0x%08x IND:%d VAL:0x%08x] %s", fn, addr, index, val,
              "Expansion1");
    return -1;
  }

  if (!Expansion2::range.offset(index, addr)) {
    LOG_ERROR("[FN:%s ADDR:0x%08x IND:%d VAL:0x%08x] %s", fn, addr, index, val,
              "Expansion2");
    return -1;
  }

  if (!IRQ::range.offset(index, addr)) {
    return irq.store16(val, index);
  }

  if (!Timers::range.offset(index, addr)) {
    return timers.store16(val, index, clock, irq, gpu);
  }
  
  if (!DMA::range.offset(index, addr)) {
    LOG_ERROR("[FN:%s ADDR:0x%08x IND:%d VAL:0x%08x] %s", fn, addr, index, val,
              "DMA");
    return -1;
  }

  if (!GPU::range.offset(index, addr)) {
    LOG_ERROR("[FN:%s ADDR:0x%08x IND:%d VAL:0x%08x] %s", fn, addr, index, val,
              "GPU");
    return -1;
  }

  LOG_ERROR("[FN:%s ADDR:0x%08x VAL:0x%08x] Unhandled", fn, addr, val);
  return -1;
}

int PCI::store8(u8 val, u32 addr) {
  static const char *fn = "PCI::store8";
  u32 index;

  addr = mask_addr_to_region(addr);

  if (!Bios::range.offset(index, addr)) {
    LOG_ERROR("[FN:%s ADDR:0x%08x IND:%d VAL:0x%08x] %s", fn, addr, index, val,
              "Bios");
    return -1;
  }

  if (!HWregs::range.offset(index, addr)) {
    LOG_ERROR("[FN:%s ADDR:0x%08x IND:%d VAL:0x%08x] %s", fn, addr, index, val,
              "HWregs");
    return -1;
  }

  if (!CDROM::range.offset(index, addr)) {
    return cdrom.store8(val, index, irq);
  }

  if (!RamSize::range.offset(index, addr)) {
    LOG_ERROR("[FN:%s ADDR:0x%08x IND:%d VAL:0x%08x] %s", fn, addr, index, val,
              "RamSize");
    return -1;
  }

  if (!JOYmemcard::range.offset(index, addr)) {
    return ignore_store(val, fn, addr, index, "JOYmemcard");
  }

  if (!CacheCtrl::range.offset(index, addr)) {
    LOG_ERROR("[FN:%s ADDR:0x%08x IND:%d VAL:0x%08x] %s", fn, addr, index, val,
              "CacheCtrl");
    return -1;
  }

  if (!RAM::range.offset(index, addr)) {
    memory::store8(ram.data, index, val);
    return 0;
  }

  if (!SPU::range.offset(index, addr)) {
    LOG_ERROR("[FN:%s ADDR:0x%08x IND:%d VAL:0x%08x] Ignored %s", fn, addr,
              index, val, "IRQ");
    return -1;
  }

  if (!Expansion1::range.offset(index, addr)) {
    LOG_ERROR("[FN:%s ADDR:0x%08x IND:%d VAL:0x%08x] %s", fn, addr, index, val,
              "Expansion1");
    return -1;
  }

  if (!Expansion2::range.offset(index, addr)) {
    return ignore_store(val, fn, addr, index, "Expansion2");
  }

  if (!IRQ::range.offset(index, addr)) {
    LOG_ERROR("[FN:%s ADDR:0x%08x IND:%d VAL:0x%08x] %s", fn, addr, index, val,
              "IRQ");
    return -1;
  }

  if (!Timers::range.offset(index, addr)) {
    LOG_ERROR("[FN:%s ADDR:0x%08x IND:%d VAL:0x%08x] %s", fn, addr, index, val,
              "Timers");
    return -1;
  }
  
  if (!DMA::range.offset(index, addr)) {
    LOG_ERROR("[FN:%s ADDR:0x%08x IND:%d VAL:0x%08x] %s", fn, addr, index, val,
              "DMA");
    return -1;
  }

  if (!GPU::range.offset(index, addr)) {
    LOG_ERROR("[FN:%s ADDR:0x%08x IND:%d VAL:0x%08x] %s", fn, addr, index, val,
              "GPU");
    return -1;
  }

  LOG_ERROR("[FN:%s ADDR:0x%08x VAL:0x%08x] Unhandled", fn, addr, val);
  return -1;
}

int PCI::load_instruction(Instruction &ins, u32 addr) {
  static const char *fn = "PCI::load_instruction";
  u32 index;

  addr = mask_addr_to_region(addr);

  if (!Bios::range.offset(index, addr)) {
    ins.data = memory::load32(bios.data, index);
    return 0;
  }

  if (!RAM::range.offset(index, addr)) {
    ins.data = memory::load32(ram.data, index);
    return 0;
  }

  LOG_ERROR("[FN:%s ADDR:0x%08x] Unhandled", fn, addr);
  return -1;
}
