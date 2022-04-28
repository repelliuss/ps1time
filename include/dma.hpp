#pragma once

#include "types.hpp"
#include "range.hpp"
#include "ram.hpp"


struct DMA {
  static constexpr u32 size = 0x80;
  static constexpr Range range = {0x1f801080, 0x1f801100};

  // TODO: remove struct, prefix reg_ not really a type
  struct reg {
    static constexpr u32 control = 0x70;
    static constexpr u32 interrupt = 0x74;

    // only [0:23] are significant
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

  struct Channel {
    // add offsets to get proper register
    // for example:
    // +8 to get channel control
    // +4 to get block control
    // +0 to get base address
    // TODO: may be useless, see make_channel
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

    Type type;
    u32 base_address;
    u32 block_control;
    u32 channel_control;

    bool transfer_triggered();
    bool transfer_enabled();
    bool transfer_active();
    int try_transfer();
    SyncMode sync_mode();
  };

  u8 data[size] = {0};
  RAM &ram;

  DMA(RAM &ram);

  Channel make_channel(u32 dma_reg_index); // TODO: should not be really needed

  //reg::interrupt
  bool irq_active();
  void set_interrupt(u32 val);

  //may only be called reg::.*_channel_control when written
  //int channel_try_transfer(u32 dma_reg);
  
};
