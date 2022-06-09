#pragma once

#include "types.hpp"
#include "log.hpp"

struct MSF {
  u8 m;
  u8 s;
  u8 f;

  constexpr u32 sector_index() {
    auto from_bcd = [](u8 b) -> u8 { return (b >> 4) * 10U + (b & 0xf); };

    u32 m = from_bcd(this->m);
    u32 s = from_bcd(this->s);
    u32 f = from_bcd(this->f);

    // 60 seconds in a minute, 75 sectors(frames) in a second
    return (60 * 75 * m) + (75 * s) + f;
  }

  constexpr int next(MSF &next) {
    auto bcd_inc = [](u8 b) -> u8 {
      if ((b & 0xf) < 9) {
        return b + 1;
      } else {
        return (b & 0xf0) + 0x10;
      }
    };

    if (f < 0x74) {
      next = {m, s, bcd_inc(f)};
      return 0;
    }

    if (s < 0x59) {
      next = {m, bcd_inc(s), 0};
      return 0;
    }

    if (m < 0x99) {
      next = {bcd_inc(m), 0, 0};
      return 0;
    }

    LOG_ERROR("MSF overflow");
    return -1;
  }

  constexpr u32 as_u32_bcd() const {
    return (static_cast<u32>(m) << 16) | (static_cast<u32>(s) << 8) |
           static_cast<u32>(f);
  }

  constexpr bool operator<(const MSF &other) {
    u32 a = as_u32_bcd();
    u32 b = other.as_u32_bcd();

    return a < b;
  }

  constexpr bool operator>(const MSF &other) {
    u32 a = as_u32_bcd();
    u32 b = other.as_u32_bcd();

    return a > b;
  }

  constexpr bool operator==(const MSF &other) {
    u32 a = as_u32_bcd();
    u32 b = other.as_u32_bcd();

    return a == b;
  }

  constexpr bool operator<=(const MSF &other) {
    u32 a = as_u32_bcd();
    u32 b = other.as_u32_bcd();

    return a <= b;
  }

  constexpr bool operator!=(const MSF &other) {
    u32 a = as_u32_bcd();
    u32 b = other.as_u32_bcd();

    return a != b;
  }

  constexpr MSF max(const MSF m2) {
    u32 a = as_u32_bcd();
    u32 b = m2.as_u32_bcd();

    return a < b ? m2 : *this;
  }

  constexpr MSF min(const MSF m2) {
    u32 a = as_u32_bcd();
    u32 b = m2.as_u32_bcd();

    return a < b ? *this : m2;
  }

  constexpr MSF clamp(const MSF m1, const MSF m2) {
    u32 self = as_u32_bcd();

    u32 a = m1.as_u32_bcd();

    if (self < a) {
      return m1;
    }

    u32 b = m2.as_u32_bcd();

    if (self > b) {
      return m2;
    }

    return *this;
  }

  void print() { printf("%02x:%02x:%02x", m, s, f); }
};

constexpr MSF zero() { return {0, 0, 0}; }

constexpr int from_bcd(MSF &msf, u8 m, u8 s, u8 f) {
  msf = {m, s, f};

  if (m > 0x99 || (m & 0xf) > 0x9) {
    LOG_ERROR("Invalid MSF: ");
    msf.print();
    return -1;
  }

  if (s > 0x99 || (s & 0xf) > 0x9) {
    LOG_ERROR("Invalid MSF: ");
    msf.print();
    return -1;
  }

  if (f > 0x99 || (f & 0xf) > 0x9) {
    LOG_ERROR("Invalid MSF: ");
    msf.print();
    return -1;
  }

  if (s >= 0x60 || f >= 0x75) {
    LOG_ERROR("Invalid MSF, no sense: ");
    msf.print();
    return -1;
  }

  return 0;
}
