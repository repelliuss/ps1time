#include "dma.hpp"
#include "data.hpp"
#include "bits.hpp"

#include <assert.h>

DMA::DMA(RAM &ram) : ram(ram) {
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

DMA::Channel DMA::make_channel(u32 dma_reg_index) {
  assert(dma_reg_index >= 0x00 && dma_reg_index < 0x70);
  // TODO: check if this is safe and faster than conditionals (all expected)
  u32 entry_address = dma_reg_index & 0xfffffff0;

  return {
      .type = static_cast<Channel::Type>(entry_address),
      .base_address = load32(data, entry_address),
      .block_control = load32(data, entry_address + 0x4),
      .channel_control = load32(data, entry_address + 0x8),
  };
}

bool DMA::Channel::transfer_triggered() {
  return bit(channel_control, 28) != 0;
}

bool DMA::Channel::transfer_enabled() { return bit(channel_control, 24) != 0; }

bool DMA::Channel::transfer_active() {
  if (sync_mode() == SyncMode::manual) {
    return transfer_enabled() && transfer_triggered();
  }

  return transfer_enabled();
}

// TODO: check all asserts and handle more gracefully?
DMA::Channel::SyncMode DMA::Channel::sync_mode() {
  switch (bits_in_range(channel_control, 9, 10)) {
  case 0:
    return SyncMode::manual;
  case 1:
    return SyncMode::request;
  case 2:
    return SyncMode::linked_list;
  default:
    assert(false);
    return SyncMode::reserved;
  }
}

static int channel_do_dma_block(DMA::Channel &channel) { return -1; }

static int channel_start_transfer(DMA::Channel &channel) {
  switch (channel.sync_mode()) {
  case DMA::Channel::SyncMode::linked_list:
    printf("Linked list mode unsupported\n");
    return -1;

  default:
    return channel_do_dma_block(channel);
  }

  return 0;
}

int DMA::Channel::try_transfer() {
  if (transfer_active()) {
    return channel_start_transfer(*this);
  }

  return 0;
}

