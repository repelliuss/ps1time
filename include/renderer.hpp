#pragma once

#include "intrinsic.hpp"

// TODO: handle this defines
#include <SDL.h>

// TODO: port to windows
// struct GL {
//   // TODO: should USE APIENTRY for pointers for Windows
//   typedef void (*ActiveTextureARB)(unsigned int);

//   static ActiveTextureARB active_texture_arb;
// };

struct Renderer {
  Renderer(bool debug = false);
  ~Renderer();

  Renderer(const Renderer &) = delete;
  Renderer(const Renderer &&) = delete;

  Renderer &operator=(const Renderer &) = delete;
  Renderer &operator=(const Renderer &&) = delete;

  bool had_error = false;
  int has_errors();

  int create_window_and_context();
  
  int compile_shaders_link_program();
  void clean_program_and_shaders();

  void init_buffers();
  void clean_buffers();

  int put_triangle(const Position positions[3], const Color colors[3]);
  int put_quad(const Position positions[4], const Color colors[4]);
  int draw();
  int display();
  int set_drawing_offset(GLint x, GLint y);

  SDL_Window *window = nullptr;
  SDL_GLContext gl_context = nullptr;

  GLuint vertex_shader = 0;
  GLint offset_program_index = 0;
  GLuint fragment_shader = 0;
  GLuint program = 0;
  GLuint vertex_array_object = 0;
  u32 count_vertices = 0;
  Buffer<Position> buf_pos;
  Buffer<Color> buf_color;
};

