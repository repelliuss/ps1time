#pragma once

#include "types.hpp"
#include "instruction.hpp"

// NOTE: PS1 has 4KB instruction cache and with 32 bit address width, 1024
// instructions. Cache line is 16 bytes which means each cache line has 4
// instructions. For each address, bits [1:0] are 0 because they need to be
// aligned for 32 bit access. [3:2] gives index for each cache line. Because
// cache line is 16 bytes, there are 256 cache lines so [11:4] gives cache line
// index. Then finally, [31:12] gives the tag to compare for hit or miss.
// Instruction index points to first valid instruction in cache line.
struct ICacheLine {
  // NOTE: All instructions are initially valid (Index 0). This header needs to
  // keep track of tag ([31:12]) and first instruction index which may be
  // invalid (index>3).
  // [31:12] is for tags, [4:2] is for instruction index
  u32 header = 0;

  // NOTE: Cache starts with a random state, 0x00bad0d is break opcode
  Instruction instruction[4] = {0x00bad0d, 0x00bad0d, 0x00bad0d, 0x00bad0d};

  constexpr u32 tag() { return header & 0xfffff000; }

  constexpr int first_valid_index() { return (header >> 2) & 0x7; }

  constexpr void update(u32 pc) { header = pc & 0xfffff00c; }

  // NOTE: Making instruction index > 3
  constexpr void invalidate() { header |= 0x10; }
};
