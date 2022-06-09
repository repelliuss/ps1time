#pragma once

#include "clock.hpp"
#include "disc.hpp"
#include "interrupt.hpp"
#include "log.hpp"
#include "types.hpp"
#include "macro.hpp"

#include <assert.h>
#include <string.h>

enum struct IRQcode {
  sector_ready = 1,
  done = 2,
  ok = 3,
  error = 5,
};

struct Fifo {
  u8 buffer[16] = {
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  };
  u8 write_idx = 0;
  u8 read_idx = 0;

  constexpr bool empty() { return write_idx == read_idx; }

  constexpr bool full() { return write_idx == (read_idx ^ 0x10); }

  constexpr void clear() {
    read_idx = write_idx = 0;
    memset(buffer, 0, sizeof(u8) * 16);
  }

  constexpr u8 len() { return (write_idx - read_idx) & 0x1f; }

  constexpr void push(u8 val) {
    size_t idx = (write_idx & 0xf);
    buffer[idx] = val;
    write_idx = (write_idx + 1) & 0x1f;
  }

  constexpr u8 pop() {
    size_t idx = (read_idx & 0xf);
    read_idx = (read_idx + 1) & 0x1f;
    return buffer[idx];
  }
};

constexpr Fifo from_bytes(u8 *data, size_t size) {
  Fifo fifo;

  for (size_t i = 0; i < size; ++i) {
    fifo.push(data[i]);
  }

  return fifo;
}

struct CommandState {
  enum State {
    idle,
    RxPending,
    IrqPending,
  } state;

  union {
    struct {
      u32 rx_delay;
      u32 irq_delay;
      IRQcode irq_code;
      Fifo response;
    } rx_pending;

    struct {
      u32 irq_delay;
      IRQcode irq_code;
    } irq_pending;
  } data;

  constexpr bool is_idle() {
    if (state == idle) {
      return true;
    }

    return false;
  }
};

struct ReadState {
  enum State {
    idle,
    Reading,
  } state;

  union {
    struct {
      u32 zero;
    } reading;
  } data;

  constexpr bool is_idle() {
    if (state == idle) {
      return true;
    }

    return false;
  }
};

struct CDROM {
  static constexpr u32 size = 4;
  static constexpr Range range = {0x1f801800, 0x1f801804};

  CommandState command_state = {.state = CommandState::State::idle};
  ReadState read_state = {.state = ReadState::State::idle};
  u8 index = 0;
  Fifo params;
  Fifo response;
  u8 irq_msk = 0;
  u8 irq_flags = 0;

  int (CDROM::*on_ack)(CommandState &) = &CDROM::ack_idle;
  std::optional<Disc> disc = std::nullopt;
  MSF seek_target = zero();
  MSF position = zero();
  bool double_speed = false;
  XaSector rx_sector;
  bool rx_active = false;
  u16 rx_index = 0;

  int load8(u32 &val, u32 offset, Clock &clock, IRQ &irq) {
    clock_sync(clock, irq);
    
    u8 v = 0;

    u8 index = this->index;
    int stus = 0;

    auto unimplemented = [index, offset, val, &stus] {
      LOG_ERROR("write CDROM register %d.%d %x", offset, index, val);
      stus = -1;
    };

    switch (offset) {
    case 0:
      v = status();
      break;

    case 1: {
      if (response.empty()) {
        LOG_DEBUG("CDROM response FIFO underflow");
      }

      v = response.pop();
    } break;

    case 3:
      switch (index) {
      case 0:
	v = irq_msk | 0xe0;
	break;
      case 1:
        v = irq_flags | 0xe0;
        break;
      default:
        unimplemented();
        break;
      }
      break;

    default:
      unimplemented();
      break;
    }

    val = v;

    return 0;
  }

  int store8(u8 val, u32 offset, Clock &clock, IRQ &int_rq) {
    clock_sync(clock, int_rq);
    
    u8 index = this->index;
    int status = 0;

    auto unimplemented = [index, offset, val, &status] {
      LOG_ERROR("write CDROM register %d.%d %x", offset, index, val);
      status = -1;
    };

    switch (offset) {
    case 0:
      set_index(val);
      break;

    case 1:
      switch (index) {
      case 0:
        status = command(val, clock);
        break;
      default:
        unimplemented();
        break;
      }
      break;

    case 2:
      switch (index) {
      case 0:
        push_param(val);
        break;
      case 1:
        irq_mask(val);
        break;
      default:
        unimplemented();
        break;
      }
      break;

    case 3:
      switch (index) {
      case 0:
	status |= config(val);
	break;
      case 1: {
        if (irq_ack(val & 0x1f)) {
          LOG_ERROR("irq ack error on store 8");
          return -1;
        }

        if ((val & 0x40) != 0) {
          params.clear();
        }

        if ((val & 0xa0) != 0) {
          LOG_ERROR("unhandled CDROM 3.1: %02x", val);
          return -1;
        }
      } break;
      default:
        unimplemented();
        break;
      }
      break;
    }

    return status;
  }

  CommandState helper_clock_sync(u64 delta, Clock &clock, IRQ &irq) {
    CommandState new_state = command_state;

    switch (new_state.state) {
    case CommandState::State::idle: {
      clock.no_sync_needed(Clock::Host::cdrom);

      return {.state = CommandState::State::idle};
    } break;

    case CommandState::State::RxPending: {
      if (new_state.data.rx_pending.rx_delay > delta) {
        u32 delta32 = static_cast<u32>(delta);

        new_state.data.rx_pending.rx_delay -= delta32;
        new_state.data.rx_pending.irq_delay -= delta32;

        clock.set_alarm_after(Clock::Host::cdrom,
                              new_state.data.rx_pending.rx_delay);
        return new_state;
      } else {
        response = new_state.data.rx_pending.response;

        if (new_state.data.rx_pending.irq_delay > delta) {
          u32 irq_delay =
              new_state.data.rx_pending.irq_delay - static_cast<u32>(delta);

          clock.set_alarm_after(Clock::Host::cdrom, irq_delay);

          return {.state = CommandState::State::IrqPending,
                  .data = {.irq_pending = {
                               .irq_delay = irq_delay,
                               .irq_code = new_state.data.rx_pending.irq_code}}};
        } else {
          // TODO: handle error
          trigger_irq(new_state.data.rx_pending.irq_code, irq);

	  clock.no_sync_needed(Clock::Host::cdrom);

          return {.state = CommandState::State::idle};
        }
      }
    } break;

    case CommandState::State::IrqPending: {
      if (new_state.data.irq_pending.irq_delay > delta) {
        u32 irq_delay =
            new_state.data.irq_pending.irq_delay - static_cast<u32>(delta);

        clock.set_alarm_after(Clock::Host::cdrom, irq_delay);

        new_state.data.irq_pending.irq_delay = irq_delay;

        return new_state;
      } else {
        // TODO: handle error
        trigger_irq(new_state.data.irq_pending.irq_code, irq);

        clock.no_sync_needed(Clock::Host::cdrom);

        return {.state = CommandState::State::idle};
      }
    } break;
    }

    return new_state;
  }

  void clock_sync(Clock &clock, IRQ &irq) {
    u64 delta = clock.sync(Clock::Host::cdrom);

    command_state = helper_clock_sync(delta, clock, irq);

    if (read_state.state == ReadState::State::Reading) {
      u32 next_sync;
      if (read_state.data.reading.zero > delta) {
        next_sync = read_state.data.reading.zero - static_cast<u32>(delta);
      } else {
        // TODO: handle error
        sector_read(irq);
        next_sync = cycles_per_sector();
      }

      read_state.data.reading.zero = next_sync;

      clock.set_alarm_after_if_closer(Clock::Host::cdrom, next_sync);
    }
  }

  constexpr int read_byte(u8 &byte) {
    if (rx_index >= 0x800) {
      LOG_ERROR("Unhandled CDROM long read");
      return -1;
    }

    byte = rx_sector.data_byte(rx_index);

    if (rx_active) {
      rx_index += 1;
    }
    else {
      LOG_ERROR("read byte while !rx_active");
      return -1;
    }

    return 0;
  }

  constexpr int dma_read_word(u32 &word) {
    int status = 0;
    u8 b0 = 0, b1 = 0, b2 = 0, b3 = 0;

    status |= read_byte(b0);
    status |= read_byte(b1);
    status |= read_byte(b2);
    status |= read_byte(b3);

    word = static_cast<u32>(b0) | (static_cast<u32>(b1) << 8) |
           (static_cast<u32>(b2) << 16) | (static_cast<u32>(b3) << 24);

    return status;
  }

  int sector_read(IRQ &irq) {
    int status = 0;
    MSF position = this->position;

    LOG_INFO("CDROM: read sector %02x:%02x:%02x", position.m, position.s, position.f);

    if(!disc) {
      LOG_CRITICAL("unreachable code! no disc");
      assert("unreachable code! no disc");
      return -1;
    }

    auto o = disc->read_data_sector(position);
    if(!o) {
      LOG_ERROR("error on sector read");
      return -1;
    }

    rx_sector = o.value();

    if (irq_flags == 0) {
      u8 dstatus = drive_status();
      response = from_bytes(&dstatus, 1);
      status |= trigger_irq(IRQcode::sector_ready, irq);
    }

    MSF next;
    status |= this->position.next(next);
    this->position = next;

    return status;
  }

  constexpr u8 status() {
    u8 r = index;

    // TODO: "XA-ADPCM fifo empty"
    r |= 0 << 2;
    r |= static_cast<u8>(params.empty()) << 3;
    r |= static_cast<u8>(!params.full()) << 4;
    r |= static_cast<u8>(!response.empty()) << 5;
    // TODO: "Data FIFO not empty"
    r |= 0 << 6;

    if(command_state.state == CommandState::State::RxPending) {
      r |= 1 << 7;
    }

    return r;
  }

  constexpr void push_param(u8 param) {
    if (params.full()) {
      LOG_DEBUG("CDROM parameter FIFO overflow");
    }

    params.push(param);
  }

  constexpr bool irq() { return (irq_flags & irq_msk) != 0; }

  constexpr int trigger_irq(IRQcode code, IRQ &iirq) {
    if (irq_flags != 0) {
      LOG_ERROR("Unsupported nested CDROM interrupt");
      return -1;
    }

    bool prev_irq = this->irq();

    irq_flags = static_cast<u8>(code);

    if (!prev_irq && this->irq()) {
      iirq.request(Interrupt::cdrom);
    }

    return 0;
  }

  void set_index(u8 i) {
    index = i & 3;
  }

  int irq_ack(u8 v) {
    irq_flags &= ~v;

    if (irq_flags == 0) {
      if (!command_state.is_idle()) {
	LOG_ERROR("CDROM IRQ ack while controller is busy state: %d", command_state.state);
	return -1;
      }

      auto prev_on_ack = on_ack;

      on_ack = &CDROM::ack_idle;

      if ((this->*(prev_on_ack))(command_state)) {
        LOG_ERROR("error on command state on ack");
        return -1;
      }

      return 0;
    }

    return 0;
  }

  constexpr int config(u8 conf) {
    bool prev_active = rx_active;

    rx_active = (conf & 0x80) != 0;

    if (rx_active) {
      if (!prev_active) {
	rx_index = 0;
      }
    }
    else {
      u16 i = rx_index;
      u16 adjust = (i & 4) << 1;
      rx_index = (i & ~7) + adjust;
    }

    if ((conf & 0x7f) != 0) {
      LOG_ERROR("CDROM: unhandled config %02x", conf);
      return -1;
    }

    return 0;
  }

  constexpr void irq_mask(u8 v) { irq_msk = v & 0x1f; }

  constexpr u32 cycles_per_sector() {
    const u64 cycles_1x = CPU_FREQ_HZ / 75;

    return (cycles_1x >> (static_cast<u32>(double_speed)));
  }

  int command(u8 cmd, Clock &clock) {
    if(!command_state.is_idle()) {
      LOG_ERROR("CDROM command while controller is busy");
      return -1;
    }
    
    response.clear();
    
    LOG_INFO("cdrom command: %02x", cmd);

    int (CDROM::*handler)(CommandState &) = &CDROM::cmd_get_stat;
    
    switch (cmd) {
    case 0x01:
      handler = &CDROM::cmd_get_stat;
      break;

    case 0x02:
      handler = &CDROM::cmd_set_loc;
      break;

    case 0x06:
      handler = &CDROM::cmd_read_n;
      break;

    case 0x09:
      handler = &CDROM::cmd_pause;
      break;

    case 0x0e:
      handler = &CDROM::cmd_set_mode;
      break;

    case 0x15:
      handler = &CDROM::cmd_seek_l;
      break;

    case 0x1a:
      handler = &CDROM::cmd_get_id;
      break;

    case 0x1e:
      handler = &CDROM::cmd_read_toc;
      break;
      
    case 0x19:
      handler = &CDROM::cmd_test;
      break;

    default:
      LOG_ERROR("Unhandled CDROM command 0x%02x %d", cmd, params.len());
      return -1;
    }

    if (irq_flags == 0) {
      if((this->*(handler))(command_state)) {
	LOG_ERROR("Error on command state handler");
	return -1;
      }

      if (command_state.state == CommandState::State::RxPending) {
        clock.set_alarm_after(Clock::Host::cdrom,
                              command_state.data.rx_pending.irq_delay);
      }
    } else {
      on_ack = handler;
    }

    if (read_state.state == ReadState::State::Reading) {
      clock.set_alarm_after_if_closer(Clock::Host::cdrom,
                                      read_state.data.reading.zero);
    }

    params.clear();

    return 0;
  }

  constexpr int cmd_get_stat(CommandState &cs) {
    if (!params.empty()) {
      LOG_ERROR("Unexpected parameters for CDROM get_stat command");
      return -1;
    }

    Fifo response;

    response.push(drive_status());

    u32 rx_delay = 0;
    if(disc) {
      rx_delay = 24000;
    }
    else {
      rx_delay = 17000;
    }

    cs = {
        .state = CommandState::State::RxPending,
        .data =
            {
                .rx_pending =
                    {
                        .rx_delay = rx_delay,
                        .irq_delay = rx_delay + 5401,
                        .irq_code = IRQcode::ok,
                        .response = response,
                    },
            },
    };

    return 0;
  }

  constexpr int cmd_test(CommandState &cs) {
    if (params.len() != 1) {
      LOG_ERROR("Unexpected number of parameters for CDROM test command");
      return -1;
    }

    u8 cmd = params.pop();
    switch (cmd) {
    case 0x20:
      cs = test_version();
      return 0;
    }

    LOG_ERROR("Unhandled CDROM test subcommand 0x%02x", cmd);
    return -1;
  }

  constexpr int cmd_set_loc(CommandState &cs) {
    if (params.len() != 3) {
      LOG_ERROR("CDROM: bad number of parameters for set_loc: %d", params.len());
      return -1;
    }

    u8 m = params.pop();
    u8 s = params.pop();
    u8 f = params.pop();

    MSF msf = zero();
    RETONERR(from_bcd(msf, m, s, f), "Bad MSF");

    seek_target = msf;

    if(disc) {
      u8 status = drive_status();
      cs = {
          .state = CommandState::State::RxPending,
          .data =
              {
                  .rx_pending =
                      {
                          .rx_delay = 35000,
                          .irq_delay = 35000 + 5399,
                          .irq_code = IRQcode::ok,
                          .response = from_bytes(&status, 1),
                      },
              },
      };
    }
    else {
      u8 bytes[2] = {0x11, 0x80};
      cs = {
          .state = CommandState::State::RxPending,
          .data =
              {
                  .rx_pending =
                      {
                          .rx_delay = 25000,
                          .irq_delay = 25000 + 6763,
                          .irq_code = IRQcode::error,
                          .response = from_bytes(bytes, 2),
                      },
              },
      };
    }

    return 0;
  }

  constexpr int cmd_read_n(CommandState &cs) {
    if (!read_state.is_idle()) {
      LOG_ERROR("CDROM read_n while we're already reading");
      return -1;
    }

    u32 read_delay = cycles_per_sector();

    read_state = {
      .state = ReadState::State::Reading,
      .data = {
	.reading {
	  .zero = read_delay
	}
      }
    };

    u8 status = drive_status();

    cs = {
        .state = CommandState::State::RxPending,
        .data =
            {
                .rx_pending =
                    {
                        .rx_delay = 28000,
                        .irq_delay = 28000 + 5401,
                        .irq_code = IRQcode::ok,
                        .response = from_bytes(&status, 1),
                    },
            },
    };

    return 0;
  }
  
  constexpr int cmd_pause(CommandState &cs) {
    if (read_state.is_idle()) {
      LOG_ERROR("Pause when we're not reading");
      return -1;
    }

    on_ack = &CDROM::ack_pause;
    u8 status = drive_status();

    cs = {
        .state = CommandState::State::RxPending,
        .data =
            {
                .rx_pending =
                    {
                        .rx_delay = 25000,
                        .irq_delay = 25000 + 5393,
                        .irq_code = IRQcode::ok,
                        .response = from_bytes(&status, 1),
                    },
            },
    };

    return 0;
  }

  constexpr int cmd_set_mode(CommandState &cs) {
    if (params.len() != 1) {
      LOG_ERROR("CDROM: bad number of parameters for SetMode: %d", params.len());
      return -1;
    }

    u8 mode = params.pop();

    double_speed = (mode & 0x80) != 0;

    if ((mode & 0x7f) != 0) {
      LOG_ERROR("CDROM: unhandled mode %02x\n", mode);
      return -1;
    }

    u8 status = drive_status();

    cs =  {
        .state = CommandState::State::RxPending,
        .data =
            {
                .rx_pending =
                    {
                        .rx_delay = 22000,
                        .irq_delay = 22000 + 5391,
                        .irq_code = IRQcode::ok,
                        .response = from_bytes(&status, 1),
                    },
            },
    };

    return 0;
  }

  constexpr int cmd_seek_l(CommandState &cs) {
    MSF m = zero();
    RETONERR(from_bcd(m, 0x00, 0x02, 0x00), "Bad MSF");
    
    if (seek_target < m) {
      LOG_ERROR("Seek to track 1 pregap: %d %d %d", seek_target.m,
                seek_target.s, seek_target.f);
      return -1;
    }

    position = seek_target;
    on_ack = &CDROM::ack_seek_l;
    u8 status = drive_status();

    cs = {
        .state = CommandState::State::RxPending,
        .data =
            {
                .rx_pending =
                    {
                        .rx_delay = 35000,
                        .irq_delay = 35000 + 5401,
                        .irq_code = IRQcode::ok,
                        .response = from_bytes(&status, 1),
                    },
            },
    };

    return 0;
  }

  constexpr int cmd_get_id(CommandState &cs) {
    if(!disc) {
      u8 bytes[2] = {0x11, 0x80};
      
      cs = {
          .state = CommandState::State::RxPending,
          .data =
              {
                  .rx_pending =
                      {
                          .rx_delay = 20000,
                          .irq_delay = 20000 + 6776,
                          .irq_code = IRQcode::error,
                          .response = from_bytes(bytes, 2),
                      },
              },
      };

      return 0;
    }

    on_ack = &CDROM::ack_get_id;
    u8 status = drive_status();

    cs = {
        .state = CommandState::State::RxPending,
        .data =
            {
                .rx_pending =
                    {
                        .rx_delay = 26000,
                        .irq_delay = 26000 + 5401,
                        .irq_code = IRQcode::ok,
                        .response = from_bytes(&status, 1),
                    },
            },
    };

    return 0;
  }

  constexpr int cmd_read_toc(CommandState &cs) {
    on_ack = &CDROM::ack_read_toc;

    u8 status = drive_status();

    cs = {
        .state = CommandState::State::RxPending,
        .data =
            {
                .rx_pending =
                    {
                        .rx_delay = 45000,
                        .irq_delay = 45000 + 5401,
                        .irq_code = IRQcode::ok,
                        .response = from_bytes(&status, 1),
                    },
            },
    };

    return 0;
  }

  constexpr CommandState test_version() {
    u8 bytes[4] = {0x98, 0x06, 0x10, 0xc3};

    u32 rx_delay = disc ? 21000 : 29000;

    return {
        .state = CommandState::State::RxPending,
        .data =
            {
                .rx_pending =
                    {
                        .rx_delay = rx_delay,
                        .irq_delay = rx_delay + 9711,
                        .irq_code = IRQcode::ok,
                        .response = from_bytes(bytes, 4),
                    },
            },
    };
  }

  constexpr u8 drive_status() {
    if (!disc) {
      return 0x10;
    }

    u8 r = 0;

    bool reading = !read_state.is_idle();

    r |= 1 << 1;
    r |= static_cast<u8>(reading) << 5;

    return r;
  }

  constexpr int ack_idle(CommandState &cs) {
    cs = {.state = CommandState::State::idle};
    return 0;
  }

  constexpr int ack_seek_l(CommandState &cs) {
    u8 status = drive_status();
    cs = {
        .state = CommandState::State::RxPending,
        .data =
            {
                .rx_pending =
                    {
                        .rx_delay = 1000000,
                        .irq_delay = 1000000 + 1859,
                        .irq_code = IRQcode::done,
                        .response = from_bytes(&status, 1),
                    },
            },
    };
    return 0;
  }

  constexpr int ack_get_id(CommandState &cs) {
    if (!disc) {
      LOG_CRITICAL("Unreachable code!");
      assert("Unreachable code!");
    }

    u8 region_char = 'I';
    switch (disc->region) {
    case Region::japan:
      region_char = 'I';
      break;
    case Region::north_america:
      region_char = 'A';
      break;
    case Region::europe:
      region_char = 'E';
      break;
    }

    u8 bytes[8] = {
        drive_status(), 0x00, 0x20, 0x00, 'S', 'C', 'E', region_char};

    Fifo response = from_bytes(bytes, 8);

    cs = {
        .state = CommandState::State::RxPending,
        .data =
            {
                .rx_pending =
                    {
                        .rx_delay = 7336,
                        .irq_delay = 7336 + 12376,
                        .irq_code = IRQcode::done,
                        .response = response,
                    },
            },
    };

    return 0;
  }

  constexpr int ack_read_toc(CommandState &cs) {
    u32 rx_delay = 16000000;
    if (disc) {
      rx_delay = 16000000;
    } else {
      rx_delay = 11000;
    }

    u8 status = drive_status();

    cs = {
        .state = CommandState::State::RxPending,
        .data =
            {
                .rx_pending =
                    {
                        .rx_delay = rx_delay,
                        .irq_delay = rx_delay + 1859,
                        .irq_code = IRQcode::done,
                        .response = from_bytes(&status, 1),
                    },
            },
    };

    return 0;
  }

  constexpr int ack_pause(CommandState &cs) {
    read_state.state = ReadState::State::idle;
    u8 status = drive_status();
    cs = {
        .state = CommandState::State::RxPending,
        .data =
            {
                .rx_pending =
                    {
                        .rx_delay = 2000000,
                        .irq_delay = 2000000 + 1858,
                        .irq_code = IRQcode::done,
                        .response = from_bytes(&status, 1),
                    },
            },
    };

    return 0;
  }
};
