#pragma once

#include "types.hpp"

#include <filesystem>

namespace memory {
u32 load32(u8 *data, u32 index);
u16 load16(u8 *data, u32 index);
u8 load8(u8 *data, u32 index);

void store32(u8 *data, u32 index, u32 val);
void store16(u8 *data, u32 index, u16 val);
void store8(u8 *data, u32 index, u8 val);
} // namespace memory

namespace file {
int read_whole_file(char **str, const std::filesystem::path path);
int read_file(u8 *out, const std::filesystem::path path, const u32 size);
} // namespace file
