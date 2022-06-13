#pragma once

#include "types.hpp"
#include "log.hpp"
#include "gte_divider.hpp"

#include "assert.h"


enum Matrix {
  rotation = 0,
  light = 1,
  colr = 2,
  invalid,
};

constexpr Matrix matrix_from_command(u32 command) {
  switch((command >> 17) & 3) {
  case 0:
    return Matrix::rotation;
  case 1:
    return Matrix::light;
  case 2:
    return Matrix::colr;
  case 3:
    return Matrix::invalid;
  default:
    LOG_CRITICAL("unreachable");
    assert(false);
    return Matrix::invalid;
  }
}

enum ControlVector {
  translation = 0,
  background_color = 1,
  far_color = 2,
  zerro = 3,
};

constexpr ControlVector control_vector_from_command(u32 command) {
  switch ((command >> 13) & 3) {
  case 0:
    return ControlVector::translation;
  case 1:
    return ControlVector::background_color;
  case 2:
    return ControlVector::far_color;
  case 3:
    return ControlVector::zerro;
  default:
    LOG_CRITICAL("unreachable");
    assert(false);
    return ControlVector::zerro;
  }
}

struct CommandConfig {
  u8 shift;
  bool clamp_negative;
  Matrix matrix;
  ControlVector control_vector;
};

constexpr CommandConfig command_config_from_command(u32 command) {
  u8 shift = 0;

  if ((command & (1 << 19)) != 0) {
    shift = 12;
  }

  bool clamp_negative = (command & (1 << 10)) != 0;

  return {.shift = shift,
          .clamp_negative = clamp_negative,
          .matrix = matrix_from_command(command),
          .control_vector = control_vector_from_command(command)};
}

struct GTE {
  i32 ofx = 0;

  i32 ofy = 0;

  u16 h = 0;

  i16 dqa = 0;

  i32 dqb = 0;

  i16 zsf3 = 0;

  i16 zsf4 = 0;

  i16 matrices[3][3][3] = {
      {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
      {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
      {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
  };

  i32 control_vectors[4][3] = {
      {0, 0, 0},
      {0, 0, 0},
      {0, 0, 0},
      {0, 0, 0},
  };

  u32 flags = 0;

  i16 v[4][3] = {
      {0, 0, 0},
      {0, 0, 0},
      {0, 0, 0},
      {0, 0, 0},
  };

  i32 mac[4] = {0,0,0,0};

  u16 otz = 0;

  struct RGBX {
    u8 r, g, b, x;
  };

  RGBX rgb = {0, 0, 0, 0};

  i16 ir[4] = {0, 0, 0, 0};

  struct Tuple2i16 {
    i16 x, y;
  };

  Tuple2i16 xy_fifo[4] = {
      {0, 0},
      {0, 0},
      {0, 0},
      {0, 0},
  };

  u16 z_fifo[4] = {0, 0, 0, 0};

  RGBX rgb_fifo[3] = {{0,0,0,0}, {0,0,0,0}, {0,0,0,0}};

  u32 lzcs = 0;
  
  u8 lzcr = 32;

  u32 control(u32 reg) {
    switch(reg) {
    case 0: {
      auto& matrix = matrices[Matrix::rotation];

      u32 v0 = static_cast<u16>(matrix[0][0]);
      u32 v1 = static_cast<u16>(matrix[0][1]);

      return v0 | (v1 << 16);
    } break;

    case 1: {
      auto &matrix = matrices[Matrix::rotation];

      u32 v0 = static_cast<u16>(matrix[0][2]);
      u32 v1 = static_cast<u16>(matrix[1][0]);

      return v0 | (v1 << 16);
    } break;
      
    case 2: {
      auto& matrix = matrices[Matrix::rotation];

      u32 v0 = static_cast<u16>(matrix[1][1]);
      u32 v1 = static_cast<u16>(matrix[1][2]);

      return v0 | (v1 << 16);
    } break;
      
    case 3: {
      auto &matrix = matrices[Matrix::rotation];

      u32 v0 = static_cast<u16>(matrix[2][0]);
      u32 v1 = static_cast<u16>(matrix[2][1]);

      return v0 | (v1 << 16);

    } break;
      
    case 4: {
      auto &matrix = matrices[Matrix::rotation];

      return static_cast<u16>(matrix[2][2]);
    } break;

    case 5:
    case 6:
    case 7: {
      auto& vector = control_vectors[ControlVector::translation];
      return vector[reg - 5];
    } break;
      
    case 8: {
      auto& matrix = matrices[Matrix::light];

      u32 v0 = static_cast<u16>(matrix[0][0]);
      u32 v1 = static_cast<u16>(matrix[0][1]);

      return v0 | (v1 << 16);
    } break;

    case 9: {
      auto &matrix = matrices[Matrix::light];

      u32 v0 = static_cast<u16>(matrix[0][2]);
      u32 v1 = static_cast<u16>(matrix[1][0]);

      return v0 | (v1 << 16);
    } break;
      
    case 10: {
      auto& matrix = matrices[Matrix::light];

      u32 v0 = static_cast<u16>(matrix[1][1]);
      u32 v1 = static_cast<u16>(matrix[1][2]);

      return v0 | (v1 << 16);
    } break;
      
    case 11: {
      auto &matrix = matrices[Matrix::light];

      u32 v0 = static_cast<u16>(matrix[2][0]);
      u32 v1 = static_cast<u16>(matrix[2][1]);

      return v0 | (v1 << 16);

    } break;

    case 12: {
      auto &matrix = matrices[Matrix::light];

      return static_cast<u16>(matrix[2][2]);
    } break;

    case 13:
    case 14:
    case 15: {
      auto &vector = control_vectors[ControlVector::background_color];
      return vector[reg - 13];
    } break;

    case 16: {
      auto &matrix = matrices[Matrix::colr];

      u32 v0 = static_cast<u16>(matrix[0][0]);
      u32 v1 = static_cast<u16>(matrix[0][1]);

      return v0 | (v1 << 16);
    } break;

    case 17: {
      auto &matrix = matrices[Matrix::colr];

      u32 v0 = static_cast<u16>(matrix[0][2]);
      u32 v1 = static_cast<u16>(matrix[1][0]);

      return v0 | (v1 << 16);
    } break;

    case 18: {
      auto &matrix = matrices[Matrix::colr];

      u32 v0 = static_cast<u16>(matrix[1][1]);
      u32 v1 = static_cast<u16>(matrix[1][2]);

      return v0 | (v1 << 16);
    } break;

    case 19: {
      auto &matrix = matrices[Matrix::colr];

      u32 v0 = static_cast<u16>(matrix[2][0]);
      u32 v1 = static_cast<u16>(matrix[2][1]);

      return v0 | (v1 << 16);

    } break;

    case 20: {
      auto &matrix = matrices[Matrix::colr];

      return static_cast<u16>(matrix[2][2]);
    } break;

    case 21:
    case 22:
    case 23: {
      auto &vector = control_vectors[ControlVector::far_color];
      return vector[reg - 21];
    } break;

    case 24: {
      return ofx;
    } break;

    case 25: {
      return ofy;
    } break;

    case 26: {
      return static_cast<i16>(h);
    } break;

    case 27: {
      return dqa;
    } break;

    case 28: {
      return dqb;
    } break;

    case 29: {
      return zsf3;
    } break;

    case 30: {
      return zsf4;
    } break;

    case 31: {
      return flags;
    } break;
    }

    LOG_ERROR("Unhandled GTE control register %d", reg);
    assert("false && should return error here");
    return -1;
  }

  int set_control(u32 reg, u32 val) {
    switch (reg) {

    case 0: {
      i16 v0 = val;
      i16 v1(val >> 16);

      auto &matrix = matrices[Matrix::rotation];

      matrix[0][0] = v0;
      matrix[0][1] = v1;
    } break;

    case 1: {
      i16 v0 = val;
      i16 v1(val >> 16);

      auto &matrix = matrices[Matrix::rotation];

      matrix[0][2] = v0;
      matrix[1][0] = v1;
    } break;

    case 2: {
      i16 v0 = val;
      i16 v1(val >> 16);

      auto &matrix = matrices[Matrix::rotation];

      matrix[1][1] = v0;
      matrix[1][2] = v1;
    } break;

    case 3: {
      i16 v0 = val;
      i16 v1(val >> 16);

      auto &matrix = matrices[Matrix::rotation];

      matrix[2][0] = v0;
      matrix[2][1] = v1;
    } break;

    case 4: {
      auto &matrix = matrices[Matrix::rotation];

      matrix[2][2] = static_cast<i16>(val);
    } break;

    case 5:
    case 6:
    case 7: {
      auto &vector = control_vectors[ControlVector::translation];
      vector[reg - 5] = static_cast<i32>(val);
    } break;

    case 8: {
      i16 v0 = val;
      i16 v1(val >> 16);

      auto &matrix = matrices[Matrix::light];

      matrix[0][0] = v0;
      matrix[0][1] = v1;
    } break;

    case 9: {
      i16 v0 = val;
      i16 v1(val >> 16);

      auto &matrix = matrices[Matrix::light];

      matrix[0][2] = v0;
      matrix[1][0] = v1;
    } break;

    case 10: {
      i16 v0 = val;
      i16 v1(val >> 16);

      auto &matrix = matrices[Matrix::light];

      matrix[1][1] = v0;
      matrix[1][2] = v1;
    } break;

    case 11: {
      i16 v0 = val;
      i16 v1(val >> 16);

      auto &matrix = matrices[Matrix::light];

      matrix[2][0] = v0;
      matrix[2][1] = v1;
    } break;

    case 12: {
      auto &matrix = matrices[Matrix::light];

      matrix[2][2] = static_cast<i16>(val);
    } break;

    case 13:
    case 14:
    case 15: {
      auto &vector = control_vectors[ControlVector::background_color];
      vector[reg - 13] = static_cast<i32>(val);
    } break;

    case 16: {
      i16 v0 = val;
      i16 v1(val >> 16);

      auto &matrix = matrices[Matrix::colr];

      matrix[0][0] = v0;
      matrix[0][1] = v1;
    } break;

    case 17: {
      i16 v0 = val;
      i16 v1(val >> 16);

      auto &matrix = matrices[Matrix::colr];

      matrix[0][2] = v0;
      matrix[1][0] = v1;
    } break;

    case 18: {
      i16 v0 = val;
      i16 v1(val >> 16);

      auto &matrix = matrices[Matrix::colr];

      matrix[1][1] = v0;
      matrix[1][2] = v1;
    } break;

    case 19: {
      i16 v0 = val;
      i16 v1(val >> 16);

      auto &matrix = matrices[Matrix::colr];

      matrix[2][0] = v0;
      matrix[2][1] = v1;
    } break;

    case 20: {
      auto &matrix = matrices[Matrix::colr];

      matrix[2][2] = static_cast<i16>(val);
    } break;

    case 21:
    case 22:
    case 23: {
      auto &vector = control_vectors[ControlVector::far_color];
      vector[reg - 21] = static_cast<i32>(val);
    } break;

    case 24:
      ofx = val;
      break;

    case 25:
      ofy = val;
      break;

    case 26:
      h = val;
      break;

    case 27:
      dqa = val;
      break;

    case 28:
      dqb = val;
      break;

    case 29:
      zsf3 = val;
      break;

    case 30:
      zsf4 = val;
      break;

    case 31: {
      flags = val & 0x7ffff00;

      bool msb = (val & 0x7f87e000) != 0;

      flags |= (static_cast<u32>(msb)) << 31;
    } break;

    default:
      LOG_ERROR("Unhandled GTE control register %d %x", reg, val);
      return -1;
    }

    return 0;
  }

  int data(u32 &val, u32 reg) {
    switch (reg) {
      
    case 0: {
      u32 v0 = static_cast<u16>(v[0][0]);
      u32 v1 = static_cast<u16>(v[0][1]);
      val = v0 | (v1 << 16);
    } break;

    case 1:
      val = v[0][2];
      break;

    case 2: {
      u32 v0 = static_cast<u16>(v[1][0]);
      u32 v1 = static_cast<u16>(v[1][1]);
      val = v0 | (v1 << 16);
    } break;

    case 3:
      val = v[1][2];
      break;

    case 4: {
      u32 v0 = static_cast<u16>(v[2][0]);
      u32 v1 = static_cast<u16>(v[2][1]);
      val = v0 | (v1 << 16);
    } break;

    case 5:
      val = v[2][2];
      break;

    case 6: {
      u32 r = rgb.r;
      u32 g = rgb.b;
      u32 b = rgb.b;
      u32 x = rgb.x;

      val = r | (g << 8) | (b << 16) | (x << 24);
    } break;


    case 7:
      val = otz;
      break;

    case 8:
      val = ir[0];
      break;

    case 9:
      val = ir[1];
      break;

    case 10:
      val = ir[2];
      break;

    case 11:
      val = ir[3];
      break;


    case 12: {
      auto& xy = xy_fifo[0];
      u32 a = xy.x;
      u32 b = xy.y;

      val = a | (b << 16);
    } break;
    case 13: {
      auto& xy = xy_fifo[1];
      u32 a = xy.x;
      u32 b = xy.y;

      val = a | (b << 16);
    } break;
    case 14: {
      auto& xy = xy_fifo[2];
      u32 a = xy.x;
      u32 b = xy.y;

      val = a | (b << 16);
    } break;
    case 15: {
      auto& xy = xy_fifo[3];
      u32 a = xy.x;
      u32 b = xy.y;

      val = a | (b << 16);
    } break;

    case 16:
      val = z_fifo[0];
      break;
    case 17:
      val = z_fifo[1];
      break;
    case 18:
      val = z_fifo[2];
      break;
    case 19:
      val = z_fifo[3];
      break;

    case 22: {
      u32 r = rgb_fifo[2].r;
      u32 g = rgb_fifo[2].g;
      u32 b = rgb_fifo[2].b;
      u32 x = rgb_fifo[2].x;

      val = r | (g << 8) | (b << 16) | (x << 24);
    } break;

    case 24:
      val = mac[0];
      break;
    case 25:
      val = mac[1];
      break;
    case 26:
      val = mac[2];
      break;
    case 27:
      val = mac[3];
      break;

    case 28:
    case 29:
      {
	auto saturate = [](i16 v) -> u32 {
	  if (v < 0) {
	    return 0;
	  }
	  else if(v > 0x1f) {
	    return 0x1f;
	  }
	  else {
	    return v;
	  }
	};

	u32 a = saturate(ir[1] >> 7);
	u32 b = saturate(ir[2] >> 7);
        u32 c = saturate(ir[3] >> 7);

        val = a | (b << 5) | (c << 10);
    } break;

    case 31:
      val = lzcr;
      break;

    default:
      LOG_ERROR("Unhandled GTE data register %d", reg);
      return -1;
    }

    return 0;
  }

  int set_data(u32 reg, u32 val) {
    switch (reg) {
    case 0: {
      i16 v0 = val;
      i16 v1 = val >> 16;

      v[0][0] = v0;
      v[0][1] = v1;
    } break;

    case 1: {
      v[0][2] = static_cast<i16>(val);
    } break;

    case 2: {
      i16 v0 = val;
      i16 v1 = val >> 16;

      v[1][0] = v0;
      v[1][1] = v1;
    } break;

    case 3: {
      v[1][2] = static_cast<i16>(val);
    } break;

    case 4: {
      i16 v0 = val;
      i16 v1 = val >> 16;

      v[2][0] = v0;
      v[2][1] = v1;
    } break;

    case 5: {
      v[2][2] = static_cast<i16>(val);
    } break;

    case 6: {
      rgb.r = val;
      rgb.g = val >> 8;
      rgb.b = val >> 16;
      rgb.x = val >> 24;
    } break;

    case 7:
      otz = val;
      break;

    case 8:
      ir[0] = val;
      break;

    case 9:
      ir[1] = val;
      break;

    case 10:
      ir[2] = val;
      break;

    case 11:
      ir[3] = val;
      break;

    case 12: {
      xy_fifo[0].x = val;
      xy_fifo[0].y = (val >> 16);
    } break;

    case 13: {
      xy_fifo[1].x = val;
      xy_fifo[1].y = (val >> 16);
    } break;

    case 14: {
      xy_fifo[2].x = val;
      xy_fifo[2].y = (val >> 16);
      
      xy_fifo[3].x = val;
      xy_fifo[3].y = (val >> 16);
    } break;

    case 16:
      z_fifo[0] = val;
      break;
    case 17:
      z_fifo[1] = val;
      break;
    case 18:
      z_fifo[2] = val;
      break;
    case 19:
      z_fifo[3] = val;
      break;

    case 22: {
      rgb_fifo[2].r = val;
      rgb_fifo[2].g = val >> 8;
      rgb_fifo[2].b = val >> 16;
      rgb_fifo[2].x = val >> 24;
    } break;

    case 24:
      mac[0] = val;
      break;

    case 25:
      mac[1] = val;
      break;

    case 26:
      mac[2] = val;
      break;

    case 27:
      mac[3] = val;
      break;

    case 31:
      LOG_DEBUG("Write to read-only GTE data register 31");
      break;

    default:
      LOG_ERROR("Unhandled GTE data register %d %x", reg, val);
      return -1;
    }

    return 0;
  }

  int command(u32 command) {
    u32 opcode = command & 0x3f;
    auto config = command_config_from_command(command);
    flags = 0;

    int status = 0;
    switch(opcode) {
    case 0x06:
      status |= cmd_nclip();
      break;

    case 0x13:
      status |= cmd_ncds(config);
      break;

    case 0x2d:
      status |= cmd_avsz3();
      break;

    case 0x30:
      status |= cmd_rtpt(config);
      break;

    default:
      LOG_ERROR("Unhandled GTE opdcode %02x", opcode);
      return -1;
    }

    bool msb = (flags & 0x7f87e000) != 0;
    flags |= static_cast<u32>(msb) << 31;
    return status;
  }

  int cmd_nclip() {
    i32 x0 = xy_fifo[0].x;
    i32 y0 = xy_fifo[0].y;

    i32 x1 = xy_fifo[1].x;
    i32 y1 = xy_fifo[1].y;

    i32 x2 = xy_fifo[2].x;
    i32 y2 = xy_fifo[2].y;

    i32 a = x0 * (y1 - y2);
    i32 b = x1 * (y2 - y0);
    i32 c = x2 * (y0 - y1);

    i64 sum = static_cast<i64>(a) + static_cast<i64>(b) + static_cast<i64>(c);

    mac[0] = i64_to_i32_result(sum);

    return 0;
  }

  int cmd_ncds(CommandConfig config) {
    do_ncd(config, 0);
    return 0;
  }

  int cmd_avsz3() {
    u32 z1 = z_fifo[1];
    u32 z2 = z_fifo[2];
    u32 z3 = z_fifo[3];

    u32 sum = z1 + z2 + z3;

    i64 zsf3 = this->zsf3;

    i64 average = zsf3 * static_cast<i64>(sum);

    mac[0] = i64_to_i32_result(average);
    otz = i64_to_otz(average);
    return 0;
  }

  int cmd_rtpt(CommandConfig config) {
    do_rtp(config, 0);
    do_rtp(config, 1);
    u32 projection_factor = do_rtp(config, 2);

    depth_queuing(projection_factor);

    return 0;
  }

  void do_ncd(CommandConfig config, u32 vector_index) {
    multiply_matrix_by_vector(config, Matrix::light, vector_index,
                              ControlVector::zerro);

    v[3][0] = ir[1];
    v[3][1] = ir[2];
    v[3][2] = ir[3];

    multiply_matrix_by_vector(config, Matrix::colr, 3,
                              ControlVector::background_color);

    u8 r = rgb.r;
    u8 g = rgb.g;
    u8 b = rgb.b;
    
    u8 cool[3] = {r, g, b};
    for(int i = 0; i < 3; ++i) {
      i64 fc = static_cast<i64>(control_vectors[ControlVector::far_color][i])
               << 12;
      i32 ir = this->ir[i+1];
      i32 col = static_cast<i32>(cool[i]) << 4;
      i64 shading = (col * ir);
      i64 product = fc - shading;
      i32 tmp = i64_to_i32_result(product) >> config.shift;
      i64 ir0 = this->ir[0];
      i64 foo = i32_to_i16_saturate(command_config_from_command(0), i, tmp);
      i32 res = i64_to_i32_result(shading + ir0 * foo);
      this->mac[i+1] = res >> config.shift;
    }

    mac_to_ir(config);
    mac_to_rgb_fifo();
  }

  int multiply_matrix_by_vector(CommandConfig config, Matrix mat,
                                u32 vector_index, ControlVector crv) {
    if (mat == Matrix::invalid) {
      LOG_ERROR("GTE multiplication with invalid matrix");
      return -1;
    }

    if (crv == ControlVector::far_color) {
      LOG_ERROR("GTE multiplication with far color vector");
      return -1;
    }

    for (int r = 0; r < 3; ++r) {
      i64 res = static_cast<i64>(control_vectors[crv][r]) << 12;

      for (int c = 0; c < 3; ++c) {
        i32 v = this->v[vector_index][c];
        i32 m = matrices[mat][r][c];

        i32 product = v * m;

        res = i64_to_i44(c, res + static_cast<i64>(product));
      }

      mac[r + 1] = res >> config.shift;
    }

    mac_to_ir(config);

    return 0;
  }

  void mac_to_ir(CommandConfig config) {
    i32 mac1 = mac[1];
    ir[1] = i32_to_i16_saturate(config, 0, mac1);

    i32 mac2 = mac[2];
    ir[2] = i32_to_i16_saturate(config, 1, mac2);

    i32 mac3 = mac[3];
    ir[3] = i32_to_i16_saturate(config, 2, mac3);
  }

  u8 mac_to_color(i32 mac, u8 which) {
    i32 c = mac >> 4;

    if (c < 0) {
      set_flag(21 - which);
      return 0;
    }
    else if (c > 0xff) {
      set_flag(21 - which);
      return 0xff;
    }
    else {
      return c;
    }
  }

  void mac_to_rgb_fifo() {
    i32 mac1 = mac[1];
    i32 mac2 = mac[2];
    i32 mac3 = mac[3];

    u8 r = mac_to_color(mac1, 0);
    u8 g = mac_to_color(mac2, 1);
    u8 b = mac_to_color(mac3, 2);

    rgb_fifo[0] = rgb_fifo[1];
    rgb_fifo[1] = rgb_fifo[2];
    rgb_fifo[2].r = r;
    rgb_fifo[2].g = g;
    rgb_fifo[2].b = b;
    rgb_fifo[2].x = rgb.x;
  }

  u32 do_rtp(CommandConfig config, u64 vector_index) {
    i32 z_shifted = 0;
    int rm = Matrix::rotation;
    int tr = ControlVector::translation;

    for(int r = 0; r < 3; ++r) {
      i64 res = static_cast<i64>(control_vectors[tr][r]) << 12;

      for (int c = 0; c < 3; ++c) {
	i32 v = this->v[vector_index][c];
	i32 m = this->matrices[rm][r][c];

	i32 rot = v * m;

        res = i64_to_i44(static_cast<u8>(c), res + static_cast<i64>(rot));
      }

      mac[r + 1] = (res >> config.shift);

      z_shifted = (res >> 12);
    }

    i32 val = mac[1];
    this->ir[1] = i32_to_i16_saturate(config, 0, val);

    val = mac[2];
    this->ir[2] = i32_to_i16_saturate(config, 1, val);

    const i16 min16 = -32768;
    const i16 max16 = 32767;

    i32 min = min16;
    i32 max = max16;

    if (z_shifted > max || z_shifted < min) {
      set_flag(22);
    }

    if (config.clamp_negative) {
      min = 0;
    }
    else {
      min = static_cast<i32>(min16);
    }

    val = mac[3];

    if(val < min) {
      ir[3] = static_cast<i16>(min);
    }
    else if (val > max) {
      ir[3] = static_cast<i16>(max);
    }
    else {
      ir[3] = static_cast<i16>(val);
    }

    u16 z_saturated;
    if(z_shifted < 0) {
      set_flag(18);
      z_saturated = 0;
    }
    else if (z_shifted > static_cast<i32>(static_cast<u16>(-1))) {
      set_flag(18);
      z_saturated = -1;
    }
    else {
      z_saturated = z_shifted;
    }

    z_fifo[0] = z_fifo[1];
    z_fifo[1] = z_fifo[2];
    z_fifo[2] = z_fifo[3];
    z_fifo[3] = z_saturated;

    u32 projection_factor;
    if (z_shifted > this->h / 2) {
      projection_factor = gte::divide(this->h, z_saturated);
    }
    else {
      set_flag(17);
      projection_factor = 0x1ffff;
    }

    i16 factor = projection_factor;
    i64 x = ir[1];
    i64 y = ir[2];
    i64 ofx = this->ofx;
    i64 ofy = this->ofy;

    i32 screen_x = i64_to_i32_result(x*factor + ofx) >> 16;
    i32 screen_y = i64_to_i32_result(y*factor + ofy) >> 16;

    xy_fifo[3].x = i32_to_i11_saturate(0, screen_x);
    xy_fifo[3].y = i32_to_i11_saturate(1, screen_y);

    xy_fifo[0] = xy_fifo[1];
    xy_fifo[1] = xy_fifo[2];
    xy_fifo[2] = xy_fifo[3];

    return projection_factor;
  }

  void depth_queuing(u32 projection_factor) {
    i64 factor = projection_factor;
    i64 dqa = this->dqa;
    i64 dqb = this->dqb;

    i64 depth = dqb + (dqa * factor);

    this->mac[0] = i64_to_i32_result(depth);

    depth = depth >> 12;

    if (depth < 0) {
      set_flag(12);
      ir[0] = 0;
    }
    else if (depth > 4096) {
      set_flag(12);
      ir[0] = 4096;
    }
    else {
      ir[0] = depth;
    }
  }

  void set_flag(u8 bit) { flags |= 1 << bit; }

  i64 i64_to_i44(u8 flag, i64 val) {
    if (val > 0x7ffffffffff) {
      set_flag(30 - flag);
    } else if (val < -0x80000000000) {
      set_flag(27 - flag);
    }

    return (val << (64 - 44)) >> (64 - 44);
  }

  i16 i32_to_i16_saturate(CommandConfig config, u8 flag, i32 val) {
    const i16 min16 = -32768;
    const i16 max16 = 32767;
    
    i32 min;
    if(config.clamp_negative) {
      min = 0;
    }
    else {
      min = min16;
    }

    i32 max = max16;
    if (val > max) {
      set_flag(24 - flag);
      return max;
    } else if (val < min) {
      set_flag(24 - flag);
      return min;
    } else {
      return val;
    }
  }

  i16 i32_to_i11_saturate(u8 flag, i32 val) {
    if (val < -0x400) {
      set_flag(14 - flag);
      return -0x400;
    }
    else if (val > 0x3ff) {
      set_flag(14 - flag);
      return 0x3ff;
    }
    else {
      return val;
    }
  }

  i32 i64_to_i32_result(i64 val) {
    if (val < -0x80000000) {
      set_flag(15);
    } else if (val > 0x7fffffff) {
      set_flag(16);
    }

    return val;
  }

  u16 i64_to_otz(i64 average) {
    i64 val = average >> 12;

    if (val < 0) {
      set_flag(18);
      return 0;
    } else if (val > 0xffff) {
      set_flag(18);
      return 0xffff;
    } else {
      return val;
    }
  }
};
