/**
 * @file keyboard.h
 * @author Forairaaaaa
 * @brief 
 * @version 0.1
 * @date 2023-09-22
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#pragma once
#include <iostream>
#include <vector>
#include <cstring>
#include "keymap.h"
#if defined(ESP_PLATFORM)
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#else
using SemaphoreHandle_t = void*;
using TaskHandle_t = void*;
#endif


namespace KEYBOARD 
{
    struct Chart_t 
    {
        uint8_t value;
        uint8_t x_1;
        uint8_t x_2;
    };

    struct Point2D_t 
    {
        int x;
        int y;
    };

    const std::vector<int> output_list = {8, 9, 11};
    const std::vector<int> input_list = {13, 15, 3, 4, 5, 6, 7};
    const std::vector<int> led_pin_list = {21, 48, 47}; //LED_EN, LED_CLK, LED_DAT

    static constexpr uint8_t LED_BRIGHTNESS_MAX = 15;

    const Chart_t X_map_chart[7] = 
    {
        {1, 0, 1},
        {2, 2, 3},
        {4, 4, 5},
        {8, 6, 7},
        {16, 8, 9},
        {32, 10, 11},
        {64, 12, 13}
    };

    struct KeyValue_t 
    {
        const char* value_first;
        const int value_num_first;
        const char* value_second;
        const int value_num_second;
    };

    const KeyValue_t _key_value_map[4][14] = {
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
     {"space", KEY_SPACE, "space", KEY_SPACE}}};

    class Keyboard
    {
        public:
            
            struct KeysState
            {
                bool tab = false;
                bool fn = false;
                bool shift = false;
                bool ctrl = false;
                bool opt = false;
                bool alt = false;
                bool del = false;
                bool enter = false;
                bool space  = false;

                std::vector<char> values;
                std::vector<int> hidKey;

                void reset()
                {
                    tab = false;
                    fn = false;
                    shift = false;
                    ctrl = false;
                    opt = false;
                    alt = false;
                    del = false;
                    enter = false;
                    space = false;

                    values.clear();
                    hidKey.clear();
                }
            };


        private:
            // Keyboard state (app-side copies)
            std::vector<Point2D_t> _key_list_buffer;
            std::vector<Point2D_t> _key_values_without_special_keys;
            KeysState _keys_state_buffer;
            bool _is_caps_locked;
            uint8_t _last_key_size;

            // Background task shared state
            std::vector<Point2D_t> _task_key_list;
            uint8_t _raw_key_scan[8];
            SemaphoreHandle_t _mutex;
            TaskHandle_t _scan_task_handle;

            // LED state
            bool _led_enabled;
            uint8_t _led_level[4][14];
            uint8_t _global_brightness;
            uint8_t _pwm_phase;

            // Diagnostic direct-output state (scan task keeps running for keyboard)
            volatile bool _diag_direct_mode;
            volatile int _diag_en_level;
            volatile uint8_t _diag_col;
            volatile uint8_t _diag_row;

            // Hardware helpers
            void _set_output(const std::vector<int>& pinList, uint8_t output);
            uint8_t _get_input(const std::vector<int>& pinList);
            void _shift_led_columns(uint8_t value);
            uint8_t _compute_led_col_mask(uint8_t scan_index);

            // Background task
            static void _scan_task_entry(void* arg);
            void _scan_task_loop();


        public:
            Keyboard() :
                _is_caps_locked(false),
                _last_key_size(0),
                _mutex(nullptr),
                _scan_task_handle(nullptr),
                _led_enabled(false),
                _global_brightness(LED_BRIGHTNESS_MAX),
                _pwm_phase(0),
                _diag_direct_mode(false),
                _diag_en_level(1),
                _diag_col(0),
                _diag_row(0)
            {
                memset(_raw_key_scan, 0, sizeof(_raw_key_scan));
                memset(_led_level, 0, sizeof(_led_level));
            }

            void init();

            Point2D_t getKey();

            uint8_t getKeyNum(Point2D_t keyCoor);

            void updateKeyList();
            inline std::vector<KEYBOARD::Point2D_t>& keyList() { return _key_list_buffer; }

            inline KeyValue_t getKeyValue(const Point2D_t& keyCoor) { return _key_value_map[keyCoor.y][keyCoor.x]; }

            bool isKeyPressing(int keyNum);

            void updateKeysState();
            inline KeysState& keysState() { return _keys_state_buffer; }

            inline bool capslocked(void) { return _is_caps_locked; }
            inline void setCapsLocked(bool isLocked) { _is_caps_locked = isLocked; }

            bool isChanged();
            inline uint8_t isPressed() { return _key_list_buffer.size(); }

            // LED API
            bool enableLED(bool enabled);
            void setLED(Point2D_t pos, uint8_t brightness);
            void clearAllLEDs();
            void setAllLEDs(uint8_t brightness);
            void setLEDGlobalBrightness(uint8_t level);
            uint8_t ledGlobalBrightness() const { return _global_brightness; }
            void updateLED() {} // no-op, background task handles refresh

            // Diagnostic: pause/resume the background scan task
            void suspendScan();
            void resumeScan();

            // Diagnostic: direct static LED GPIO output while keeping keyboard alive
            void setDiagDirect(bool enable, int en_level = 1, uint8_t col = 0, uint8_t row = 0);
    };
}
