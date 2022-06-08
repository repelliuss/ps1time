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

  int set_control(u32 reg, u32 val) {

    LOG_DEBUG("Set GTE Control %d: %x", reg, val);
    
    switch(reg) {
    case 13:
      rbk = static_cast<i32>(val);
      break;

    case 14:
      gbk = static_cast<i32>(val);
      break;
    case 15:
      bbk = static_cast<i32>(val);
      break;
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
      LOG_ERROR("Unhandled GTE control register");
      return -1;
    }

    return 0;
  }
};

