#pragma once

#include <vector>

#include "types.h"

namespace ComputeCardHAL::Keyboard {

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
