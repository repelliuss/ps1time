#include "gpu.hpp"

#include <stdio.h>

int GPU::gp0_draw_mode(u32 val) {
  page_base_x = bits_in_range(val, 0, 3);
  page_base_y = bit(val, 4);
  semi_transparency = bits_in_range(val, 5, 6);

  switch (bits_in_range(val, 7, 8)) {
  case 0:
    tex_depth = TextureDepth::t4bit;
    break;
  case 1:
    tex_depth = TextureDepth::t8bit;
    break;
  case 2:
    tex_depth = TextureDepth::t15bit;
    break;
  default:
    fprintf(stderr, "Unhandled texture depth %d\n", bits_in_range(val, 7, 9));
    return -1;
  }

  dithering = bit(val, 9) != 0;
  draw_to_display = bit(val, 10) != 0;
  texture_disable = bit(val, 11) != 0;
  rectangle_texture_x_flip = bit(val, 12) != 0;
  rectangle_texture_y_flip = bit(val, 13) != 0;

  return 0;
}

int GPU::gp0(u32 val) {
  u32 opcode = bits_in_range(val, 24, 31);

  switch (opcode) {
  case 0x00:
    return 0;
  case 0xe1:
    return gp0_draw_mode(val);
  }

  fprintf(stderr, "Unhandled GP0 command %08x\n", val);
  return -1;
}

int GPU::gp1_soft_reset(u32 val) {
  interrupt = false;

  page_base_x = 0;
  page_base_y = 0;
  semi_transparency = 0;
  tex_depth = TextureDepth::t4bit;
  
  texture_window_x_mask = 0;
  texture_window_y_mask = 0;
  texture_window_x_offset = 0;
  texture_window_y_offset = 0;
  
  dithering = false;
  draw_to_display = false;
  texture_disable = false;
  rectangle_texture_x_flip = false;
  rectangle_texture_y_flip = false;
  drawing_area_left = 0;
  drawing_area_top = 0;
  drawing_area_right = 0;
  drawing_area_bottom = 0;
  drawing_x_offset = 0;
  drawing_y_offset = 0;
  force_set_mask_bit = false;
  preserve_masked_pixels = false;

  // REVIEW: field is not being reset

  dma_direction = DMAdirection::off;

  display_disabled = true;
  display_vram_x_start = 0;
  display_vram_y_start = 0;
  hres = hres_from_fields(0, 0);
  vres = VerticalRes::y240lines;

  video_mode = VideoMode::ntsc;
  interlaced = true;
  display_horiz_start = 0x200;
  display_horiz_end = 0xc00;
  display_line_start = 0x10;
  display_line_end = 0x100;
  display_depth = DisplayDepth::d15bits;

  // TODO: should also clear the command FIFO
  // TODO: should also invalidate GPU cache

  return 0;
}

int GPU::gp1_display_mode(u32 val) {
  u8 hr1 = bits_in_range(val, 0, 1);
  u8 hr2 = bit(val, 6);
  hres = hres_from_fields(hr1, hr2);

  if(bit(val, 2) != 0) {
    vres = VerticalRes::y480lines;
  }
  else {
    vres = VerticalRes::y240lines;
  }

  if(bit(val, 3) != 0) {
    video_mode = VideoMode::pal;
  }
  else {
    video_mode = VideoMode::ntsc;
  }

  if(bit(val, 4) != 0) {
    display_depth = DisplayDepth::d15bits;
  }
  else {
    display_depth = DisplayDepth::d24bits;
  }

  interlaced = bit(val, 5) != 0;

  if(bit(val, 7) != 0) {
    printf("Unsupported display mode %08x\n", val);
    return -1;
  }

  return 0;
}

int GPU::gp1(u32 val) {
  u32 opcode = bits_in_range(val, 24, 31);

  switch(opcode) {
  case 0x00:
    return gp1_soft_reset(val);
  case 0x08:
    return gp1_display_mode(val);
  default:
    printf("Unhandled GP1 command %08x\n", val);
    return -1;
  }
}
