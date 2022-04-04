#include "asm.hpp"

#include <cstdio>

static int sll(const Instruction& i) {
  printf("sll R%02d, R%02d << %d", i.rd(), i.rt(), i.shamt());
  return 0;
}

static int jr(const Instruction& i) {
  printf("jr R%02d", i.rs());
  return 0;
}

static int add(const Instruction& i) {
  printf("add R%02d, R%02d, R%02d", i.rd(), i.rs(), i.rt());
  return 0;
}

static int addu(const Instruction& i) {
  printf("addu R%02d, R%02d, R%02d", i.rd(), i.rs(), i.rt());
  return 0;
}

static int ins_and(const Instruction& i) {
  printf("and R%02d, R%02d, R%02d", i.rd(), i.rs(), i.rt());
  return 0;
}

static int ins_or(const Instruction& i) {
  printf("or R%02d, R%02d, R%02d", i.rd(), i.rs(), i.rt());
  return 0;
}

static int sltu(const Instruction& i) {
  printf("sltu R%02d, R%02d, R%02d", i.rd(), i.rs(), i.rt());
  return 0;
}

static int j(const Instruction& i) {
  printf("j (PC & 0xf0000000) | %x", i.imm26() << 2);
  return 0;
}

static int jal(const Instruction& i) {
  printf("jal (PC & 0xf0000000) | %x", i.imm26() << 2);
  return 0;
}

static int beq(const Instruction &i) {
  printf("beq R%02d, R%02d, %02d", i.rs(), i.rt(),
         static_cast<i32>(i.imm16_se() << 2));
  return 0;
}

static int bne(const Instruction &i) {
  printf("bne R%02d, R%02d, %02d", i.rs(), i.rt(),
         static_cast<i32>(i.imm16_se() << 2));
  return 0;
}

static int addi(const Instruction& i) {
  printf("addi R%02d, R%02d, 0x%x", i.rt(), i.rs(), i.imm16_se());
  return 0;
}

static int addiu(const Instruction& i) {
  printf("addiu R%02d, R%02d, 0x%x", i.rt(), i.rs(), i.imm16_se());
  return 0;
}

static int andi(const Instruction& i) {
  printf("andi R%02d, R%02d, 0x%x", i.rt(), i.rs(), i.imm16());
  return 0;
}

static int ori(const Instruction& i) {
  printf("ori R%02d, R%02d, 0x%x", i.rt(), i.rs(), i.imm16());
  return 0;
}

static int lui(const Instruction& i) {
  printf("lui R%02d, 0x%x", i.rt(), i.imm16());
  return 0;
}

static int mfc0(const Instruction& i) {
  printf("mfc0 R%02d, cop0r%d", i.rt(), i.rd());
  return 0;
}

static int mtc0(const Instruction& i) {
  printf("mtc0 R%02d, cop0r%d", i.rt(), i.rd());
  return 0;
}

static int lb(const Instruction& i) {
  printf("lb R%02d, [R%02d + 0x%x]", i.rt(), i.rs(), i.imm16_se());
  return 0;
}

static int lw(const Instruction& i) {
  printf("lw R%02d, [R%02d + 0x%x]", i.rt(), i.rs(), i.imm16_se());
  return 0;
}

static int sb(const Instruction& i) {
  printf("sb R%02d, [R%02d + 0x%x]", i.rt(), i.rs(), i.imm16_se());
  return 0;
}

static int sh(const Instruction& i) {
  printf("sh R%02d, [R%02d + 0x%x]", i.rt(), i.rs(), i.imm16_se());
  return 0;
}

static int sw(const Instruction& i) {
  printf("sw R%02d, [R%02d + 0x%x]", i.rt(), i.rs(), i.imm16_se());
  return 0;
}

static int decode_cop0(const Instruction &instruction) {
  switch (instruction.rs()) {
  case 0b00100:
    return mtc0(instruction);
  case 0b00000:
    return mfc0(instruction);
  }

  printf("!UNKNOWN!");

  return -1;
}

static int decode_sub(const Instruction &instruction) {
  switch (instruction.funct()) {
  case 0x20:
    return add(instruction);
  case 0x24:
    return ins_and(instruction);
  case 0x21:
    return addu(instruction);
  case 0x0:
    sll(instruction);
    return 0;
  case 0x25:
    return ins_or(instruction);
  case 0x2b:
    return sltu(instruction);
  case 0x08:
    return jr(instruction);
  }

  printf("!UNKNOWN!");

  return -1;
}

int decode(const Instruction &instruction) {
  switch (instruction.opcode()) {
  case 0b000000:
    return decode_sub(instruction);
  case 0x28:
    return sb(instruction);
  case 0xc:
    return andi(instruction);
  case 0x29:
    return sh(instruction);
  case 0b001111:
    lui(instruction);
    return 0;
  case 0xd:
    ori(instruction);
    return 0;
  case 0b101011:
    return sw(instruction);
  case 0x9:
    addiu(instruction);
    return 0;
  case 0x2:
    j(instruction);
    return 0;
  case 0x10:
    return decode_cop0(instruction);
  case 0x5:
    return bne(instruction);
  case 0x8:
    return addi(instruction);
  case 0x23:
    return lw(instruction);
  case 0x3:
    return jal(instruction);
  case 0x20:
    return lb(instruction);
  case 0x04:
    return beq(instruction);
  }

  printf("!UNKNOWN!");
  
  return -1;
}
