#include "dma.hpp"
#include "data.hpp"
#include "bits.hpp"

DMA::DMA() {
  // REVIEW: maybe introduce own store&load for each peripheral fns?
  store32(data, 0x07654321, reg::control);
}

// is an interrupt active?
bool DMA::irq_active() { return bit(data[reg::interrupt], 31); }

// logic for 31th bit in dma interrupt register
static u8 extract_irq_active(u32 interrupt) {
  bool forced_interrupt = bit(interrupt, 15) == 1;
  bool master_allows = bit(interrupt, 23) == 1;

  u32 channels_interrupted_status = bits_in_range(interrupt, 16, 22);

  u32 channels_interrupt_ack_status = bits_in_range(interrupt, 24, 30);

  u32 channels_has_irq =
      channels_interrupted_status & channels_interrupt_ack_status;

  return forced_interrupt || ((master_allows && channels_has_irq) != 0);
}

void DMA::set_interrupt(u32 val) {
  u8 irq = extract_irq_active(val);
  u8 channels_interrupt_ack_status = bits_in_range(data[reg::interrupt], 24, 30);
  u8 new_channels_interrupt_ack_status = bits_in_range(val, 24, 30);

  // writing 1 to ack flags resets it
  channels_interrupt_ack_status &= ~new_channels_interrupt_ack_status;

  u8 last_byte = (irq << 7) | channels_interrupt_ack_status;
  bits_copy_to_range(val, 24, 31, last_byte);

  data[reg::interrupt] = val;
}
