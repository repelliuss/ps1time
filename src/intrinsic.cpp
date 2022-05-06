#include "intrinsic.hpp"

#include <GL/glext.h>
#include <string.h>
#include <assert.h>

template <class T> T &Buffer<T>::operator[](int index) {
  assert(index < MAX_VERTEX_BUFFER_LEN);
  return data[index];
}
template Color& Buffer<Color>::operator[](int index);
template Position& Buffer<Position>::operator[](int index);

template <class T> Buffer<T> make_gl_buffer_and_bind() {
  GLuint object;
  T *data;

  glGenBuffers(1, &object);
  glBindBuffer(GL_ARRAY_BUFFER, object);

  GLsizeiptr buf_size = sizeof(T) * MAX_VERTEX_BUFFER_LEN;
  GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT;

  glBufferStorage(GL_ARRAY_BUFFER, buf_size, nullptr, flags);
  data = reinterpret_cast<T *>(
      glMapBufferRange(GL_ARRAY_BUFFER, 0, buf_size, flags));

  memset(data, 0, buf_size);

  return {
      .object = object,
      .data = data,
  };
}
template Buffer<Color> make_gl_buffer_and_bind();
template Buffer<Position> make_gl_buffer_and_bind();

template <class T>
void clean(Buffer<T> buffer) {
  glBindBuffer(GL_ARRAY_BUFFER, buffer.object);
  glUnmapBuffer(GL_ARRAY_BUFFER);
  glDeleteBuffers(1, &buffer.object);
}
template void clean(Buffer<Color>);
template void clean(Buffer<Position>);
