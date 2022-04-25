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

DMA::Channel DMA::channel_from_dma_reg(u32 offset) {
  assert(offset >= 0x00 && offset < 0x70);
  // TODO: check if this is safe and faster than conditionals (all expected)
  return static_cast<Channel>(offset & 0xfffffffe);
}

static bool channel_triggered(u32 value) { return bit(value, 28) != 0; }

enum ChannelSyncMode : u32 {
  manual,
  request,
  linked_list,
  reserved // not used
};

static ChannelSyncMode channel_sync_mode(u32 value) {
  switch (bits_in_range(value, 9, 10)) {
  case 0:
    return ChannelSyncMode::manual;
  case 1:
    return ChannelSyncMode::request;
  case 2:
    return ChannelSyncMode::linked_list;
  default:
    assert(false);
    return ChannelSyncMode::reserved;
  }
}

static bool channel_enabled(u32 val) { return bit(val, 24) != 0; }

static bool channel_transfer_active(u32 control_val) {
  bool enabled = channel_enabled(control_val);
  
  if(channel_sync_mode(control_val) == ChannelSyncMode::manual) {
    return enabled && channel_triggered(control_val);
  }

  return enabled;
}

static int channel_start_transfer(DMA::Channel channel) {
  //unimplemented do_dma(...)
  return -1;
}

int DMA::channel_try_transfer(u32 dma_reg) {
  DMA::Channel channel = DMA::channel_from_dma_reg(dma_reg);
  u32 control_val = channel_control(channel);
  if (channel_transfer_active(control_val)) {
    printf("transfer...\n");
    return channel_start_transfer(channel);
  }

  return 0;
}

// TODO: check all enum declarations 
u32 DMA::channel_control(Channel channel) {
  return data[static_cast<u32>(channel) + 0x8];
}

u32 DMA::channel_block(Channel channel) {
  return data[static_cast<u32>(channel) + 0x4];
}

u32 DMA::channel_base_address(Channel channel) {
  return data[static_cast<u32>(channel)];
}
