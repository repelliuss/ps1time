#include "renderer.hpp"
#include "data.hpp"
#include "intrinsic.hpp"

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>

#define SDL_MAIN_HANDLED
#include <SDL_main.h>
#include <SDL_video.h>

#include <assert.h>

Renderer::Renderer() {
  SDL_SetMainReady();
  SDL_Init(SDL_INIT_VIDEO);

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
}

int Renderer::create_window_and_context() {
  // clang-format off
  window = SDL_CreateWindow("ps1emu",
			    SDL_WINDOWPOS_CENTERED,
			    SDL_WINDOWPOS_CENTERED,
			    1024, 512,
			    SDL_WINDOW_OPENGL);
  // clang-format on

  if (window == NULL) {
    printf("SDL err: %s\n", SDL_GetError());
    return -1;
  }

  gl_context = SDL_GL_CreateContext(window);
  if (gl_context == NULL) {
    printf("SDL err: %s\n", SDL_GetError());
    return -1;
  }

  SDL_GL_SetSwapInterval(1);

  glClearColor(0, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT);

  SDL_GL_SwapWindow(window);

  return 0;
}

static int compile_shader(GLuint &s, const GLchar *code, GLenum type) {
  GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &code, nullptr);
  glCompileShader(shader);

  GLint status = GL_FALSE;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
  if (status != GL_TRUE) {
    glDeleteShader(shader);
    printf("Shader compilation failed!\n");
    return -1;
  }

  s = shader;

  return 0;
}

static int read_then_compile_shader(GLuint &s, GLenum type, const char *path) {
  char *code;
  int status = 0;

  status = read_whole_file(&code, path);
  if (status < 0)
    return status;

  status = compile_shader(s, code, type);

  free(code);

  return status;
}

static void clean_shaders(Renderer &renderer) {
  glDeleteShader(renderer.vertex_shader);
  glDeleteShader(renderer.fragment_shader);
}

void Renderer::clean_program_and_shaders() {
  glDeleteProgram(program);
  clean_shaders(*this);
}

int Renderer::compile_shaders_link_program() {
  GLint status = 0;

  // clang-format off
  status |= read_then_compile_shader(vertex_shader,
				     GL_VERTEX_SHADER,
				     "res/shader/vertex.glsl");
  status |= read_then_compile_shader(fragment_shader,
				     GL_FRAGMENT_SHADER,
				     "res/shader/fragment.glsl");
  // clang-format on

  if (status < 0) {
    clean_shaders(*this);
    return status;
  }

  program = glCreateProgram();

  glAttachShader(program, vertex_shader);
  glAttachShader(program, fragment_shader);

  glLinkProgram(program);

  status = GL_FALSE;
  glGetProgramiv(program, GL_LINK_STATUS, &status);
  if(status != GL_TRUE) {
    printf("OpenGL program linking failed!\n");
    glDeleteProgram(program);
    clean_shaders(*this);
    return -1;
  }

  glDetachShader(program, vertex_shader);
  glDetachShader(program, fragment_shader);

  glUseProgram(program);

  return 0;
}

static GLuint find_program_attrib(GLuint program, const char *attr) {
  GLuint attr_index = glGetAttribLocation(program, attr);

  if(attr_index < 0) {
    printf("Attribute '%s' not found in program", attr);
  }

  return attr_index;
}

void Renderer::init_buffers() {
  glGenVertexArrays(1, &vertex_array_object);
  glBindVertexArray(vertex_array_object);

  {
    buf_pos = make_gl_buffer_and_bind<Position>();
    GLuint index = find_program_attrib(program, "vertex_position");
    glEnableVertexAttribArray(index);
    glVertexAttribIPointer(index, 2, GL_SHORT, 0, nullptr);
  }

  {
    buf_color = make_gl_buffer_and_bind<Color>();
    GLuint index = find_program_attrib(program, "vertex_color");
    glEnableVertexAttribArray(index);
    glVertexAttribIPointer(index, 3, GL_UNSIGNED_BYTE, 0, nullptr);
  }
}

void Renderer::clean_buffers() {
  clean(buf_color);
  clean(buf_pos);
  glDeleteVertexArrays(1, &vertex_array_object);
}

Renderer::~Renderer() {
  if (gl_context)
    SDL_GL_DeleteContext(gl_context);

  if (window)
    SDL_DestroyWindow(window);

  SDL_Quit();
}

void Renderer::draw() {
  glMemoryBarrier(GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);

  glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(count_vertices));

  GLsync sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

  GLenum status;
  do {
    status = glClientWaitSync(sync, GL_SYNC_FLUSH_COMMANDS_BIT, 10000000);
  } while (status != GL_ALREADY_SIGNALED && status != GL_CONDITION_SATISFIED);

  count_vertices = 0;
}

void Renderer::display() {
  draw();
  SDL_GL_SwapWindow(window);
}

int Renderer::put_triangle(const Position positions[3], const Color colors[3]) {
  if (count_vertices + 3 >= MAX_VERTEX_BUFFER_LEN) {
    printf("Vertex attribute buffers full, forcing_draw\n");
    draw();
  }

  for (int i = 0; i < 3; ++i) {
    buf_pos[count_vertices] = positions[i];
    buf_color[count_vertices] = colors[i];
    ++count_vertices;
  }

  return 0;
}
