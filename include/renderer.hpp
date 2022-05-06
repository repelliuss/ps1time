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
  Renderer();
  ~Renderer();

  Renderer(const Renderer &) = delete;
  Renderer(const Renderer &&) = delete;

  Renderer &operator=(const Renderer &) = delete;
  Renderer &operator=(const Renderer &&) = delete;

  int create_window_and_context();
  
  int compile_shaders_link_program();
  void clean_program_and_shaders();

  void init_buffers();
  void clean_buffers();

  int put_triangle(const Position positions[3], const Color colors[3]);
  void draw();
  void display();

  SDL_Window *window = nullptr;
  SDL_GLContext gl_context = nullptr;

  GLuint vertex_shader = 0;
  GLuint fragment_shader = 0;
  GLuint program = 0;
  GLuint vertex_array_object = 0;
  u32 count_vertices = 0;
  Buffer<Position> buf_pos;
  Buffer<Color> buf_color;
};

