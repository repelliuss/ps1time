#pragma once

#include "types.hpp"
#include "interrupt.hpp"

// TODO: writing to cause is missing

enum struct Cause : u32 {
  // irq
  interrupt = 0x0,
  unaligned_load_addr = 0x4,
  unaligned_store_addr = 0x5,
  syscall = 0x8,
  brek = 0x9, //break
  illegal_instruction = 0xa,
  unimplemented_coprocessor = 0xb,
  overflow = 0xc,
};

struct COP0 {
  // REVIEW: setting all to 0 may not be accurate i.e. sr-$12. though sr is
  // being set to mask ISOLATE_CACHE REVIEW: nocash says 32-63 is N/A should I
  // remove?
  u32 data[64] = {0};
  IRQ &irq;

  COP0(IRQ &irq) : irq(irq) {}

  enum Reg {
    sr = 12,
    cause = 13,
    epc = 14,
  };

  constexpr u32 get(Reg reg) {
    if (reg == Reg::cause) {
      return data[cause] | (irq.active() << 10);
    }

    return data[reg];
  }

  constexpr void set(Reg reg, u32 val) { data[reg] = val; }

  constexpr bool cache_isolated() {
    return (get(sr) & 0x10000) != 0;
  }

  // NOTE: shouldn't use get or set
  constexpr u32 enter_exception(Cause cause, u32 pc, bool in_delay_slot) {
    // Shift bits [5:0] of `SR` two places to the left. Those bits
    // are three pairs of Interrupt Enable/User Mode bits behaving
    // like a stack 3 entries deep. Entering an exception pushes a
    // pair of zeroes by left shifting the stack which disables
    // interrupts and puts the CPU in kernel mode. The original
    // third entry is discarded (it's up to the kernel to handle
    // more than two recursive exception levels).
    u32 mode = data[sr] & 0x3f;

    data[sr] &= ~0x3f;
    data[sr] |= (mode << 2) & 0x3f;

    // Update `CAUSE` register with the exception code (bits
    // [6:2])
    data[Reg::cause] &= ~0x7c;
    data[Reg::cause] |= static_cast<u32>(cause) << 2;

    if (in_delay_slot) {
      data[epc] = pc - 4;
      data[Reg::cause] |= 1 << 31;
    } else {
      data[epc] = pc;
      data[Reg::cause] &= ~(1 << 31);
    }

    if ((data[sr] & (1 << 22)) != 0) {
      return 0xbfc00180;
    } else {
      return 0x80000080;
    }
  }

  // NOTE: shouldn't use get set
  constexpr void rfe() {
    u32 mode = data[sr] & 0x3f;
    data[sr] &= ~0xf;
    data[sr] |= mode >> 2;
  }

  constexpr bool current_interrupt_enable() { return (get(sr) & 1) != 0; }

  constexpr bool interrupt_pending() {
    u32 cause = get(Reg::cause);

    // NOTE: [9:8] of cause is software interrupts, 10 is external interrupt
    // state. They can be masked with sr
    u32 pending = (cause & get(sr)) & 0x700;

    return current_interrupt_enable() && (pending != 0);
  }
};
