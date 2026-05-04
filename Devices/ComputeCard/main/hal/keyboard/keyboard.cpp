/**
 * @file keyboard.cpp
 * @author Forairaaaaa
 * @brief 
 * @version 0.1
 * @date 2023-09-22
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#include "keyboard.h"

#if __has_include(<driver/gpio.h>) && __has_include(<esp_rom_sys.h>) && __has_include(<freertos/FreeRTOS.h>)
#include <driver/gpio.h>
#include <cstring>
#include <esp_rom_sys.h>


#define digitalWrite(pin, level) gpio_set_level((gpio_num_t)pin, level)
#define digitalRead(pin) gpio_get_level((gpio_num_t)pin)


using namespace KEYBOARD;


void Keyboard::_set_output(const std::vector<int>& pinList, uint8_t output)
{
    // Temp fix for computer card prototype 1. TODO remove
    switch (output) {
        case 0: output = 0; break;
        case 1: output = 2; break;
        case 2: output = 4; break;
        case 3: output = 6; break;
        case 4: output = 1; break;
        case 5: output = 3; break;
        case 6: output = 5; break;
        case 7: output = 7; break;
    }
    // End temp fix

    output = output & 0B00000111;

    digitalWrite(pinList[0], (output & 0B00000001));
    digitalWrite(pinList[1], (output & 0B00000010));
    digitalWrite(pinList[2], (output & 0B00000100));
}


uint8_t Keyboard::_get_input(const std::vector<int>& pinList)
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


void Keyboard::init()
{
    // for (auto i : output_list) {
    //     gpio_reset_pin((gpio_num_t)i);
    //     pinMode(i, OUTPUT);
    //     digitalWrite(i, 0);
    // }

    // for (auto i : input_list) {
    //     gpio_reset_pin((gpio_num_t)i);
    //     pinMode(i, INPUT_PULLUP);
    // }

    // _set_output(output_list, 0);


    // Initialize LED pins FIRST to prevent boot flash.
    // The 74HC595 powers up with undefined register contents;
    // clearing it before touching the shared 74HC138 address lines
    // prevents a visible flash on all LEDs.
    for (auto i : led_pin_list)
    {
        gpio_reset_pin((gpio_num_t)i);
        gpio_set_direction((gpio_num_t)i, GPIO_MODE_OUTPUT);
        digitalWrite(i, 0);
    }
    // Disable 595 outputs (OE# HIGH)
    digitalWrite(led_pin_list[0], 1);
    // Shift all-zeros into the 595 (Q0-Q7 = LOW = all LEDs off)
    _shift_led_columns(0x00);

    // Now init keyboard address and input lines
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

    // Create mutex and start background scan task
    _mutex = xSemaphoreCreateMutex();
    xTaskCreatePinnedToCore(_scan_task_entry, "kb_scan", 4096, this, 5, &_scan_task_handle, 1);
}


Point2D_t Keyboard::getKey()
{
    updateKeyList();
    if (_key_list_buffer.empty()) {
        Point2D_t coor;
        coor.x = -1;
        coor.y = -1;
        return coor;
    }
    return _key_list_buffer[0];
}


uint8_t Keyboard::getKeyNum(Point2D_t keyCoor)
{
    uint8_t ret = 0;

    if ((keyCoor.x < 0) || (keyCoor.y < 0)) {
        return 0;
    }

    ret = (keyCoor.y * 14) + (keyCoor.x + 1);

    return ret;
}


void Keyboard::updateKeyList()
{
    if (_mutex) {
        xSemaphoreTake(_mutex, portMAX_DELAY);
        _key_list_buffer = _task_key_list;
        xSemaphoreGive(_mutex);
    }
}


bool Keyboard::isKeyPressing(int keyNum)
{
    if (_key_list_buffer.size())
    {
        for (const auto& i : _key_list_buffer)
        {
            if (getKeyNum(i) == keyNum)
                return true;
        }
    }
    return false;
}


void Keyboard::updateKeysState()
{
    _keys_state_buffer.reset();
    _key_values_without_special_keys.clear();

    // Get special keys 
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

    // Deal what left
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


bool Keyboard::isChanged() 
{
    if (_last_key_size != _key_list_buffer.size()) 
    {
        _last_key_size = _key_list_buffer.size();
        return true;
    } 
    return false;
}


/* ---------- LED matrix ---------- */

void Keyboard::suspendScan()
{
    if (_scan_task_handle)
    {
        vTaskSuspend(_scan_task_handle);
        // Put outputs into a known-safe state
        digitalWrite(led_pin_list[0], 1); // OE# HIGH (assumed disable)
    }
}

void Keyboard::resumeScan()
{
    if (_scan_task_handle)
        vTaskResume(_scan_task_handle);
}

void Keyboard::setDiagDirect(bool enable, int en_level, uint8_t col, uint8_t row)
{
    _diag_en_level = en_level;
    _diag_col = col;
    _diag_row = row;
    _diag_direct_mode = enable;
}

void Keyboard::_shift_led_columns(uint8_t value)
{
    int pin_dat = led_pin_list[2]; // LED_DAT
    int pin_clk = led_pin_list[1]; // LED_CLK

    for (int bit = 7; bit >= 0; --bit)
    {
        digitalWrite(pin_dat, (value >> bit) & 0x01);
        digitalWrite(pin_clk, 1);
        digitalWrite(pin_clk, 0);
    }
}


uint8_t Keyboard::_compute_led_col_mask(uint8_t scan_index)
{
    int y, x_base;
    if (scan_index < 4) {
        y = 3 - scan_index;
        x_base = 1; // odd x values
    } else {
        y = 7 - scan_index;
        x_base = 0; // even x values
    }

    uint8_t mask = 0;
    uint8_t global_br = _global_brightness;
    uint8_t phase = _pwm_phase;

    for (int col = 0; col < 7; col++) {
        int x = x_base + col * 2;
        uint8_t level = _led_level[y][x];
        uint8_t effective = (level < global_br) ? level : global_br;
        if (effective > phase)
            mask |= (1 << col);
    }
    return mask;
}


bool Keyboard::enableLED(bool enabled)
{
    _led_enabled = enabled;
    if (!enabled)
        digitalWrite(led_pin_list[0], 1);
    return _led_enabled;
}


void Keyboard::setLED(Point2D_t pos, uint8_t brightness)
{
    if (pos.x < 0 || pos.x > 13 || pos.y < 0 || pos.y > 3)
        return;
    if (brightness > LED_BRIGHTNESS_MAX)
        brightness = LED_BRIGHTNESS_MAX;
    _led_level[pos.y][pos.x] = brightness;
}


void Keyboard::clearAllLEDs()
{
    memset(_led_level, 0, sizeof(_led_level));
}


void Keyboard::setAllLEDs(uint8_t brightness)
{
    if (brightness > LED_BRIGHTNESS_MAX)
        brightness = LED_BRIGHTNESS_MAX;
    memset(_led_level, brightness, sizeof(_led_level));
}


void Keyboard::setLEDGlobalBrightness(uint8_t level)
{
    if (level > LED_BRIGHTNESS_MAX)
        level = LED_BRIGHTNESS_MAX;
    _global_brightness = level;
}


/* ---------- Background scan task ---------- */

void Keyboard::_scan_task_entry(void* arg)
{
    static_cast<Keyboard*>(arg)->_scan_task_loop();
}


void Keyboard::_scan_task_loop()
{
    std::vector<Point2D_t> local_key_list;
    local_key_list.reserve(16);

    while (true)
    {
        /* --- Diagnostic direct-output mode --- */
        if (_diag_direct_mode)
        {
            // Keyboard-only scan (LEDs blanked during scan)
            digitalWrite(led_pin_list[0], 1);
            for (int row = 0; row < 8; row++)
            {
                _set_output(output_list, row);
                esp_rom_delay_us(10);
                _raw_key_scan[row] = _get_input(input_list);
            }

            // Rebuild key list
            local_key_list.clear();
            for (int i = 0; i < 8; i++)
            {
                uint8_t input_value = _raw_key_scan[i];
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

            xSemaphoreTake(_mutex, portMAX_DELAY);
            _task_key_list = local_key_list;
            xSemaphoreGive(_mutex);

            // Apply diagnostic static GPIO state
            _set_output(output_list, _diag_row);
            _shift_led_columns(_diag_col);
            digitalWrite(led_pin_list[0], _diag_en_level);

            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        /* --- Normal scan mode --- */
        // One PWM sub-frame: scan all 8 rows
        for (int row = 0; row < 8; row++)
        {
            // 1. Disable LED output (prevents LED column data from
            //    bleeding across rows while the 74HC138 switches)
            digitalWrite(led_pin_list[0], 1);

            // 2. Select row via 74HC138 (includes board-specific remap)
            _set_output(output_list, row);

            // 3. Settle time for row multiplexer + input pull-ups
            esp_rom_delay_us(10);

            // 4. Read keyboard column inputs for this row
            _raw_key_scan[row] = _get_input(input_list);

            // 5. Drive LED columns for this row
            if (_led_enabled) {
                uint8_t col_mask = _compute_led_col_mask(row);
                // Q HIGH = LED on (active-high column drive: anode at 595,
                // cathode sunk by active-low 74HC138 row)
                _shift_led_columns(col_mask);
                if (col_mask)
                    digitalWrite(led_pin_list[0], 0); // Enable LED output
            }

            // 6. Hold row for dwell time
            esp_rom_delay_us(100);
        }

        // Disable LED after sweep (clean state before next phase)
        digitalWrite(led_pin_list[0], 1);

        // Rebuild key list from raw scan data
        local_key_list.clear();
        for (int i = 0; i < 8; i++)
        {
            uint8_t input_value = _raw_key_scan[i];
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

        // Publish key list under mutex
        xSemaphoreTake(_mutex, portMAX_DELAY);
        _task_key_list = local_key_list;
        xSemaphoreGive(_mutex);

        // Advance PWM phase (cycles 0 .. LED_BRIGHTNESS_MAX-1)
        _pwm_phase++;
        if (_pwm_phase >= LED_BRIGHTNESS_MAX)
        {
            _pwm_phase = 0;
            // Yield once per full PWM cycle to let lower-priority tasks run
            vTaskDelay(1);
        }
    }
}

#else

#include <cstring>

using namespace KEYBOARD;

void Keyboard::_set_output(const std::vector<int>&, uint8_t)
{
}

uint8_t Keyboard::_get_input(const std::vector<int>&)
{
    return 0;
}

void Keyboard::init()
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
    memset(_led_level, 0, sizeof(_led_level));
}

Point2D_t Keyboard::getKey()
{
    updateKeyList();
    if (_key_list_buffer.empty()) {
        Point2D_t coor;
        coor.x = -1;
        coor.y = -1;
        return coor;
    }
    return _key_list_buffer[0];
}

uint8_t Keyboard::getKeyNum(Point2D_t keyCoor)
{
    uint8_t ret = 0;

    if ((keyCoor.x < 0) || (keyCoor.y < 0)) {
        return 0;
    }

    ret = (keyCoor.y * 14) + (keyCoor.x + 1);

    return ret;
}

void Keyboard::updateKeyList()
{
    _key_list_buffer = _task_key_list;
}

bool Keyboard::isKeyPressing(int keyNum)
{
    if (_key_list_buffer.size())
    {
        for (const auto& i : _key_list_buffer)
        {
            if (getKeyNum(i) == keyNum)
                return true;
        }
    }
    return false;
}

void Keyboard::updateKeysState()
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

bool Keyboard::isChanged()
{
    if (_last_key_size != _key_list_buffer.size())
    {
        _last_key_size = _key_list_buffer.size();
        return true;
    }
    return false;
}

void Keyboard::suspendScan()
{
}

void Keyboard::resumeScan()
{
}

void Keyboard::setDiagDirect(bool enable, int en_level, uint8_t col, uint8_t row)
{
    _diag_en_level = en_level;
    _diag_col = col;
    _diag_row = row;
    _diag_direct_mode = enable;
}

void Keyboard::_shift_led_columns(uint8_t)
{
}

uint8_t Keyboard::_compute_led_col_mask(uint8_t)
{
    return 0;
}

bool Keyboard::enableLED(bool enabled)
{
    _led_enabled = enabled;
    return _led_enabled;
}

void Keyboard::setLED(Point2D_t pos, uint8_t brightness)
{
    if (pos.x < 0 || pos.x > 13 || pos.y < 0 || pos.y > 3)
        return;
    if (brightness > LED_BRIGHTNESS_MAX)
        brightness = LED_BRIGHTNESS_MAX;
    _led_level[pos.y][pos.x] = brightness;
}

void Keyboard::clearAllLEDs()
{
    memset(_led_level, 0, sizeof(_led_level));
}

void Keyboard::setAllLEDs(uint8_t brightness)
{
    if (brightness > LED_BRIGHTNESS_MAX)
        brightness = LED_BRIGHTNESS_MAX;
    memset(_led_level, brightness, sizeof(_led_level));
}

void Keyboard::setLEDGlobalBrightness(uint8_t level)
{
    if (level > LED_BRIGHTNESS_MAX)
        level = LED_BRIGHTNESS_MAX;
    _global_brightness = level;
}

void Keyboard::_scan_task_entry(void*)
{
}

void Keyboard::_scan_task_loop()
{
}

#endif
