#pragma once

#include "types.hpp"

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>

struct Position {
  GLshort x, y;
};
constexpr Position pos_from_gp0(u32 val) {
  i16 a = val;
  i16 b = val >> 16;
  return {.x = a, .y = b};
}

struct Color {
  GLubyte r, g, b;
};
constexpr Color color_from_gp0(u32 val) {
  u8 a = val;
  u8 b = val >> 8;
  u8 c = val >> 16;
  return {.r = a, .g = b, .b = c};
}

template <class T> struct Buffer {
  GLuint object;
  T *data;
  constexpr T& operator[](int index);
};
template <class T> Buffer<T> make_buffer();
template <class T> int clean(Buffer<T> buffer);
