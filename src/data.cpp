#include "data.hpp"

#include <filesystem>
#include <cassert>

namespace memory {
// little endian
u32 load32(u8 *data, u32 index) {
  // REVIEW: may require casts to u32
  u32 b0 = data[index];
  u32 b1 = data[index + 1];
  u32 b2 = data[index + 2];
  u32 b3 = data[index + 3];

  return (b3 << 24) | (b2 << 16) | (b1 << 8) | b0;
}

u16 load16(u8 *data, u32 index) {
  u16 b0 = data[index];
  u16 b1 = data[index + 1];

  return (b1 << 8) | b0;
}

u8 load8(u8 *data, u32 index) { return data[index]; }

void store32(u8 *data, u32 value, u32 index) {
  // REVIEW: is & required here?
  // REVIEW: may require casts to u8
  data[index] = value & 0xff;
  data[index + 1] = (value >> 8) & 0xff;
  data[index + 2] = (value >> 16) & 0xff;
  data[index + 3] = value >> 24;
}

void store16(u8 *data, u16 value, u32 index) {
  data[index] = value & 0xff;
  data[index + 1] = value >> 8;
}

void store8(u8 *data, u8 value, u32 index) { data[index] = value; }

} // namespace memory

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

int read_file(char *out, const std::filesystem::path path, const u32 size) {
  int status = 0;

  if (out == nullptr) {
    printf("Unable to read file, out is null");
    return -1;
  }

  FILE *fp = std::fopen(path.c_str(), "r");
  if (!fp) {
    printf("Unable to open BIOS image file '%s'.\n", path.c_str());
    status = -1;
  } else if (std::fread(out, 1, size, fp) != size) {
    printf("Failed to read BIOS image.\n");
    status = -1;
  }

  std::fclose(fp);

  return status;
}

int file_size(u32 &size, std::filesystem::path path) {
  std::error_code ec;
  size = std::filesystem::file_size(path, ec);
  if (ec) {
    fprintf(stderr, "Failed to read file size of file %s : %s\n", path.c_str(),
            ec.message().c_str());
    return -1;
  }

  return 0;
}

int read_whole_file(char **str, const std::filesystem::path path) {
  int status;
  u32 size;

  status = file_size(size, path);
  if (status < 0)
    return status;

  *str = static_cast<char *>(malloc((size + 1) * sizeof(char)));

  status = read_file(*str, path, size);
  if (status < 0) {
    free(str);
    return status;
  }

  (*str)[size] = 0;

  return 0;
}
