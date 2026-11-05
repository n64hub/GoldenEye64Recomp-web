#ifndef __ZELDA_CONFIG_H__
#define __ZELDA_CONFIG_H__

#include <cstdint>
#include <string_view>

#include "recomp.h"

namespace zelda64 {
    constexpr std::u8string_view program_id = u8"GoldenRecomp";
    constexpr std::string_view program_name = "Goldeneye 007: Recompiled";

    // The settings (config.cpp): RecompFrontend's General, Controls, Graphics and Sound tabs with this game's own
    // options added. Loads them from the app folder, so it comes after the default bindings (init_input).
    void init_config();

    // Input (input.cpp): the default bindings, the pad the game reads, and the mouse as the game's patches take it.
    void init_input();
    bool get_n64_input(int controller_num, uint16_t* buttons_out, float* x_out, float* y_out);
    // Whether the mouse was used in the game more recently than a controller.
    bool mouse_in_use();
    // The mouse's motion since the patches last took it, scaled by Mouse Sensitivity.
    void get_mouse_deltas(float* x, float* y);
    // The wheel's notches since the last call, up positive.
    int take_mouse_wheel();

    // Mouse (mouse.cpp): the mouse's buttons and the tank's turning, applied to a pad the game reads.
    void apply_mouse_input(int controller_num, bool ext, uint16_t* buttons, float* x);
    bool get_mouse_invert_y();

    // The Ext control style (ext_controls.cpp): whether it plays, a pad the game reads while it does, from the pad and
    // the keyboard's N64 buttons and stick (keys, key_x, key_y), and the pad's presses of the actions N64 pads lack.
    bool ext_playing();
    void ext_input(int controller_num, uint16_t keys, float key_x, float key_y, uint16_t* buttons, float* x, float* y);
    uint32_t ext_take_actions();

    // The game entry's on_init_callback: turns the stored retail ROM into the TLBFREE ROM the code was recompiled
    // from (rom_conversion.cpp).
    void convert_rom(uint8_t* rdram, recomp_context* ctx);
};

#endif
