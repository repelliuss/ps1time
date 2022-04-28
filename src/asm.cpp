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

static int sltiu(const Instruction& i) {
  printf("sltiu R%02d, R%02d, 0x%x", i.rt(), i.rs(), i.imm16_se());
  return 0;
}

static int slt(const Instruction& i) {
  printf("slt R%02d, R%02d, R%02d", i.rd(), i.rs(), i.rt());
  return 0;
}

static int subu(const Instruction& i) {
  printf("subu R%02d, R%02d, R%02d", i.rd(), i.rs(), i.rt());
  return 0;
}

static int slti(const Instruction& i) {
  printf("slti R%02d, R%02d, %d", i.rt(), i.rs(),
         static_cast<i32>(i.imm16_se()));
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

static int jalr(const Instruction& i) {
  printf("jalr R%02d, R%02d", i.rd(), i.rs());
  return 0;
}

static int beq(const Instruction &i) {
  printf("beq R%02d, R%02d, %d", i.rs(), i.rt(),
         static_cast<i32>(i.imm16_se() << 2));
  return 0;
}

static int bne(const Instruction &i) {
  printf("bne R%02d, R%02d, %d", i.rs(), i.rt(),
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

static int mfhi(const Instruction& i) {
  printf("mfhi R%02d", i.rd());
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

static int lbu(const Instruction& i) {
  printf("lbu R%02d, [R%02d + 0x%x]", i.rt(), i.rs(), i.imm16_se());
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

static int bcondz(const Instruction &i) {
  const char *op;
  const char *al;

  if((i.data & (1 << 16)) != 0) {
    op = "bgez";
  }
  else {
    op = "bltz";
  }

  if ((i.data & (1 << 20)) != 0) {
    al = "al";
  }
  else {
    al = "";
  }

  printf("%s%s R%02d, %d", op, al, i.rs(), static_cast<i32>(i.imm16_se() << 2));
  return 0;
}

static int blez(const Instruction &i) {
  printf("blez R%02d, %d", i.rs(), static_cast<i32>(i.imm16_se() << 2));
  return 0;
}

static int bgtz(const Instruction &i) {
  printf("bgtz R%02d, %d", i.rs(), static_cast<i32>(i.imm16_se() << 2));
  return 0;
}

static int mthi(const Instruction &i) {
  printf("mthi R%02d", i.rs());
  return 0;
}

static int mflo(const Instruction &i) {
  printf("mflo R%02d", i.rd());
  return 0;
}

static int mtlo(const Instruction &i) {
  printf("mtlo R%02d", i.rs());
  return 0;
}

static int syscall(const Instruction &i) {
  printf("syscall %x", (i.data >> 6) & 0xfffff);
  return 0;
}

static int divu(const Instruction &i) {
  printf("divu R%02d, R%02d", i.rs(), i.rt());
  return 0;
}

static int div(const Instruction &i) {
  printf("div R%02d, R%02d", i.rs(), i.rt());
  return 0;
}

static int srl(const Instruction &i) {
  printf("srl R%02d, R%02d >> %d", i.rd(), i.rt(), i.shamt());
  return 0;
}

static int sra(const Instruction &i) {
  printf("sra R%02d, R%02d >> %d", i.rd(), i.rt(), i.shamt());
  return 0;
}

static int rfe(const Instruction &i) {
  if((i.data & 0x3f) == 0b010000) {
    printf("rfe");
  }
  else {
    printf("!INVALID cop0 instruction %d!", i.data);
  }
  return 0;
}

static int decode_cop0(const Instruction &instruction) {
  switch (instruction.rs()) {
  case 0b00100:
    return mtc0(instruction);
  case 0b00000:
    return mfc0(instruction);
  case 0b10000:
    return rfe(instruction);
  }

  printf("!UNKNOWN!");

  return -1;
}

static int decode_sub(const Instruction &instruction) {
  switch (instruction.funct()) {
  case 0x11:
    return mthi(instruction);
  case 0x13:
    return mtlo(instruction);
  case 0x0c:
    return syscall(instruction);
  case 0x2a:
    return slt(instruction);
  case 0x10:
    return mfhi(instruction);
  case 0x1b:
    return divu(instruction);
  case 0x02:
    return srl(instruction);
  case 0x03:
    return sra(instruction);
  case 0x12:
    return mflo(instruction);
  case 0x1a:
    return div(instruction);
  case 0x23:
    return subu(instruction);
  case 0x20:
    return add(instruction);
  case 0x24:
    return ins_and(instruction);
  case 0x09:
    return jalr(instruction);
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
  case 0x01:
    return bcondz(instruction);
  case 0x0b:
    return sltiu(instruction);
  case 0x0a:
    return slti(instruction);
  case 0x24:
    return lbu(instruction);
  case 0x06:
    return blez(instruction);
  case 0x07:
    return bgtz(instruction);
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
