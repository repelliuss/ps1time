#include "data.hpp"

#include <filesystem>
#include <cassert>

// little endian
u32 load32(u8 *data, u32 index) {
  assert(index % 4 == 0);

  // REVIEW: may require casts to u32
  u32 b0 = data[index];
  u32 b1 = data[index + 1];
  u32 b2 = data[index + 2];
  u32 b3 = data[index + 3];

  return (b3 << 24) | (b2 << 16) | (b1 << 8) | b0;
}

u8 load8(u8 *data, u32 index) { return data[index]; }

void store32(u8 *data, u32 value, u32 index) {
  assert(index % 4 == 0);

  // REVIEW: is & required here?
  // REVIEW: may require casts to u8
  data[index] = value & 0xff;
  data[index + 1] = (value >> 8) & 0xff;
  data[index + 2] = (value >> 16) & 0xff;
  data[index + 3] = value >> 24;
}

void store16(u8 *data, u16 value, u32 index) {
  assert(index % 2 == 0);

  data[index] = value & 0xff;
  data[index + 1] = value >> 8;
}

void store8(u8 *data, u8 value, u32 index) { data[index] = value; }

int read_file(u8 *out, const std::filesystem::path path, const u32 size) {
  int status = 0;
  FILE* fp = std::fopen(path.c_str(), "r");
  
  if (!fp) {
    printf("Unable to open BIOS image file '%s'.\n", path.c_str());
    status = -1;
  }
  else if(std::fread(out, 1, size, fp) != size) {
    printf("Failed to read BIOS image.\n");
    status = -1;
  }

  std::fclose(fp);
  
  return status;
}
