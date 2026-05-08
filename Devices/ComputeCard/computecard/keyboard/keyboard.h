#pragma once

#include <cstring>
#include <vector>

#include "Devices/ComputeCard/computecard/board/pins.h"
#include "./keymap.h"

namespace ComputeCardHAL::Keyboard {

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

inline const std::vector<int> output_list(Pins::kKeyboardOutputs.begin(), Pins::kKeyboardOutputs.end());
inline const std::vector<int> input_list(Pins::kKeyboardInputs.begin(), Pins::kKeyboardInputs.end());
inline const std::vector<int> led_pin_list(Pins::kKeyboardLedPins.begin(), Pins::kKeyboardLedPins.end());

inline constexpr uint8_t LED_BRIGHTNESS_MAX = 15;
inline constexpr uint8_t KEY_DEBOUNCE_SCAN_COUNT = 3;

inline constexpr Chart_t X_map_chart[7] = {
    {1, 0, 1},
    {2, 2, 3},
    {4, 4, 5},
    {8, 6, 7},
    {16, 8, 9},
    {32, 10, 11},
    {64, 12, 13},
};

struct KeyValue_t
{
    const char* value_first;
    const int value_num_first;
    const char* value_second;
    const int value_num_second;
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
    bool space = false;

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

class KeyboardDriver
{
  private:
    std::vector<Point2D_t> _key_list_buffer;
    std::vector<Point2D_t> _key_values_without_special_keys;
    KeysState _keys_state_buffer;
    bool _is_caps_locked;
    uint8_t _last_key_size;

    std::vector<Point2D_t> _task_key_list;
    uint8_t _raw_key_scan[8];
    uint8_t _debounced_key_scan[8];
    uint8_t _candidate_key_scan[8];
    uint8_t _debounce_count[8];
    uint8_t _scan_raw_key_scan[8];
    void* _mutex;
    void* _scan_task_handle;
    void* _scan_timer;
    bool _scan_running;
    volatile uint8_t _scan_row;
    volatile uint8_t _diag_scan_row;

    bool _led_enabled;
    bool _scan_led_enabled;
    uint8_t _led_level[4][14];
    uint8_t _scan_led_level[4][14];
    uint8_t _scan_led_col_mask[4][8];
    uint8_t _global_brightness;
    uint8_t _scan_global_brightness;
    uint8_t _pwm_phase;

    volatile bool _diag_direct_mode;
    volatile int _diag_en_level;
    volatile uint8_t _diag_col;
    volatile uint8_t _diag_row;

    void _set_output(const std::vector<int>& pinList, uint8_t output);
    uint8_t _get_input(const std::vector<int>& pinList);
    void _shift_led_columns(uint8_t value);
    uint8_t _compute_led_col_mask(uint8_t scan_index, uint8_t phase) const;
    void _commit_led_frame();

    static void _scan_task_entry(void* arg);
    void _scan_task_loop();

  public:
    KeyboardDriver() :
        _is_caps_locked(false),
        _last_key_size(0),
        _mutex(nullptr),
        _scan_task_handle(nullptr),
        _scan_timer(nullptr),
        _scan_running(false),
        _scan_row(0),
        _diag_scan_row(0),
        _led_enabled(false),
        _scan_led_enabled(false),
        _global_brightness(LED_BRIGHTNESS_MAX),
        _scan_global_brightness(LED_BRIGHTNESS_MAX),
        _pwm_phase(0),
        _diag_direct_mode(false),
        _diag_en_level(1),
        _diag_col(0),
        _diag_row(0)
    {
        memset(_raw_key_scan, 0, sizeof(_raw_key_scan));
        memset(_debounced_key_scan, 0, sizeof(_debounced_key_scan));
        memset(_candidate_key_scan, 0, sizeof(_candidate_key_scan));
        memset(_debounce_count, 0, sizeof(_debounce_count));
        memset(_scan_raw_key_scan, 0, sizeof(_scan_raw_key_scan));
        memset(_led_level, 0, sizeof(_led_level));
        memset(_scan_led_level, 0, sizeof(_scan_led_level));
        memset(_scan_led_col_mask, 0, sizeof(_scan_led_col_mask));
    }

    void init();

    Point2D_t getKey();

    uint8_t getKeyNum(Point2D_t keyCoor);

    void updateKeyList();
    std::vector<Point2D_t>& keyList() { return _key_list_buffer; }

    KeyValue_t getKeyValue(const Point2D_t& keyCoor) { return key_value_map[keyCoor.y][keyCoor.x]; }

    bool isKeyPressing(int keyNum);

    void updateKeysState();
    KeysState& keysState() { return _keys_state_buffer; }

    bool capslocked() const { return _is_caps_locked; }
    void setCapsLocked(bool isLocked) { _is_caps_locked = isLocked; }

    bool isChanged();
    uint8_t isPressed() const { return static_cast<uint8_t>(_key_list_buffer.size()); }

    bool enableLED(bool enabled);
    void setLED(Point2D_t pos, uint8_t brightness);
    void clearAllLEDs();
    void setAllLEDs(uint8_t brightness);
    void setLEDGlobalBrightness(uint8_t level);
    uint8_t ledGlobalBrightness() const { return _global_brightness; }
    void updateLED() {}
    bool handleTimerTickFromIsr();

    void suspendScan();
    void resumeScan();
    void setDiagDirect(bool enable, int en_level = 1, uint8_t col = 0, uint8_t row = 0);
};

void init();
Point2D_t getKey();
uint8_t getKeyNum(Point2D_t keyCoor);
void updateKeyList();
std::vector<Point2D_t>& keyList();
KeyValue_t getKeyValue(const Point2D_t& keyCoor);
bool isKeyPressing(int keyNum);
void updateKeysState();
KeysState& keysState();
bool capslocked();
void setCapsLocked(bool isLocked);
bool isChanged();
uint8_t isPressed();
bool enableLED(bool enabled);
void setLED(Point2D_t pos, uint8_t brightness);
void clearAllLEDs();
void setAllLEDs(uint8_t brightness);
void setLEDGlobalBrightness(uint8_t level);
uint8_t ledGlobalBrightness();
void updateLED();

}  // namespace ComputeCardHAL::Keyboard
