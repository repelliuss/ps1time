#include "instruction.hpp"

u32 Instruction::opcode() { return data >> 26; }

u32 Instruction::rs() { return (data >> 21) & 0x1f; }

u32 Instruction::rt() { return (data >> 16) & 0x1f; }

u32 Instruction::rd() { return (data >> 10) & 0x1f; }

u32 Instruction::shamt() { return (data >> 6) & 0x1f; }

u32 Instruction::funct() { return data & 0x1f; }

u32 Instruction::imm15() { return data & 0x7fff; }

u32 Instruction::imm16() { return data & 0xffff; }

u32 Instruction::imm16_se() { return static_cast<i16>(imm16()); }

u32 Instruction::imm26() { return data & 0x3ffffff; }

u32 Instruction::comment() { return (data >> 6) & 0xfffff; }

u32 Instruction::bit25() { return (data >> 25) & 1; }
