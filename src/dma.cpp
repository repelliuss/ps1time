#include "dma.hpp"
#include "data.hpp"
#include "bits.hpp"

#include <assert.h>

namespace chview {
namespace {

bool direction_from_ram(const DMA::ChannelView &chview) {
  return bit(chview.channel_control, 0) != 0;
}

u32 increment(const DMA::ChannelView &chview) {
  return (bit(chview.channel_control, 1) != 0) ? -4 : 4;
}

bool transfer_triggered(const DMA::ChannelView &chview) {
  return bit(chview.channel_control, 28) != 0;
}

bool transfer_enabled(const DMA::ChannelView &chview) {
  return bit(chview.channel_control, 24) != 0;
}

// TODO: check all asserts and handle more gracefully?
DMA::ChannelView::SyncMode sync_mode(const DMA::ChannelView &chview) {
  switch (bits_in_range(chview.channel_control, 9, 10)) {
  case 0:
    return DMA::ChannelView::SyncMode::manual;
  case 1:
    return DMA::ChannelView::SyncMode::request;
  case 2:
    return DMA::ChannelView::SyncMode::linked_list;
  default:
    assert(false);
    return DMA::ChannelView::SyncMode::reserved;
  }
}

bool transfer_active(const DMA::ChannelView &chview) {
  if (sync_mode(chview) == DMA::ChannelView::SyncMode::manual) {
    return transfer_enabled(chview) && transfer_triggered(chview);
  }

  return transfer_enabled(chview);
}

// TODO: this function should not need to be called for linked list mode
// linked list traverse ends with a specific header value 0xffffff

int transfer_size(u32 &size, const DMA::ChannelView &chview) {
  switch (sync_mode(chview)) {
  case DMA::ChannelView::SyncMode::manual:
    size = bits_in_range(chview.block_control, 0, 15);
    return 0;

    // TODO: not quite right, they represent different things
  case DMA::ChannelView::SyncMode::request:
    size = bits_in_range(chview.block_control, 0, 15) *
           bits_in_range(chview.block_control, 16, 31);
    return 0;

  default:
    return -1;
  }
}

void finalize_transfer(DMA::ChannelView &chview) {
  bit_clear(chview.channel_control, 24);
  bit_clear(chview.channel_control, 28);

  store32(chview.channel_control_addr, chview.channel_control, 0);

  // TODO: Need to set the correct value for other fields, particularly interrupts (according to simias)
}

} // namespace
} // namespace chview

// for DMA
namespace {

using transfer_value_generator = u32 (*)(u32, u32);

constexpr u32 mask_reg_index_to_chview_type_val(u32 reg_index) {
  return reg_index & 0xfffffff0;
}

constexpr u32 get_otc_channel_transfer_value(u32 remaining_size, u32 addr) {
  // REVIEW: this value is for end of linked list but a specific loop could be
  // written for OTC which would remove this check
  if (remaining_size == 1) {
    return 0xffffff;
  }

  return (addr - 4) & 0x1fffff;
}

int transfer_dma_block(const DMA &dma, DMA::ChannelView &chview) {
  u32 increment = chview::increment(chview);
  u32 addr = chview.base_address;
  u32 remaining_size;
  transfer_value_generator generator;

  switch (chview.type) {
  case DMA::ChannelView::Type::OTC:
    generator = get_otc_channel_transfer_value;
    break;
  default:
    printf("Unhandled DMA source port %d\n", chview.type);
    return -1;
  }

  if (chview::transfer_size(remaining_size, chview) < 0) {
    printf("Couldn't figure out DMA block transfer size\n");
    return -1;
  }

  if (chview::direction_from_ram(chview)) {
    printf("Unhandled DMA direction\n");
    return -1;
  } else {
    while (remaining_size > 0) {
      // NOTE: it seems like addr may be bad
      //	 duckstation handles similar to this
      // TODO: shouldn't be 0xfffffc ?
      u32 cur_addr = addr & 0x1ffffc;
      u32 src_word = generator(remaining_size, cur_addr);

      store32(dma.ram.data, src_word, cur_addr);

      addr += increment;
      --remaining_size;
    }
  }

  chview::finalize_transfer(chview);

  return 0;
}

int transfer(const DMA &dma, DMA::ChannelView &chview) {
  switch (chview::sync_mode(chview)) {
  case DMA::ChannelView::SyncMode::linked_list:
    printf("Linked list mode unsupported\n");
    return -1;

  default:
    return transfer_dma_block(dma, chview);
  }

  return 0;
}

} // namespace

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
  u8 channels_interrupt_ack_status =
      bits_in_range(data[reg::interrupt], 24, 30);
  u8 new_channels_interrupt_ack_status = bits_in_range(val, 24, 30);

  // writing 1 to ack flags resets it
  channels_interrupt_ack_status &= ~new_channels_interrupt_ack_status;

  u8 last_byte = (irq << 7) | channels_interrupt_ack_status;
  bits_copy_to_range(val, 24, 31, last_byte);

  data[reg::interrupt] = val;
}

// TODO: may be needless as addr is already being masked each iteration
void DMA::set_base_addr(u32 base_addr_reg_index, u32 val) {
  assert(0x00 <= base_addr_reg_index && base_addr_reg_index < 0x6a);
  assert(mask_reg_index_to_chview_type_val(base_addr_reg_index) ==
         base_addr_reg_index);

  // only 16MB are addressable by DMA, so cut redundant MSBs
  data[base_addr_reg_index] = val & 0xffffff;
}

DMA::ChannelView DMA::make_channel_view(u32 dma_reg_index) {
  assert(dma_reg_index >= 0x00 && dma_reg_index < 0x6a);
  // TODO: check if this is safe and faster than conditionals (all expected)
  u32 entry_address = mask_reg_index_to_chview_type_val(dma_reg_index);

  return {
      .type = static_cast<ChannelView::Type>(entry_address),
      .base_address = load32(data, entry_address),
      .block_control = load32(data, entry_address + 0x4),

      .channel_control_addr = data + entry_address + 0x8,
      .channel_control = load32(data, entry_address + 0x8),
  };
}

// REVIEW: may make input channel const, it is not const because of
// finalize_transfer, pros: reusable channel object cons: safety?
int DMA::try_transfer(ChannelView &chview) {
  if (chview::transfer_active(chview)) {
    return transfer(*this, chview);
  }

  return 0;
}
