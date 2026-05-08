#pragma once

#include <vector>

#include "Devices/ComputeCard/computecard/board/pins.h"
#include "keymap.h"
#include "types.h"

namespace ComputeCardHAL::Keyboard {

inline const std::vector<int> output_list(Pins::kKeyboardOutputs.begin(), Pins::kKeyboardOutputs.end());
inline const std::vector<int> input_list(Pins::kKeyboardInputs.begin(), Pins::kKeyboardInputs.end());
inline const std::vector<int> led_pin_list(Pins::kKeyboardLedPins.begin(), Pins::kKeyboardLedPins.end());

inline constexpr Chart_t X_map_chart[7] = {
    {1, 0, 1},
    {2, 2, 3},
    {4, 4, 5},
    {8, 6, 7},
    {16, 8, 9},
    {32, 10, 11},
    {64, 12, 13},
};

inline constexpr KeyValue_t key_value_map[4][14] = {
    {{"`", KEY_GRAVE, "~", KEY_GRAVE},
     {"1", KEY_1, "!", KEY_1},
     {"2", KEY_2, "@", KEY_2},
     {"3", KEY_3, "#", KEY_3},
     {"4", KEY_4, "$", KEY_4},
     {"5", KEY_5, "%", KEY_5},
     {"6", KEY_6, "^", KEY_6},
     {"7", KEY_7, "&", KEY_7},
     {"8", KEY_8, "*", KEY_KPASTERISK},
     {"9", KEY_9, "(", KEY_KPLEFTPAREN},
     {"0", KEY_0, ")", KEY_KPRIGHTPAREN},
     {"-", KEY_MINUS, "_", KEY_KPMINUS},
     {"=", KEY_EQUAL, "+", KEY_KPPLUS},
     {"del", KEY_BACKSPACE, "del", KEY_BACKSPACE}},
    {{"tab", KEY_TAB, "tab", KEY_TAB},
     {"q", KEY_Q, "Q", KEY_Q},
     {"w", KEY_W, "W", KEY_W},
     {"e", KEY_E, "E", KEY_E},
     {"r", KEY_R, "R", KEY_R},
     {"t", KEY_T, "T", KEY_T},
     {"y", KEY_Y, "Y", KEY_Y},
     {"u", KEY_U, "U", KEY_U},
     {"i", KEY_I, "I", KEY_I},
     {"o", KEY_O, "O", KEY_O},
     {"p", KEY_P, "P", KEY_P},
     {"[", KEY_LEFTBRACE, "{", KEY_LEFTBRACE},
     {"]", KEY_RIGHTBRACE, "}", KEY_RIGHTBRACE},
     {"\\", KEY_BACKSLASH, "|", KEY_BACKSLASH}},
    {{"fn", 0, "fn", 0},
     {"shift", 0, "shift", 0},
     {"a", KEY_A, "A", KEY_A},
     {"s", KEY_S, "S", KEY_S},
     {"d", KEY_D, "D", KEY_D},
     {"f", KEY_F, "F", KEY_F},
     {"g", KEY_G, "G", KEY_G},
     {"h", KEY_H, "H", KEY_H},
     {"j", KEY_J, "J", KEY_J},
     {"k", KEY_K, "K", KEY_K},
     {"l", KEY_L, "L", KEY_L},
     {";", KEY_SEMICOLON, ":", KEY_SEMICOLON},
     {"'", KEY_APOSTROPHE, "\"", KEY_APOSTROPHE},
     {"enter", KEY_ENTER, "enter", KEY_ENTER}},
    {{"ctrl", KEY_LEFTCTRL, "ctrl", KEY_LEFTCTRL},
     {"opt", 0, "opt", 0},
     {"alt", KEY_LEFTALT, "alt", KEY_LEFTALT},
     {"z", KEY_Z, "Z", KEY_Z},
     {"x", KEY_X, "X", KEY_X},
     {"c", KEY_C, "C", KEY_C},
     {"v", KEY_V, "V", KEY_V},
     {"b", KEY_B, "B", KEY_B},
     {"n", KEY_N, "N", KEY_N},
     {"m", KEY_M, "M", KEY_M},
     {",", KEY_COMMA, "<", KEY_COMMA},
     {".", KEY_DOT, ">", KEY_DOT},
     {"/", KEY_KPSLASH, "?", KEY_KPSLASH},
     {"space", KEY_SPACE, "space", KEY_SPACE}},
};

}  // namespace ComputeCardHAL::Keyboard
