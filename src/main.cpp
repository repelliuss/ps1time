#include "bios.hpp"
#include "cpu.hpp"
#include "data.hpp"

#include <iostream>
#include <filesystem>

int main() {
  Bios bios;

  // TODO: handle fixed path
  if (read_file(bios.data, "res/SCPH1001.BIN", Bios::size)) {
    return -1;
  }

  PCI pci = PCI();
  pci.bios = bios;
  
  CPU cpu = CPU(pci);

  while(!cpu.next());

  return 0;
}
