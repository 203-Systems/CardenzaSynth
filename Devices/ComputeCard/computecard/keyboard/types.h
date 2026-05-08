#pragma once

#include <cstdint>
#include <vector>

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

struct KeyValue_t
{
    const char* value_first;
    const int value_num_first;
    const char* value_second;
    const int value_num_second;
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

inline constexpr uint8_t LED_BRIGHTNESS_MAX = 15;
inline constexpr uint8_t KEY_DEBOUNCE_SCAN_COUNT = 3;

}  // namespace ComputeCardHAL::Keyboard
