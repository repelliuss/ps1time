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

  // TODO: i to ins for instruction, index to i for index
  static constexpr int index_sr = 12;
  static constexpr int index_cause = 13;
  static constexpr int index_epc = 14;
};

struct PendingLoad {
  u32 reg_index;
  u32 val;
};

enum struct Cause : u32 {
  syscall = 0x8,
  overflow = 0xc,
  unaligned_load_addr = 0x4,
  unaligned_store_addr = 0x5,
  brek = 0x9, //break
  unimplemented_coprocessor = 0xb,
};

struct CPU {
  PCI &pci;
  COP0 cop0;

  u32 cur_pc;			// pc for current executed instruction
  u32 pc = 0xbfc00000;		// cur_pc + 4 in decode_execute
  u32 next_pc = 0xbfc00004;	// mostly cur_pc + 8 in decode_execute

  bool branch_ocurred = false;
  bool in_delay_slot = false;
  
  u32 out_regs[32];
  u32 in_regs[32];
  u32 hi = 0xdeadbeef; //div instruction remainder
  u32 lo = 0xdeadbeef; //div instruction quotient

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
  int dump_and_next();
  int fetch(u32& instruction_data, u32 addr);
  int decode_execute(const Instruction& instruction);
  int decode_execute_sub(const Instruction& instruction); // TODO: to static
  int decode_execute_cop0(const Instruction& instruction); // TODO: to static
  int decode_execute_cop1(const Instruction& instruction); // TODO: to static
  int decode_execute_cop2(const Instruction& instruction); // TODO: to static
  int decode_execute_cop3(const Instruction& instruction); // TODO: to static

  void branch(u32 offset);
  int exception(const Cause &cause);

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
  int bgtz(const Instruction &instruction);
  int blez(const Instruction &instruction);
  int lbu(const Instruction &instruction);
  int jalr(const Instruction &instruction);
  int bcondz(const Instruction &instruction);
  int slti(const Instruction &instruction);
  int subu(const Instruction &instruction);
  int sra(const Instruction &instruction);
  int div(const Instruction &instruction);
  int mflo(const Instruction &instruction);
  int srl(const Instruction &instruction);
  int sltiu(const Instruction &instruction);
  int divu(const Instruction &instruction);
  int mfhi(const Instruction &instruction);
  int slt(const Instruction &instruction);
  int syscall(const Instruction &instruction);
  int mtlo(const Instruction &instruction);
  int mthi(const Instruction &instruction);
  int rfe(const Instruction &instruction);
  int lhu(const Instruction &instruction);
  int sllv(const Instruction &instruction);
  int lh(const Instruction &instruction);
  int nor(const Instruction &instruction);
  int srav(const Instruction &instruction);
  int srlv(const Instruction &instruction);
  int multu(const Instruction &instruction);
  int ins_xor(const Instruction &instruction);
  int ins_break(const Instruction &instruction);
  int mult(const Instruction &instruction);
  int sub(const Instruction &instruction);
  int xori(const Instruction &instruction);
  int lwl(const Instruction &instruction);
  int lwr(const Instruction &instruction);
  int swl(const Instruction &instruction);
  int swr(const Instruction &instruction);
  int lwc0(const Instruction &instruction);
  int lwc1(const Instruction &instruction);
  int lwc2(const Instruction &instruction);
  int lwc3(const Instruction &instruction);
  int swc0(const Instruction &instruction);
  int swc1(const Instruction &instruction);
  int swc2(const Instruction &instruction);
  int swc3(const Instruction &instruction);
  
  
  // TODO: may move cop0 instructions to its own
  
  bool cache_isolated();
};
