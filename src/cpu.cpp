#include "cpu.hpp"
#include "data.hpp"
#include "instruction.hpp"

#include <cassert>
#include <cstdio>
#include <iostream>

int CPU::next() {
  // TODO: remove this kind of initialization style
  Instruction instruction = {fetch(pc)}; // TODO: use pc internally

  if(decode_execute(instruction)) return -1;

  pc += 4;

  return 0;
}

u32 CPU::fetch(u32 addr) {
  u8* data;
  u32 offset;
   
  if(pci.match(data, offset, addr) != PCIMatch::none) {
    return load32(data, offset);
  }

  // FIXME: negative value when unsigned return
  return -1; // REVIEW: what to return? or exception here?
}

int CPU::decode_execute(Instruction instruction) {
  switch(instruction.opcode()) {
  case 0b001111:
    lui(instruction);
    break;
  case 0b001101:
    ori(instruction);
    break;
  case 0b101011:
    return sw(instruction);
  }

  return 0;
}

void CPU::lui(Instruction i) { regs[i.rt()] = i.imm16() << 16; }

// TODO: ori may require zero extended immmediate
void CPU::ori(Instruction i) { regs[i.rt()] = regs[i.rs()] | i.imm16(); }

// TODO: should I quit at errors?
// TODO: change offset to index as data is separated
int CPU::sw(Instruction i) {
  u8* data;
  u32 offset;
  u32 addr = regs[i.rs()] + i.imm16_se();
  u32 value = regs[i.rt()];
  PCIMatch match = pci.match(data, offset, addr);
  
  if(match == PCIMatch::none) return -1; 
  if(pci.prohibited(match, offset, value)) return -1;
  
  store32(data, value, offset);

  return 0;
}
