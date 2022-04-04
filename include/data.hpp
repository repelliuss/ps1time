#pragma once

#include "types.hpp"
#include <filesystem>

u32 load32(u8 *data, u32 offset);
u16 load16(u8 *data, u32 index);
u8 load8(u8 *data, u32 offset);
void store32(u8 *data, u32 value, u32 index);
void store16(u8 *data, u16 value, u32 index);
void store8(u8 *data, u8 value, u32 index);
int read_file(u8 *out, const std::filesystem::path path, const u32 size);
