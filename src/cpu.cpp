#include "cpu.hpp"
#include "asm.hpp"
#include "data.hpp"
#include "instruction.hpp"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <iostream>

void CPU::dump() {
  Instruction ins;

  fetch(ins, cur_pc);

  printf("PC: %08x\n", cur_pc);

  for (int i = 0; i < 8; ++i) {
    int r1 = i * 4;
    int r2 = r1 + 1;
    int r3 = r2 + 1;
    int r4 = r3 + 1;

    printf("R%02d: %08x  R%02d: %08x  R%02d: %08x  R%02d: %08x\n", r1,
           in_regs[r1], r2, in_regs[r2], r3, in_regs[r3], r4, in_regs[r4]);
  }

  printf("HI: %08x  LO: %08x\n", hi, lo);

  if (pending_load.reg_index != 0) {
    printf("Pending load: R%02d <- %08x\n", pending_load.reg_index,
           pending_load.val);
  }

  printf("Next instruction: 0x%08x ", ins.data);
  decode(ins);

  printf("\n\n");
}

int CPU::dump_and_next() {
  dump();
  return next();
}

int CPU::next() {
  if (clock.sync_pending()) {
    pci.clock_sync(clock);
    clock.update_sync_pending();
  }

  // save cur pc here in case of expceiton for EPC
  cur_pc = pc;

  // TODO: move this to pc setting instructions, jump and branch
  if (cur_pc % 4 != 0) {
    return exception(Cause::unaligned_load_addr);
  }
  
  Instruction instruction;
  int cpu_fetch_result = fetch(instruction, cur_pc);
  // REVIEW: instruction fetch may be said to be always succeed
  assert(cpu_fetch_result == 0);

  pc = next_pc;
  next_pc += 4;

  {
    // absorb pending load, this came out because load delay slot
    set_reg(pending_load.reg_index, pending_load.val);
    pending_load = no_op_load;
  }

  in_delay_slot = branch_ocurred;
  branch_ocurred = false;

  if (cop0.interrupt_pending()) {
    LOG_DEBUG("Interrupt pending");
    exception(Cause::interrupt);
  }
  else {
    int cpu_exec_result = decode_execute(instruction);
    if (cpu_exec_result) {
      dump();
      return -1;
    }
  }

  {
    // TODO: re-design here
    // copy out_regs to in_regs, continuation of pending load absorb
    memcpy(in_regs, out_regs, sizeof(u32) * 32);
  }

  return 0;
}

int CPU::fetch(Instruction &ins, u32 addr) {
  CacheCtrl &cc = pci.cache_ctrl;
  bool is_kseg1 = (addr & 0xe0000000) == 0xa0000000;

  if (is_kseg1 || !cc.icache_enabled()) {
    clock.tick(4);
    return pci.load_instruction(ins, addr);
  }

  u32 tag = addr & 0xfffff000;
  ICacheLine &line = icache[(addr >> 4) & 0xff];
  u32 index = (addr >> 2) & 0b11;
  int status = 0;

  if (line.tag() != tag || line.first_valid_index() > index) {
    // cache miss, fetch icache line
    clock.tick(3);
    line.update(addr);
    
    for (int i = index; i < 4; ++i) {
      clock.tick(1);
      
      // REVIEW: instruction fetch may be said to be always succeed
      status |= pci.load_instruction(line.instruction[i], addr);
      addr += 4;
    }
  }

  ins = line.instruction[index];
  return status;
}

int CPU::store8(u8 val, u32 addr) {
  if (cop0.cache_isolated()) {
    LOG_ERROR("[VAL:0x%08x] Unsupported write while cache is isolated", val);
    return -1;
  }

  return pci.store8(val, addr);
}

int CPU::store16(u16 val, u32 addr) {
  if (cop0.cache_isolated()) {
    LOG_ERROR("[VAL:0x%08x] Unsupported write while cache is isolated", val);
    return -1;
  }

  return pci.store16(val, addr, clock);
}

int CPU::store32(u32 val, u32 addr) {
  if (cop0.cache_isolated()) {
    return handle_cache(val, addr);
  }

  return pci.store32(val, addr, clock);
}

int CPU::handle_cache(u32 val, u32 addr) {
  // Implementing full cache emulation requires handling many
  // corner cases. For now I'm just going to add support for
  // cache invalidation which is the only use case for cache
  // isolation as far as I know.

  CacheCtrl &cc = pci.cache_ctrl;

  if (!cc.icache_enabled()) {
    LOG_ERROR("Can't handle cache while instruction cache is disabled");
    return -1;
  }

  if (val != 0) {
    LOG_ERROR("[VAL:0x%08x] Unsupported write while cache is isolated", val);
    return -1;
  }

  ICacheLine &line = icache[(addr >> 4) & 0xff];

  if (cc.tag_test_mode()) {
    line.invalidate();
  } else {
    u32 index = (addr >> 2) & 0b11;
    line.instruction[index].data = val;
  }

  return 0;
}

int CPU::decode_execute_cop0(const Instruction &instruction) {
  switch (instruction.rs()) {
  case 0b00100:
    return mtc0(instruction);
  case 0b00000:
    return mfc0(instruction);
  case 0b10000:
    return rfe(instruction);
  }

  LOG_WARN("Illegal cop0 instruction 0x%08x", instruction.data);
  return exception(Cause::illegal_instruction);
}

int CPU::decode_execute_cop1(const Instruction &instruction) {
  return exception(Cause::unimplemented_coprocessor);
}

int CPU::decode_execute_cop2(const Instruction &instruction) {
  LOG_ERROR("Unhandled GTE instruction: 0x%08x", instruction.data);
  return -1;
}

int CPU::decode_execute_cop3(const Instruction &instruction) {
  return exception(Cause::unimplemented_coprocessor);
}

int CPU::decode_execute_sub(const Instruction &instruction) {
  switch (instruction.funct()) {
  case 0x22:
    return sub(instruction);
  case 0x18:
    return mult(instruction);
  case 0xd:
    return ins_break(instruction);
  case 0x26:
    return ins_xor(instruction);
  case 0x19:
    return multu(instruction);
  case 0x06:
    return srlv(instruction);
  case 0x07:
    return srav(instruction);
  case 0x27:
    return nor(instruction);
  case 0x04:
    return sllv(instruction);
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
    return sll(instruction);
  case 0x25:
    return ins_or(instruction);
  case 0x2b:
    return sltu(instruction);
  case 0x08:
    return jr(instruction);
  }

  LOG_WARN("Illegal sub instruction 0x%08x", instruction.data);
  return exception(Cause::illegal_instruction);
}

int CPU::decode_execute(const Instruction &instruction) {
  clock.tick(1);
  
  switch (instruction.opcode()) {
  case 0b000000:
    return decode_execute_sub(instruction);
  case 0x10:
    return decode_execute_cop0(instruction);
  case 0x11:
    return decode_execute_cop1(instruction);
  case 0x12:
    return decode_execute_cop2(instruction);
  case 0x13:
    return decode_execute_cop3(instruction);
  case 0x30:
    return lwc0(instruction);
  case 0x31:
    return lwc1(instruction);
  case 0x32:
    return lwc2(instruction);
  case 0x33:
    return lwc3(instruction);
  case 0x38:
    return swc0(instruction);
  case 0x39:
    return swc1(instruction);
  case 0x3a:
    return swc2(instruction);
  case 0x3b:
    return swc3(instruction);
  case 0x2a:
    return swl(instruction);
  case 0x2e:
    return swr(instruction);
  case 0x22:
    return lwl(instruction);
  case 0x26:
    return lwr(instruction);
  case 0xe:
    return xori(instruction);
  case 0x21:
    return lh(instruction);
  case 0x25:
    return lhu(instruction);
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
    return lui(instruction);
  case 0xd:
    return ori(instruction);
  case 0b101011:
    return sw(instruction);
  case 0x9:
    return addiu(instruction);
  case 0x2:
    return j(instruction);
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

  LOG_WARN("Illegal instruction 0x%08x", instruction.data);
  return exception(Cause::illegal_instruction);
}

// TODO: need to push/pop SR if nested exceptions are wanted
int CPU::exception(const Cause &cause) {
  pc = cop0.enter_exception(cause, cur_pc, in_delay_slot);
  next_pc = pc + 4;
  return 0;
}

u32 CPU::reg(u32 index) { return in_regs[index]; }

void CPU::set_reg(u32 index, u32 val) {
  out_regs[index] = val;
  out_regs[0] = 0;
}

int CPU::lui(const Instruction &i) {
  set_reg(i.rt(), i.imm16() << 16);
  return 0;
}

int CPU::ori(const Instruction &i) {
  set_reg(i.rt(), reg(i.rs()) | i.imm16());
  return 0;
}

int CPU::sw(const Instruction &i) {
  u32 addr = reg(i.rs()) + i.imm16_se();
  u32 val = reg(i.rt());

  if (addr % 4 != 0) {
    return exception(Cause::unaligned_store_addr);
  }

  return store32(val, addr);
}

int CPU::sll(const Instruction &i) {
  set_reg(i.rd(), reg(i.rt()) << i.shamt());
  return 0;
}

int CPU::addiu(const Instruction &i) {
  set_reg(i.rt(), reg(i.rs()) + i.imm16_se());
  return 0;
}

int CPU::j(const Instruction &i) {
  next_pc = (pc & 0xf0000000) | (i.imm26() << 2);
  branch_ocurred = false;
  return 0;
}

int CPU::ins_or(const Instruction &i) {
  set_reg(i.rd(), reg(i.rs()) | reg(i.rt()));
  return 0;
}

int CPU::mtc0(const Instruction &i) {
  int cop_r = i.rd();
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
      LOG_ERROR("[VAL:0x%x] Unhandled write to cop0 register %u", val, cop_r);
      return -1;
    }
    break;

    // status register SR is safe
  case 12:
    break;

  case 13:
    if (val != 0) {
      LOG_ERROR("[VAL:0x%x] Unhandled write to CAUSE(13) register", val);
      return -1;
    }
    break;

  default:
    LOG_ERROR("[VAL:0x%x] Unhandled cop0 register %u", val, cop_r);
    return -1;
  }

  // NOTE: cop0 doesn't have in reg out reg concept because of load delay
  cop0.set(static_cast<COP0::Reg>(cop_r), val);

  return 0;
}

void CPU::branch(u32 offset) {
  next_pc = pc + (offset << 2);
  branch_ocurred = true;
}

int CPU::bne(const Instruction &i) {
  if (reg(i.rs()) != reg(i.rt())) {
    branch(i.imm16_se());
  }

  return 0;
}

// returns 1 on overflow
[[nodiscard]] static constexpr int checked_sum(i32 &sum, i32 a, i32 b) {
  sum = a + b;
  return (sum < a) != (b < 0);
}

int CPU::addi(const Instruction &i) {
  i32 sum;
  u32 rs_v = reg(i.rs());
  u32 imm = i.imm16_se();

  if (checked_sum(sum, rs_v, imm)) {
    return exception(Cause::overflow);
  }

  set_reg(i.rt(), sum);

  return 0;
}

int CPU::lw(const Instruction &i) {
  u32 addr = reg(i.rs()) + i.imm16_se();

  if (addr % 4 != 0) {
    return exception(Cause::unaligned_load_addr);
  }

  pending_load.reg_index = i.rt();

  return pci.load32(pending_load.val, addr, clock);
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
  u32 addr = reg(i.rs()) + i.imm16_se();
  u32 val = reg(i.rt());

  if (addr % 2 != 0) {
    return exception(Cause::unaligned_store_addr);
  }

  return store16(val, addr);
}

int CPU::jal(const Instruction &i) {
  set_reg(31, next_pc); // next_pc is currently at pc+8, so it is fine
  j(i);
  branch_ocurred = true;
  return 0;
}

int CPU::andi(const Instruction &i) {
  set_reg(i.rt(), reg(i.rs()) & i.imm16());
  return 0;
}

int CPU::sb(const Instruction &i) {
  u32 addr = reg(i.rs()) + i.imm16_se();
  u32 val = reg(i.rt());

  return store8(val, addr);
}

int CPU::jr(const Instruction &i) {
  next_pc = reg(i.rs());
  branch_ocurred = true;
  return 0;
}

int CPU::lb(const Instruction &i) {
  u32 addr = reg(i.rs()) + i.imm16_se();

  pending_load.reg_index = i.rt();

  int status = pci.load8(pending_load.val, addr);
  pending_load.val = static_cast<i8>(pending_load.val);
  return status;
}

int CPU::beq(const Instruction &i) {

  if (reg(i.rs()) == reg(i.rt())) {
    branch(i.imm16_se());
  }

  return 0;
}

int CPU::mfc0(const Instruction &i) {

  int cop_r = i.rd();
  u32 val;

  switch (cop_r) {
  case COP0::Reg::sr:
  case COP0::Reg::cause:
  case COP0::Reg::epc:
    pending_load.reg_index = i.rt();
    pending_load.val = cop0.get(static_cast<COP0::Reg>(cop_r));
    return 0;
  }

  LOG_ERROR("Unhandled read from cop0 register");
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
    return exception(Cause::overflow);
  }

  set_reg(i.rd(), sum);

  return 0;
}

int CPU::bgtz(const Instruction &i) {
  i32 val = reg(i.rs());

  if (val > 0)
    branch(i.imm16_se());

  return 0;
}

int CPU::blez(const Instruction &i) {
  i32 val = reg(i.rs());

  if (val <= 0)
    branch(i.imm16_se());

  return 0;
}

int CPU::lbu(const Instruction &i) {
  u32 addr = reg(i.rs()) + i.imm16_se();

  pending_load.reg_index = i.rt();

  return pci.load8(pending_load.val, addr);
}

int CPU::jalr(const Instruction &i) {
  set_reg(i.rd(), next_pc);
  next_pc = reg(i.rs());
  branch_ocurred = true;
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
      set_reg(31, next_pc);
    }

    branch(i.imm16_se());
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

int CPU::syscall(const Instruction &i) { return exception(Cause::syscall); }

int CPU::mtlo(const Instruction &i) {
  lo = reg(i.rs());
  return 0;
}

int CPU::mthi(const Instruction &i) {
  hi = reg(i.rs());
  return 0;
}

int CPU::rfe(const Instruction &i) {
  // There are other instructions with same encoding but
  // all are virtual memory related and PS1 doesn't
  // implement them
  // REVIEW: may remove this rfe check
  if ((i.data & 0x3f) != 0b010000) {
    LOG_ERROR("Invalid cop0 instruction: 0x%08x", i.data);
    return -1;
  }

  cop0.rfe();
  return 0;
}

int CPU::lhu(const Instruction &i) {
  u32 addr = reg(i.rs()) + i.imm16_se();

  if (addr % 2 != 0) {
    return exception(Cause::unaligned_load_addr);
  }

  pending_load.reg_index = i.rt();

  return pci.load16(pending_load.val, addr, clock);
}

int CPU::sllv(const Instruction &i) {
  set_reg(i.rd(), reg(i.rt()) << (reg(i.rs()) & 0x1f));
  return 0;
}

int CPU::lh(const Instruction &i) {
  u32 addr = reg(i.rs()) + i.imm16_se();

  if (addr % 2 != 0) {
    return exception(Cause::unaligned_load_addr);
  }

  pending_load.reg_index = i.rt();

  int status = pci.load16(pending_load.val, addr, clock);
  pending_load.val = static_cast<i16>(pending_load.val);
  return status;
}

int CPU::nor(const Instruction &i) {
  set_reg(i.rd(), ~(reg(i.rt()) | reg(i.rs())));
  return 0;
}

int CPU::srav(const Instruction &i) {
  set_reg(i.rd(), static_cast<i32>(reg(i.rt())) >> (reg(i.rs()) & 0x1f));
  return 0;
}

int CPU::srlv(const Instruction &i) {
  set_reg(i.rd(), reg(i.rt()) >> (reg(i.rs()) & 0x1f));
  return 0;
}

int CPU::multu(const Instruction &i) {
  u64 rs_val = reg(i.rs());
  u64 rt_val = reg(i.rt());
  u64 val = rs_val * rt_val;

  hi = val >> 32;
  lo = val;

  return 0;
}

int CPU::ins_xor(const Instruction &i) {
  set_reg(i.rd(), reg(i.rs()) ^ reg(i.rt()));
  return 0;
}

int CPU::ins_break(const Instruction &i) { return exception(Cause::brek); }

int CPU::mult(const Instruction &i) {
  i64 rs_val = static_cast<i32>(reg(i.rs()));
  i64 rt_val = static_cast<i32>(reg(i.rt()));
  u64 val = rs_val * rt_val;

  hi = val >> 32;
  lo = val;

  return 0;
}

int CPU::sub(const Instruction &i) {
  i32 sum;

  if (checked_sum(sum, reg(i.rs()), reg(i.rt()))) {
    return exception(Cause::overflow);
  }

  set_reg(i.rd(), sum);

  return 0;
}

int CPU::xori(const Instruction &i) {
  set_reg(i.rt(), reg(i.rs()) ^ i.imm16());
  return 0;
}

int CPU::lwl(const Instruction &i) {
  u32 unaligned_addr = reg(i.rs()) + i.imm16_se();
  u32 aligned_addr = unaligned_addr & 0b00;

  // bypass load delay restriction. instruction will merge new contents
  // with the value currently being loaded if need be.
  u32 cur_val = out_regs[i.rt()];

  pending_load.reg_index = i.rt();

  u32 aligned_word;
  int status = pci.load32(aligned_word, aligned_addr, clock);
  if (status < 0)
    return status;

  switch (unaligned_addr & 0b11) {
  case 0:
    pending_load.val = (cur_val & 0x00ffffff) | (aligned_word << 24);
    break;
  case 1:
    pending_load.val = (cur_val & 0x0000ffff) | (aligned_word << 16);
    break;
  case 2:
    pending_load.val = (cur_val & 0x000000ff) | (aligned_word << 8);
    break;
  case 3:
    pending_load.val = (cur_val & 0x00000000) | aligned_word;
    break;
  }

  return 0;
}

int CPU::lwr(const Instruction &i) {
  u32 unaligned_addr = reg(i.rs()) + i.imm16_se();
  u32 aligned_addr = unaligned_addr & 0b00;

  // bypass load delay restriction. instruction will merge new contents
  // with the value currently being loaded if need be.
  u32 cur_val = out_regs[i.rt()];

  pending_load.reg_index = i.rt();

  u32 aligned_word;
  int status = pci.load32(aligned_word, aligned_addr, clock);
  if (status < 0)
    return status;

  switch (unaligned_addr & 0b11) {
  case 0:
    pending_load.val = (cur_val & 0x00000000) | aligned_word;
    break;
  case 1:
    pending_load.val = (cur_val & 0xff000000) | (aligned_word >> 8);
    break;
  case 2:
    pending_load.val = (cur_val & 0xffff0000) | (aligned_word >> 16);
    break;
  case 3:
    pending_load.val = (cur_val & 0xffffff00) | (aligned_word >> 24);
    break;
  }

  return 0;
}

int CPU::swl(const Instruction &i) {
  u32 unaligned_addr = reg(i.rs()) + i.imm16_se();
  u32 aligned_addr = unaligned_addr & 0b00;
  u32 cur_reg_val = reg(i.rt());

  u32 cur_mem_val;
  int status = pci.load32(cur_mem_val, aligned_addr, clock);
  if (status < 0)
    return status;

  u32 new_mem_val;
  switch (unaligned_addr & 0b11) {
  case 0:
    new_mem_val = (cur_mem_val & 0xffffff00) | (cur_reg_val >> 24);
    break;
  case 1:
    new_mem_val = (cur_mem_val & 0xffff0000) | (cur_reg_val >> 16);
    break;
  case 2:
    new_mem_val = (cur_mem_val & 0xff000000) | (cur_reg_val >> 8);
    break;
  case 3:
    new_mem_val = (cur_mem_val & 0x00000000) | cur_reg_val;
    break;
  }

  return store32(new_mem_val, unaligned_addr);
}

int CPU::swr(const Instruction &i) {
  u32 unaligned_addr = reg(i.rs()) + i.imm16_se();
  u32 aligned_addr = unaligned_addr & 0b00;
  u32 cur_reg_val = reg(i.rt());

  u32 cur_mem_val;
  int status = pci.load32(cur_mem_val, aligned_addr, clock);
  if (status < 0)
    return status;

  u32 new_mem_val;
  switch (unaligned_addr & 0b11) {
  case 0:
    new_mem_val = (cur_mem_val & 0x00000000) | cur_reg_val;
    break;
  case 1:
    new_mem_val = (cur_mem_val & 0x000000ff) | (cur_reg_val << 8);
    break;
  case 2:
    new_mem_val = (cur_mem_val & 0x0000ffff) | (cur_reg_val << 16);
    break;
  case 3:
    new_mem_val = (cur_mem_val & 0x00ffffff) | (cur_reg_val << 24);
    break;
  }
  
  return store32(new_mem_val, unaligned_addr);
}

int CPU::lwc0(const Instruction &i) {
  return exception(Cause::unimplemented_coprocessor);
}

int CPU::lwc1(const Instruction &i) {
  return exception(Cause::unimplemented_coprocessor);
}

int CPU::lwc2(const Instruction &i) {
  LOG_ERROR("Unhandled GTE LWC: 0x%08x", i.data);
  return -1;
}

int CPU::lwc3(const Instruction &i) {
  return exception(Cause::unimplemented_coprocessor);
}

int CPU::swc0(const Instruction &i) {
  return exception(Cause::unimplemented_coprocessor);
}

int CPU::swc1(const Instruction &i) {
  return exception(Cause::unimplemented_coprocessor);
}

int CPU::swc2(const Instruction &i) {
  LOG_ERROR("Unhandled GTE SWC: 0x%08x", i.data);
  return -1;
}

int CPU::swc3(const Instruction &i) {
  return exception(Cause::unimplemented_coprocessor);
}

