#include "bios.hpp"
#include "cpu.hpp"
#include "data.hpp"

#include <iostream>
#include <filesystem>

int main() {
  Bios bios;

  // TODO: handle fixed path
  if(read_file(bios.data, "res/SCPH1001.BIN", Bios::size)) {
    return -1;
  }

  PCI pci = {.bios =  bios};
  CPU cpu = {.pci = pci};

  printf("pc: %#x\n", cpu.pc);

  cpu.next();
  cpu.next();
  cpu.next();

  printf("%#x\n", cpu.regs[1]);
  
  return 0;
}


