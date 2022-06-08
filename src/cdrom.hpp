#pragma once

#include "types.hpp"
#include "log.hpp"
#include "interrupt.hpp"

#include <string.h>

enum struct IRQcode { ok = 3 };

struct Fifo {
  u8 buffer[16] = {
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  };
  u8 write_idx = 0;
  u8 read_idx = 0;

  constexpr bool empty() { return ((write_idx ^ read_idx) & 0x1f) == 0; }

  constexpr bool full() { return ((read_idx ^ write_idx ^ 0x10) & 0x1f) == 0; }

  constexpr void clear() {
    read_idx = write_idx;
    memset(buffer, 0, sizeof(u8) * 16);
  }

  constexpr u8 len() { return (write_idx - read_idx) & 0x1f; }

  constexpr void push(u8 val) {
    int idx = (write_idx & 0xf);
    buffer[idx] = val;
    write_idx += 1;
  }

  constexpr u8 pop() {
    int idx = (read_idx & 0xf);
    read_idx -= 1;
    return buffer[idx];
  }
};

struct CDROM {
  static constexpr u32 size = 4;
  static constexpr Range range = {0x1f801800, 0x1f801804};
  
  u8 index = 0;
  Fifo params;
  Fifo response;
  u8 irq_msk = 0;
  u8 irq_flags = 0;

  constexpr int load8(u32 &val, u32 offset) {
    u8 v = 0;

    u8 index = this->index;
    int stus = 0;

    auto unimplemented = [index, offset, val, &stus] {
      LOG_ERROR("write CDROM register %d.%d %x", offset, index, val);
      stus = -1;
    };

    switch(offset) {
    case 0:
      v = status();
      break;

    case 1:
      {
	if (response.empty()) {
	  LOG_DEBUG("CDROM response FIFO underflow");
	}

        v = response.pop();
    } break;

    case 3:
      switch (index) {
      case 1:
        v = irq_flags;
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

  constexpr int store8(u8 val, u32 offset, IRQ &int_rq) {
    u8 index = this->index;
    int status = 0;

    auto unimplemented = [index, offset, val, &status] {
      LOG_ERROR("write CDROM register %d.%d %x", offset, index, val);
      status = -1;
    };

    bool prev_irq = irq();

    switch(offset) {
    case 0:
      set_index(val);
      break;
      
    case 1:
      switch(index) {
      case 0:
        status = command(val);
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
      case 1:
	{
	  irq_ack(val & 0x1f);

	  if ((val & 0x40) != 0) {
	    params.clear();
	  }

	  if ((val & 0xa0) != 0) {
	    LOG_ERROR("unhandled CDROM 3.1: %02x", val);
	    return -1;
	  }
	}
	break;
      default:
        unimplemented();
        break;
      }
      break;
    }

    if (!prev_irq && irq()) {
      int_rq.request(Interrupt::cdrom);
    }

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
    // TODO: "Command busy"
    r |= 0 << 7;

    return r;
  }

  constexpr void push_param(u8 param) {
    if (params.full()) {
      LOG_DEBUG("CDROM parameter FIFO overflow");
    }

    params.push(param);
  }

  constexpr bool irq() { return (irq_flags & irq_msk) != 0; }

  constexpr int trigger_irq(IRQcode code) {
    if (irq_flags != 0) {
      LOG_ERROR("Unsupported nested CDROM interrupt");
      return -1;
    }

    irq_flags = static_cast<u8>(code);

    return 0;
  }

  constexpr void set_index(u8 i) { index = i & 3; }

  constexpr void irq_ack(u8 v) { irq_flags &= ~v; }

  constexpr void irq_mask(u8 v) { irq_msk = v & 0x1f; }

  constexpr int command(u8 cmd) {
    response.clear();

    switch (cmd) {
    case 0x01: {
      int status = cmd_get_stat();
      params.clear();
      return status;
    }
    case 0x19: {
      int status = cmd_test();
      params.clear();
      return status;
    }
    default:
      LOG_ERROR("Unhandled CDROM command 0x%02x", cmd);
      return -1;
    }
  }

  constexpr int cmd_get_stat() {
    if (!params.empty()) {
      LOG_ERROR("Unexpected parameters for CDROM get_stat command");
      return -1;
    }

    response.push(0x10);

    return trigger_irq(IRQcode::ok);
  }

  constexpr int cmd_test() {
    if (params.len() != 1) {
      LOG_ERROR("Unexpected number of parameters for CDROM test command");
      return -1;
    }

    u8 cmd = params.pop();
    switch (cmd) {
    case 0x20:
      return test_version();
    }

    LOG_ERROR("Unhandled CDROM test subcommand 0x%02x", cmd);
    return -1;
  }

  constexpr int test_version() {
    // Values returned by my PAL SCPH-7502:
    // Year
    response.push(0x98);
    // Month
    response.push(0x06);
    // Day
    response.push(0x10);
    // Version
    response.push(0xc3);

    return trigger_irq(IRQcode::ok);
  }
};
