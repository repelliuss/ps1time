#include "instruction.hpp"

u32 Instruction::opcode() const noexcept { return data >> 26; }

u32 Instruction::rs() const noexcept { return (data >> 21) & 0x1f; }

u32 Instruction::rt() const noexcept { return (data >> 16) & 0x1f; }

u32 Instruction::rd() const noexcept { return (data >> 11) & 0x1f; }

u32 Instruction::shamt() const noexcept { return (data >> 6) & 0x1f; }

u32 Instruction::funct() const noexcept { return data & 0x3f; }

u32 Instruction::imm15() const noexcept { return data & 0x7fff; }

u32 Instruction::imm16() const noexcept { return data & 0xffff; }

u32 Instruction::imm16_se() const noexcept { return static_cast<i16>(imm16()); }

u32 Instruction::imm26() const noexcept { return data & 0x3ffffff; }

u32 Instruction::comment() const noexcept { return (data >> 6) & 0xfffff; }

u32 Instruction::bit25() const noexcept { return (data >> 25) & 1; }
