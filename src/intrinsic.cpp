#include "intrinsic.hpp"

#include <string.h>
#include <assert.h>

// REVIEW: this constant
constexpr u32 MAX_VERTEX_BUFFER_LEN = 64 * 1024;

template <class T>
constexpr T& Buffer<T>::operator[](int index) {
  assert(index < MAX_VERTEX_BUFFER_LEN);
  return data[index];
}

template <class T>
Buffer<T> make_buffer() {
  GLuint object;
  T *data;

  glGenBuffers(1, &object);
  glBindBuffer(GL_ARRAY_BUFFER, object);

  GLsizeiptr buf_size = sizeof(T) * MAX_VERTEX_BUFFER_LEN;
  GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT;

  glBufferStorage(GL_ARRAY_BUFFER, buf_size, nullptr, flags);
  data = glMapBufferRange(GL_ARRAY_BUFFER, 0, buf_size, flags);

  memset(data, 0, buf_size);
}

template <class T>
int clean(Buffer<T> buffer) {
  glBindBuffer(GL_ARRAY_BUFFER, buffer.object);
  glUnmapBuffer(GL_ARRAY_BUFFER);
  glDeleteBuffers(1, &buffer.object);
}
