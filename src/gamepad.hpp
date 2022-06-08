#pragma once

#include "types.hpp"

/// GamePad types supported by the emulator
enum GamePadType {
  /// No gamepad connected
  Disconnected,
  /// SCPH-1080: original gamepad without analog sticks
  Digital,
};

enum Button {
  Select = 0,
  Start = 3,
  DUp = 4,
  DRight = 5,
  DDown = 6,
  DLeft = 7,
  L2 = 8,
  R2 = 9,
  L1 = 10,
  R1 = 11,
  Triangle = 12,
  Circle = 13,
  Cross = 14,
  Square = 15,
};

enum ButtonState {
  Pressed,
  Released,
};

struct Pair {
  u8 response;
  bool dsr;
};

struct Profile {
  u16 value;
  Pair (*fn_handle_command)(u16 &self, u8 seq, u8 cmd);
  void (*fn_set_button_state)(u16 &self, ButtonState, Button);

  Pair handle_command(u8 seq, u8 cmd) {
    return fn_handle_command(value, seq, cmd);
  }

  void set_button_state(ButtonState state, Button button) {
    return fn_set_button_state(value, state, button);
  }
};

constexpr Pair disconnected_handle_command(u16 &, u8 seq, u8 cmd) {
  return {0xff, false};
}

constexpr void disconnected_set_button_state(u16 &, ButtonState _, Button __) {}

constexpr Profile disconnected_profile = {
    .value = 0,
    .fn_handle_command = disconnected_handle_command,
    .fn_set_button_state = disconnected_set_button_state,
};

constexpr Pair digital_handle_command(u16 &self, u8 seq, u8 cmd) {
  switch (seq) {
    // First byte should be 0x01 if the command targets
    // the controller
  case 0:
    return {0xff, (cmd == 0x01)};

    // Digital gamepad only supports command 0x42: read
    // buttons.
    // Response 0x41: we're a digital PSX controller
  case 1:
    return {0x41, (cmd == 0x42)};

    // From then on the command byte is ignored.
    // Response 0x5a: 2nd controller ID byte
  case 2:
    return {0x5a, true};

    // First button state byte: direction cross, start and
    // select.
  case 3:
    return {static_cast<u8>(self), true};

    // 2nd button state byte: shoulder buttons and "shape"
    // buttons. We don't asert DSR for the last byte.
  case 4:
    return {static_cast<u8>((self >> 8)), false};

    // Shouldn't be reached
  default:
    return {0xff, false};
  }
}

constexpr void digital_set_button_state(u16 &self, ButtonState state,
                                        Button button) {
  u16 s = self;
  u16 mask = 1 << (button);

  switch (state) {
  case ButtonState::Pressed:
    self = s & ~mask;
    break;

  case ButtonState::Released:
    self = s | mask;
    break;
  }
}

constexpr Profile digital_profile = {
    .value = 0xffff,
    .fn_handle_command = digital_handle_command,
    .fn_set_button_state = digital_set_button_state,
};

struct GamePad {
  /// Gamepad profile
  Profile profile;
  /// Counter keeping track of the current position in the reply
  /// sequence
  u8 seq = 0;
  /// False if the pad is done processing the current command
  bool active = true;

  GamePad(GamePadType type) {
    switch (type) {
    case GamePadType::Disconnected:
      profile = disconnected_profile;
      break;

    case GamePadType::Digital:
      profile = digital_profile;
      break;
    }
  }

  /// Called when the "select" line goes down.
  void select() {
    // Prepare for incomming command
    active = true;
    seq = 0;
  }

  /// The first return value is true if the gamepad issues a DSR
  /// pulse after the byte is read to notify the controller that
  /// more data can be read. The 2nd return value is the response
  /// byte.
  Pair send_command(u8 cmd) {
    if (!active) {
      return {0xff, false};
    }

    Pair p = profile.handle_command(seq, cmd);

    // If we're not asserting DSR it either means that we've
    // encountered an error or that we have nothing else to
    // reply. In either case we won't be handling any more command
    // bytes in this transaction.
    active = p.dsr;

    seq += 1;

    return p;
  }

  /// Return a mutable reference to the underlying gamepad Profile
  Profile *get_profile() { return &profile; }
};
