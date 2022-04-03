#include "cpu.hpp"
#include "data.hpp"
#include "instruction.hpp"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <iostream>

static const char* REGISTER_MNEMONICS[32] = {
    "R00",
    "R01",
    "R02", "R03",
    "R04", "R05", "R06", "R07",
    "R08", "R09", "R10", "R11",
    "R12", "R13", "R14", "R15",
    "R16", "R17", "R18", "R19",
    "R20", "R21", "R22", "R23",
    "R24", "R25",
    "R26", "R27",
    "R28",
    "R29",
    "R30",
    "R31",
};

void CPU::dump() {
  Instruction ins = next_instruction;

  printf("PC: %08x\n", pc);

  for(int i = 0; i < 8; ++i) {
    int r1 = i * 4;
    int r2 = r1 + 1;
    int r3 = r2 + 1;
    int r4 = r3 + 1;

    printf("%s: %08x  %s: %08x  %s: %08x  %s: %08x\n",
	   REGISTER_MNEMONICS[r1], in_regs[r1],
	   REGISTER_MNEMONICS[r2], in_regs[r2],
	   REGISTER_MNEMONICS[r3], in_regs[r3],
	   REGISTER_MNEMONICS[r4], in_regs[r4]);
  }

  if (pending_load.reg_index != 0) {
    printf("Pending load: %s <- %08x\n",
           REGISTER_MNEMONICS[pending_load.reg_index], pending_load.val);
  }

  printf("Next instruction: 0x%08x\n", ins.data);

  printf("\n");
}

bool CPU::cache_isolated() { return (cop0.regs[12] & 0x10000) != 0; }

// TODO: remove assertions
int CPU::next() {
  Instruction instruction = next_instruction;
  u32 instruction_data;

  int cpu_fetch_result = fetch(instruction_data, pc);
  assert(cpu_fetch_result == 0);
  next_instruction = {.data = instruction_data};

  pc += 4;

  {
    // absorb pending load, this came out because load delay slot
    set_reg(pending_load.reg_index, pending_load.val);
    pending_load = no_op_load;
  }

  int cpu_exec_result = decode_execute(instruction);
  if (cpu_exec_result) {
    printf("cpu can't exec inst: %#08x pc+4: %#08x nextinst: %#08x\n",
           instruction.data, pc, next_instruction.data);
    return -1;
  }

  {
    // copy out_regs to in_regs, continuation of pending load absorb
    // TODO: re-design here
    memcpy(in_regs, out_regs, sizeof(u32) * 32);
  }

  return 0;
}

int CPU::fetch(u32 &instruction_data, u32 addr) {
  u8 *data;
  u32 offset;

  if (pci.match(data, offset, addr) != PCIMatch::none) {
    instruction_data = load32(data, offset);
    return 0;
  }

  // FIXME: negative value when unsigned return
  return -1; // REVIEW: what to return? or exception here?
}

int CPU::decode_execute_cop0(const Instruction &instruction) {
  switch (instruction.rs()) {
  case 0b00100:
    return mtc0(instruction);
  case 0b00000:
    return mfc0(instruction);
  }

  return -1;
}

int CPU::decode_execute_sub(const Instruction &instruction) {
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

  return -1;
}

int CPU::decode_execute(const Instruction &instruction) {
  switch (instruction.opcode()) {
  case 0b000000:
    return decode_execute_sub(instruction);
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
    return decode_execute_cop0(instruction);
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

  return -1;
}

constexpr u32 CPU::reg(u32 index) { return in_regs[index]; }

constexpr void CPU::set_reg(u32 index, u32 val) { out_regs[index] = val; }

void CPU::lui(const Instruction &i) { set_reg(i.rt(), i.imm16() << 16); }

void CPU::ori(const Instruction &i) {
  set_reg(i.rt(), reg(i.rs()) | i.imm16());
}

// TODO: should I quit at errors?
// TODO: change offset to index as data is separated
int CPU::sw(const Instruction &i) {
  if (cache_isolated()) {
    // TODO: read/writes should go through cache
    printf("Ignoring store while cache is isolated\n");
    return 0;
  }

  u8 *data;
  u32 offset;
  u32 addr = reg(i.rs()) + i.imm16_se();
  u32 value = reg(i.rt());
  PCIMatch match = pci.match(data, offset, addr);

  if (match == PCIMatch::none)
    return -1;
  if (pci.prohibited(match, offset, value)) {
    printf("IGNORING PROHIBITED WRITE\n");
    return 0;
  }

  store32(data, value, offset);

  return 0;
}

void CPU::sll(const Instruction &i) {
  set_reg(i.rd(), reg(i.rt()) << i.shamt());
}

void CPU::addiu(const Instruction &i) {
  set_reg(i.rt(), reg(i.rs()) + i.imm16_se());
}

void CPU::j(const Instruction &i) { pc = (pc & 0xf0000000) | (i.imm26() << 2); }

int CPU::ins_or(const Instruction &i) {
  set_reg(i.rd(), reg(i.rs()) | reg(i.rt()));
  return 0;
}

int CPU::mtc0(const Instruction &i) {
  u32 cop_r = i.rd();
  u32 val = reg(i.rt());

  switch(cop_r) {
    // breakpoint register for COP0
  case 3:
  case 5:
  case 6:
  case 7:
  case 9:
  case 11:
    if (val != 0) {
      printf("Unhandled write to cop0 register %u\n", cop_r);
      return -1;
    }
    break;

    // status register SR is safe
  case 12:
    break;
    
  case 13:
    if (val != 0) {
      printf("Unhandled write to CAUSE(13) register\n");
      return -1;
    }
    break;

  default:
    printf("Unhandled cop0 register %u\n", cop_r);
    return -1;
  }
  
  // TODO: cop0 doesn't have in reg out reg concept because of load delay
  cop0.regs[cop_r] = val;

  return 0;
}

static constexpr u32 branch_offset(u32 offset) {
  // REVIEW: is -4 correct?
  return (offset << 2) - 4;
}

int CPU::bne(const Instruction &i) {

  if (reg(i.rs()) != reg(i.rt())) {
    pc += branch_offset(i.imm16_se());
  }

  return 0;
}

// returns 1 on overflow
[[nodiscard]] static constexpr int checked_sum(i32 &sum, i32 a, i32 b) {
  // REVIEW: volatile may be required
  sum = a + b;
  return (sum < a) != (b < 0);
}

int CPU::addi(const Instruction &i) {
  
  i32 sum;
  u32 rs_v = reg(i.rs());
  u32 imm = i.imm16_se();

  if(checked_sum(sum, rs_v, imm)) {
    printf("ADDI overflow\n");
    return -1;
  }

  set_reg(i.rt(), sum);

  return 0;
}

// BUG: LW and LB didn't have cache_isolated check?

int CPU::lw(const Instruction& i) {

  u8* data;
  u32 offset;
  u32 addr = reg(i.rs()) + i.imm16_se();
  PCIMatch match = pci.match(data, offset, addr);

  if(match == PCIMatch::none) return -1;
  if (pci.prohibited(match, offset, 0xdeadbeef)) {
    printf("IGNORING PROHIBITED LOAD\n");
    return 0;
  }

  pending_load.reg_index = i.rt();
  pending_load.val = load32(data, offset);

  return 0;
}

int CPU::sltu(const Instruction &i) {
  set_reg(i.rd(), reg(i.rs()) < reg(i.rt()));
  return 0;
}

int CPU::addu(const Instruction &i) {
  set_reg(i.rd(), reg(i.rs()) + reg(i.rt()));
  return 0;
}

int CPU::sh(const Instruction &i) {

  if (cache_isolated()) {
    // TODO: read/writes should go through cache
    printf("Ignoring store while cache is isolated\n");
    return 0;
  }

  u8 *data;
  u32 offset;
  u32 addr = reg(i.rs()) + i.imm16_se();
  u32 value = reg(i.rt());
  PCIMatch match = pci.match(data, offset, addr);

  if (match == PCIMatch::none)
    return -1;
  if (pci.prohibited(match, offset, value)) {
    // TODO: remove ignored prohobited writes
    printf("IGNORING PROHIBITED WRITE\n");
    return 0;
  }

  // TODO: value to u16

  printf("unhandled SH, not implemented\n");
  return -1;

  // store16(data, value, offset);

  // return 0;
}

int CPU::jal(const Instruction &i) {
  set_reg(31, pc); // pc is currently at pc+8, so it is fine
  j(i);
  return 0;
}

int CPU::andi(const Instruction &i) {
  set_reg(i.rt(), reg(i.rs()) & i.imm16());
  return 0;
}

int CPU::sb(const Instruction &i) {

  if (cache_isolated()) {
    // TODO: read/writes should go through cache
    printf("Ignoring store while cache is isolated\n");
    return 0;
  }

  u8 *data;
  u32 offset;
  u32 addr = reg(i.rs()) + i.imm16_se();
  u32 value = reg(i.rt());
  PCIMatch match = pci.match(data, offset, addr);

  if (match == PCIMatch::none)
    return -1;

  if (pci.prohibited(match, offset, value)) {
    // TODO: remove ignored prohobited writes
    printf("IGNORING PROHIBITED WRITE\n");
    return 0;
  }

  if(match == PCIMatch::ram) {
    //store without any explicit u32 to u8 value conversion
    store8(data, value, offset);
    return 0;
  }

  printf("unhandled SB, not implemented\n");
  return -1;
}

int CPU::jr(const Instruction &i) {
  pc = reg(i.rs());
  return 0;
}

int CPU::lb(const Instruction &i) {

  u8 *data;
  u32 offset;
  u32 addr = reg(i.rs()) + i.imm16_se();
  PCIMatch match = pci.match(data, offset, addr);

  if (match == PCIMatch::none)
    return -1;

  if (pci.prohibited(match, offset, 0xdeadbeef)) {
    printf("IGNORING PROHIBITED LOAD BYTE\n");
    return 0;
  }

  if (match == PCIMatch::bios ||
      match == PCIMatch::ram ||
      match == PCIMatch::expansion1) {
    // REVIEW: no need for this when all is handled, logic is right here
    pending_load.reg_index = i.rt();
    //byte is sign extended, so we are being careful
    pending_load.val = static_cast<u32>(static_cast<i8>(load8(data, offset)));

    return 0;
  }

  printf("unhandled LB, not implemented\n");
  return -1;
}

int CPU::beq(const Instruction &i) {

  if (reg(i.rs()) == reg(i.rt())) {
    pc += branch_offset(i.imm16_se());
  }

  return 0;
}


int CPU::mfc0(const Instruction& i) {

  u32 cop_r = i.rd();
  u32 val;

  switch(cop_r) {
  case 12:
    pending_load.reg_index = i.rt();
    pending_load.val = cop0.regs[12];
    return 0;
  case 13:
    printf("Unhandled read from CAUSE-$13 register!\n");
    return -1;
  }

  printf("Unhandled read from cop0 register!\n");
  return -1;
}

int CPU::ins_and(const Instruction &i) {
  set_reg(i.rd(), reg(i.rs()) & reg(i.rt()));
  return 0;
}

int CPU::add(const Instruction& i) {

  i32 sum;
  i32 rs_v = reg(i.rs());
  i32 rt_v = reg(i.rt());
  
  if(checked_sum(sum, rs_v, rt_v)) {
    printf("ADD overflow\n");
    return -1;
  }

  set_reg(i.rd(), sum);

  return 0;
}
