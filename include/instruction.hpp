#pragma once

#include "types.hpp"

struct Instruction {
  u32 data;

  // See MIPS Green Sheet and nocash's table

  //[31:26]
  u32 opcode();

  //[25:21]
  u32 rs();

  //[20:16]
  u32 rt();

  //[15:10]
  u32 rd();

  //[10:6]
  u32 shamt();
  
  //[5:0]
  u32 funct();

  //[14:0]
  u32 imm15();

  //[15:0]
  u32 imm16();

  //[15:0] sign-extended!
  u32 imm16_se();

  //[25:0]
  u32 imm26();

  //[25:6]
  u32 comment();

  //[25]
  u32 bit25();
};
