#include "timers.hpp"

int Timers::video_timings_changed(GPU &gpu, Clock &clock, IRQ &irq) {
  int status = 0;
  for (size_t i = 0; i < 3; ++i) {
    Timer &timer = timers[i];
    if (timer.needs_gpu()) {
      status |= timer.clock_sync(clock, irq);
      status |= timer.reconfigure(gpu, clock);
    }
  }

  return status;
}

int Timers::load16(u32 &val, u32 offset, Clock &clock, IRQ &irq) {
  u32 v;
  int status = load32(v, offset, clock, irq);
  val = static_cast<u16>(v);
  return status;
}

int Timers::load32(u32 &val, u32 offset, Clock &clock, IRQ &irq) {
  u32 instance = offset >> 4;
  Timer &timer = timers[instance];
  int status = timer.clock_sync(clock, irq);

  switch (offset & 0xf) {
  case 0:
    val = timer.get_counter();
    break;
  case 4:
    val = timer.mode();
    break;
  case 8:
    val = timer.get_target();
    break;
  default:
    LOG_CRITICAL("Unhandled timer register %d", offset);
    assert("Unhandled timer register");
    return -1;
  }

  return status;
}

int Timers::store16(u16 val, u32 offset, Clock &clock, IRQ &irq, GPU &gpu) {
  u32 instance = offset >> 4;
  int status = 0;

  Timer &timer = timers[instance];

  status |= timer.clock_sync(clock, irq);

  switch (offset & 0xf) {
  case 0:
    timer.set_counter(val);
    break;
  case 4:
    status |= timer.set_mode(val);
    break;
  case 8:
    timer.set_target(val);
    break;
  default:
    LOG_CRITICAL("Unhandled timer register %d", offset);
    return -1;
  }

  if (timer.needs_gpu()) {
    gpu.clock_sync(clock, irq);
  }

  status |= timer.reconfigure(gpu, clock);

  return status;
}

int Timers::store32(u32 val, u32 offset, Clock &clock, IRQ &irq, GPU &gpu) {
  return store16(static_cast<u16>(val), offset, clock, irq, gpu);
}

int Timers::clock_sync(Clock &clock, IRQ &irq) {
  int status = 0;
  
  if(clock.alarmed(Clock::Host::timer0)) {
    status |= timers[0].clock_sync(clock, irq);
  }

  if (clock.alarmed(Clock::Host::timer1)) {
    status |= timers[1].clock_sync(clock, irq);
  }

  if (clock.alarmed(Clock::Host::timer2)) {
    status |= timers[2].clock_sync(clock, irq);
  }

  return 0;
}

int Timer::reconfigure(GPU &gpu, Clock &clock) {
  int status = 0;
  
  switch (clock_source.purpose(instance)) {
  case Purpose::sys_clock:
    period = FractionalCycle::from_cycles(1);
    phase = FractionalCycle::from_cycles(0);
    break;

  case Purpose::sys_clock_div8:
    period = FractionalCycle::from_cycles(8);
    phase = FractionalCycle::from_cycles(0);
    break;

  case Purpose::gpu_dot_clock:
    period = gpu.dotclock_period();
    status |= gpu.dotclock_phase(phase);
    break;

  case Purpose::gpu_hsync:
    period = gpu.hsync_period();
    phase = gpu.hsync_phase();
    break;
  }

  predict_next_sync(clock);

  return status;
}
