#include "data.hpp"
#include "log.hpp"

#include <filesystem>
#include <cassert>

// TODO: can just use memcpy here, values are little endian anyway iff passed an
// internal PS1 value, just be careful that these functions are only called on
// PS1 memory structures

namespace memory {
// little endian
u32 load32(u8 *data, u32 index) {
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

void store32(u8 *data, u32 index, u32 val) {
  data[index] = val & 0xff;
  data[index + 1] = (val >> 8) & 0xff;
  data[index + 2] = (val >> 16) & 0xff;
  data[index + 3] = val >> 24;
}

void store16(u8 *data, u32 index, u16 val) {
  data[index] = val & 0xff;
  data[index + 1] = val >> 8;
}

void store8(u8 *data, u32 index, u8 val) { data[index] = val; }

} // namespace memory

namespace file {
int read_file(u8 *out, const std::filesystem::path path, const u32 size) {
  int status = 0;
  FILE *fp = std::fopen(path.c_str(), "r");

  if (!fp) {
    LOG_ERROR("Unable to open BIOS image file '%s'", path.c_str());
    status = -1;
  } else if (std::fread(out, 1, size, fp) != size) {
    LOG_ERROR("Failed to read BIOS image");
    status = -1;
  }

  std::fclose(fp);

  return status;
}

int read_file(char *out, const std::filesystem::path path, const u32 size) {
  int status = 0;

  if (out == nullptr) {
    LOG_ERROR("Unable to read file, out is null");
    return -1;
  }

  FILE *fp = std::fopen(path.c_str(), "r");
  if (!fp) {
    LOG_ERROR("Unable to open BIOS image file '%s'", path.c_str());
    status = -1;
  } else if (std::fread(out, 1, size, fp) != size) {
    LOG_ERROR("Failed to read BIOS image");
    status = -1;
  }

  std::fclose(fp);

  return status;
}

int file_size(u32 &size, std::filesystem::path path) {
  std::error_code ec;
  size = std::filesystem::file_size(path, ec);
  if (ec) {
    LOG_ERROR("Failed to read file size of file '%s': %s", path.c_str(),
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
} // namespace file
