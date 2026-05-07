#include "keyboard.h"

#if defined(ESP_PLATFORM)
#include <driver/gpio.h>
#include <esp_rom_sys.h>

#define digitalWrite(pin, level) gpio_set_level((gpio_num_t)pin, level)
#define digitalRead(pin) gpio_get_level((gpio_num_t)pin)
#endif

namespace ComputeCardHAL::Keyboard {

namespace {

KeyboardDriver keyboard;

#if defined(ESP_PLATFORM)
constexpr uint8_t kLedColumnBitOffset = 1;
constexpr uint8_t kLedPwmPhaseCount = 4;
constexpr uint32_t kLedRowSelectSettleUs = 8;
constexpr uint32_t kLedRowDwellUs = 30;
// Keep the LED on-window non-preemptible so a scheduler interruption cannot
// accidentally extend one row's dwell time and make it flash brighter.
portMUX_TYPE kLedScanCriticalMux = portMUX_INITIALIZER_UNLOCKED;
#endif

}  // namespace

#if defined(ESP_PLATFORM)

void KeyboardDriver::_set_output(const std::vector<int>& pinList, uint8_t output)
{
    output = output & 0B00000111;

    digitalWrite(pinList[0], (output & 0B00000001));
    digitalWrite(pinList[1], (output & 0B00000010));
    digitalWrite(pinList[2], (output & 0B00000100));
}

uint8_t KeyboardDriver::_get_input(const std::vector<int>& pinList)
{
    uint8_t buffer = 0x00;
    uint8_t pin_value = 0x00;

    for (int i = 0; i < 7; i++)
    {
        pin_value = (digitalRead(pinList[i]) == 1) ? 0x00 : 0x01;
        pin_value = pin_value << i;
        buffer = buffer | pin_value;
    }

    return buffer;
}

void KeyboardDriver::init()
{
    for (auto i : led_pin_list)
    {
        gpio_reset_pin((gpio_num_t)i);
        gpio_set_direction((gpio_num_t)i, GPIO_MODE_OUTPUT);
        digitalWrite(i, 0);
    }
    digitalWrite(led_pin_list[0], 1);
    _shift_led_columns(0x00);

    for (auto i : output_list)
    {
        gpio_reset_pin((gpio_num_t)i);
        gpio_set_direction((gpio_num_t)i, GPIO_MODE_OUTPUT);
        gpio_set_pull_mode((gpio_num_t)i, GPIO_PULLUP_PULLDOWN);
        digitalWrite(i, 0);
    }

    for (auto i : input_list)
    {
        gpio_reset_pin((gpio_num_t)i);
        gpio_set_direction((gpio_num_t)i, GPIO_MODE_INPUT);
        gpio_set_pull_mode((gpio_num_t)i, GPIO_PULLUP_ONLY);
    }

    _set_output(output_list, 0);

    _mutex = xSemaphoreCreateMutex();
    xTaskCreatePinnedToCore(_scan_task_entry, "kb_scan", 4096, this, 5, &_scan_task_handle, 0);
}

Point2D_t KeyboardDriver::getKey()
{
    updateKeyList();
    if (_key_list_buffer.empty())
    {
        Point2D_t coor;
        coor.x = -1;
        coor.y = -1;
        return coor;
    }
    return _key_list_buffer[0];
}

uint8_t KeyboardDriver::getKeyNum(Point2D_t keyCoor)
{
    if ((keyCoor.x < 0) || (keyCoor.y < 0))
    {
        return 0;
    }

    return static_cast<uint8_t>((keyCoor.y * 14) + (keyCoor.x + 1));
}

void KeyboardDriver::updateKeyList()
{
    if (_mutex)
    {
        xSemaphoreTake(_mutex, portMAX_DELAY);
        _key_list_buffer = _task_key_list;
        xSemaphoreGive(_mutex);
    }
}

bool KeyboardDriver::isKeyPressing(int keyNum)
{
    if (_key_list_buffer.size())
    {
        for (const auto& i : _key_list_buffer)
        {
            if (getKeyNum(i) == keyNum)
            {
                return true;
            }
        }
    }
    return false;
}

void KeyboardDriver::updateKeysState()
{
    _keys_state_buffer.reset();
    _key_values_without_special_keys.clear();

    for (auto& i : _key_list_buffer)
    {
        if (strcmp(getKeyValue(i).value_first, "tab") == 0)
        {
            _keys_state_buffer.tab = true;
            continue;
        }

        if (strcmp(getKeyValue(i).value_first, "fn") == 0)
        {
            _keys_state_buffer.fn = true;
            continue;
        }

        if (strcmp(getKeyValue(i).value_first, "shift") == 0)
        {
            _keys_state_buffer.shift = true;
            continue;
        }

        if (strcmp(getKeyValue(i).value_first, "ctrl") == 0)
        {
            _keys_state_buffer.ctrl = true;
            continue;
        }
        if (strcmp(getKeyValue(i).value_first, "opt") == 0)
        {
            _keys_state_buffer.opt = true;
            continue;
        }

        if (strcmp(getKeyValue(i).value_first, "alt") == 0)
        {
            _keys_state_buffer.alt = true;
            continue;
        }

        if (strcmp(getKeyValue(i).value_first, "del") == 0)
        {
            _keys_state_buffer.del = true;
            _keys_state_buffer.hidKey.push_back(KEY_BACKSPACE);
            continue;
        }

        if (strcmp(getKeyValue(i).value_first, "enter") == 0)
        {
            _keys_state_buffer.enter = true;
            _keys_state_buffer.hidKey.push_back(KEY_ENTER);
            continue;
        }

        if (strcmp(getKeyValue(i).value_first, "space") == 0)
        {
            _keys_state_buffer.space = true;
            _keys_state_buffer.hidKey.push_back(KEY_SPACE);
            continue;
        }

        _key_values_without_special_keys.push_back(i);
    }

    for (auto& i : _key_values_without_special_keys)
    {
        if (_keys_state_buffer.ctrl || _keys_state_buffer.shift || _is_caps_locked)
        {
            _keys_state_buffer.values.push_back(*getKeyValue(i).value_second);
            _keys_state_buffer.hidKey.push_back(getKeyValue(i).value_num_second);
        }
        else
        {
            _keys_state_buffer.values.push_back(*getKeyValue(i).value_first);
            _keys_state_buffer.hidKey.push_back(getKeyValue(i).value_num_first);
        }
    }
}

bool KeyboardDriver::isChanged()
{
    if (_last_key_size != _key_list_buffer.size())
    {
        _last_key_size = static_cast<uint8_t>(_key_list_buffer.size());
        return true;
    }
    return false;
}

void KeyboardDriver::suspendScan()
{
    if (_scan_task_handle)
    {
        vTaskSuspend(_scan_task_handle);
        digitalWrite(led_pin_list[0], 1);
    }
}

void KeyboardDriver::resumeScan()
{
    if (_scan_task_handle)
    {
        vTaskResume(_scan_task_handle);
    }
}

void KeyboardDriver::setDiagDirect(bool enable, int en_level, uint8_t col, uint8_t row)
{
    _diag_en_level = en_level;
    _diag_col = col;
    _diag_row = row;
    _diag_direct_mode = enable;
}

void KeyboardDriver::_shift_led_columns(uint8_t value)
{
    const int pin_dat = led_pin_list[2];
    const int pin_clk = led_pin_list[1];

    for (int bit = 7; bit >= 0; --bit)
    {
        digitalWrite(pin_dat, (value >> bit) & 0x01);
        digitalWrite(pin_clk, 1);
        digitalWrite(pin_clk, 0);
    }
}

uint8_t KeyboardDriver::_compute_led_col_mask(uint8_t scan_index)
{
    int y = 0;
    int x_base = 0;
    if (scan_index < 4)
    {
        y = 3 - scan_index;
        x_base = 1;
    }
    else
    {
        y = 7 - scan_index;
        x_base = 0;
    }

    uint8_t mask = 0;
    const uint8_t global_br = _global_brightness;
    const uint8_t phase = _pwm_phase;

    for (int col = 0; col < 7; col++)
    {
        const int x = x_base + col * 2;
        const uint8_t level = _led_level[y][x];
        const uint8_t effective = (level < global_br) ? level : global_br;
        if (static_cast<uint16_t>(effective) * kLedPwmPhaseCount > static_cast<uint16_t>(phase) * (LED_BRIGHTNESS_MAX + 1))
        {
            const int physical_bit = col + kLedColumnBitOffset;
            if (physical_bit >= 0 && physical_bit < 8)
            {
                mask |= static_cast<uint8_t>(1u << physical_bit);
            }
        }
    }

    return mask;
}

bool KeyboardDriver::enableLED(bool enabled)
{
    _led_enabled = enabled;
    if (!enabled)
    {
        digitalWrite(led_pin_list[0], 1);
    }
    return _led_enabled;
}

void KeyboardDriver::setLED(Point2D_t pos, uint8_t brightness)
{
    if (pos.x < 0 || pos.x > 13 || pos.y < 0 || pos.y > 3)
    {
        return;
    }
    if (brightness > LED_BRIGHTNESS_MAX)
    {
        brightness = LED_BRIGHTNESS_MAX;
    }
    _led_level[pos.y][pos.x] = brightness;
}

void KeyboardDriver::clearAllLEDs()
{
    memset(_led_level, 0, sizeof(_led_level));
}

void KeyboardDriver::setAllLEDs(uint8_t brightness)
{
    if (brightness > LED_BRIGHTNESS_MAX)
    {
        brightness = LED_BRIGHTNESS_MAX;
    }
    memset(_led_level, brightness, sizeof(_led_level));
}

void KeyboardDriver::setLEDGlobalBrightness(uint8_t level)
{
    if (level > LED_BRIGHTNESS_MAX)
    {
        level = LED_BRIGHTNESS_MAX;
    }
    _global_brightness = level;
}

void KeyboardDriver::_scan_task_entry(void* arg)
{
    static_cast<KeyboardDriver*>(arg)->_scan_task_loop();
}

void KeyboardDriver::_scan_task_loop()
{
    std::vector<Point2D_t> local_key_list;
    local_key_list.reserve(16);

    auto rebuild_debounced_key_list = [this, &local_key_list]() {
        for (int row = 0; row < 8; ++row)
        {
            const uint8_t raw_value = _raw_key_scan[row];
            if (raw_value == _debounced_key_scan[row])
            {
                _candidate_key_scan[row] = raw_value;
                _debounce_count[row] = 0;
                continue;
            }

            if (_candidate_key_scan[row] != raw_value)
            {
                _candidate_key_scan[row] = raw_value;
                _debounce_count[row] = 1;
                continue;
            }

            if (_debounce_count[row] < KEY_DEBOUNCE_SCAN_COUNT)
            {
                ++_debounce_count[row];
            }

            if (_debounce_count[row] >= KEY_DEBOUNCE_SCAN_COUNT)
            {
                _debounced_key_scan[row] = raw_value;
                _debounce_count[row] = 0;
            }
        }

        local_key_list.clear();
        for (int i = 0; i < 8; i++)
        {
            const uint8_t input_value = _debounced_key_scan[i];
            if (input_value)
            {
                Point2D_t coor;
                for (int j = 0; j < 7; j++)
                {
                    if (input_value & (0x01 << j))
                    {
                        coor.x = (i > 3) ? X_map_chart[j].x_1 : X_map_chart[j].x_2;
                        coor.y = (i > 3) ? (i - 4) : i;
                        coor.y = -coor.y + 3;
                        local_key_list.push_back(coor);
                    }
                }
            }
        }
    };

    while (true)
    {
        if (_diag_direct_mode)
        {
            digitalWrite(led_pin_list[0], 1);
            for (int row = 0; row < 8; row++)
            {
                _set_output(output_list, row);
                esp_rom_delay_us(10);
                _raw_key_scan[row] = _get_input(input_list);
            }

            rebuild_debounced_key_list();

            xSemaphoreTake(_mutex, portMAX_DELAY);
            _task_key_list = local_key_list;
            xSemaphoreGive(_mutex);

            _set_output(output_list, _diag_row);
            _shift_led_columns(_diag_col);
            digitalWrite(led_pin_list[0], _diag_en_level);

            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        for (int row = 0; row < 8; row++)
        {
            digitalWrite(led_pin_list[0], 1);
            _set_output(output_list, row);
            esp_rom_delay_us(kLedRowSelectSettleUs);
            _raw_key_scan[row] = _get_input(input_list);

            if (_led_enabled)
            {
                const uint8_t col_mask = _compute_led_col_mask(row);
                _shift_led_columns(col_mask);
                if (col_mask)
                {
                    portENTER_CRITICAL(&kLedScanCriticalMux);
                    digitalWrite(led_pin_list[0], 0);
                    esp_rom_delay_us(kLedRowDwellUs);
                    digitalWrite(led_pin_list[0], 1);
                    portEXIT_CRITICAL(&kLedScanCriticalMux);
                    continue;
                }
            }

            esp_rom_delay_us(kLedRowDwellUs);
        }

        digitalWrite(led_pin_list[0], 1);

        rebuild_debounced_key_list();

        xSemaphoreTake(_mutex, portMAX_DELAY);
        _task_key_list = local_key_list;
        xSemaphoreGive(_mutex);

        _pwm_phase++;
        if (_pwm_phase >= kLedPwmPhaseCount)
        {
            _pwm_phase = 0;
            taskYIELD();
        }
    }
}

#else

void KeyboardDriver::_set_output(const std::vector<int>&, uint8_t)
{
}

uint8_t KeyboardDriver::_get_input(const std::vector<int>&)
{
    return 0;
}

void KeyboardDriver::init()
{
    _key_list_buffer.clear();
    _key_values_without_special_keys.clear();
    _keys_state_buffer.reset();
    _is_caps_locked = false;
    _last_key_size = 0;
    _task_key_list.clear();
    _mutex = nullptr;
    _scan_task_handle = nullptr;
    _led_enabled = false;
    _global_brightness = LED_BRIGHTNESS_MAX;
    _pwm_phase = 0;
    _diag_direct_mode = false;
    _diag_en_level = 1;
    _diag_col = 0;
    _diag_row = 0;
    memset(_raw_key_scan, 0, sizeof(_raw_key_scan));
    memset(_debounced_key_scan, 0, sizeof(_debounced_key_scan));
    memset(_candidate_key_scan, 0, sizeof(_candidate_key_scan));
    memset(_debounce_count, 0, sizeof(_debounce_count));
    memset(_led_level, 0, sizeof(_led_level));
}

Point2D_t KeyboardDriver::getKey()
{
    updateKeyList();
    if (_key_list_buffer.empty())
    {
        Point2D_t coor;
        coor.x = -1;
        coor.y = -1;
        return coor;
    }
    return _key_list_buffer[0];
}

uint8_t KeyboardDriver::getKeyNum(Point2D_t keyCoor)
{
    if ((keyCoor.x < 0) || (keyCoor.y < 0))
    {
        return 0;
    }

    return static_cast<uint8_t>((keyCoor.y * 14) + (keyCoor.x + 1));
}

void KeyboardDriver::updateKeyList()
{
    _key_list_buffer = _task_key_list;
}

bool KeyboardDriver::isKeyPressing(int keyNum)
{
    if (_key_list_buffer.size())
    {
        for (const auto& i : _key_list_buffer)
        {
            if (getKeyNum(i) == keyNum)
            {
                return true;
            }
        }
    }
    return false;
}

void KeyboardDriver::updateKeysState()
{
    _keys_state_buffer.reset();
    _key_values_without_special_keys.clear();

    for (auto& i : _key_list_buffer)
    {
        if (strcmp(getKeyValue(i).value_first, "tab") == 0)
        {
            _keys_state_buffer.tab = true;
            continue;
        }

        if (strcmp(getKeyValue(i).value_first, "fn") == 0)
        {
            _keys_state_buffer.fn = true;
            continue;
        }

        if (strcmp(getKeyValue(i).value_first, "shift") == 0)
        {
            _keys_state_buffer.shift = true;
            continue;
        }

        if (strcmp(getKeyValue(i).value_first, "ctrl") == 0)
        {
            _keys_state_buffer.ctrl = true;
            continue;
        }
        if (strcmp(getKeyValue(i).value_first, "opt") == 0)
        {
            _keys_state_buffer.opt = true;
            continue;
        }

        if (strcmp(getKeyValue(i).value_first, "alt") == 0)
        {
            _keys_state_buffer.alt = true;
            continue;
        }

        if (strcmp(getKeyValue(i).value_first, "del") == 0)
        {
            _keys_state_buffer.del = true;
            _keys_state_buffer.hidKey.push_back(KEY_BACKSPACE);
            continue;
        }

        if (strcmp(getKeyValue(i).value_first, "enter") == 0)
        {
            _keys_state_buffer.enter = true;
            _keys_state_buffer.hidKey.push_back(KEY_ENTER);
            continue;
        }

        if (strcmp(getKeyValue(i).value_first, "space") == 0)
        {
            _keys_state_buffer.space = true;
            _keys_state_buffer.hidKey.push_back(KEY_SPACE);
            continue;
        }

        _key_values_without_special_keys.push_back(i);
    }

    for (auto& i : _key_values_without_special_keys)
    {
        if (_keys_state_buffer.ctrl || _keys_state_buffer.shift || _is_caps_locked)
        {
            _keys_state_buffer.values.push_back(*getKeyValue(i).value_second);
            _keys_state_buffer.hidKey.push_back(getKeyValue(i).value_num_second);
        }
        else
        {
            _keys_state_buffer.values.push_back(*getKeyValue(i).value_first);
            _keys_state_buffer.hidKey.push_back(getKeyValue(i).value_num_first);
        }
    }
}

bool KeyboardDriver::isChanged()
{
    if (_last_key_size != _key_list_buffer.size())
    {
        _last_key_size = static_cast<uint8_t>(_key_list_buffer.size());
        return true;
    }
    return false;
}

void KeyboardDriver::suspendScan()
{
}

void KeyboardDriver::resumeScan()
{
}

void KeyboardDriver::setDiagDirect(bool enable, int en_level, uint8_t col, uint8_t row)
{
    _diag_direct_mode = enable;
    _diag_en_level = en_level;
    _diag_col = col;
    _diag_row = row;
}

void KeyboardDriver::_shift_led_columns(uint8_t)
{
}

uint8_t KeyboardDriver::_compute_led_col_mask(uint8_t)
{
    return 0;
}

bool KeyboardDriver::enableLED(bool enabled)
{
    _led_enabled = enabled;
    return _led_enabled;
}

void KeyboardDriver::setLED(Point2D_t pos, uint8_t brightness)
{
    if (pos.x < 0 || pos.x > 13 || pos.y < 0 || pos.y > 3)
    {
        return;
    }
    if (brightness > LED_BRIGHTNESS_MAX)
    {
        brightness = LED_BRIGHTNESS_MAX;
    }
    _led_level[pos.y][pos.x] = brightness;
}

void KeyboardDriver::clearAllLEDs()
{
    memset(_led_level, 0, sizeof(_led_level));
}

void KeyboardDriver::setAllLEDs(uint8_t brightness)
{
    if (brightness > LED_BRIGHTNESS_MAX)
    {
        brightness = LED_BRIGHTNESS_MAX;
    }
    memset(_led_level, brightness, sizeof(_led_level));
}

void KeyboardDriver::setLEDGlobalBrightness(uint8_t level)
{
    if (level > LED_BRIGHTNESS_MAX)
    {
        level = LED_BRIGHTNESS_MAX;
    }
    _global_brightness = level;
}

void KeyboardDriver::_scan_task_entry(void*)
{
}

void KeyboardDriver::_scan_task_loop()
{
}

#endif

void init()
{
    keyboard.init();
}

Point2D_t getKey()
{
    return keyboard.getKey();
}

uint8_t getKeyNum(Point2D_t keyCoor)
{
    return keyboard.getKeyNum(keyCoor);
}

void updateKeyList()
{
    keyboard.updateKeyList();
}

std::vector<Point2D_t>& keyList()
{
    return keyboard.keyList();
}

KeyValue_t getKeyValue(const Point2D_t& keyCoor)
{
    return keyboard.getKeyValue(keyCoor);
}

bool isKeyPressing(int keyNum)
{
    return keyboard.isKeyPressing(keyNum);
}

void updateKeysState()
{
    keyboard.updateKeysState();
}

KeysState& keysState()
{
    return keyboard.keysState();
}

bool capslocked()
{
    return keyboard.capslocked();
}

void setCapsLocked(bool isLocked)
{
    keyboard.setCapsLocked(isLocked);
}

bool isChanged()
{
    return keyboard.isChanged();
}

uint8_t isPressed()
{
    return keyboard.isPressed();
}

bool enableLED(bool enabled)
{
    return keyboard.enableLED(enabled);
}

void setLED(Point2D_t pos, uint8_t brightness)
{
    keyboard.setLED(pos, brightness);
}

void clearAllLEDs()
{
    keyboard.clearAllLEDs();
}

void setAllLEDs(uint8_t brightness)
{
    keyboard.setAllLEDs(brightness);
}

void setLEDGlobalBrightness(uint8_t level)
{
    keyboard.setLEDGlobalBrightness(level);
}

uint8_t ledGlobalBrightness()
{
    return keyboard.ledGlobalBrightness();
}

void updateLED()
{
    keyboard.updateLED();
}

}  // namespace ComputeCardHAL::Keyboard
