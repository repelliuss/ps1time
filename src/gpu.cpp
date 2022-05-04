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

  switch(opcode) {
  case 0x00:
    return 0;
  case 0xe1:
    return gp0_draw_mode(val);
  }

  fprintf(stderr, "Unhandled GP0 opcode %02x\n", opcode);
  return -1;
}
