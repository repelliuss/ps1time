#include "bios.hpp"
#include "cpu.hpp"
#include "data.hpp"

#include <iostream>
#include <filesystem>

// TODO: printfs to log functions, errors to stderr
// TODO: refactor non-const specified const local variables to be const

int main() {
  Bios bios;

  // TODO: handle fixed path
  if (read_file(bios.data, "res/SCPH1001.BIN", Bios::size)) {
    return -1;
  }

  PCI pci = PCI();
  pci.bios = bios;
  
  CPU cpu = CPU(pci);

  // u64 i = 0;

  while(!cpu.next()) {
    // ++i;
  }

  cpu.dump();

  // printf("%lu", i);

   // NOTE: good instructions count: 20062004

  return 0;
}
