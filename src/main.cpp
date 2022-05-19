#include "bios.hpp"
#include "cpu.hpp"
#include "data.hpp"
#include "renderer.hpp"

#include <SDL_events.h>
#include <iostream>
#include <filesystem>

// TODO: printfs to log functions, errors to stderr
// TODO: refactor non-const specified const local variables to be const

int main() {
  Bios bios;

  // TODO: handle fixed path
  if (read_file(bios.data, "res/bios/SCPH1001.BIN", Bios::size)) {
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

  PCI pci = PCI();
  pci.bios = bios;
  pci.gpu.renderer = &renderer;
  
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

  // printf("%lu", i);

  renderer.clean_buffers();
  renderer.clean_program_and_shaders();

  return status;
}
