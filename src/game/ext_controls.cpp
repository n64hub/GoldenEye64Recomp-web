// Ext, the control style a modern pad plays as the PC ports play one: the program's side (the game's is
// patches/ext_controls.c, which plays Ext as the game's 2.4, two N64 pads). While the game says Ext plays (the player
// walks in a level: recomp_ext_playing, once a tick; once it stops saying so for 100 ms, as behind the watch, in a
// cutscene or the menus, everything is as before), the game reads two pads:
//  - the first: the pad's right stick, so it turns and looks as 2.4's first stick, with the keyboard's left and right
//    (the arrows) and its C-up and C-down (the up and down arrows) on it, as 1.1 has them turn and look, up looking up
//    with the game's Look Up/Down option as it comes (below); LT (and the keyboard's R, Left Shift) as Z, which aims;
//    A and RB as B, use; LB as A, the game's weapon button; and Start;
//  - the second: the pad's left stick, so it walks and strafes as 2.4's second stick, with the keyboard's stick up and
//    down (W and S) and its C-left and C-right (A and D) on it, as 1.1 has them walk and strafe; RT, the keyboard's Z
//    (F) and the mouse's left click as Z, which fires.
// The pad's buttons are read whatever the Controls tab binds it to (which the menus keep), and the keyboard by its
// bindings. X, Y, B and a click of the left stick, which N64 pads don't have, are reload, the next weapon, the previous
// one and crouch, the actions patches/mouse.c performs (take_actions). The mouse turns and aims as in every style.

#include <algorithm>
#include <array>
#include <chrono>
#include <mutex>

#include "SDL.h"

#include "recomp.h"
#include "librecomp/helpers.hpp"
#include "recompinput/recompinput.h"
#include "recompinput/input_state.h"
#include "recompinput/input_mapping.h"
#include "recompinput/profiles.h"
#include "zelda_config.h"

namespace {
    using recompinput::GameInput;
    using recompinput::InputField;

    constexpr auto stale = std::chrono::milliseconds(100);

    // recomp_get_mouse_actions' bits, as patches/mouse.c has them.
    constexpr uint32_t action_reload = 0x1;
    constexpr uint32_t action_crouch = 0x2;
    constexpr uint32_t action_next_weapon = 0x4;
    constexpr uint32_t action_prev_weapon = 0x8;

    constexpr uint16_t n64_a = 0x8000;
    constexpr uint16_t n64_b = 0x4000;
    constexpr uint16_t n64_z = 0x2000;
    constexpr uint16_t n64_l = 0x0020;
    constexpr uint16_t n64_r = 0x0010;
    constexpr uint16_t n64_start = 0x1000;
    constexpr uint16_t n64_c_up = 0x0008;
    constexpr uint16_t n64_c_down = 0x0004;
    constexpr uint16_t n64_c_left = 0x0002;
    constexpr uint16_t n64_c_right = 0x0001;
    constexpr uint16_t n64_dpad = 0x0F00;

    std::mutex ext_mutex;
    std::chrono::steady_clock::time_point playing_until{};
    uint32_t actions_held = 0;

    bool pad(SDL_GameControllerButton button) {
        return recompinput::get_input_digital(0, InputField::controller_digital(button));
    }

    bool trigger(SDL_GameControllerAxis axis) {
        return recompinput::get_input_digital(0, InputField::controller_analog(axis, true));
    }

    // The pad's left stick, right and up positive, past the frontend's deadzone.
    void left_stick(float* x, float* y) {
        using recompinput::get_input_analog;
        float raw_x = get_input_analog(0, InputField::controller_analog(SDL_CONTROLLER_AXIS_LEFTX, true)) -
            get_input_analog(0, InputField::controller_analog(SDL_CONTROLLER_AXIS_LEFTX, false));
        float raw_y = get_input_analog(0, InputField::controller_analog(SDL_CONTROLLER_AXIS_LEFTY, false)) -
            get_input_analog(0, InputField::controller_analog(SDL_CONTROLLER_AXIS_LEFTY, true));
        recompinput::apply_joystick_deadzone(raw_x, raw_y, x, y);
    }

    float key(uint16_t keys, uint16_t positive, uint16_t negative) {
        return ((keys & positive) ? 1.0f : 0.0f) - ((keys & negative) ? 1.0f : 0.0f);
    }
}

bool zelda64::ext_playing() {
    std::lock_guard lock{ ext_mutex };
    return std::chrono::steady_clock::now() < playing_until;
}

void zelda64::ext_input(int controller_num, uint16_t keys, float key_x, float key_y, uint16_t* buttons, float* x,
                        float* y) {
    *buttons = 0;
    *x = 0.0f;
    *y = 0.0f;
    if (recompinput::game_input_disabled()) {
        return;
    }

    if (controller_num == 0) {
        float right_x, right_y;
        // SDL's down is positive, and so is the game's here: its Look Up/Down option, as it comes, has a stick pushed up
        // look down, as a plane's stick does, which the option's other setting turns round. So the stick and the keys
        // go to the game upside down, and look up as the PC ports' do, unless the option says otherwise.
        recompinput::get_right_analog(0, &right_x, &right_y);
        *x = right_x + key_x;
        *y = right_y - key(keys, n64_c_up, n64_c_down);
        *buttons = (keys & (n64_a | n64_b | n64_l | n64_start | n64_dpad)) |
            ((keys & n64_r) || trigger(SDL_CONTROLLER_AXIS_TRIGGERLEFT) ? n64_z : 0) |
            (pad(recompinput::SDL_CONTROLLER_BUTTON_SOUTH) || pad(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) ? n64_b : 0) |
            (pad(SDL_CONTROLLER_BUTTON_LEFTSHOULDER) ? n64_a : 0) |
            (pad(SDL_CONTROLLER_BUTTON_START) ? n64_start : 0);
    } else {
        float left_x, left_y;
        left_stick(&left_x, &left_y);
        *x = left_x + key(keys, n64_c_right, n64_c_left);
        *y = left_y + key_y;
        *buttons = (keys & n64_z) || trigger(SDL_CONTROLLER_AXIS_TRIGGERRIGHT) ? n64_z : 0;
    }
}

uint32_t zelda64::ext_take_actions() {
    uint32_t held =
        (pad(recompinput::SDL_CONTROLLER_BUTTON_WEST) ? action_reload : 0) |
        (pad(recompinput::SDL_CONTROLLER_BUTTON_NORTH) ? action_next_weapon : 0) |
        (pad(recompinput::SDL_CONTROLLER_BUTTON_EAST) ? action_prev_weapon : 0) |
        (pad(SDL_CONTROLLER_BUTTON_LEFTSTICK) ? action_crouch : 0);
    if (recompinput::game_input_disabled() || !ext_playing()) {
        held = 0;
    }
    std::lock_guard lock{ ext_mutex };
    uint32_t pressed = held & ~actions_held;
    actions_held = held;
    return pressed;
}

// recomp_ext_playing(s32 playing): whether Ext plays from now, said each tick the game reads Bond's controls.
extern "C" void recomp_ext_playing(uint8_t* rdram, recomp_context* ctx) {
    bool playing = _arg<0, int32_t>(rdram, ctx) != 0;
    std::lock_guard lock{ ext_mutex };
    playing_until = playing ? std::chrono::steady_clock::now() + stale : std::chrono::steady_clock::time_point{};
}
