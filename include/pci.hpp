#pragma once

#include "bios.hpp"
#include "range.hpp"
#include "types.hpp"
#include "heap_byte_data.hpp"
#include "dma.hpp"
#include "ram.hpp"

#include <cstdlib>
#include <cstring>

// TODO: add u32, u16 pointers that is casted from u8*data and read specifically to read and write at once
// TODO:  remove heap data and allocate all memory at once

enum struct PCIMatch {
  none = -1,
  bios,
  mem_ctrl,
  ram_size,
  cache_ctrl,
  ram,
  spu,
  expansion1,
  expansion2,
  irq,
  timers,
  dma,
  gpu,
};

// TODO: may remove size constants

// This is not our ram size, but a register that is being set in hw_regs area    
struct RamSize {
  static constexpr u32 size = 4;
  static constexpr Range range = {0x1f801060, 0x1f801064};
  u8 data[size];
};

// NOTE: this area lies in hardware registers or I/O map
// NOTE: simias docs say this is SYS_CONTROL 
// REVIEW: simias docs say these are the only important ones for now 
struct MemCtrl {
  static constexpr u32 size = 36;
  static constexpr Range range = {0x1f801000, 0x1f801024};
  u8 data[size];
};

struct CacheCtrl {
  static constexpr u32 size = 4;
  static constexpr Range range = {0xfffe0130, 0xfffe0134};
  u8 data[size];
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
};

struct GPU {
  static constexpr u32 size = 16;
  static constexpr Range range = {0x1f801810, 0x1f801820};
  u8 data[size];
};

// TODO: we should really consider our PCI implementation
// TODO: should only be moved not copied due to HeapByteData implementation, double free may add NULL check?
// Peripheral Component Interconnect
struct PCI {
  Bios bios;
  MemCtrl hw_regs;
  RamSize ram_size;
  CacheCtrl cache_ctrl;
  RAM ram;
  SPU spu;
  Expansion1 expansion1;
  Expansion2 expansion2;
  IRQ irq;
  Timers timers;
  DMA dma;
  GPU gpu;

  PCI() : dma(ram) {}
  
  PCIMatch match(u8*& out_data, u32& offset, u32 addr);
  int store32_data(PCIMatch match, u8 *data, u32 offset, u32 val);
};
