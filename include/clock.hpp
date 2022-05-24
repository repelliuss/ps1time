#pragma once

#include "types.hpp"
#include "peripheral.hpp"

struct Clock {
  struct State {
    u64 prev = 0;
    // NOTE: next is initially 0, forces sync
    u64 next = 0;

    constexpr u64 sync(u64 time) {
      u64 delta = time - prev;
      prev = time;
      return delta;
    }

    constexpr void set_alarm(u64 when) { next = when; }
    constexpr bool should_alarm(u64 time) { return next <= time; }
  };

  u64 now = 0;
  State states[PCIType::SIZE];

  constexpr u64 sync(PCIType who) { return states[who].sync(now); }
  constexpr void set_alarm_after(PCIType who, u64 delta) {
    states[who].set_alarm(now + delta);
  }
  constexpr bool alarmed(PCIType who) { return states[who].should_alarm(now); }
  constexpr void tick(u64 delta) { now += delta; }
};
