#pragma once

#include "legacy/keyboard.h"

namespace ComputeCardHAL::Keyboard {

    using Point2D_t = KEYBOARD::Point2D_t;
    using KeyValue_t = KEYBOARD::KeyValue_t;
    using KeysState = KEYBOARD::Keyboard::KeysState;

    inline constexpr uint8_t LED_BRIGHTNESS_MAX = KEYBOARD::LED_BRIGHTNESS_MAX;

    void init();

    Point2D_t getKey();

    uint8_t getKeyNum(Point2D_t keyCoor);

    void updateKeyList();
    std::vector<KEYBOARD::Point2D_t>& keyList();

    KeyValue_t getKeyValue(const Point2D_t& keyCoor);

    bool isKeyPressing(int keyNum);

    void updateKeysState();
    KeysState& keysState();

    bool capslocked();
    void setCapsLocked(bool isLocked);

    bool isChanged();
    uint8_t isPressed();

    // LED API
    bool enableLED(bool enabled);
    void setLED(Point2D_t pos, uint8_t brightness);
    void clearAllLEDs();
    void setAllLEDs(uint8_t brightness);
    void setLEDGlobalBrightness(uint8_t level);
    uint8_t ledGlobalBrightness();
    void updateLED();


}  // namespace ComputeCardHAL::Keyboard
