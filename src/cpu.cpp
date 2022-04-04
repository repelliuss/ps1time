#include "cpu.hpp"
#include "data.hpp"
#include "instruction.hpp"
#include "asm.hpp"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <iostream>

void CPU::dump() {
  Instruction ins = next_instruction;

  printf("PC: %08x\n", pc);

  for(int i = 0; i < 8; ++i) {
    int r1 = i * 4;
    int r2 = r1 + 1;
    int r3 = r2 + 1;
    int r4 = r3 + 1;

    printf("R%02d: %08x  R%02d: %08x  R%02d: %08x  R%02d: %08x\n", r1,
           in_regs[r1], r2, in_regs[r2], r3, in_regs[r3], r4, in_regs[r4]);
  }

  if (pending_load.reg_index != 0) {
    printf("Pending load: R%02d <- %08x\n", pending_load.reg_index,
           pending_load.val);
  }

  printf("Next instruction: 0x%08x ", ins.data);
  decode(ins);
  
  printf("\n\n");
}

bool CPU::cache_isolated() { return (cop0.regs[12] & 0x10000) != 0; }

int CPU::dump_and_next() {
  dump();
  return next();
}

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
    printf("cpu can't exec inst: 0x%08x pc+4: %#08x nextinst: %#08x\n",
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

  return -1;
}

int CPU::decode_execute(const Instruction &instruction) {
  switch (instruction.opcode()) {
  case 0b000000:
    return decode_execute_sub(instruction);
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

static int sw_prohibited(PCIMatch match, u32 offset, u32 val, u32 addr) {
  switch (match) {
  case PCIMatch::ram:
    return 0;

  case PCIMatch::irq:
    printf("IRQ control: %x <- %08x\n", offset, val);
    return 1;

  case PCIMatch::cache_ctrl:
    printf("Unhandled write to CACHE_CONTROL: %08x\n", val);
    return 1;

  case PCIMatch::mem_ctrl:
    switch (offset) {
    case 0:
      if (val != 0x1f000000) {
        printf("Bad expansion 1 base address: 0x%08x\n", val);
        return -1;
      }

      return 0;

    case 4:
      if (val != 0x1f802000) {
        printf("Bad expansion 2 base address: 0x%08x\n", val);
        return -1;
      }

      return 0;
    }

    printf("Unhandled write to MEM_CONTROL register %x: 0x%08x\n", offset, val);
    return 1;

  case PCIMatch::ram_size:
    printf("Unhandled write to RAM_SIZE register %x <- %08x\n", offset, val);
    return 1;

  default:
    printf("unhandled store32 into address %08x\n", addr);
    return -1;
  }
}

// TODO: change offset to index as data is separated
int CPU::sw(const Instruction &i) {
  if (cache_isolated()) {
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

  int status = sw_prohibited(match, offset, value, addr);
  if (status < 0)
    return -1;
  if (status == 1)
    return 0;

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

  switch (cop_r) {
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

  if (checked_sum(sum, rs_v, imm)) {
    printf("ADDI overflow\n");
    return -1;
  }

  set_reg(i.rt(), sum);

  return 0;
}

// NOTE: LW and LB didn't have cache_isolated check?

static int load32_prohibited(PCIMatch match, u32 offset, u32 addr) {
  switch (match) {
  case PCIMatch::bios:
  case PCIMatch::ram:
    return 0;
  case PCIMatch::irq:
    printf("IRQ control read %x\n", offset);
    return 0;
  default:
    printf("unhandled load32 at address %08x\n", addr);
    return -1;
  }
}

static u32 lw_data(PCIMatch match, u8 *data, u32 offset) {
  switch(match) {
  case PCIMatch::irq:
    return 0;
  default:
    return load32(data, offset);
  }
}

int CPU::lw(const Instruction &i) {
  u8 *data;
  u32 offset;
  u32 addr = reg(i.rs()) + i.imm16_se();
  PCIMatch match = pci.match(data, offset, addr);

  if (match == PCIMatch::none)
    return -1;

  int status = load32_prohibited(match, offset, addr);
  if (status < 0)
    return -1;
  if (status == 1)
    return 0;

  pending_load.reg_index = i.rt();
  pending_load.val = lw_data(match, data, offset);

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

static int store16_prohibited(PCIMatch match, u32 offset, u32 addr) {
  switch (match) {
  case PCIMatch::spu:
    printf("Unhandled write to SPU register %x\n", offset);
    return 1;

  case PCIMatch::timers:
    printf("Unhandled write to timer register %x\n", offset);
    return 1;

  default:
    printf("unhandled store16 into address %08x\n", addr);
    return -1;
  }
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

  int status = store16_prohibited(match, offset, addr);
  if (status < 0)
    return -1;
  if (status == 1)
    return 0;

  // TODO: store value to u16

  return 0;
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

static int sb_prohibited(PCIMatch match, u32 offset, u32 addr) {
  switch (match) {
  case PCIMatch::ram:
    return 0;
  case PCIMatch::expansion2:
    printf("Unhandled write to expansion 2 register %x\n", offset);
    return 1;

  default:
    printf("unhandled store8 into address %x\n", addr);
    return -1;
  }
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

  int status = sb_prohibited(match, offset, addr);
  if (status < 0)
    return -1;
  if (status == 1)
    return 0;

  // NOTE: only ram store8 is valid and it doesn't do conversion

  store8(data, value, offset);
  return 0;
}

int CPU::jr(const Instruction &i) {
  pc = reg(i.rs());
  return 0;
}

static int load8_prohibited(PCIMatch match, u32 offset, u32 addr) {
  switch (match) {
  case PCIMatch::ram:
  case PCIMatch::bios:
  case PCIMatch::expansion1: // NOTE: requires specific load value
    return 0;
  default:
    printf("unhandled load8 at address %08x\n", addr);
    return -1;
  }
}

static u8 lb_data(PCIMatch match, u8 *data, u32 offset) {
  switch (match) {
    // NOTE: lb expansion1 always return 1
  case PCIMatch::expansion1:
    return 0xff;
  default:
    return load8(data, offset);
  }
}

int CPU::lb(const Instruction &i) {
  u8 *data;
  u32 offset;
  u32 addr = reg(i.rs()) + i.imm16_se();
  PCIMatch match = pci.match(data, offset, addr);

  if (match == PCIMatch::none)
    return -1;

  int status = load8_prohibited(match, offset, addr);
  if (status < 0)
    return -1;
  if (status == 1)
    return 0;

  pending_load.reg_index = i.rt();

  // byte is sign extended, so we are being careful
  pending_load.val =
      static_cast<u32>(static_cast<i8>(lb_data(match, data, offset)));

  return 0;
}

int CPU::beq(const Instruction &i) {

  if (reg(i.rs()) == reg(i.rt())) {
    pc += branch_offset(i.imm16_se());
  }

  return 0;
}

int CPU::mfc0(const Instruction &i) {

  u32 cop_r = i.rd();
  u32 val;

  switch (cop_r) {
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

int CPU::add(const Instruction &i) {
  i32 sum;
  i32 rs_v = reg(i.rs());
  i32 rt_v = reg(i.rt());

  if (checked_sum(sum, rs_v, rt_v)) {
    printf("ADD overflow\n");
    return -1;
  }

  set_reg(i.rd(), sum);

  return 0;
}

int CPU::bgtz(const Instruction &i) {
  i32 val = reg(i.rs());

  if (val > 0)
    pc += branch_offset(i.imm16_se());

  return 0;
}

int CPU::blez(const Instruction &i) {
  i32 val = reg(i.rs());

  if (val <= 0)
    pc += branch_offset(i.imm16_se());

  return 0;
}

int CPU::lbu(const Instruction &i) {
  u8 *data;
  u32 offset;
  u32 addr = reg(i.rs()) + i.imm16_se();
  PCIMatch match = pci.match(data, offset, addr);

  if (match == PCIMatch::none)
    return -1;

  int status = load8_prohibited(match, offset, addr);
  if (status < 0)
    return -1;
  if (status == 1)
    return 0;

  pending_load.reg_index = i.rt();
  pending_load.val = lb_data(match, data, offset);

  return 0;
}

int CPU::jalr(const Instruction &i) {
  set_reg(i.rd(), pc);
  pc = reg(i.rs());
  return 0;
}

int CPU::bcondz(const Instruction &i) {
  bool is_bgez = i.bit16();
  bool is_link = i.bit20() != 0;

  i32 val = reg(i.rs());
  u32 test = val < 0;

  test ^= is_bgez;

  if (test != 0) {
    if (is_link) {
      set_reg(31, pc);
    }

    pc += branch_offset(i.imm16_se());
  }

  return 0;
}

int CPU::slti(const Instruction &i) {
  i32 simm16 = i.imm16_se();
  i32 rs_val = reg(i.rs());

  i32 test = rs_val < simm16; // REVIEW: may not need to i32 conversion

  set_reg(i.rt(), test);

  return 0;
}

int CPU::subu(const Instruction &i) {
  set_reg(i.rd(), reg(i.rs()) - reg(i.rt()));
  return 0;
}

int CPU::sra(const Instruction &i) {
  // REVIEW: may remove the cast and just assign
  i32 val = static_cast<i32>(reg(i.rt())) >> i.shamt();
  set_reg(i.rd(), val);
  return 0;
}

// NOTE: there are some special cases like div by 0, no exception just trash
// values are put
int CPU::div(const Instruction &i) {
  i32 numerator = reg(i.rs());
  i32 denominator = reg(i.rt());

  if (denominator == 0) {
    hi = numerator;

    if (numerator >= 0) {
      lo = 0xffffffff;
    } else {
      lo = 1;
    }
  } else if (static_cast<u32>(numerator) == 0x80000000 && denominator == -1) {
    // result doesn't fit to 32bit signed intger
    hi = 0;
    lo = 0x80000000;
  } else {
    hi = numerator % denominator;
    lo = numerator / denominator;
  }

  return 0;
}

// TODO: div takes more than one cycle in hardware and apparently we also need
// to emulate that behavior, and if div is still going on this op should stall
int CPU::mflo(const Instruction &i) {
  set_reg(i.rd(), lo);
  return 0;
}

int CPU::srl(const Instruction &i) {
  set_reg(i.rd(), reg(i.rt()) >> i.shamt());
  return 0;
}

int CPU::sltiu(const Instruction &i) {
  set_reg(i.rt(), reg(i.rs()) < i.imm16_se());
  return 0;
}

int CPU::divu(const Instruction &i) {
  u32 numerator = reg(i.rs());
  u32 denominator = reg(i.rt());

  if (denominator == 0) {
    hi = numerator;
    lo = 0xffffffff;
  } else {
    hi = numerator % denominator;
    lo = numerator / denominator;
  }

  return 0;
}

// TODO: div takes more than one cycle in hardware and apparently we also need
// to emulate that behavior, and if div is still going on this op should stall
int CPU::mfhi(const Instruction &i) {
  set_reg(i.rd(), hi);
  return 0;
}

int CPU::slt(const Instruction &i) {
  i32 rs_val = reg(i.rs());
  i32 rt_val = reg(i.rt());
  i32 val = rs_val < rt_val;

  set_reg(i.rd(), val);
  
  return 0;
}
