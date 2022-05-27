#pragma once

#include "types.hpp"
#include "range.hpp"
#include "log.hpp"

enum struct Interrupt : u32 {
  vblank = 0,
  dma = 3,
};

// NOTE: writes are ignored for these registers and return 0
// IRQ stands for interrupt request
// 0x1f801070 register is for: Interrupt Status
// 0x1f801074 register is for: Interrupt Mask
// TODO: use u8*data and u32 *data pointed to it
struct IRQ {
  static constexpr u32 size = 8;
  static constexpr Range range = {0x1f801070, 0x1f801078};

  u16 status = 0;
  u16 mask = 0;

  constexpr bool active() { return (status & mask) != 0; }
  constexpr void ack(u16 ack) { status &= ack; }
  constexpr void request(Interrupt interrupt) {
    status |= 1 << static_cast<u32>(interrupt);
  }

  constexpr int load32(u32 &val, u32 index) {
    switch (index) {
    case 0:
      val = status;
      return 0;
    case 4:
      val = mask;
      return 0;
    }

    LOG_ERROR("[FN:%s IND:%d] Unhandled", "IRQ::load32", index);
    return -1;
  }

  constexpr int load16(u32 &val, u32 index) {
    int status = load32(val, index);
    val = static_cast<u16>(val);
    return status;
  }

  constexpr int store32(u32 val, u32 index) {
    switch(index) {
    case 0:
      ack(val);
      return 0;
    case 4:
      mask = val;
      return 0;
    }

    LOG_ERROR("[FN:%s IND:%d VAL:0x%08x] Unhandled", "IRQ::store32", index,
              val);
    return -1;
  }

  constexpr int store16(u32 val, u32 index) {
    return store32(static_cast<u16>(val), index);
  }
};
