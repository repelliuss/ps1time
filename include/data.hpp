#pragma once

#include "types.hpp"
#include <filesystem>

u32 load32(u8 *data, u32 offset);
int read_file(u8 *out, const std::filesystem::path path, const u32 size);
void store32(u8 *data, u32 value, u32 index);
