#include "gpu.hpp"
#include "intrinsic.hpp"

#include <stdio.h>

static int gp0_draw_mode(GPU &gpu, const GPUcommandBuffer &buf) {
  u32 val = buf.data[0];
  
  gpu.page_base_x = bits_in_range(val, 0, 3);
  gpu.page_base_y = bit(val, 4);
  gpu.semi_transparency = bits_in_range(val, 5, 6);

  switch (bits_in_range(val, 7, 8)) {
  case 0:
    gpu.tex_depth = TextureDepth::t4bit;
    break;
  case 1:
    gpu.tex_depth = TextureDepth::t8bit;
    break;
  case 2:
    gpu.tex_depth = TextureDepth::t15bit;
    break;
  default:
    fprintf(stderr, "Unhandled texture depth %d\n", bits_in_range(val, 7, 9));
    return -1;
  }

  gpu.dithering = bit(val, 9) != 0;
  gpu.draw_to_display = bit(val, 10) != 0;
  gpu.texture_disable = bit(val, 11) != 0;
  gpu.rectangle_texture_x_flip = bit(val, 12) != 0;
  gpu.rectangle_texture_y_flip = bit(val, 13) != 0;

  return 0;
}

static int gp0_drawing_area_top_left(GPU &gpu, const GPUcommandBuffer &buf) {
  u32 val = buf.data[0];

  gpu.drawing_area_left = bits_in_range(val, 0, 9);
  gpu.drawing_area_top = bits_in_range(val, 10, 19);
  return 0;
}

static int gp0_drawing_area_bottom_right(GPU &gpu,
                                         const GPUcommandBuffer &buf) {
  u32 val = buf.data[0];

  gpu.drawing_area_right = bits_in_range(val, 0, 9);
  gpu.drawing_area_bottom = bits_in_range(val, 10, 19);
  return 0;
}

static int gp0_drawing_offset(GPU &gpu, const GPUcommandBuffer &buf) {
  u32 val = buf.data[0];
  
  u32 x = bits_in_range(val, 0, 10);
  u32 y = bits_in_range(val, 11, 21);

  // values are 11 bit signed, forcing sign extension
  gpu.drawing_x_offset = (static_cast<i16>(x << 5)) >> 5;
  gpu.drawing_y_offset = (static_cast<i16>(y << 5)) >> 5;

  gpu.renderer->display();

  return 0;
}

static int gp0_texture_window(GPU &gpu, const GPUcommandBuffer &buf) {
  u32 val = buf.data[0];

  gpu.texture_window_x_mask = bits_in_range(val, 0, 4);
  gpu.texture_window_y_mask = bits_in_range(val, 5, 9);
  gpu.texture_window_x_offset = bits_in_range(val, 10, 14);
  gpu.texture_window_y_offset = bits_in_range(val, 15, 19);

  return 0;
}

static int gp0_mask_bit_setting(GPU &gpu, const GPUcommandBuffer &buf) {
  u32 val = buf.data[0];

  gpu.force_set_mask_bit = bit(val, 0) != 0;
  gpu.preserve_masked_pixels = bit(val, 1) != 0;

  return 0;
}

// TODO: unimplemented
static int gp0_quad_mono_opaque(GPU &gpu, const GPUcommandBuffer &buf) {
  printf("Draw quad\n");
  return 0;
}

static int gp0_nope(GPU &gpu, const GPUcommandBuffer &buf) { return 0; }

// TODO: unimplemented
static int gp0_clear_cache(GPU &gpu, const GPUcommandBuffer &buf) { return 0; }

static int gp0_load_image(GPU &gpu, const GPUcommandBuffer &buf) {
  u32 resolution = buf.data[2];

  u32 width = resolution & 0xffff;
  u32 height = resolution >> 16;

  u32 img_size = width * height;

  // NOTE: round up if odd, gpu uses 16bits aligned
  img_size = (img_size + 1) & (~1);

  // NOTE: number of words
  gpu.gp0_cmd_pending_words_count = img_size / 2;

  gpu.gp0_mode = GP0mode::img_load;
  
  return 0;
}

static int gp0_store_image(GPU &gpu, const GPUcommandBuffer &buf) {
  u32 resolution = buf.data[2];

  u32 width = resolution & 0xffff;
  u32 height = resolution >> 16;

  printf("Unhandled image store: %dx%d\n", width, height);
  return 0;
}

static int gp0_quad_shaded_opaque(GPU &gpu, const GPUcommandBuffer &buf) {
  printf("Draw quad shaded\n");
  return 0;
}

static int gp0_triangle_shaded_opaque(GPU &gpu, const GPUcommandBuffer &buf) {
  Position positions[3] = {
      pos_from_gp0(buf.data[1]),
      pos_from_gp0(buf.data[3]),
      pos_from_gp0(buf.data[5]),
  };

  Color colors[3] = {
    color_from_gp0(buf.data[0]),
    color_from_gp0(buf.data[2]),
    color_from_gp0(buf.data[4]),
  };

  gpu.renderer->put_triangle(positions, colors);
    
  return 0;
}

int gp0_quad_texture_blend_opaque(GPU &gpu, const GPUcommandBuffer &buf) {
  printf("Draw quad texture blending\n");
  return 0;
}

int GPU::gp0(u32 val) {
  u32 opcode = bits_in_range(val, 24, 31);

  if (gp0_cmd_pending_words_count == 0) {

    clear(gp0_cmd_buf);

    switch (opcode) {
    case 0x00:
      gp0_cmd_pending_words_count = 1;
      gp0_cmd = gp0_nope;
      break;
    case 0x01:
      gp0_cmd_pending_words_count = 1;
      gp0_cmd = gp0_clear_cache;
      break;
    case 0x28:
      gp0_cmd_pending_words_count = 5;
      gp0_cmd = gp0_quad_mono_opaque;
      break;
    case 0x2c:
      gp0_cmd_pending_words_count = 9;
      gp0_cmd = gp0_quad_mono_opaque;
      break;
    case 0x30:
      gp0_cmd_pending_words_count = 6;
      gp0_cmd = gp0_triangle_shaded_opaque;
      break;
    case 0x38:
      gp0_cmd_pending_words_count = 8;
      gp0_cmd = gp0_quad_shaded_opaque;
      break;
    case 0xa0:
      gp0_cmd_pending_words_count = 3;
      gp0_cmd = gp0_load_image;
      break;
    case 0xc0:
      gp0_cmd_pending_words_count = 3;
      gp0_cmd = gp0_store_image;
      break;
    case 0xe1:
      gp0_cmd_pending_words_count = 1;
      gp0_cmd = gp0_draw_mode;
      break;
    case 0xe2:
      gp0_cmd_pending_words_count = 1;
      gp0_cmd = gp0_texture_window;
      break;
    case 0xe3:
      gp0_cmd_pending_words_count = 1;
      gp0_cmd = gp0_drawing_area_top_left;
      break;
    case 0xe4:
      gp0_cmd_pending_words_count = 1;
      gp0_cmd = gp0_drawing_area_bottom_right;
      break;
    case 0xe5:
      gp0_cmd_pending_words_count = 1;
      gp0_cmd = gp0_drawing_offset;
      break;
    case 0xe6:
      gp0_cmd_pending_words_count = 1;
      gp0_cmd = gp0_mask_bit_setting;
      break;

    default:
      printf("Unhandled GP0 command %08x\n", val);
      return -1;
    }
  }

  --gp0_cmd_pending_words_count;

  if (gp0_mode == GP0mode::command) {
    append(gp0_cmd_buf, val);
    if (gp0_cmd_pending_words_count == 0) {
      return gp0_cmd(*this, gp0_cmd_buf);
    }
  } else {
    // TODO: should copy pixel data to vram
    if (gp0_cmd_pending_words_count == 0) {
      // load done
      gp0_mode = GP0mode::command;
    }
  }

  return 0;
}

int gp1_dma_direction(GPU &gpu, u32 val) {
  switch (bits_in_range(val, 0, 1)) {
  case 0:
    gpu.dma_direction = DMAdirection::off;
    break;
  case 1:
    gpu.dma_direction = DMAdirection::fifo;
    break;
  case 2:
    gpu.dma_direction = DMAdirection::cpu_to_gp0;
    break;
  case 3:
    gpu.dma_direction = DMAdirection::vram_to_cpu;
    break;
  }

  return 0;
}

int gp1_soft_reset(GPU &gpu, u32 val) {
  gpu.interrupt = false;

  gpu.page_base_x = 0;
  gpu.page_base_y = 0;
  gpu.semi_transparency = 0;
  gpu.tex_depth = TextureDepth::t4bit;

  gpu.texture_window_x_mask = 0;
  gpu.texture_window_y_mask = 0;
  gpu.texture_window_x_offset = 0;
  gpu.texture_window_y_offset = 0;

  gpu.dithering = false;
  gpu.draw_to_display = false;
  gpu.texture_disable = false;
  gpu.rectangle_texture_x_flip = false;
  gpu.rectangle_texture_y_flip = false;
  gpu.drawing_area_left = 0;
  gpu.drawing_area_top = 0;
  gpu.drawing_area_right = 0;
  gpu.drawing_area_bottom = 0;
  gpu.drawing_x_offset = 0;
  gpu.drawing_y_offset = 0;
  gpu.force_set_mask_bit = false;
  gpu.preserve_masked_pixels = false;

  // REVIEW: field is not being reset

  gpu.dma_direction = DMAdirection::off;

  gpu.display_disabled = true;
  gpu.display_vram_x_start = 0;
  gpu.display_vram_y_start = 0;
  gpu.hres = hres_from_fields(0, 0);
  gpu.vres = VerticalRes::y240lines;

  gpu.video_mode = VideoMode::ntsc;
  gpu.interlaced = true;
  gpu.display_horiz_start = 0x200;
  gpu.display_horiz_end = 0xc00;
  gpu.display_line_start = 0x10;
  gpu.display_line_end = 0x100;
  gpu.display_depth = DisplayDepth::d15bits;

  // TODO: should also clear the command FIFO
  // TODO: should also invalidate GPU cache

  return 0;
}

int gp1_display_mode(GPU &gpu, u32 val) {
  u8 hr1 = bits_in_range(val, 0, 1);
  u8 hr2 = bit(val, 6);
  gpu.hres = hres_from_fields(hr1, hr2);

  if (bit(val, 2) != 0) {
    gpu.vres = VerticalRes::y480lines;
  } else {
    gpu.vres = VerticalRes::y240lines;
  }

  if (bit(val, 3) != 0) {
    gpu.video_mode = VideoMode::pal;
  } else {
    gpu.video_mode = VideoMode::ntsc;
  }

  if (bit(val, 4) != 0) {
    gpu.display_depth = DisplayDepth::d15bits;
  } else {
    gpu.display_depth = DisplayDepth::d24bits;
  }

  gpu.interlaced = bit(val, 5) != 0;

  if (bit(val, 7) != 0) {
    printf("Unsupported display mode %08x\n", val);
    return -1;
  }

  return 0;
}

int gp1_display_vram_start(GPU &gpu, u32 val) {
  // lsb ignored to be always aligned to a 16 bit pixel
  gpu.display_vram_x_start = (val & 0b1111111110);
  gpu.display_vram_y_start = bits_in_range(val, 10, 18);
  return 0;
}

int gp1_display_horizontal_range(GPU &gpu, u32 val) {
  gpu.display_horiz_start = bits_in_range(val, 0, 11);
  gpu.display_horiz_end = bits_in_range(val, 12, 23);
  return 0;
}

int gp1_display_vertical_range(GPU &gpu, u32 val) {
  gpu.display_line_start = bits_in_range(val, 0, 9);
  gpu.display_line_end = bits_in_range(val, 10, 19);
  return 0;
}

static int gp1_display_enable(GPU &gpu, u32 val) {
  gpu.display_disabled = bit(val, 0) != 0;
  return 0;
}

static int gp1_ack_irq(GPU &gpu, u32 val) {
  gpu.interrupt = false;
  return 0;
}

static int gp1_reset_command_buffer(GPU &gpu, u32 val) {
  clear(gpu.gp0_cmd_buf);
  gpu.gp0_cmd_pending_words_count = 0;
  gpu.gp0_mode = GP0mode::command;
  return 0;
}

int GPU::gp1(u32 val) {
  u32 opcode = bits_in_range(val, 24, 31);

  switch (opcode) {
  case 0x00:
    return gp1_soft_reset(*this, val);
  case 0x01:
    return gp1_reset_command_buffer(*this, val);
  case 0x02:
    return gp1_ack_irq(*this, val);
  case 0x03:
    return gp1_display_enable(*this, val);
  case 0x04:
    return gp1_dma_direction(*this, val);
  case 0x05:
    return gp1_display_vram_start(*this, val);
  case 0x06:
    return gp1_display_horizontal_range(*this, val);
  case 0x07:
    return gp1_display_vertical_range(*this, val);
  case 0x08:
    return gp1_display_mode(*this, val);
  default:
    printf("Unhandled GP1 command %08x\n", val);
    return -1;
  }
}

u32 GPU::read() {
  printf("GPUREAD\n");
  return 0;
}
