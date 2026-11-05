// Input, on RecompFrontend's recompinput: it reads the keyboard and controllers, keeps the bindings (the Controls tab,
// controls.json) and locks the pointer to the game while Mouse Sensitivity is above zero. This adds the game's side:
// - the default bindings, as the PC ports have them for the keyboard (with the mouse turning Bond and aiming,
//   mouse.cpp), and for a controller;
// - the pad the game reads: the bound buttons and stick, the mouse's buttons, the stick at the N64's range (0.65 of a
//   full deflection), and the test harness's buttons; while the Ext style plays, the two pads ext_controls.cpp makes
//   of a modern pad and the keyboard in their place;
// - the mouse as the patches take it: its motion and wheel, gathered from every event until the game takes them, and
//   whether it was used more recently than a controller (mouse_in_use), which says whether the mouse plays.

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <mutex>

#include "SDL.h"

#include "recompinput/recompinput.h"
#include "recompinput/input_state.h"
#include "recompinput/input_mapping.h"
#include "recompinput/profiles.h"
#include "recompui/config.h"
#include "zelda_config.h"

namespace {
    using recompinput::GameInput;
    using recompinput::InputField;

    std::mutex mouse_mutex;
    std::array<float, 2> pending_mouse_delta{};
    std::chrono::steady_clock::time_point last_mouse_delta_take{};
    std::atomic_int32_t mouse_wheel_pos = 0;
    // Whether the mouse was used in the game more recently than a controller.
    std::atomic_bool mouse_used = false;

    // Watches SDL's events as they arrive, beside RecompFrontend's own handling of them.
    int watch_events(void*, SDL_Event* event) {
        switch (event->type) {
        case SDL_MOUSEMOTION:
            if (!recompinput::game_input_disabled()) {
                std::lock_guard lock{ mouse_mutex };
                pending_mouse_delta[0] += event->motion.xrel;
                pending_mouse_delta[1] += event->motion.yrel;
                mouse_used = true;
            }
            break;
        case SDL_MOUSEWHEEL:
            if (!recompinput::game_input_disabled()) {
                mouse_wheel_pos.fetch_add(event->wheel.y * (event->wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -1 : 1));
                mouse_used = true;
            }
            break;
        case SDL_MOUSEBUTTONDOWN:
            if (!recompinput::game_input_disabled()) {
                mouse_used = true;
            }
            break;
        case SDL_CONTROLLERBUTTONDOWN:
            mouse_used = false;
            break;
        case SDL_CONTROLLERAXISMOTION:
            if (std::abs(event->caxis.value * (1 / 32768.0f)) > recompinput::axis_digital_threshold) {
                mouse_used = false;
            }
            break;
        default:
            break;
        }
        return 0;
    }

    void set_keyboard(GameInput input, std::initializer_list<SDL_Scancode> keys) {
        std::vector<InputField> fields;
        for (SDL_Scancode key : keys) {
            fields.push_back(InputField::keyboard(key));
        }
        recompinput::set_default_mapping_for_keyboard(input, fields);
    }

    void set_controller(GameInput input, std::vector<InputField> fields) {
        recompinput::set_default_mapping_for_controller(input, fields);
    }

    InputField button(SDL_GameControllerButton button) {
        return InputField::controller_digital(button);
    }

    InputField axis(SDL_GameControllerAxis axis, bool positive) {
        return InputField::controller_analog(axis, positive);
    }

    using Bindings = std::array<InputField, recompinput::num_bindings_per_input>;

    Bindings get_bindings(int profile_index, GameInput input) {
        Bindings bindings{};
        if (profile_index >= 0) {
            for (size_t i = 0; i < bindings.size(); i++) {
                bindings[i] = recompinput::profiles::get_input_binding(profile_index, input, i);
            }
        }
        return bindings;
    }

    bool get_digital(int profile_index, GameInput input) {
        Bindings bindings = get_bindings(profile_index, input);
        return recompinput::get_input_digital(0, bindings);
    }

    float get_analog(int profile_index, GameInput input) {
        Bindings bindings = get_bindings(profile_index, input);
        return recompinput::get_input_analog(0, bindings);
    }

    // The N64's buttons, in GameInput's order from N64_BUTTON_START.
    constexpr std::array<uint16_t, 14> n64_button_values = {
        0x8000, // A
        0x4000, // B
        0x2000, // Z
        0x0020, // L
        0x0010, // R
        0x1000, // Start
        0x0008, // C Up
        0x0004, // C Down
        0x0002, // C Left
        0x0001, // C Right
        0x0800, // D-Pad Up
        0x0400, // D-Pad Down
        0x0200, // D-Pad Left
        0x0100, // D-Pad Right
    };
}

void zelda64::init_input() {
    using namespace recompinput;

    // As the PC ports have it, with the mouse turning Bond and aiming (src/game/mouse.cpp): W and S walk, A and D strafe
    // (C-left and C-right), the arrows turn and look, F fires as left click does, Left Shift is R (the game's aim mode),
    // Space or Q is A (next weapon), E is B (use), X is L, and Return or Tab is Start (the watch).
    set_keyboard(GameInput::A, { SDL_SCANCODE_SPACE, SDL_SCANCODE_Q });
    set_keyboard(GameInput::B, { SDL_SCANCODE_E });
    set_keyboard(GameInput::L, { SDL_SCANCODE_X });
    set_keyboard(GameInput::R, { SDL_SCANCODE_LSHIFT });
    set_keyboard(GameInput::Z, { SDL_SCANCODE_F });
    set_keyboard(GameInput::START, { SDL_SCANCODE_RETURN, SDL_SCANCODE_TAB });
    set_keyboard(GameInput::C_LEFT, { SDL_SCANCODE_A });
    set_keyboard(GameInput::C_RIGHT, { SDL_SCANCODE_D });
    set_keyboard(GameInput::C_UP, { SDL_SCANCODE_UP });
    set_keyboard(GameInput::C_DOWN, { SDL_SCANCODE_DOWN });
    set_keyboard(GameInput::DPAD_LEFT, { SDL_SCANCODE_J });
    set_keyboard(GameInput::DPAD_RIGHT, { SDL_SCANCODE_L });
    set_keyboard(GameInput::DPAD_UP, { SDL_SCANCODE_I });
    set_keyboard(GameInput::DPAD_DOWN, { SDL_SCANCODE_K });
    set_keyboard(GameInput::X_AXIS_NEG, { SDL_SCANCODE_LEFT });
    set_keyboard(GameInput::X_AXIS_POS, { SDL_SCANCODE_RIGHT });
    set_keyboard(GameInput::Y_AXIS_POS, { SDL_SCANCODE_W });
    set_keyboard(GameInput::Y_AXIS_NEG, { SDL_SCANCODE_S });
    set_keyboard(GameInput::TOGGLE_MENU, { SDL_SCANCODE_ESCAPE });
    set_keyboard(GameInput::ACCEPT_MENU, { SDL_SCANCODE_RETURN });
    set_keyboard(GameInput::APPLY_MENU, { SDL_SCANCODE_F });
    set_keyboard(GameInput::RELOAD, { SDL_SCANCODE_R });
    set_keyboard(GameInput::CROUCH, { SDL_SCANCODE_LCTRL, SDL_SCANCODE_C });

    set_controller(GameInput::A, { button(SDL_CONTROLLER_BUTTON_SOUTH) });
    set_controller(GameInput::B, { button(SDL_CONTROLLER_BUTTON_WEST) });
    set_controller(GameInput::L, { button(SDL_CONTROLLER_BUTTON_LEFTSTICK) });
    set_controller(GameInput::R, { button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) });
    set_controller(GameInput::Z, { button(SDL_CONTROLLER_BUTTON_LEFTSHOULDER) });
    set_controller(GameInput::START, { button(SDL_CONTROLLER_BUTTON_START) });
    set_controller(GameInput::C_LEFT, { axis(SDL_CONTROLLER_AXIS_RIGHTX, false), button(SDL_CONTROLLER_BUTTON_NORTH) });
    set_controller(GameInput::C_RIGHT, { axis(SDL_CONTROLLER_AXIS_RIGHTX, true), button(SDL_CONTROLLER_BUTTON_EAST) });
    set_controller(GameInput::C_UP, { axis(SDL_CONTROLLER_AXIS_RIGHTY, false), axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT, true) });
    set_controller(GameInput::C_DOWN, { axis(SDL_CONTROLLER_AXIS_RIGHTY, true), axis(SDL_CONTROLLER_AXIS_TRIGGERLEFT, true) });
    set_controller(GameInput::DPAD_LEFT, { button(SDL_CONTROLLER_BUTTON_DPAD_LEFT) });
    set_controller(GameInput::DPAD_RIGHT, { button(SDL_CONTROLLER_BUTTON_DPAD_RIGHT) });
    set_controller(GameInput::DPAD_UP, { button(SDL_CONTROLLER_BUTTON_DPAD_UP) });
    set_controller(GameInput::DPAD_DOWN, { button(SDL_CONTROLLER_BUTTON_DPAD_DOWN) });
    set_controller(GameInput::X_AXIS_NEG, { axis(SDL_CONTROLLER_AXIS_LEFTX, false) });
    set_controller(GameInput::X_AXIS_POS, { axis(SDL_CONTROLLER_AXIS_LEFTX, true) });
    set_controller(GameInput::Y_AXIS_POS, { axis(SDL_CONTROLLER_AXIS_LEFTY, false) });
    set_controller(GameInput::Y_AXIS_NEG, { axis(SDL_CONTROLLER_AXIS_LEFTY, true) });
    set_controller(GameInput::TOGGLE_MENU, { button(SDL_CONTROLLER_BUTTON_BACK) });
    set_controller(GameInput::ACCEPT_MENU, { button(SDL_CONTROLLER_BUTTON_SOUTH) });
    set_controller(GameInput::APPLY_MENU, { button(SDL_CONTROLLER_BUTTON_WEST), button(SDL_CONTROLLER_BUTTON_START) });
    set_controller(GameInput::RELOAD, {});
    set_controller(GameInput::CROUCH, {});

    set_game_input_description(GameInput::RELOAD, "Reloads the weapon in hand.");
    set_game_input_description(GameInput::CROUCH, "Crouches, or stands back up.");

    SDL_AddEventWatch(watch_events, nullptr);
}

bool zelda64::get_n64_input(int controller_num, uint16_t* buttons_out, float* x_out, float* y_out) {
    uint16_t cur_buttons = 0;
    float cur_x = 0.0f;
    float cur_y = 0.0f;

    // The first pad, and the second while Ext plays (ext_controls.cpp), which reads none otherwise.
    if (controller_num != 0 && controller_num != 1) {
        return false;
    }
    bool ext = zelda64::ext_playing();

    if (!recompinput::game_input_disabled() && (controller_num == 0 || ext)) {
        int controller_profile = recompinput::profiles::get_sp_controller_profile_index();
        int keyboard_profile = recompinput::profiles::get_sp_keyboard_profile_index();

        uint16_t keys = 0;
        for (size_t i = 0; i < n64_button_values.size(); i++) {
            GameInput input = static_cast<GameInput>((size_t)GameInput::N64_BUTTON_START + i);
            keys |= get_digital(keyboard_profile, input) ? n64_button_values[i] : 0;
        }
        float key_x = get_analog(keyboard_profile, GameInput::X_AXIS_POS) - get_analog(keyboard_profile, GameInput::X_AXIS_NEG);
        float key_y = get_analog(keyboard_profile, GameInput::Y_AXIS_POS) - get_analog(keyboard_profile, GameInput::Y_AXIS_NEG);

        if (ext) {
            zelda64::ext_input(controller_num, keys, key_x, key_y, &cur_buttons, &cur_x, &cur_y);
        } else {
            cur_buttons = keys;
            for (size_t i = 0; i < n64_button_values.size(); i++) {
                GameInput input = static_cast<GameInput>((size_t)GameInput::N64_BUTTON_START + i);
                cur_buttons |= get_digital(controller_profile, input) ? n64_button_values[i] : 0;
            }

            float joystick_x = get_analog(controller_profile, GameInput::X_AXIS_POS) - get_analog(controller_profile, GameInput::X_AXIS_NEG);
            float joystick_y = get_analog(controller_profile, GameInput::Y_AXIS_POS) - get_analog(controller_profile, GameInput::Y_AXIS_NEG);
            recompinput::apply_joystick_deadzone(joystick_x, joystick_y, &joystick_x, &joystick_y);

            cur_x = key_x + joystick_x;
            cur_y = key_y + joystick_y;
        }

        zelda64::apply_mouse_input(controller_num, ext, &cur_buttons, &cur_x);
    }

    // Test harness (tools/harness.sh): GE_HARNESS_BUTTONS=<N64 button mask> holds those buttons, e.g. 0x0010 = R.
    static const uint16_t harness_buttons = [] {
        const char* value = getenv("GE_HARNESS_BUTTONS");
        return value != nullptr ? (uint16_t) strtoul(value, nullptr, 0) : (uint16_t) 0;
    }();
    if (controller_num == 0) {
        cur_buttons |= harness_buttons;
    }

    *buttons_out = cur_buttons;
    *x_out = std::clamp(cur_x * 0.65f, -1.0f, 1.0f);
    *y_out = std::clamp(cur_y * 0.65f, -1.0f, 1.0f);

    return true;
}

bool zelda64::mouse_in_use() {
    return mouse_used;
}

void zelda64::get_mouse_deltas(float* x, float* y) {
    // Motion the game hasn't taken for this long was made while Bond wasn't the game's to move (a cutscene, the watch,
    // a pause), and would turn him all at once after it.
    constexpr auto stale_mouse_delta = std::chrono::milliseconds(100);
    std::array<float, 2> cur_mouse_delta{};
    {
        std::lock_guard lock{ mouse_mutex };
        auto now = std::chrono::steady_clock::now();
        if (now - last_mouse_delta_take < stale_mouse_delta) {
            cur_mouse_delta = pending_mouse_delta;
        }
        pending_mouse_delta = { 0.0f, 0.0f };
        last_mouse_delta_take = now;
    }
    float sensitivity = (float)recompui::config::general::get_mouse_sensitivity() / 100.0f;
    *x = cur_mouse_delta[0] * sensitivity;
    *y = cur_mouse_delta[1] * sensitivity;
}

int zelda64::take_mouse_wheel() {
    return mouse_wheel_pos.exchange(0);
}
