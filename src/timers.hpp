#pragma once

#include "interrupt.hpp"
#include "types.hpp"
#include "range.hpp"
#include "clock.hpp"
#include "gpu.hpp"

#include <assert.h>

// TODO: only free-run mode is implemented, no interrupts
// REVIEW: using the gpu's dotclock or hsync as a timer source is implemented,
// requires further testing

struct Timer {

  enum struct Sync {
    invalid = -1,

    /// For timer 1/2: pause during H/VBlank. For timer 3: stop counter
    pause = 0,

    /// For timer 1/2: reset counter at H/VBlank. For timer 3: free run
    reset = 1,

    /// For timer 1/2: Reset counter at H/VBlank and pause outside of it. For
    /// timer 3: free run
    reset_and_pause = 2,

    /// For timer 1/2: wait for H/VBlank and then free-run. For timer 3: stop
    /// counter
    wait_for_sync = 3,
  };

  constexpr Sync from_timer_bits_1_2(u16 field) {
    switch (field) {
    case 0:
      return Sync::pause;
    case 1:
      return Sync::reset;
    case 2:
      return Sync::reset_and_pause;
    case 3:
      return Sync::wait_for_sync;
    default:
      LOG_ERROR("Invalid sync mode %d", field);
      return Sync::invalid;
    }
  }

  enum struct Purpose {
    /// CPU clock at ~33.87MHz
    sys_clock,
    /// sys_clock / 8
    sys_clock_div8,
    /// GPU's dotclock (depends on hw, ~53MHz)
    gpu_dot_clock,
    /// GPU's hsync signal (depends on hw and video timings)
    gpu_hsync,
  };

  constexpr bool timer_needs_gpu(Purpose purpose) {
    if (purpose == Purpose::gpu_dot_clock || purpose == Purpose::gpu_hsync) {
      return true;
    }

    return false;
  }

  struct ClockSource {
    u8 source;

    constexpr Purpose purpose(Clock::Host host) {
      constexpr Purpose lookup[3][4] = {
          {Purpose::sys_clock, Purpose::gpu_dot_clock, Purpose::sys_clock,
           Purpose::gpu_dot_clock},

          {Purpose::sys_clock, Purpose::gpu_hsync, Purpose::sys_clock,
           Purpose::gpu_hsync},

          {Purpose::sys_clock, Purpose::sys_clock, Purpose::sys_clock_div8,
           Purpose::sys_clock_div8}};

      switch (host) {
      case Clock::Host::timer0:
        return lookup[0][source];
      case Clock::Host::timer1:
        return lookup[1][source];
      case Clock::Host::timer2:
        return lookup[2][source];
      default:
        LOG_CRITICAL("invalid Clock::Host");
        assert(false);
        return Purpose::sys_clock;
      }
    }
  };

  // Timer bits [9:8]
  constexpr ClockSource from_timer_bits_8_9(u8 bits) {
    if ((bits & ~3) != 0) {
      LOG_CRITICAL("invalid clock source 0x%08x", bits);
      assert(false);
      return {0};
    }

    return {bits};
  }

  /// 0, 1, 2
  Clock::Host instance;

  /// Counter value;
  u16 counter = 0;

  /// Counter target
  u16 target = 0;

  /// If true, do not synchronize the timer with an external signal
  bool free_run = false;

  /// The sync mode when free_run is false. Each one of the three timers
  /// interprets this mode differently
  Sync sync = from_timer_bits_1_2(0);

  /// If true the counter is reset when it reaches the target value. Otherwise
  /// let it count all the way to 0xffff and wrap around.
  bool target_wrap = false;

  /// Raise interrupt when the counter reaches the target
  bool target_irq = false;

  /// Raise interrupt when the counter passes 0xffff and wraps around
  bool wrap_irq = false;

  /// If true the interrupt is automatically cleared and will re-trigger when
  /// one of the interrupt conditions occurs again
  bool repeat_irq = false;

  /// TODO: not sure
  bool pulse_irq = false;

  /// Clock source (2 bits). Each timer can either use the CPU sysclock or an
  /// alternative clock source
  ClockSource clock_source = from_timer_bits_8_9(0);

  /// TODO: not sure
  bool request_interrupt = false;

  /// true if the target has been reached since the last read
  bool target_reached = false;

  /// true if the counter reached 0xffff and overflowed since the last read
  bool overflow_reached = false;

  /// Period of a counter tick. Stored as fractional cycle count since the gpu
  /// can be used as a source
  FractionalCycle period = FractionalCycle::from_cycles(1);

  /// current position within a period of a counter tick
  FractionalCycle phase = FractionalCycle::from_cycles(0);

  Timer(Clock::Host instance) : instance(instance) {}

  void reconfigure(GPU &gpu);

  void clock_sync(Clock &clock, IRQ &irq) {
    u64 delta = clock.sync(instance);
    FractionalCycle delta_frac = FractionalCycle::from_cycles(delta);
    FractionalCycle ticks = delta_frac.add(phase);

    u64 count = ticks.fixed_point_cycle / period.fixed_point_cycle;
    u64 phase = ticks.fixed_point_cycle % period.fixed_point_cycle;

    this->phase = FractionalCycle::from_fixed_point(phase);

    u64 target = 0x10000;
    if(target_wrap) {
      target = this->target + 1;
    }

    count += this->counter;

    if (count >= target) {
      count %= target;

      this->target_reached = true;

      if (target == 0x10000) {
	this->overflow_reached = true;
      }
    }

    this->counter = count;
  }

  bool needs_gpu() {
    if (!free_run) {
      assert("Can't do sync mode");
      LOG_CRITICAL("Can't do sync mode");
      return false;
    }

    return timer_needs_gpu(this->clock_source.purpose(this->instance));
  }

  u16 mode() {
    u16 r = 0;

    r |= static_cast<u16>(free_run);
    r |= static_cast<u16>(sync) << 1;
    r |= static_cast<u16>(target_wrap) << 3;
    r |= static_cast<u16>(wrap_irq) << 5;
    r |= static_cast<u16>(repeat_irq) << 6;
    r |= static_cast<u16>(pulse_irq) << 7;
    r |= static_cast<u16>(clock_source.source) << 8;
    r |= static_cast<u16>(request_interrupt) << 10;
    r |= static_cast<u16>(target_reached) << 11;
    r |= static_cast<u16>(overflow_reached) << 12;

    // Reading mode resets those flags
    this->target_reached = false;
    this->overflow_reached = false;

    return r;
  }

  void set_mode(u16 val) {
    this->free_run = (val & 1) == 0;
    this->sync = from_timer_bits_1_2((val >> 1) & 3);
    this->target_wrap = ((val >> 3) & 1) != 0;
    this->target_irq = ((val >> 4) & 1) != 0;
    this->wrap_irq = ((val >> 5) & 1) != 0;
    this->repeat_irq = ((val >> 6) & 1) != 0;
    this->pulse_irq = ((val >> 7) & 1) != 0;
    this->clock_source = from_timer_bits_8_9((val >> 8) & 3);
    // Polarity of this flag appears to be reversed. I'm still not
    // sure what it does though...
    this->request_interrupt = ((val >> 10) & 1) != 0;

    // Writing to mode resets the counter
    this->counter = 0;

    if (this->request_interrupt) {
      assert("Unsupported timer IRQ request");
      LOG_CRITICAL("Unsupported timer IRQ request");
    }

    if (!this->free_run) {
      LOG_CRITICAL("Only free run is supported timer: %d", instance);
      assert("Only free run is supported");
    }
  }

  u16 get_target() {
    return this->target;
  }

  void set_target(u16 val) {
    this->target = val;
  }

  u16 get_counter() {
    return this->counter;
  }

  void set_counter(u16 val) {
    this->counter = val;
  }
};

struct Timers {
  static constexpr u32 size = 48;
  static constexpr Range range = {0x1f801100, 0x1f801130};

  Timer timers[3] = {Timer(Clock::Host::timer0), Timer(Clock::Host::timer1),
                     Timer(Clock::Host::timer2)};

  int store16(u16 val, u32 offset, Clock &clock, IRQ &irq, GPU &gpu);
  int store32(u32 val, u32 offset, Clock &clock, IRQ &irq, GPU &gpu);
  int load16(u32 &val, u32 offset, Clock &clock, IRQ &irq);
  int load32(u32 &val, u32 offset, Clock &clock, IRQ &irq);
  void video_timings_changed(GPU &gpu, Clock &clock, IRQ &irq);

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
