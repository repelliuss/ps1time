#pragma once

#include "gamepad.hpp"
#include "interrupt.hpp"
#include "clock.hpp"

/// Identifies the target of the serial communication, either the
/// gamepad/memory card port 0 or 1.
enum Target {
    PadMemCard1 = 0,
    PadMemCard2 = 1,
};

constexpr Target target_from_control(u16 ctrl) {
  if ((ctrl & 0x2000) != 0) {
    return Target::PadMemCard2;
  } else {
    return Target::PadMemCard1;
  }
}

/// Controller transaction state machine
struct Bus {
  enum State {
    idle,
    transfer,
    dsr,
  }state;

  union {
    struct {
      u8 response;
      bool dsr;
      u64 delay;
    } transfer;
    struct {
      u64 delay;
    } dsr;
  } data;

  bool is_busy() {
    if (state == State::idle) {
      return false;
    }

    return true;
  }
};

struct PadMemCard {
  static constexpr u32 size = 32;
  static constexpr Range range = {0x1f801040, 0x1f801060};

  /// Serial clock divider. The LSB is read/write but is not used,
  /// This way the hardware divide the CPU clock by half of
  /// `baud_div` and can invert the serial clock polarity twice
  /// every `baud_div` which effectively means that the resulting
  /// frequency is CPU clock / (`baud_div` & 0xfe).
  u16 baud_div = 0;
    /// Serial config, not implemented for now...
  u8 mode = 0;
    /// Transmission enabled if true
  bool tx_en = false;
    /// If true the targeted peripheral select signal is asserted (the
    /// actual signal is active low, so it's driving low on the
    /// controller port when `select` is true). The `target` field
    /// says which peripheral is addressed.
  bool select = false;
    /// This bit says which of the two pad/memorycard port pair
    /// we're selecting with `select_n` above. Multitaps are handled
    /// at the serial protocol level, not by dedicated hardware pins.
  Target target = Target::PadMemCard1;
    /// Control register bits 3 and 5 are read/write but I don't know
    /// what they do. I just same them here for accurate readback.
  u8 unknown = 0;
  /// XXX not sure what this does exactly, forces a read without any
  /// TX?
  bool rx_en = false;
  /// Data Set Ready signal, active low (driven by the gamepad)
  bool dsr = false;
  /// If true an interrupt is generated when a DSR pulse is received
  /// from the pad/memory card
  bool dsr_it = false;
  /// Current interrupt level
  bool interrupt = false;
  /// Current response byte.
  /// XXX Normally it should be a FIFO but I'm not sure how it works
  /// really. Besides the game should check for the response after
  /// each byte anyway, so it's probably unused the vast majority of
  /// times.
  u8 response = 0xff;
  /// True when we the RX FIFO is not empty.
  bool rx_not_empty = false;
  /// Gamepad in slot 1
  GamePad pad1 = GamePad(GamePadType::Digital);
  /// Gamepad in slot 2
  GamePad pad2 = GamePad(GamePadType::Disconnected);

  Profile* pad_profiles[2] = {pad1.get_profile(), pad2.get_profile()};

  int clock_sync(Clock &clock, IRQ &irq) {
    u64 delta = clock.sync(Clock::Host::gpu);

    switch(bus.state) {
    case Bus::State::idle:
      clock.no_sync_needed(Clock::Host::pad_memcard);
      break;

    case Bus::State::transfer:
      if (delta < bus.data.transfer.delay) {
	u64 delay = bus.data.transfer.delay - delta;
	bus.data.transfer.delay = delay;

	if (dsr_it) {
	  clock.set_alarm_after(Clock::Host::pad_memcard, delay);
	}
	else {
	  clock.no_sync_needed(Clock::Host::pad_memcard);
	}
      }
      else {
	if (rx_not_empty) {
	  LOG_ERROR("Gamepad RX while FIFO isn't empty");
	  return -1;
	}

	response = bus.data.transfer.response;
	rx_not_empty = true;
	dsr = bus.data.transfer.dsr;

	if (dsr) {
	  if (dsr_it) {
	    if (!interrupt) {
	      irq.request(Interrupt::pad_memcard);
	    }

	    interrupt = true;
	  }

	  bus.state = Bus::State::dsr;
	  bus.data.dsr.delay = 10;
	}
	else {
	  bus.state = Bus::State::idle;
	}

	clock.no_sync_needed(Clock::Host::pad_memcard);
      }
      break;

    case Bus::State::dsr:
      if (delta < bus.data.dsr.delay) {
	u64 delay = bus.data.dsr.delay - delta;
	bus.data.dsr.delay = delay;
      }
      else {
	dsr = false;
	bus.state = Bus::State::idle;
      }
      clock.no_sync_needed(Clock::Host::pad_memcard);
      break;
    }

    return 0;
  }

  /// Bus state machine
  Bus bus = {.state = Bus::State::idle};

  int send_command(u8 cmd, Clock &clock) {
    if (!tx_en) {
      // It should be stored in the FIFO and sent when tx_en is
      // set (I think)
      LOG_ERROR("Unhandled gamepad command while tx_en is disabled");
      return -1;
    }

    if (bus.is_busy()) {
      // I suppose the transfer should be queued in the TX FIFO?
      LOG_DEBUG("Gamepad command {:x} while bus is busy!", cmd);
    }

    Pair p;

    if (select) {
      switch (target) {
      case Target::PadMemCard1:
        p = pad1.send_command(cmd);
        break;

      case Target::PadMemCard2:
        p = pad2.send_command(cmd);
        break;
      }
    } else {
      p = {0xff, false};
    }

    // XXX Handle `mode` as well, especially the "baudrate reload
    // factor". For now I assume we're sending 8 bits, one every
    // `baud_div` CPU cycles.
    u64 tx_duration = 8 * baud_div;

    bus.state = Bus::State::transfer;
    bus.data.transfer = {response, dsr, tx_duration};

    clock.set_alarm_after(Clock::Host::pad_memcard, tx_duration);

    return 0;
  }

  u32 stat() {
    u32 stat = 0;

    // TX Ready bits 1 and 2 (Not sure when they go low)
    stat |= 5;
    stat |= static_cast<u32>(rx_not_empty) << 1;
    // RX parity error should always be 0 in our case.
    stat |= 0 << 3;
    // XXX Since we don't emulate joystick timings properly we're
    // going to pretend the ack line (active low) is always high.
    stat |= 0 << 7;
    stat |= static_cast<u32>(interrupt) << 9;
    // XXX needs to add the baudrate counter in bits [31:11];
    stat |= 0 << 11;

    return stat;
  }

  void set_mode(u8 mode) { this->mode = mode; }

  u16 control() {
    u16 ctrl = 0;

    ctrl |= static_cast<u16>(unknown);

    ctrl |= static_cast<u16>(tx_en) << 0;
    ctrl |= static_cast<u16>(select) << 1;
    // XXX I assume this flag self-resets? When?
    ctrl |= static_cast<u16>(rx_en) << 2;
    // XXX Add other interrupts when they're implemented
    ctrl |= static_cast<u16>(dsr_it) << 12;
    ctrl |= static_cast<u16>(target) << 13;

    return ctrl;
  }

  int set_control(u16 ctrl, IRQ &irq) {
    if ((ctrl & 0x40) != 0) {
      // Soft reset
      baud_div = 0;
      mode = 0;
      select = false;
      target = Target::PadMemCard1;
      unknown = 0;
      interrupt = false;
      rx_not_empty = false;
      bus.state = Bus::State::idle;
      // XXX since the gamepad/memory card asserts this signal
      // it actually probably shouldn't release here but it'll
      // make our state machine simpler for the time being.
      dsr = false;
    } else {
      if ((ctrl & 0x10) != 0) {
        // Interrupt acknowledge

        interrupt = false;

        if (dsr && dsr_it) {
          LOG_DEBUG("Gamepad interrupt acknowledge while DSR is active");
          interrupt = true;
          irq.request(Interrupt::pad_memcard);
        }
      }

      bool prev_select = select;

      unknown = static_cast<u8>(ctrl) & 0x28;

      tx_en = (ctrl & 1) != 0;
      select = ((ctrl >> 1) & 1) != 0;
      rx_en = ((ctrl >> 2) & 1) != 0;
      dsr_it = ((ctrl >> 12) & 1) != 0;
      target = target_from_control(ctrl);

      if (rx_en) {
        LOG_ERROR("Gamepad rx_en not implemented");
        return -1;
      }

      if (dsr_it && !interrupt && dsr) {
        // Interrupt should trigger here but that really
        // shouldn't happen I think.
        LOG_ERROR("dsr_it enabled while DSR signal is active");
        return -1;
      }

      if ((ctrl & 0xf00) != 0) {
        LOG_ERROR("Unsupported gamepad interrupts %04x\n", ctrl);
        return -1;
      }

      if (!prev_select && select) {
        pad1.select();
      }
    }

    return 0;
  }

  int store8(u8 val, u32 index, Clock &clock, IRQ &irq) {
    clock_sync(clock, irq);

    switch(index) {
    case 0:
      return send_command(val, clock);

    case 8:
      set_mode(val);
      return 0;

    case 14:
      baud_div = val;
      return 0;

    default:
      LOG_ERROR("Unhandled write 8 to gamepad register %d, %04x", index,
                static_cast<u16>(val));
      return -1;
    }
  }

  int store16(u16 val, u32 index, Clock &clock, IRQ &irq) {
    return store32(val, index, clock, irq);
  }

  int store32(u32 val, u32 index, Clock &clock, IRQ &irq) {
    clock_sync(clock, irq);

    switch(index) {
    case 8:
      set_mode(val);
      return 0;

    case 10:
      return set_control(val, irq);

    case 14:
      baud_div = val;
      return 0;

    default:
      LOG_ERROR("Unhandled write 16/32 to gamepad register %d, %04x", index,
                static_cast<u16>(val));
      return -1;
    }
  }

  int load8(u32 &val, u32 index, Clock &clock, IRQ &irq) {
    clock_sync(clock, irq);

    int status = 0;
    u8 v;

    switch (index) {
    case 0:
      v = response;
      rx_not_empty = false;
      response = 0xff;
      break;

    case 4:
      v = stat();
      break;

    case 10:
      v = control();
      break;

    default:
      LOG_ERROR("Unhandled gamepad read 8 0x%x", index);
      return -1;
    }

    val = v;

    return 0;
  }

  int load16(u32 &val, u32 index, Clock &clock, IRQ &irq) {
    u32 v;
    int status = load32(v, index, clock, irq);
    val = static_cast<u16>(v);
    return status;
  }

  int load32(u32 &val, u32 index, Clock &clock, IRQ &irq) {
    clock_sync(clock, irq);

    int status = 0;

    switch (index) {
    case 4:
      val = stat();
      break;

    case 10:
      val = control();
      break;

    default:
      LOG_ERROR("Unhandled gamepad read 16/32 0x%x", index);
      return -1;
    }

    return 0;
  }
};
