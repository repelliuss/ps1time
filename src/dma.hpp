#pragma once

#include "types.hpp"
#include "range.hpp"
#include "ram.hpp"
#include "gpu.hpp"

struct DMA : HeapByteData {
  static constexpr u32 size = 0x80;
  static constexpr Range range = {0x1f801080, 0x1f801100};

  struct Reg {
    /// register indexes
    static constexpr u32 control = 0x70;
    static constexpr u32 interrupt = 0x74;

    // NOTE: only [0:23] are significant
    static constexpr u32 mdecin_base_address = 0x00;
    static constexpr u32 mdecout_base_address = 0x10;
    static constexpr u32 gpu_base_address = 0x20;
    static constexpr u32 cdrom_base_address = 0x30;
    static constexpr u32 spu_base_address = 0x40;
    static constexpr u32 pio_base_address = 0x50;
    static constexpr u32 otc_base_address = 0x60;

    static constexpr u32 mdecin_block_control = 0x04;
    static constexpr u32 mdecout_block_control = 0x14;
    static constexpr u32 gpu_block_control = 0x24;
    static constexpr u32 cdrom_block_control = 0x34;
    static constexpr u32 spu_block_control = 0x44;
    static constexpr u32 pio_block_control = 0x54;
    static constexpr u32 otc_block_control = 0x64;

    static constexpr u32 mdecin_channel_control = 0x08;
    static constexpr u32 mdecout_channel_control = 0x18;
    static constexpr u32 gpu_channel_control = 0x28;
    static constexpr u32 cdrom_channel_control = 0x38;
    static constexpr u32 spu_channel_control = 0x48;
    static constexpr u32 pio_channel_control = 0x58;
    static constexpr u32 otc_channel_control = 0x68;
  };

  // NOTE: all data that is being mutated should be reflected by calling
  // internal sync()
  struct ChannelView {
    // add offsets to get proper register
    // for example:
    // +8 to get channel control
    // +4 to get block control
    // +0 to get base address
    // TODO: may be useless, see make_channel
    // NOTE: values are important as they are used in computation
    enum struct Type : u32 {
      MdecIn = 0x00,
      MdecOut = 0x10,
      GPU = 0x20,
      CDROM = 0x30,
      SPU = 0x40,
      PIO = 0x50,
      OTC = 0x60,
    };

    enum struct SyncMode {
      manual,
      request,
      linked_list,
      reserved // not used
    };

    // REVIEW: if there becomes many writes to a channel at the same time, it
    // can be optimized with storing only before loading with a function like
    // sync_to_dma()

    Type type;
    u32 base_address;

    /// [0, 15] contains block_size [16, 31] contains block_count
    /// for manual sync mode, only block_size is used
    /// for request sync mode, both used
    /// others shouldn't need
    u32 block_control;

    u8 *channel_control_addr; // NOTE: next 4 words contain channel_control
    u32 channel_control;      // copied at initialization
  };

  RAM &ram;
  GPU &gpu;

  DMA(RAM &ram, GPU &gpu);

  // TODO: may not be needed, may create chviews at beginning and track it there
  ChannelView make_channel_view(u32 dma_reg_index); 

  //reg::interrupt
  bool irq_active();
  void set_interrupt(u32 val, IRQ &irq);

  //reg::*_base_address
  void set_base_addr(u32 base_address_reg_index, u32 val);

  //may only be called reg::.*_channel_control when written
  int try_transfer(ChannelView &channel, IRQ &irq);

  int load32(u32 &val, u32 index);
  int store32(u32 val, u32 index, IRQ &irq);
};
