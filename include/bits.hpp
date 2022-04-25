#pragma once

#include "types.hpp"

u32 bit(u32 data, unsigned at);
void bit_copy_to(u32 &data, unsigned at, unsigned bit);

u32 bits_in_range(u32 data, unsigned beg, unsigned end);
void bits_copy_to_range(u32 &data, unsigned beg, unsigned end, u32 val);
