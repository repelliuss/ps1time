#include "cpu.hpp"
#include "data.hpp"
#include "instruction.hpp"
#include "asm.hpp"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <iostream>

void CPU::dump() {
  Instruction ins;

  fetch(ins.data, cur_pc);

  printf("PC: %08x\n", cur_pc);

  for(int i = 0; i < 8; ++i) {
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

bool CPU::cache_isolated() { return (cop0.regs[12] & 0x10000) != 0; }

int CPU::dump_and_next() {
  dump();
  return next();
}

// TODO: remove assertions
int CPU::next() {
  Instruction instruction;
  
  int cpu_fetch_result = fetch(instruction.data, pc);
  assert(cpu_fetch_result == 0);

  in_delay_slot = branch_ocurred;
  branch_ocurred = false;

  // TODO: move this to pc setting instructions, jump and branch
  if(pc % 4 != 0) {
    return exception(Cause::unaligned_load_addr);
  }

  cur_pc = pc;
  pc = next_pc;
  next_pc += 4;

  {
    // absorb pending load, this came out because load delay slot
    set_reg(pending_load.reg_index, pending_load.val);
    pending_load = no_op_load;
  }

  int cpu_exec_result = decode_execute(instruction);
  if (cpu_exec_result) {
    printf("cpu can't exec inst: 0x%08x pc+4: %#08x\n", instruction.data, pc);
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
  case 0b10000:
    return rfe(instruction);
  }

  printf("Illegal instruction %x!\n", instruction.data);
  return exception(Cause::illegal_instruction);
}

int CPU::decode_execute_cop1(const Instruction &instruction) {
  return exception(Cause::unimplemented_coprocessor);
}

int CPU::decode_execute_cop2(const Instruction &instruction) {
  printf("unhandled GTE instruction: %x\n", instruction.data);
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
    sll(instruction);
    return 0;
  case 0x25:
    return ins_or(instruction);
  case 0x2b:
    return sltu(instruction);
  case 0x08:
    return jr(instruction);
  }

  printf("Illegal instruction %x!\n", instruction.data);
  return exception(Cause::illegal_instruction);
}

int CPU::decode_execute(const Instruction &instruction) {
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

  printf("Illegal instruction %x!\n", instruction.data);
  return exception(Cause::illegal_instruction);
}

int CPU::exception(const Cause &cause) {
  u32 handler;

  // NOTE: nocash says BEV=1 exception vectors doesn't happen
  // Exception handler address depends on the 'BEV' bit
  if ((cop0.regs[COP0::index_sr] & (1 << 22)) != 0) {
    handler = 0xbfc00180;
  } else {
    handler = 0x80000080;
  }

  // Shift bits [5:0] of `SR` two places to the left. Those bits
  // are three pairs of Interrupt Enable/User Mode bits behaving
  // like a stack 3 entries deep. Entering an exception pushes a
  // pair of zeroes by left shifting the stack which disables
  // interrupts and puts the CPU in kernel mode. The original
  // third entry is discarded (it's up to the kernel to handle
  // more than two recursive exception levels).
  u32 mode = cop0.regs[COP0::index_sr] & 0x3f;
  cop0.regs[COP0::index_sr] &= ~0x3f;
  cop0.regs[COP0::index_sr] |= (mode << 2) & 0x3f;

  // Update `CAUSE` register with the exception code (bits
  // [6:2])
  cop0.regs[COP0::index_cause] = static_cast<u32>(cause) << 2;

  // Save current instruction address in `EPC`
  cop0.regs[COP0::index_epc] = cur_pc;

  if (in_delay_slot) {
    cop0.regs[COP0::index_epc] -= 4;
    cop0.regs[COP0::index_cause] |= 1 << 31;
  }

  pc = handler;
  next_pc = pc + 4;

  return 0;
}

constexpr u32 CPU::reg(u32 index) { return in_regs[index]; }

constexpr void CPU::set_reg(u32 index, u32 val) { out_regs[index] = val; }

void CPU::lui(const Instruction &i) { set_reg(i.rt(), i.imm16() << 16); }

void CPU::ori(const Instruction &i) {
  set_reg(i.rt(), reg(i.rs()) | i.imm16());
}

static int store32_prohibited(PCIMatch match, u32 offset, u32 val, u32 addr) {
  switch (match) {
  case PCIMatch::gpu:
  case PCIMatch::ram:
    return 0;

  case PCIMatch::irq:
    printf("IRQ control: %x <- %08x\n", offset, val);
    return 1;

  case PCIMatch::cache_ctrl:
    printf("Unhandled write to CACHE_CONTROL: %08x\n", val);
    return 1;

  case PCIMatch::dma:
    switch (offset) {
    case DMA::reg::control:
    case DMA::reg::interrupt:
      return 0;

      // channels
    case DMA::reg::mdecin_base_address:
    case DMA::reg::mdecout_base_address:
    case DMA::reg::gpu_base_address:
    case DMA::reg::cdrom_base_address:
    case DMA::reg::spu_base_address:
    case DMA::reg::pio_base_address:
    case DMA::reg::otc_base_address:

    case DMA::reg::mdecin_block_control:
    case DMA::reg::mdecout_block_control:
    case DMA::reg::gpu_block_control:
    case DMA::reg::cdrom_block_control:
    case DMA::reg::spu_block_control:
    case DMA::reg::pio_block_control:
    case DMA::reg::otc_block_control:

    case DMA::reg::mdecin_channel_control:
    case DMA::reg::mdecout_channel_control:
    case DMA::reg::gpu_channel_control:
    case DMA::reg::cdrom_channel_control:
    case DMA::reg::spu_channel_control:
    case DMA::reg::pio_channel_control:
    case DMA::reg::otc_channel_control:
      return 0;

    default:
      printf("Unhandled DMA write %x: %08x\n", offset, val);
      return -1;
    }

  case PCIMatch::timers:
    printf("Unhandled write to timer register %x: %08x\n", offset, val);
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

  if (addr % 4 != 0) {
    return exception(Cause::unaligned_store_addr);
  }

  PCIMatch match = pci.match(data, offset, addr);

  if (match == PCIMatch::none)
    return -1;

  int status = store32_prohibited(match, offset, value, addr);
  if (status < 0)
    return -1;
  if (status == 1)
    return 0;

  return pci.store32_data(match, data, offset, value);
}

void CPU::sll(const Instruction &i) {
  set_reg(i.rd(), reg(i.rt()) << i.shamt());
}

void CPU::addiu(const Instruction &i) {
  set_reg(i.rt(), reg(i.rs()) + i.imm16_se());
}

void CPU::j(const Instruction &i) {
  next_pc = (pc & 0xf0000000) | (i.imm26() << 2);
  branch_ocurred = false;
}

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
  // REVIEW: volatile may be required
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

// NOTE: LW and LB didn't have cache_isolated check?

// 0 okay, 1 ignored, negative error
static int load32_prohibited(PCIMatch match, u32 offset, u32 addr) {
  switch (match) {
  case PCIMatch::bios:
  case PCIMatch::ram:
  case PCIMatch::gpu:
    return 0;

    // may put info messages to .*_data fns
  case PCIMatch::irq: // NOTE: requires specific value
    printf("IRQ control read %x\n", offset);
    return 0;

  case PCIMatch::timers:
    printf("Unhandled read from timer register %x\n", offset);
    return 0;

  case PCIMatch::dma:
    switch (offset) {
    case DMA::reg::control:
    case DMA::reg::interrupt:

    case DMA::reg::mdecin_base_address:
    case DMA::reg::mdecout_base_address:
    case DMA::reg::gpu_base_address:
    case DMA::reg::cdrom_base_address:
    case DMA::reg::spu_base_address:
    case DMA::reg::pio_base_address:
    case DMA::reg::otc_base_address:

    case DMA::reg::mdecin_block_control:
    case DMA::reg::mdecout_block_control:
    case DMA::reg::gpu_block_control:
    case DMA::reg::cdrom_block_control:
    case DMA::reg::spu_block_control:
    case DMA::reg::pio_block_control:
    case DMA::reg::otc_block_control:

    case DMA::reg::mdecin_channel_control:
    case DMA::reg::mdecout_channel_control:
    case DMA::reg::gpu_channel_control:
    case DMA::reg::cdrom_channel_control:
    case DMA::reg::spu_channel_control:
    case DMA::reg::pio_channel_control:
    case DMA::reg::otc_channel_control:
      return 0;

    default:
      printf("Unhandled DMA read at %x\n", offset);
      return -1;
    }

  default:
    printf("unhandled load32 at address %08x\n", addr);
    return -1;
  }
}

int CPU::lw(const Instruction &i) {
  u8 *data;
  u32 offset;
  u32 addr = reg(i.rs()) + i.imm16_se();

  if (addr % 4 != 0) {
    return exception(Cause::unaligned_load_addr);
  }

  PCIMatch match = pci.match(data, offset, addr);

  if (match == PCIMatch::none)
    return -1;

  int status = load32_prohibited(match, offset, addr);
  if (status < 0)
    return -1;
  if (status == 1)
    return 0;

  pending_load.reg_index = i.rt();
  pending_load.val = pci.load32_data(match, data, offset);

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

static int store16_prohibited(PCIMatch match, u32 offset, u32 addr, u16 val) {
  switch (match) {
  case PCIMatch::ram:
    return 0;

  case PCIMatch::spu:
    printf("Unhandled write to SPU register %08x: %04x\n", addr, val);
    return 1;

  case PCIMatch::timers:
    printf("Unhandled write to timer register %x\n", offset);
    return 1;

  case PCIMatch::irq:
    printf("IRQ control write %x, %04x\n", offset, val);
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

  if (addr % 2 != 0) {
    return exception(Cause::unaligned_store_addr);
  }

  PCIMatch match = pci.match(data, offset, addr);

  if (match == PCIMatch::none)
    return -1;

  int status = store16_prohibited(match, offset, addr, value);
  if (status < 0)
    return -1;
  if (status == 1)
    return 0;

  store16(data, value, offset);

  return 0;
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

static int store8_prohibited(PCIMatch match, u32 offset, u32 addr) {
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

  int status = store8_prohibited(match, offset, addr);
  if (status < 0)
    return -1;
  if (status == 1)
    return 0;

  // NOTE: only ram store8 is valid and it doesn't do conversion

  store8(data, value, offset);
  return 0;
}

int CPU::jr(const Instruction &i) {
  next_pc = reg(i.rs());
  branch_ocurred = true;
  return 0;
}

// NOTE: only used for lbu, may require specific lb
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

// NOTE: only used for lbu, may require specific lb
static u8 load8_data(PCIMatch match, u8 *data, u32 offset) {
  switch (match) {
    // NOTE: lb expansion1 always return all 1s
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
      static_cast<u32>(static_cast<i8>(load8_data(match, data, offset)));

  return 0;
}

int CPU::beq(const Instruction &i) {

  if (reg(i.rs()) == reg(i.rt())) {
    branch(i.imm16_se());
  }

  return 0;
}

int CPU::mfc0(const Instruction &i) {

  u32 cop_r = i.rd();
  u32 val;

  switch (cop_r) {
  case COP0::index_sr:
  case COP0::index_cause:
  case COP0::index_epc:
    pending_load.reg_index = i.rt();
    pending_load.val = cop0.regs[cop_r];
    return 0;
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
  pending_load.val = load8_data(match, data, offset);

  return 0;
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
    printf("Invalid cop0 instruction: %x\n", i.data);
    return -1;
  }

  u32 mode = cop0.regs[COP0::index_sr] & 0x3f;
  cop0.regs[COP0::index_sr] &= ~0x3f;
  cop0.regs[COP0::index_sr] |= mode >> 2;

  return 0;
}

// NOTE: used only for lhu may require specific lh
static int load16_prohibited(PCIMatch match, u32 offset, u32 addr) {
  switch (match) {
  case PCIMatch::ram:
    return 0;
    
  case PCIMatch::spu: // NOTE: requires specific load value
    printf("Unhandled read from SPU register %08x\n", addr);
    return 0;

  case PCIMatch::irq: // REVIEW: requires specific value
    printf("IRQ control read %x\n", offset);
    return 0;

  default:
    printf("unhandled load16 at address %08x\n", addr);
    return -1;
  }
}

static u16 load16_data(PCIMatch match, u8 *data, u32 offset) {
  switch (match) {
    // NOTE: lh spu always return 0
  case PCIMatch::spu:
  case PCIMatch::irq:
    return 0;
  default:
    return load16(data, offset);
  }
}

int CPU::lhu(const Instruction &i) {
  u8 *data;
  u32 offset;
  u32 addr = reg(i.rs()) + i.imm16_se();

  if (addr % 2 != 0) {
    return exception(Cause::unaligned_load_addr);
  }

  PCIMatch match = pci.match(data, offset, addr);

  if (match == PCIMatch::none)
    return -1;

  int status = load16_prohibited(match, offset, addr);
  if (status < 0)
    return -1;
  if (status == 1)
    return 0;

  pending_load.reg_index = i.rt();
  pending_load.val = load16_data(match, data, offset);

  return 0;
}

int CPU::sllv(const Instruction &i) {
  set_reg(i.rd(), reg(i.rt()) << (reg(i.rs()) & 0x1f));
  return 0;
}

// TODO: call lhu and sign extend pending load value?
int CPU::lh(const Instruction &i) {
  u8 *data;
  u32 offset;
  u32 addr = reg(i.rs()) + i.imm16_se();

  if (addr % 2 != 0) {
    return exception(Cause::unaligned_load_addr);
  }

  PCIMatch match = pci.match(data, offset, addr);

  if (match == PCIMatch::none)
    return -1;

  int status = load16_prohibited(match, offset, addr);
  if (status < 0)
    return -1;
  if (status == 1)
    return 0;

  pending_load.reg_index = i.rt();
  pending_load.val = static_cast<i16>(load16_data(match, data, offset));

  return 0;
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
  u8 *data;
  u32 offset;
  u32 unaligned_addr = reg(i.rs()) + i.imm16_se();

  //bypass load delay restriction. instruction will merge new contents
  // with the value currently being loaded if need be.
  u32 cur_val = out_regs[i.rt()];
  
  u32 aligned_addr = unaligned_addr & 0b00;

  PCIMatch match = pci.match(data, offset, aligned_addr);

  if (match == PCIMatch::none)
    return -1;

  int status = load32_prohibited(match, offset, aligned_addr);
  if (status < 0)
    return -1;
  if (status == 1)
    return 0;
  
  u32 aligned_word = pci.load32_data(match, data, offset);

  pending_load.reg_index = i.rt();
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

// TODO: duplicates code with lwl
int CPU::lwr(const Instruction &i) {
  u8 *data;
  u32 offset;
  u32 unaligned_addr = reg(i.rs()) + i.imm16_se();

  // bypass load delay restriction. instruction will merge new contents
  // with the value currently being loaded if need be.
  u32 cur_val = out_regs[i.rt()];

  u32 aligned_addr = unaligned_addr & 0b00;

  PCIMatch match = pci.match(data, offset, aligned_addr);

  if (match == PCIMatch::none)
    return -1;

  int status = load32_prohibited(match, offset, aligned_addr);
  if (status < 0)
    return -1;
  if (status == 1)
    return 0;

  u32 aligned_word = pci.load32_data(match, data, offset);

  pending_load.reg_index = i.rt();
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
  // TODO: duplicate
  if (cache_isolated()) {
    printf("Ignoring store while cache is isolated\n");
    return 0;
  }
  
  u8 *data;
  u32 offset;
  int status;
  u32 unaligned_addr = reg(i.rs()) + i.imm16_se();
  u32 cur_reg_val = reg(i.rt());
  u32 aligned_addr = unaligned_addr & 0b00;
  PCIMatch match = pci.match(data, offset, aligned_addr);

  if (match == PCIMatch::none)
    return -1;

  status = load32_prohibited(match, offset, aligned_addr);
  if (status < 0)
    return -1;
  if (status == 1)
    return 0;

  u32 cur_mem_val = pci.load32_data(match, data, offset);
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

  match = pci.match(data, offset, unaligned_addr);
  if (match == PCIMatch::none)
    return -1;

  status = store32_prohibited(match, offset, new_mem_val, unaligned_addr);
  if (status < 0)
    return -1;
  if (status == 1)
    return 0;

  return pci.store32_data(match, data, offset, new_mem_val);
}

int CPU::swr(const Instruction &i) {
  // TODO: duplicate
  if (cache_isolated()) {
    printf("Ignoring store while cache is isolated\n");
    return 0;
  }
  
  u8 *data;
  u32 offset;
  int status;
  u32 unaligned_addr = reg(i.rs()) + i.imm16_se();
  u32 cur_reg_val = reg(i.rt());
  u32 aligned_addr = unaligned_addr & 0b00;
  PCIMatch match = pci.match(data, offset, aligned_addr);

  if (match == PCIMatch::none)
    return -1;

  status = load32_prohibited(match, offset, aligned_addr);
  if (status < 0)
    return -1;
  if (status == 1)
    return 0;

  u32 cur_mem_val = pci.load32_data(match, data, offset);
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

  match = pci.match(data, offset, unaligned_addr);
  if (match == PCIMatch::none)
    return -1;

  status = store32_prohibited(match, offset, new_mem_val, unaligned_addr);
  if (status < 0)
    return -1;
  if (status == 1)
    return 0;


  return pci.store32_data(match, data, offset, new_mem_val);
}

int CPU::lwc0(const Instruction &i) {
  return exception(Cause::unimplemented_coprocessor);
}

int CPU::lwc1(const Instruction &i) {
  return exception(Cause::unimplemented_coprocessor);
}

int CPU::lwc2(const Instruction &i) {
  printf("unhandled GTE LWC: %x\n", i.data);
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
  printf("unhandled GTE SWC: %x\n", i.data);
  return -1;
}

int CPU::swc3(const Instruction &i) {
  return exception(Cause::unimplemented_coprocessor);
}
