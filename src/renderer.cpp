#include "renderer.hpp"
#include "data.hpp"
#include "intrinsic.hpp"
#include "log.hpp"

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>

#define SDL_MAIN_HANDLED
#include <SDL_main.h>
#include <SDL_video.h>

#include <assert.h>

static const char *gl_debug_severity(GLenum severity) {
  switch (severity) {
  case GL_DEBUG_SEVERITY_HIGH:
    return "High";
  case GL_DEBUG_SEVERITY_MEDIUM:
    return "Medium";
  case GL_DEBUG_SEVERITY_LOW:
    return "Low";
  case GL_DEBUG_SEVERITY_NOTIFICATION:
    return "Notification";
  }

  LOG_CRITICAL("Severity: 0x%x", severity);

  assert(false);
  return "";
}

static const char *gl_debug_source(GLenum source) {
  switch (source) {
  case GL_DEBUG_SOURCE_API:
    return "API";
  case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
    return "WindowSystem";
  case GL_DEBUG_SOURCE_SHADER_COMPILER:
    return "ShaderCompiler";
  case GL_DEBUG_SOURCE_THIRD_PARTY:
    return "ThirdParty";
  case GL_DEBUG_SOURCE_APPLICATION:
    return "Application";
  case GL_DEBUG_SOURCE_OTHER:
    return "Other";
  }

  LOG_CRITICAL("Source: 0x%x", source);

  assert(false);
  return "";
}

static const char *gl_debug_type(GLenum type) {
  switch (type) {
  case GL_DEBUG_TYPE_ERROR:
    return "Error";
  case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
    return "DeprecatedBehavior";
  case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
    return "UndefinedBehavior";
  case GL_DEBUG_TYPE_PORTABILITY:
    return "Portability";
  case GL_DEBUG_TYPE_PERFORMANCE:
    return "Performance";
  case GL_DEBUG_TYPE_MARKER:
    return "Marker";
  case GL_DEBUG_TYPE_PUSH_GROUP:
    return "PushGroup";
  case GL_DEBUG_TYPE_POP_GROUP:
    return "PopGroup";
  case GL_DEBUG_TYPE_OTHER:
    return "Other";
  }

  LOG_CRITICAL("Type: 0x%x", type);

  assert(false);
  return "";
}

static int check_gl_errors() {
  bool fatal = false;

  GLint max_msg_len = 0;
  glGetIntegerv(GL_MAX_DEBUG_MESSAGE_LENGTH, &max_msg_len);

  GLchar *msg_data =
      static_cast<GLchar *>(malloc(sizeof(GLchar) * max_msg_len));
  GLenum source;
  GLenum type;
  GLenum severity;
  GLuint id;
  GLsizei length;

  GLint count = glGetDebugMessageLog(1, max_msg_len, &source, &type, &id,
                                     &severity, &length, msg_data);
  while (count != 0) {

    // clang-format off
    LOG_ERROR("OpenGL [%s|%s|%s|0x%x] %s",
	      gl_debug_severity(severity),
	      gl_debug_source(source),
	      gl_debug_type(type),
	      id,
	      msg_data);
    // clang-format on

    if (severity == GL_DEBUG_SEVERITY_HIGH ||
        severity == GL_DEBUG_SEVERITY_MEDIUM) {
      fatal = true;
    }

    count = glGetDebugMessageLog(1, max_msg_len, &source, &type, &id, &severity,
                                 &length, msg_data);
  }

  free(msg_data);

  if (fatal) {
    LOG_ERROR("Fatal OpenGL error");
    return -1;
  }

  return 0;
}

// NOTE: needs OpenGL DEBUG context
int Renderer::has_errors() {
  return had_error;
}

Renderer::Renderer(bool debug) {
  SDL_SetMainReady();
  SDL_Init(SDL_INIT_VIDEO);

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

  if (debug)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
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
    LOG_ERROR("SDL: %s", SDL_GetError());
    return -1;
  }

  gl_context = SDL_GL_CreateContext(window);
  if (gl_context == NULL) {
    LOG_ERROR("SDL: %s", SDL_GetError());
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

  if (check_gl_errors()) {
    return -1;
  }

  GLint status = GL_FALSE;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
  if (status != GL_TRUE) {
    glDeleteShader(shader);
    LOG_ERROR("Shader compilation failed");
    return -1;
  }

  s = shader;

  return 0;
}

static int read_then_compile_shader(GLuint &s, GLenum type, const char *path) {
  char *code;
  int status = 0;

  status = file::read_whole_file(&code, path);
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

  if(has_errors()) {
    return -1;
  }

  status = GL_FALSE;
  glGetProgramiv(program, GL_LINK_STATUS, &status);
  if(status != GL_TRUE) {
    LOG_ERROR("OpenGL program linking failed");
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
    LOG_ERROR("Attribute '%s' not found in program", attr);
  }

  return attr_index;
}

static GLuint find_program_uniform(GLuint program, const char *uni) {
  GLuint index = glGetUniformLocation(program, uni);

  if(index < 0) {
    LOG_ERROR("Uniform '%s' not found in program", uni);
  }

  return index;
}

static void set_error(bool &val, int status) {
  if(!val) {
    val = status < 0;
  }
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

  set_error(had_error, check_gl_errors());

  {
    buf_color = make_gl_buffer_and_bind<Color>();
    GLuint index = find_program_attrib(program, "vertex_color");
    glEnableVertexAttribArray(index);
    glVertexAttribIPointer(index, 3, GL_UNSIGNED_BYTE, 0, nullptr);
  }

  set_error(had_error, check_gl_errors());

  {
    offset_program_index = find_program_uniform(program, "offset");
    glUniform2i(offset_program_index, 0, 0);
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

int Renderer::draw() {
  glMemoryBarrier(GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);

  glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(count_vertices));

  GLsync sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

  GLenum status;
  do {
    status = glClientWaitSync(sync, GL_SYNC_FLUSH_COMMANDS_BIT, 10000000);
  } while (status != GL_ALREADY_SIGNALED && status != GL_CONDITION_SATISFIED);

  count_vertices = 0;

  return check_gl_errors();
}

int Renderer::display() {
  int status = draw();
  if (status < 0)
    return status;
  
  SDL_GL_SwapWindow(window);

  return 0;
}

int Renderer::put_triangle(const Position positions[3], const Color colors[3]) {
  if (count_vertices + 3 >= MAX_VERTEX_BUFFER_LEN) {
    LOG_WARN("Vertex attribute buffers full, forcing_draw");
    int status = draw();
    if (status < 0) {
      return status;
    }
  }

  for (int i = 0; i < 3; ++i) {
    buf_pos[count_vertices] = positions[i];
    buf_color[count_vertices] = colors[i];
    ++count_vertices;
  }

  return 0;
}

int Renderer::put_quad(const Position positions[4], const Color colors[4]) {
  if (count_vertices + 6 >= MAX_VERTEX_BUFFER_LEN) {
    LOG_WARN("Vertex attribute buffers full, forcing_draw");
    int status = draw();
    if (status < 0) {
      return status;
    }
  }

  for (int i = 0; i < 3; ++i) {
    buf_pos[count_vertices] = positions[i];
    buf_color[count_vertices] = colors[i];
    ++count_vertices;
  }

  for (int i = 1; i < 4; ++i) {
    buf_pos[count_vertices] = positions[i];
    buf_color[count_vertices] = colors[i];
    ++count_vertices;
  }

  return 0;
}

int Renderer::set_drawing_offset(GLint x, GLint y) {
  // REVIEW: can optimize here by handling vertex buffer properly. Problem here
  // we change the offset but already issued vertices need to drawed with
  // previous offset
  draw();			// render current drawings before changing offset
  glUniform2i(offset_program_index, x, y);
  return 0;
}
