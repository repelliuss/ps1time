#pragma once

#include "types.hpp"
#include "bits.hpp"
#include "range.hpp"
#include "renderer.hpp"
#include "clock.hpp"
#include "interrupt.hpp"

struct Timers;

enum struct GP0mode {
  command,
  img_load,
};

/// Depth of the pixel values in a texture page
enum struct TextureDepth : u8 {
  t4bit = 0,
  t8bit = 1,
  t15bit = 2,
};

/// Interlaced output splits each frame in two fields
enum struct Field : u64 {
  top = 1,    // NOTE: odd lines
  bottom = 0, // NOTE: even lines
};

enum struct VerticalRes : u8 {
  y240lines = 0,
  y480lines = 1, // NOTE: only used for interlaced output
};

enum struct VideoMode : u8 {
  ntsc = 0, // NOTE: 480i60H
  pal = 1,  // NOTE: 576i50z
};

enum struct DisplayDepth : u8 {
  d15bits = 0,
  d24bits = 1,
};

enum struct DMAdirection : u8 {
  off = 0,
  fifo = 1,
  cpu_to_gp0 = 2,
  vram_to_cpu = 3
};

struct GPUcommandBuffer {
  u32 data[12];
  u8 count = 0;
};

constexpr void clear(GPUcommandBuffer &buf) { buf.count = 0; }

constexpr void append(GPUcommandBuffer &buf, u32 val) {
  buf.data[buf.count++] = val;
}

/// Video output horizontal resolution
struct HorizontalRes {
  u8 val;

  constexpr u32 into_status() { return static_cast<u32>(val) << 16; }
};

constexpr HorizontalRes hres_from_fields(u8 hr1, u8 hr2) {
  return {
      .val = static_cast<u8>((hr2 & 0b1U) | ((hr1 & 0b11U) << 1U)),
  };
}

struct GPU {
  static constexpr u32 size = 8;
  static constexpr Range range = {0x1f801810, 0x1f801818};
  u8 data[size];

  Renderer *renderer;

  GPU(Renderer *renderer, VideoMode configured_hardware_video_mode)
      : renderer(renderer),
        configured_hardware_video_mode(configured_hardware_video_mode) {}

  /// Texture page base X coordinate (4 bits, 64 byte increment)
  u8 page_base_x = 0;

  /// Texture page base Y coordinate (1 bit, 256 line increment)
  u8 page_base_y = 0;

  // REVIEW: not sure how to handle this value seems to blend the src and det
  // colors
  u8 semi_transparency = 0;

  /// Texture page color depth
  TextureDepth tex_depth = TextureDepth::t4bit;

  /// Texture window x mask (8 pixel steps)
  u8 texture_window_x_mask = 0;

  /// Texture window y mask (8 pixel steps)
  u8 texture_window_y_mask = 0;

  /// Texture window x offset (8 pixel steps)
  u8 texture_window_x_offset = 0;

  /// Texture window y offset (8 pixel steps)
  u8 texture_window_y_offset = 0;

  /// Enable dithering from 24 to 15bits RGB
  bool dithering = false;

  /// Allow drawing to the display area
  bool draw_to_display = false;

  /// When true all textures are disabled
  bool texture_disable = false;

  /// Mirror textured rectangles along x axis
  bool rectangle_texture_x_flip = false;

  /// Mirror textured rectangles along y axis
  bool rectangle_texture_y_flip = false;

  /// Left-most column of drawing area
  u16 drawing_area_left = 0;

  /// Top-most line of drawing area
  u16 drawing_area_top = 0;

  /// Right-most column of drawing area
  u16 drawing_area_right = 0;

  /// Bottom-most column of drawing area
  u16 drawing_area_bottom = 0;

  /// Force "mask" bit of the pixel to 1 when writing to VRAM, otherwise don't
  /// modify
  bool force_set_mask_bit = false;

  /// Don't draw to pixels which have the 'mask' bit set
  bool preserve_masked_pixels = false;

  /// DMA request direction
  DMAdirection dma_direction = DMAdirection::off;

  /// Currently displayed field. For progressive output this is always
  /// Field::top
  Field field = Field::top;

  /// Disable the display
  bool display_disabled = true;

  /// First column of the display area in VRAM
  u16 display_vram_x_start = 0;

  /// First line of the display area in VRAM
  u16 display_vram_y_start = 0;

  /// Video output horizontal resolution
  HorizontalRes hres = hres_from_fields(0, 0);

  /// Video output vertical resolution
  VerticalRes vres = VerticalRes::y240lines;

  VideoMode video_mode = VideoMode::ntsc;

  /// Output interlaced video signal instead of progressive
  bool interlaced = false;

  /// Display output horizontal start relative to HSYNC
  u16 display_horiz_start = 0x200;

  /// Display output horizontal end relative to HSYNC
  u16 display_horiz_end = 0xc00;

  /// Display output first line relative to VSYNC
  u16 display_line_start = 0x10;

  /// Display output last line relative to VSYNC
  u16 display_line_end = 0x100;

  /// Display depth. The GPU itself always draws 15bit RGB, 24 bit output must
  /// use external assers (pre-rendered textures, MDEC, etc...)
  DisplayDepth display_depth = DisplayDepth::d15bits;

  using GPUcommand = int(*)(GPU&, const GPUcommandBuffer &buf);
  GP0mode gp0_mode = GP0mode::command;

  /// Some commands needs more arguments than that can be given at once
  GPUcommandBuffer gp0_cmd_buf;
  u32 gp0_cmd_pending_words_count = 0;
  
  GPUcommand gp0_cmd;

  /// True when GP0 IRQ
  bool gp0_interrupt = false;

  /// True when VBLANK interrupt is high
  bool vblank_interrupt = false;

  /// Fractional GPU cycle remainder resulting from the CPU clock/GPU clock time
  /// conversion
  u16 gpu_clock_phase = 0;

  /// Currently displayed video output line
  u16 display_line = 0;

  /// Current GPU clock tick for the current line
  u16 display_line_tick = 0;

  /// PAL or NTSC, depends on the region
  VideoMode configured_hardware_video_mode = VideoMode::ntsc;

  u32 status();
  u32 read();
  int gp0(u32 val);
  int gp1(u32 val, Timers *timers, Clock &clock, IRQ &irq);

  int load32(u32 &val, u32 index, Clock &clock, IRQ &irq);
  int store32(u32 val, u32 index, Timers *timers, Clock &clock, IRQ &irq);

  // GPU timings
  void vmode_timings(u16 &horizontal, u16 &vertical);
  FractionalCycle gpu_to_cpu_clock_ratio();
  void clock_sync(Clock &clock, IRQ &irq);
  bool in_vblank();
  void predict_next_clock_sync(Clock &clock);

  u16 displayed_vram_line();

  FractionalCycle dotclock_period();
  int dotclock_phase(FractionalCycle &fc);
  FractionalCycle hsync_period();
  FractionalCycle hsync_phase();
};

static constexpr f32 CPU_FREQ_MHZ = 33.8685f;
