#pragma once

#include "range.hpp"
#include "types.hpp"
#include "heap_byte_data.hpp"
#include "dma.hpp"
#include "ram.hpp"
#include "gpu.hpp"
#include "peripheral.hpp"
#include "clock.hpp"
#include "log.hpp"
#include "instruction.hpp"

#include <cstdlib>
#include <cstring>

// TODO:  remove HeapByteData struct and allocate all memory at once

// TODO: may remove size constants

struct Bios {
  static constexpr u32 size = 524288;
  static constexpr Range range = {0x1fc00000, 0x1fc80000};
  u8 data[size];
};

// This is not our ram size, but a register that is being set in hw_regs area    
struct RamSize {
  static constexpr u32 size = 4;
  static constexpr Range range = {0x1f801060, 0x1f801064};
  u8 data[size];
};

// NOTE: this area lies in hardware registers or I/O map
// NOTE: simias docs say this is SYS_CONTROL 
// REVIEW: simias docs say these are the only important ones for now 
// NOTE: Previously MemCtrl
struct HWregs {
  static constexpr u32 size = 36;
  static constexpr Range range = {0x1f801000, 0x1f801024};
  u8 data[size];

  inline int store32(u32 val, u32 index) {
    static constexpr const char *fn = "HWregs::store32";
    
    switch (index) {
    case 0:
      if (val != 0x1f000000) {
        LOG_ERROR("[FN:%s IND:%d VAL:0x%08x] Bad expansion 1 value", fn, index,
                  val);
        return -1;
      }

      return 0;

    case 4:
      if (val != 0x1f802000) {
        LOG_ERROR("[FN:%s IND:%d VAL:0x%08x] Bad expansion 2 value", fn, index,
                  val);
        return -1;
      }

      return 0;
    }

    LOG_DEBUG("[FN:%s IND:%d VAL:0x%08x] Ignored", fn, index, val);
    return 0;
  }
};

struct CacheCtrl {
  static constexpr u32 size = 4;
  static constexpr Range range = {0xfffe0130, 0xfffe0134};
  u32 val;

  bool icache_enabled() { return (val & 0x800) != 0; }
  
  // NOTE: see L64360 and ATMizer™ Architecture Technical Manual, page 190
  bool tag_test_mode() { return (val & 4) != 0; }
};

struct SPU : HeapByteData {
  static constexpr u32 size = 640;
  static constexpr Range range = {0x1f801c00, 0x1f801e80};
  SPU() : HeapByteData(size) {}
};

struct Expansion1 : HeapByteData {
  static constexpr u32 size = 8388608; // 8MB
  static constexpr Range range = {0x1f000000, 0x1f800000};
  Expansion1() : HeapByteData(size) {} 
};

struct Expansion2 {
  static constexpr u32 size = 66;
  static constexpr Range range = {0x1f802000, 0x1f802042};
  u8 data[size];
};

// NOTE: writes are ignored for these registers and return 0
// IRQ stands for interrupt request
// 0x1f801070 register is for: Interrupt Status 
// 0x1f801074 register is for: Interrupt Mask
struct IRQ {
  static constexpr u32 size = 8;
  static constexpr Range range = {0x1f801070, 0x1f801078};
  u8 data[size];
};

struct Timers {
  static constexpr u32 size = 48;
  static constexpr Range range = {0x1f801100, 0x1f801130};
  u8 data[size];

  struct Reg {
    struct Pixel {
      static constexpr u32 current = 0x00;
      static constexpr u32 mode = 0x04;
      static constexpr u32 target = 0x08;
    };
    struct HSync {
      static constexpr u32 current = 0x10;
      static constexpr u32 mode = 0x14;
      static constexpr u32 target = 0x18;
    };
    struct Eighth {
      static constexpr u32 current = 0x20;
      static constexpr u32 mode = 0x24;
      static constexpr u32 target = 0x28;
    };
  };
};

// Peripheral Component Interconnect
struct PCI {
  Bios bios;
  HWregs hw_regs;
  RamSize ram_size;
  CacheCtrl cache_ctrl;
  RAM ram;
  GPU gpu;
  SPU spu;
  Expansion1 expansion1;
  Expansion2 expansion2;
  IRQ irq;
  Timers timers;
  DMA dma;

  PCI(Bios &&bios, Renderer *renderer, VideoMode configured_hardware_video_mode)
      : bios(bios), gpu(renderer, configured_hardware_video_mode),
        dma(ram, gpu) {}

  PCI(const PCI &pci) = delete;
  PCI &operator=(const PCI &pci) = delete;

  void clock_sync(Clock &clock);

  int load_instruction(Instruction &ins, u32 addr);

  int load32(u32 &val, u32 addr, Clock &clock);
  int load16(u32 &val, u32 addr);
  int load8(u32 &val, u32 addr);

  int store32(u32 val, u32 addr, Clock &clock);
  int store16(u16 val, u32 addr);
  int store8(u8 val, u32 addr);
};
