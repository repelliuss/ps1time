#include "gpu.hpp"
#include "intrinsic.hpp"
#include "log.hpp"

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
    LOG_ERROR("Unhandled texture depth %d", bits_in_range(val, 7, 9));
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
  i16 offset_x = (static_cast<i16>(x << 5)) >> 5;
  i16 offset_y = (static_cast<i16>(y << 5)) >> 5;

  gpu.renderer->set_drawing_offset(x, y);

  return gpu.renderer->display();
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

static int gp0_quad_mono_opaque(GPU &gpu, const GPUcommandBuffer &buf) {
  const Position positions[4] = {
      pos_from_gp0(buf.data[1]),
      pos_from_gp0(buf.data[2]),
      pos_from_gp0(buf.data[3]),
      pos_from_gp0(buf.data[4]),
  };

  const Color colors[4] = {
      color_from_gp0(buf.data[0]),
      color_from_gp0(buf.data[0]),
      color_from_gp0(buf.data[0]),
      color_from_gp0(buf.data[0]),
  };

  return gpu.renderer->put_quad(positions, colors);
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

  LOG_WARN("Unhandled image store: %dx%d", width, height);
  // TODO: doesn't return an error
  return 0;
}

static int gp0_quad_shaded_opaque(GPU &gpu, const GPUcommandBuffer &buf) {
  const Position positions[4] = {
      pos_from_gp0(buf.data[1]),
      pos_from_gp0(buf.data[3]),
      pos_from_gp0(buf.data[5]),
      pos_from_gp0(buf.data[7]),
  };

  const Color colors[4]{
      color_from_gp0(buf.data[0]),
      color_from_gp0(buf.data[2]),
      color_from_gp0(buf.data[4]),
      color_from_gp0(buf.data[6]),
  };

  return gpu.renderer->put_quad(positions, colors);
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
  Position positions[4] = {
      pos_from_gp0(buf.data[1]),
      pos_from_gp0(buf.data[3]),
      pos_from_gp0(buf.data[5]),
      pos_from_gp0(buf.data[7]),
  };

  Color colors[4] = {
      color(0x80, 0x00, 0x00),
      color(0x80, 0x00, 0x00),
      color(0x80, 0x00, 0x00),
      color(0x80, 0x00, 0x00),
  };

  return gpu.renderer->put_quad(positions, colors);
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
      gp0_cmd = gp0_quad_texture_blend_opaque;
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
      LOG_ERROR("Unhandled GP0 command 0x%x", val);
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

  gpu.renderer->set_drawing_offset(0, 0);

  gpu.force_set_mask_bit = false;
  gpu.preserve_masked_pixels = false;

  // REVIEW: field member is not being reset

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
    LOG_ERROR("Unsupported display mode 0x%x", val);
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
    LOG_ERROR("Unhandled GP1 command 0x%x", val);
    return -1;
  }
}

u32 GPU::status() {
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

  // HACK: need to emulate bit 13 or returning 1 here locks BIOS
  // val |= static_cast<u32>(vres) << 19;

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

u32 GPU::read() {
  LOG_WARN("GPUREAD");
  return 0;
}

int GPU::load32(u32 &val, u32 index) {
  switch (index) {
  case 0:
    val = read();
    return 0;
  case 4:
    val = status();
    return 0;
  }

  LOG_ERROR("[FN:%s IND:%d] Unhandled", "GPU::load32", index);
  return -1;
}

int GPU::store32(u32 val, u32 index) {
  switch (index) {
  case 0:
    return gp0(val);
  case 4:
    return gp1(val);
  }

  assert(false);
  return -1;
}
