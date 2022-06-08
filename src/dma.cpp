#include "dma.hpp"
#include "bits.hpp"
#include "data.hpp"
#include "log.hpp"

#include <assert.h>

// TODO: redesign dma

namespace chview {
namespace {

bool direction_from_ram(const DMA::ChannelView &chview) {
  return bit(chview.channel_control, 0) != 0;
}

u32 addr_step(const DMA::ChannelView &chview) {
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

// NOTE: this function should not need to be called for linked list mode
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
  bit_clear(chview.channel_control, 24); //enable bit
  bit_clear(chview.channel_control, 28); //trigger bit

  // TODO: Need to set the more values here for other fields, particularly
  // interrupts (according to simias)
}

void sync(const DMA::ChannelView &chview) {
  memory::store32(chview.channel_control_addr, 0, chview.channel_control);
}

// NOTE: same as transfer_block_size() but used in SyncMode::manual
u32 transfer_word_count(const DMA::ChannelView &chview) {
  return bits_in_range(chview.block_control, 0, 15);
}

// NOTE: same as transfer_word_count() but used in SyncMode::request
u32 transfer_block_size(const DMA::ChannelView &chview) {
  return bits_in_range(chview.block_control, 0, 15);
}

u32 transfer_block_count(const DMA::ChannelView &chview) {
  return bits_in_range(chview.block_control, 16, 31);
}

} // namespace
} // namespace chview

// for DMA
namespace {

using transfer_value_generator = u32 (*)(u32, u32);

// REVIEW: make it a variable?
// TODO: It seems that this value depends on the ram configuration. We are
// currently using 2MB ram (0x200000) so we can mask all ram addresses with
// 0x1fffff. But when 8MB ram is used (0x800000) this mask should be (0x7ffffc);
constexpr u32 ram_addr_align_mask() { return 0x1ffffc; }

constexpr u32 mask_reg_index_to_chview_type_val(u32 reg_index) {
  return reg_index & 0xfffffff0;
}

constexpr u32 get_otc_channel_transfer_value(u32 remaining_size, u32 addr) {
  // REVIEW: this value is for end of linked list but a specific loop could be
  // written for OTC which would remove this check
  // REVIEW: looping by decrementing so 1 is valid, is there a more beautiful
  // way?
  if (remaining_size == 1) {
    return 0xffffff;
  }

  return (addr - 4) & ram_addr_align_mask();
}

inline u32 get_gpu_channel_transfer_value(u32 remaining_size, u32 addr) {
  LOG_DEBUG("DMA GPU READ");
  return 0;
}

transfer_value_generator get_generator(DMA::ChannelView::Type type) {
  switch (type) {
  case DMA::ChannelView::Type::OTC:
    return get_otc_channel_transfer_value;
  case DMA::ChannelView::Type::GPU:
    return get_gpu_channel_transfer_value;
  default:
    LOG_ERROR("Unhandled DMA source port %d", type);
    return nullptr;
  }
}

constexpr u32 get_otc_channel_transfer_value_see_note_about_transfer_manual(
    u32 remaining_size, u32 aligned_addr) {
  if (remaining_size == 1) {
    return 0xffffff;
  }

  return (aligned_addr - 4);
}

transfer_value_generator
get_generator_see_note_about_transfer_manual(DMA::ChannelView::Type type) {
  switch (type) {
  case DMA::ChannelView::Type::OTC:
    return get_otc_channel_transfer_value_see_note_about_transfer_manual;
  default:
    LOG_ERROR("Unhandled DMA source port %d", type);
    return nullptr;
  }
}

// REVIEW: so this is my implementation of manual transfer. simias does manual
// and request mode on the same function and I think it is wrong by code path
// but also by means of implementation because it doesn't seem to take account
// of block sizes being different than 4(?). I plan to split manual and request
// too. Another thing is that, and also valid for duckstation, they seem to be
// uselessly masking to align address. They mask at the start to align for 32bit
// access and address seems to be staying multiples of 4 because in manual mode
// increment is either 4 or -4 and in request mode block size gets multiplied
// with 4 or -4 so it is still aligned. they also always mask for OTC channel
// transfer value and my point is valid for there too. I dumped ram with both
// implementations and diff results with nothing. Yet, to follow simias's guide,
// I leave this here with an extra function for OTC channel get value.
int transfer_manual(const DMA &dma, DMA::ChannelView &chview) {
  const u32 addr_mask = ram_addr_align_mask();
  const u32 step = chview::addr_step(chview);
  const transfer_value_generator generator =
      get_generator_see_note_about_transfer_manual(chview.type);
  const u32 word_count = chview::transfer_word_count(chview);
  u32 cur_addr = chview.base_address & addr_mask;

  if (generator == nullptr)
    return -1;

  if (chview::direction_from_ram(chview)) {
    LOG_ERROR("Unhandled DMA direction");
    return -1;
  } else {

    for (u32 i = word_count; i > 0; --i) {
      u32 value = generator(i, cur_addr);
      memory::store32(dma.ram.data, cur_addr, value);
      cur_addr += step;
    }
  }

  return 0;
}

int transfer_linked_list(const DMA &dma, DMA::ChannelView &chview) {

  if(!chview::direction_from_ram(chview)) {
    LOG_ERROR("Invalid DMA direction for linked list mode");
    return -1;
  }

  if (chview.type != DMA::ChannelView::Type::GPU) {
    LOG_ERROR("Attempted linked list DMA on port %d", chview.type);
    return -1;
  }

  u32 aligned_addr = chview.base_address & ram_addr_align_mask();
  u32 header;

  do {
    memcpy(&header, dma.ram.data + aligned_addr, sizeof(u32));

    u32 remaining_size = header >> 24;

    while (remaining_size > 0) {
      aligned_addr = (aligned_addr + 4) &
             ram_addr_align_mask(); // REVIEW: masking should'nt bee needed

      u32 command;
      memcpy(&command, dma.ram.data + aligned_addr, sizeof(u32));

      int status = dma.gpu.gp0(command);
      if (status < 0)
        return status;

      --remaining_size;
    }

    aligned_addr = header & ram_addr_align_mask();

    // REVIEW: end-of-table marker is 0xffffff but
    // mednafen only checks MSB but this is not valid addr
    // maybe hardware does the same gotta check
  } while ((header & 0x800000) == 0);

  return 0;
}

// TODO: not quite right, see transfer_manual and its review
int transfer_manual_and_request(const DMA &dma, DMA::ChannelView &chview) {
  const u32 increment = chview::addr_step(chview);
  u32 cur_addr = chview.base_address;
  u32 remaining_size;

  if (chview::transfer_size(remaining_size, chview) < 0) {
    LOG_ERROR("Couldn't figure out DMA block transfer size");
    return -1;
  }

  if (chview::direction_from_ram(chview)) {
    while (remaining_size > 0) {
      u32 aligned_addr = cur_addr & ram_addr_align_mask();

      u32 src_word;
      memcpy(&src_word, dma.ram.data + aligned_addr, sizeof(u32));

      switch (chview.type) {
      case DMA::ChannelView::Type::GPU:
        dma.gpu.gp0(src_word);
        break;
      default:
        LOG_ERROR("Unhandled DMA destination port %d", chview.type);
      }

      cur_addr += increment;
      --remaining_size;
    }
  } else {
    const transfer_value_generator generator = get_generator(chview.type);
    if (generator == nullptr)
      return -1;

    while (remaining_size > 0) {
      // NOTE: it seems like addr may be bad duckstation handles similar to
      // this
      u32 aligned_addr = cur_addr & ram_addr_align_mask();
      u32 src_word = generator(remaining_size, aligned_addr);

      // load 32bit directly
      memory::store32(dma.ram.data, aligned_addr, src_word);

      cur_addr += increment;
      --remaining_size;
    }
  }

  return 0;
}

int transfer(const DMA &dma, DMA::ChannelView &chview) {
  switch (chview::sync_mode(chview)) {
  case DMA::ChannelView::SyncMode::linked_list:
    return transfer_linked_list(dma, chview);
  default:
    return transfer_manual_and_request(dma, chview);
  }

  return 0;
}

} // namespace

DMA::DMA(RAM &ram, GPU &gpu) : HeapByteData(size, 0), ram(ram), gpu(gpu) {
  memory::store32(data, Reg::control, 0x07654321);
}

// is an interrupt active?
bool DMA::irq_active() {
  return data[Reg::interrupt + 3] >> 7; //bit 31
}

// logic for 31th bit in dma interrupt register
static u8 extract_irq_active(u32 interrupt) {
  bool forced_interrupt = bit(interrupt, 15) == 1;
  bool master_allows = bit(interrupt, 23) == 1;

  u32 channels_interrupted_status = bits_in_range(interrupt, 16, 22);

  u32 channels_interrupt_ack_status = bits_in_range(interrupt, 24, 30);

  u32 channels_has_irq =
      channels_interrupted_status & channels_interrupt_ack_status;

  return forced_interrupt || (master_allows && (channels_has_irq != 0));
}

void DMA::set_interrupt(u32 val, IRQ &irq) {
  bool prev_irq = irq_active();

  u8 channels_interrupt_ack_status =
      bits_in_range(data[Reg::interrupt], 24, 30);
  u8 new_channels_interrupt_ack_status = bits_in_range(val, 24, 30);

  // writing 1 to ack flags resets it
  channels_interrupt_ack_status &= ~new_channels_interrupt_ack_status;

  u8 new_irq = extract_irq_active(val);
  u8 last_byte = (new_irq << 7) | channels_interrupt_ack_status;
  bits_copy_to_range(val, 24, 31, last_byte);

  memory::store32(data, Reg::interrupt, val);

  if (!prev_irq && irq_active()) {
    irq.request(Interrupt::dma);
  }
}

// TODO: may be needless as addr is already being masked each iteration
void DMA::set_base_addr(u32 base_addr_reg_index, u32 val) {
  assert(0x00 <= base_addr_reg_index && base_addr_reg_index < 0x6a);
  assert(mask_reg_index_to_chview_type_val(base_addr_reg_index) ==
         base_addr_reg_index);

  // only 16MB are addressable by DMA, so cut redundant MSBs
  val &= 0xffffff;
  memory::store32(data, base_addr_reg_index, val);
}

DMA::ChannelView DMA::make_channel_view(u32 dma_reg_index) {
  assert(dma_reg_index >= 0x00 && dma_reg_index < 0x6a);
  u32 entry_address = mask_reg_index_to_chview_type_val(dma_reg_index);

  return {
      .type = static_cast<ChannelView::Type>(entry_address),
      .base_address = memory::load32(data, entry_address),
      .block_control = memory::load32(data, entry_address + 0x4),

      .channel_control_addr = data + entry_address + 0x8,
      .channel_control = memory::load32(data, entry_address + 0x8),
  };
}

int DMA::try_transfer(ChannelView &chview, IRQ &irq) {
  if (chview::transfer_active(chview)) {
    int status = transfer(*this, chview);
    if (status < 0)
      return status;

    {
      chview::finalize_transfer(chview);

      // TODO: redesign dma transfer interrupt

      bool prev_irq = irq_active();
      u32 new_interrupt = memory::load32(data, Reg::interrupt);

      bit_or(new_interrupt, 24 + static_cast<u32>(chview.type) / 16,
             bit(new_interrupt, 16 + static_cast<u32>(chview.type) / 16));
      memory::store32(data, Reg::interrupt, new_interrupt);

      if (!prev_irq && extract_irq_active(new_interrupt)) {
        irq.request(Interrupt::dma);
      }
    }

    chview::sync(chview);
  }

  return 0;
}

int DMA::load32(u32 &val, u32 index) {
  switch (index) {
  case DMA::Reg::control:
  case DMA::Reg::interrupt:

  case DMA::Reg::mdecin_base_address:
  case DMA::Reg::mdecout_base_address:
  case DMA::Reg::gpu_base_address:
  case DMA::Reg::cdrom_base_address:
  case DMA::Reg::spu_base_address:
  case DMA::Reg::pio_base_address:
  case DMA::Reg::otc_base_address:

  case DMA::Reg::mdecin_block_control:
  case DMA::Reg::mdecout_block_control:
  case DMA::Reg::gpu_block_control:
  case DMA::Reg::cdrom_block_control:
  case DMA::Reg::spu_block_control:
  case DMA::Reg::pio_block_control:
  case DMA::Reg::otc_block_control:

  case DMA::Reg::mdecin_channel_control:
  case DMA::Reg::mdecout_channel_control:
  case DMA::Reg::gpu_channel_control:
  case DMA::Reg::cdrom_channel_control:
  case DMA::Reg::spu_channel_control:
  case DMA::Reg::pio_channel_control:
  case DMA::Reg::otc_channel_control:
    val = memory::load32(data, index);
    return 0;
  }

  LOG_ERROR("[FN:%s IND:%d] Unhandled", "DMA::load32", index);
  return -1;
}

int DMA::store32(u32 val, u32 index, IRQ &irq) {
  switch (index) {
  case DMA::Reg::interrupt:
    set_interrupt(val, irq);
    return 0;

  case DMA::Reg::mdecin_base_address:
  case DMA::Reg::mdecout_base_address:
  case DMA::Reg::gpu_base_address:
  case DMA::Reg::cdrom_base_address:
  case DMA::Reg::spu_base_address:
  case DMA::Reg::pio_base_address:
  case DMA::Reg::otc_base_address:
    set_base_addr(index, val);
    return 0;

  case DMA::Reg::mdecin_channel_control:
  case DMA::Reg::mdecout_channel_control:
  case DMA::Reg::gpu_channel_control:
  case DMA::Reg::cdrom_channel_control:
  case DMA::Reg::spu_channel_control:
  case DMA::Reg::pio_channel_control:
  case DMA::Reg::otc_channel_control:
    memory::store32(data, index, val);
    DMA::ChannelView channel = make_channel_view(index);
    return try_transfer(channel, irq);
  }

  memory::store32(data, index, val);
  return 0;
}
