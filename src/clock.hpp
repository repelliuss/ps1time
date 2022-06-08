#pragma once

#include "log.hpp"
#include "peripheral.hpp"
#include "types.hpp"

// TODO: overflow not handled

static constexpr int checked_sum(u64 &sum, u64 a, u64 b) {
  sum = a + b;
  return sum < a;
}

struct Clock {
  enum struct Host {
    gpu,
    timer0,
    timer1,
    timer2,
    pad_memcard,
    SIZE,
  };

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
  u64 next_sync = -1;
  State states[static_cast<int>(Host::SIZE)];

  constexpr u64 next_delta_time(u64 delta) {
    u64 sum = now;
    if (!checked_sum(sum, now, delta)) {
      return sum;
    }

    // TODO: doesn't work
    // overflow
    LOG_INFO("Clock::next_delta_time overflow!");

    u64 max_diff = 0;
    for (int i = 0; i < static_cast<int>(Host::SIZE); ++i) {
      if (now - states[i].prev > max_diff) {
        max_diff = now - states[i].prev;
      }
    }

    for (int i = 0; i < static_cast<int>(Host::SIZE); ++i) {
      states[i].prev = max_diff - (now - states[i].prev);
      states[i].next = max_diff + (states[i].next - now);
    }

    // max_diff becomes now

    return max_diff + delta;
  }

  constexpr State &get(Host who) { return states[static_cast<int>(who)]; }

  constexpr u64 sync(Host who) { return get(who).sync(now); }
  
  constexpr void set_alarm_after(Host who, u64 delta) {
    u64 date = next_delta_time(delta);

    get(who).set_alarm(date);

    if (date < next_sync) {
      next_sync = date;
    }
  }

  constexpr void no_sync_needed(Host who) { get(who).set_alarm(-1); }

  constexpr bool sync_pending() { return next_sync <= now; }

  constexpr void update_sync_pending() {
    u64 min = -1;
    for(size_t i = 0; i < static_cast<size_t>(Host::SIZE); ++i) {
      if(states[i].next < min) {
	min = states[i].next;
      }
    }

    next_sync = min;
  }

  constexpr bool alarmed(Host who) { return get(who).should_alarm(now); }

  constexpr void tick(u64 delta) { now = next_delta_time(delta); }
};

/// Fixed point representation of a cycle counter
struct FractionalCycle {
  u64 fixed_point_cycle;

  static constexpr u64 fractional_bits() { return 16; }

  static constexpr FractionalCycle from_fixed_point(u64 fixed_point) {
    return {fixed_point};
  }

  static constexpr FractionalCycle from_f32(f32 value) {
    f32 precision = (1U << fractional_bits());
    return {static_cast<u64>(value * precision)};
  }

  static constexpr FractionalCycle from_cycles(u64 cycles) {
    return {cycles << fractional_bits()};
  }

  FractionalCycle add(const FractionalCycle &cycle) {
    return {fixed_point_cycle + cycle.fixed_point_cycle};
  }

  FractionalCycle multiply(const FractionalCycle &cycle) {
    // TODO: maybe do {((fixed_point >> fractional_bits()) * (time.fixed_point
    // >> fractional_bits())) << fractional_bits()}
    return {(fixed_point_cycle * cycle.fixed_point_cycle) >> fractional_bits()};
  }

  FractionalCycle divide(const FractionalCycle &cycle) {
    return {(fixed_point_cycle << fractional_bits()) / cycle.fixed_point_cycle};
  }

  u64 ceil() {
    u64 shift = FractionalCycle::fractional_bits();
    u64 align = (1 << shift) - 1;
    return (fixed_point_cycle + align) >> shift;
  }
};
