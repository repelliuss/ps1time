#pragma once

#include "types.hpp"
#include "bits.hpp"
#include "range.hpp"

/// Depth of the pixel values in a texture page
enum struct TextureDepth : u8 {
  t4bit = 0,
  t8bit = 1,
  t15bit = 2,
};

/// Interlaced output splits each frame in two fields
enum struct Field {
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
 
/// Video output horizontal resolution
struct HorizontalRes {
  u8 val;

  constexpr u32 into_status() { return static_cast<u32>(val) << 16; }
};

constexpr HorizontalRes hres_from_fields(u8 hr1, u8 hr2) {
  return {
      .val = static_cast<u8>((hr2 & 1U) | ((hr1 & 3U) << 1U)),
  };
}

struct GPU {
  static constexpr u32 size = 8;
  static constexpr Range range = {0x1f801810, 0x1f801818};
  u8 data[size];

  /// True when the interrupt is active
  bool interrupt = false;

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

  /// Horizontal drawing offset applied to all vertex
  i16 drawing_x_offset = 0;

  /// Vertical drawing offset applied to all vertex
  i16 drawing_y_offset = 0;

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

  constexpr u32 status() {
    u32 val = 0;

    val |= static_cast<u32>(page_base_x) << 0;
    val |= static_cast<u32>(page_base_y) << 4;
    val |= static_cast<u32>(semi_transparency) << 5;
    val |= static_cast<u32>(tex_depth) << 7;
    val |= static_cast<u32>(dithering) << 9;
    val |= static_cast<u32>(draw_to_display) << 10;
    val |= static_cast<u32>(force_set_mask_bit) << 11;
    val |= static_cast<u32>(preserve_masked_pixels) << 12;
    val |= static_cast<u32>(field) << 13;
    
    // REVIEW: bit 14 is not supported, see nocash

    val |= static_cast<u32>(texture_disable) << 15;
    val |= hres.into_status();
    val |= static_cast<u32>(vres) << 19;
    val |= static_cast<u32>(video_mode) << 20;
    val |= static_cast<u32>(display_depth) << 21;
    val |= static_cast<u32>(interlaced) << 22;
    val |= static_cast<u32>(display_disabled) << 23;
    val |= static_cast<u32>(interrupt) << 24;

    // REVIEW: we pretend GPU is always ready

    // NOTE: ready to receive command
    val |= 1 << 26;
    // NOTE: ready to send VRAM to CPU
    val |= 1 << 27;
    // NOTE: ready to receive dma block
    val |= 1 << 28;

    val |= static_cast<u32>(dma_direction) << 29;

    // REVIEW: bit 31 should change depending on the currently drawn line
    // (whether it's even, odd or in the vblack apparently). we don't bother
    // with that right now
    val |= 0 << 31;

    u32 dma_request = 0;
    // REVIEW: following nocash spec here
    switch (dma_direction) {
    case DMAdirection::off:
      dma_request = 0;
      break;
    case DMAdirection::fifo:
      dma_request = 1;
      break;
    case DMAdirection::cpu_to_gp0:
      dma_request = bit(val, 28);
      break;
    case DMAdirection::vram_to_cpu:
      dma_request = bit(val, 27);
      break;
    }

    val |= dma_request << 25;

    return val;
  }

  constexpr u32 read() { return 0; }

  int gp0(u32 val);
  int gp1(u32 val);

  int gp0_draw_mode(u32 val);

  int gp1_soft_reset(u32 val);
};
