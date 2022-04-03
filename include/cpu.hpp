#pragma once

#include "types.hpp"
#include "bios.hpp"
#include "pci.hpp"
#include "instruction.hpp"
#include <utility>

struct COP0 {
  // REVIEW: setting all to 0 may not be accurate i.e. sr-$12. though sr is being set to mask ISOLATE_CACHE
  // REVIEW: nocash says 32-63 is N/A should I remove?
  u32 regs[64] = {0};
};

struct PendingLoad {
  u32 reg_index;
  u32 val;
};

struct CPU {
  PCI &pci;
  COP0 cop0;
  
  u32 pc = 0xbfc00000;
  Instruction next_instruction = {0};
  
  u32 out_regs[32];
  u32 in_regs[32];

  static constexpr PendingLoad no_op_load = {0, 0};
  PendingLoad pending_load = no_op_load;

  // REVIEW: should I be careful about $zero being overwritten?
  CPU(PCI &pci) : pci(pci) {
    in_regs[0] = out_regs[0] = 0;
    // TODO: remove this init for loop
    for(int i = 1; i < 32; ++i) {
      out_regs[i] = in_regs[i] = 0xdeadbeef;
    }
  }

  void dump();

  int next();
  int fetch(u32& instruction_data, u32 addr);
  int decode_execute(const Instruction& instruction);
  int decode_execute_sub(const Instruction& instruction); // TODO: to static
  int decode_execute_cop0(const Instruction& instruction); // TODO: to static

  constexpr void set_reg(u32 index, u32 val);
  constexpr u32 reg(u32 index);

  // TODO: void instructions to int
  void lui(const Instruction& instruction);
  void ori(const Instruction& instruction);
  int sw(const Instruction& instruction);
  void sll(const Instruction& instruction);
  void addiu(const Instruction& instruction);
  void j(const Instruction& instruction);
  int ins_or(const Instruction& instruction);
  int mtc0(const Instruction& instruction);
  int bne(const Instruction& instruction);
  int addi(const Instruction& instruction);
  int lw(const Instruction& instruction);
  int sltu(const Instruction& instruction);
  int addu(const Instruction& instruction);
  int sh(const Instruction& instruction);
  int jal(const Instruction& instruction);
  int andi(const Instruction& instruction);
  int sb(const Instruction& instruction);
  int jr(const Instruction& instruction);
  int lb(const Instruction &instruction);
  int beq(const Instruction &instruction);
  int mfc0(const Instruction &instruction);
  int ins_and(const Instruction &instruction);
  int add(const Instruction &instruction);

  bool cache_isolated();
};
