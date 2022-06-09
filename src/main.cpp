#include "pci.hpp"
#include "cpu.hpp"
#include "data.hpp"
#include "renderer.hpp"
#include "disc.hpp"

#include <SDL_error.h>
#include <SDL_events.h>
#include <SDL_joystick.h>
#include <SDL_gamecontroller.h>
#include <SDL_keycode.h>
#include <iostream>
#include <filesystem>

SDL_GameController* get_controller() {
  int joystick_count = SDL_NumJoysticks();
  if(joystick_count < 0) {
    LOG_ERROR("Can't enumerate joysticks %s", SDL_GetError());
    return nullptr;
  }

  SDL_GameController *controller = nullptr;
  for(int id = 0; id < joystick_count; ++id) {
    if(SDL_IsGameController(id) == SDL_TRUE) {
      LOG_DEBUG("Trying to open controller id %d", id);

      controller = SDL_GameControllerOpen(id);
      if(controller) {
	LOG_INFO("Opened controller %s", SDL_GameControllerNameForIndex(id));
	return controller;
      }

      LOG_ERROR("Couldn't open game controller %s: %s",
                SDL_GameControllerNameForIndex(id), SDL_GetError());
    }
  }

  LOG_INFO("No controller support");

  return nullptr;
}

void handle_keyboard(Profile *pad, SDL_Keycode key, ButtonState state) {
  switch(key) {
  case SDLK_RETURN:
    pad->set_button_state(state, Button::Start);
    break;
  case SDLK_RSHIFT:
    pad->set_button_state(state, Button::Select);
    break;
  case SDLK_UP:
    pad->set_button_state(state, Button::DUp);
    break;
  case SDLK_DOWN:
    pad->set_button_state(state, Button::DDown);
    break;
  case SDLK_LEFT:
    pad->set_button_state(state, Button::DLeft);
    break;
  case SDLK_RIGHT:
    pad->set_button_state(state, Button::DRight);
    break;
  case SDLK_KP_2:
    pad->set_button_state(state, Button::Cross);
    break;
  case SDLK_KP_4:
    pad->set_button_state(state, Button::Square);
    break;
  case SDLK_KP_6:
    pad->set_button_state(state, Button::Circle);
    break;
  case SDLK_KP_8:
    pad->set_button_state(state, Button::Triangle);
    break;
  case SDLK_KP_7:
    pad->set_button_state(state, Button::L1);
    break;
  case SDLK_NUMLOCKCLEAR:
    pad->set_button_state(state, Button::L2);
    break;
  case SDLK_KP_9:
    pad->set_button_state(state, Button::R1);
    break;
  case SDLK_KP_MULTIPLY:
    pad->set_button_state(state, Button::R2);
    break;
  }
}


void handle_controller(Profile *pad, u8 button, ButtonState state) {
  switch (button) {
  case SDL_CONTROLLER_BUTTON_START:
    pad->set_button_state(state, Button::Start);
    break;
  case SDL_CONTROLLER_BUTTON_BACK:
    pad->set_button_state(state, Button::Select);
    break;
  case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
    pad->set_button_state(state, Button::DLeft);
    break;
  case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
    pad->set_button_state(state, Button::DRight);
    break;
  case SDL_CONTROLLER_BUTTON_DPAD_UP:
    pad->set_button_state(state, Button::DUp);
    break;
  case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
    pad->set_button_state(state, Button::DDown);
    break;
  case SDL_CONTROLLER_BUTTON_A:
    pad->set_button_state(state, Button::Cross);
    break;
  case SDL_CONTROLLER_BUTTON_B:
    pad->set_button_state(state, Button::Circle);
    break;
  case SDL_CONTROLLER_BUTTON_X:
    pad->set_button_state(state, Button::Square);
    break;
  case SDL_CONTROLLER_BUTTON_Y:
    pad->set_button_state(state, Button::Triangle);
    break;
  case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
    pad->set_button_state(state, Button::L1);
    break;
  case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
    pad->set_button_state(state, Button::R1);
    break;
  default:
    break;
  };
}

void update_controller_axis(Profile *pad, u8 axis, i16 value) {
  Button button;
  ButtonState state;

  if (axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT) {
    button = Button::L2;
  } else {
    button = Button::R2;
  }

  if (value < 0x4000) {
    state = ButtonState::Released;
  } else {
    state = ButtonState::Pressed;
  }

  pad->set_button_state(state, button);
}

int main() {
  // TODO: handle fixed path
  static constexpr const char *bios_path = "res/bios/SCPH1001.BIN";

  Bios bios;
  if (file::read_file(bios.data, bios_path, Bios::size)) {
    return -1;
  }

  Renderer renderer(true);
  renderer.create_window_and_context();
  if (renderer.compile_shaders_link_program() < 0) {
    return -1;
  }

  renderer.init_buffers();
  if (renderer.has_errors()) {
    return -1;
  }

  SDL_GameController *controller = get_controller();

  std::optional<Disc> disc = from_path("res/bin/bandicoot.bin");
  VideoMode mode = VideoMode::ntsc;

  if(disc) {
    Region region = disc->region;
    LOG_INFO("Disc region %d", region);

    if(region == Region::europe) {
      mode = VideoMode::pal;
    }
  }

  PCI pci(std::move(bios), &renderer, disc, mode);
  CPU cpu = CPU(pci);

  Profile *profile = &cpu.pci.pad_mem_card.pad1.profile;

  int status = 0;
  while (!status) {
    for (int i = 0; i < 1000000 && !status; ++i) {
      status = cpu.next();
    }

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      switch (event.type) {
      case SDL_KEYDOWN:
        if (event.key.keysym.sym == SDLK_ESCAPE) {
          status = 1;
        } else {
          handle_keyboard(profile, event.key.keysym.sym,
                          ButtonState::Pressed);
        }
        break;
      case SDL_KEYUP:
        handle_keyboard(profile, event.key.keysym.sym, ButtonState::Released);
        break;

      case SDL_CONTROLLERBUTTONDOWN:
	handle_controller(profile, event.cbutton.button, ButtonState::Pressed);
	break;

      case SDL_CONTROLLERBUTTONUP:
        handle_controller(profile, event.cbutton.button, ButtonState::Released);
        break;

      case SDL_CONTROLLERAXISMOTION:
	update_controller_axis(profile, event.caxis.axis, event.caxis.value);
	break;

      case SDL_QUIT:
        status = 1;
        break;
      }
    }
  }

  renderer.clean_buffers();
  renderer.clean_program_and_shaders();

  if (controller)
    SDL_GameControllerClose(controller);

  return status;
}
