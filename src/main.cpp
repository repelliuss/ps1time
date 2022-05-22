#include "pci.hpp"
#include "cpu.hpp"
#include "data.hpp"
#include "renderer.hpp"

#include <SDL_events.h>
#include <iostream>
#include <filesystem>

int main() {
  // TODO: handle fixed path
  static constexpr const char *bios_path = "res/bios/SCPH1001.BIN";
  
  Bios bios;
  if (file::read_file(bios.data, bios_path, Bios::size)) {
    return -1;
  }

  Renderer renderer(true);
  renderer.create_window_and_context();
  if(renderer.compile_shaders_link_program() < 0) {
    return -1;
  }

  renderer.init_buffers();
  if (renderer.has_errors()) {
    return -1;
  }

  PCI pci(std::move(bios), &renderer);
  CPU cpu = CPU(pci);

  int status = 0;
  while(!status) {
    for(int i = 0; i < 1000000 && !status; ++i) {
      status = cpu.next();
    }

    SDL_Event event;
    while(SDL_PollEvent(&event)) {
      switch(event.type) {
      case SDL_KEYDOWN:
        if (event.key.keysym.sym == SDLK_ESCAPE) {
          status = 1;
        }
        break;
      case SDL_QUIT:
        status = 1;
        break;
      }
    }
  }

  renderer.clean_buffers();
  renderer.clean_program_and_shaders();

  return status;
}
