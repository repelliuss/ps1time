#pragma once

#include "types.hpp"
#include "log.hpp"

struct GTE {
  /// Background color red component: signed 20.12
  i32 rbk = 0;
  /// Background color green component: signed 20.12
  i32 gbk = 0;
  /// Background color blue component: signed 20.12
  i32 bbk = 0;
  /// Far color red component: signed 28.4
  i32 rfc = 0;
  /// Far color green component: signed 28.4
  i32 gfc = 0;
  /// Far color blue component: signed 28.4
  i32 bfc = 0;
  /// Screen offset X: signed 16.16
  i32 ofx = 0;
  /// Screen offset Y: signed 16.16
  i32 ofy = 0;
  /// Projection plane distance (XXX reads back as a signed value
  /// even though unsigned)
  u16 h = 0;
  /// Depth queing coeffient: signed 8.8
  i16 dqa = 0;
  /// Depth queing offset: signed 8.24
  i32 dqb = 0;
  /// Scale factor when computing the average of 3 Z values
  /// (triangle): signed 4.12.
  i16 zsf3 = 0;
  /// Scale factor when computing the average of 4 Z values
  /// (quad): signed 4.12
  i16 zsf4 = 0;

  i32 tr[3];

  i16 lsm[3][3];

  i16 lcm[3][3];

  i16 rm[3][3];

  i16 v0[3];

  i16 v1[3];

  i16 v2[3];

  int set_control(u32 reg, u32 val) {

    LOG_DEBUG("Set GTE Control %d: %x", reg, val);
    
    switch(reg) {
    case 0: {
      i16 v0 = val;
      i16 v1 = (val >> 16);

      rm[0][0] = v0;
      rm[0][1] = v1;
    } break;
      
    case 1: {
      i16 v0 = val;
      i16 v1 = (val >> 16);

      rm[0][2] = v0;
      rm[1][0] = v1;
    } break;

    case 2: {
      i16 v0 = val;
      i16 v1 = (val >> 16);

      rm[1][1] = v0;
      rm[1][2] = v1;
    } break;

    case 3: {
      i16 v0 = val;
      i16 v1 = (val >> 16);

      rm[2][0] = v0;
      rm[2][1] = v1;
    } break;

    case 4: {
      rm[2][2] = val;
    } break;

    case 5: {
      tr[0] = val;
    } break;

    case 6: {
      tr[1] = val;
    } break;

    case 7: {
      tr[2] = val;
    } break;

    case 8: {
      i16 v0 = val;
      i16 v1 = (val >> 16);

      lsm[0][0] = v0;
      lsm[0][1] = v1;
    } break;

    case 9: {
      i16 v0 = val;
      i16 v1 = (val >> 16);

      lsm[0][2] = v0;
      lsm[1][0] = v1;
    } break;

    case 10: {
      i16 v0 = val;
      i16 v1 = (val >> 16);

      lsm[1][1] = v0;
      lsm[1][2] = v1;
    } break;

    case 11: {
      i16 v0 = val;
      i16 v1 = (val >> 16);

      lsm[2][0] = v0;
      lsm[2][1] = v1;
    } break;

    case 12: {
      lsm[2][2] = val;
    } break;

    case 13:
      rbk = static_cast<i32>(val);
      break;

    case 14:
      gbk = static_cast<i32>(val);
      break;
    case 15:
      bbk = static_cast<i32>(val);
      break;

    case 16: {
      i16 v0 = val;
      i16 v1 = (val >> 16);

      lcm[0][0] = v0;
      lcm[0][1] = v1;
    } break;

    case 17: {
      i16 v0 = val;
      i16 v1 = (val >> 16);

      lcm[0][2] = v0;
      lcm[1][0] = v1;
    } break;

    case 18: {
      i16 v0 = val;
      i16 v1 = (val >> 16);

      lcm[1][1] = v0;
      lcm[1][2] = v1;
    } break;

    case 19: {
      i16 v0 = val;
      i16 v1 = (val >> 16);

      lcm[2][0] = v0;
      lcm[2][1] = v1;
    } break;

    case 20: {
      lcm[2][2] = val;
    } break;

    case 21:
      rfc = static_cast<i32>(val);
      break;
    case 22:
      gfc = static_cast<i32>(val);
      break;
    case 23:
      bfc = static_cast<i32>(val);
      break;
    case 24:
      ofx = static_cast<i32>(val);
      break;
    case 25:
      ofy = static_cast<i32>(val);
      break;
    case 26:
      h = static_cast<u16>(val);
      break;
    case 27:
      dqa = static_cast<i16>(val);
      break;
    case 28:
      dqb = static_cast<i32>(val);
      break;
    case 29:
      zsf3 = static_cast<i16>(val);
      break;
    case 30:
      zsf4 = static_cast<i16>(val);
      break;

    default:
      LOG_ERROR("Unhandled GTE control register %d %x", reg, val);
      return -1;
    }

    return 0;
  }

  int set_data(u32 reg, u32 val) {

    LOG_DEBUG("Set GTE data %d: %x", reg, val);

    switch (reg) {
    case 0: {
      i16 v0 = val;
      i16 v1 = (val >> 16);

      this->v0[0] = v0;
      this->v0[1] = v1;
    } break;
      
    case 1: {
      this->v0[2] = val;
    } break;

    case 2: {
      i16 v0 = val;
      i16 v1 = (val >> 16);

      this->v1[0] = v0;
      this->v1[1] = v1;
    } break;

    case 3: {
      this->v1[2] = val;
    } break;

    case 4: {
      i16 v0 = val;
      i16 v1 = (val >> 16);

      this->v2[0] = v0;
      this->v2[1] = v1;
    } break;
      
    case 5: {
      this->v2[2] = val;
    } break;

    default: {
      LOG_ERROR("Unhandled GTE data register %d %x", reg, val);
      return -1;
    } break;
      
    }

    return 0;
  }

  int command(u32 command) {
    LOG_ERROR("Unhandled GTE command %x", command);
    return -1;
  }

  
};

