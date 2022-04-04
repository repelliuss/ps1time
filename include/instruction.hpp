#pragma once

#include "types.hpp"

struct Instruction {
  u32 data;

  // See MIPS Green Sheet and nocash's table

  //[31:26]
  u32 opcode() const noexcept;

  //[25:21]
  u32 rs() const noexcept;

  //[20:16]
  u32 rt() const noexcept;

  //[15:11]
  u32 rd() const noexcept;

  //[10:6]
  u32 shamt() const noexcept;
  
  //[5:0]
  u32 funct() const noexcept;

  //[14:0]
  u32 imm15() const noexcept;

  //[15:0]
  u32 imm16() const noexcept;

  //[15:0] sign-extended!
  u32 imm16_se() const noexcept;

  //[25:0]
  u32 imm26() const noexcept;

  //[25:6]
  u32 comment() const noexcept;

  //[25]
  u32 bit25() const noexcept;

  //[20]
  u32 bit20() const noexcept;

  //[16]
  u32 bit16() const noexcept;
};
