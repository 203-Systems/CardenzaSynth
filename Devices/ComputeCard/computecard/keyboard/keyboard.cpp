#include "keyboard.h"

#if defined(ESP_PLATFORM)
#include <driver/gpio.h>
#include <driver/gptimer.h>
#include <esp_private/periph_ctrl.h>
#include <esp_rom_gpio.h>
#include <esp_check.h>
#include <esp_rom_sys.h>
#include <hal/spi_ll.h>
#include <soc/gpio_struct.h>
#include <soc/spi_periph.h>

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
constexpr uint32_t kLedRowPeriodUs = kLedRowSelectSettleUs + kLedRowDwellUs;
constexpr spi_host_device_t kKeyboardLedSpiHost = SPI2_HOST;
constexpr int kKeyboardLedSpiSourceHz = 80 * 1000 * 1000;
constexpr int kKeyboardLedSpiClockHz = 10 * 1000 * 1000;
constexpr UBaseType_t kScanTaskPriority = 5;
constexpr BaseType_t kScanTaskPinnedCore = 0;
portMUX_TYPE kKeyboardStateMux = portMUX_INITIALIZER_UNLOCKED;
spi_dev_t* gKeyboardLedSpiHw = nullptr;
bool gKeyboardLedSpiReady = false;

constexpr uint64_t PinMask(int pin)
{
    return 1ULL << pin;
}

const uint64_t kKeyboardOutputMask = PinMask(output_list[0]) | PinMask(output_list[1]) | PinMask(output_list[2]);
const uint64_t kKeyboardLedClockMask = PinMask(led_pin_list[1]);
const uint64_t kKeyboardLedDataMask = PinMask(led_pin_list[2]);

inline void FastWriteMask(uint64_t set_mask, uint64_t clear_mask)
{
    const uint32_t set_low = static_cast<uint32_t>(set_mask);
    const uint32_t clear_low = static_cast<uint32_t>(clear_mask);
    if (clear_low != 0)
    {
        GPIO.out_w1tc = clear_low;
    }
    if (set_low != 0)
    {
        GPIO.out_w1ts = set_low;
    }

    const uint32_t set_high = static_cast<uint32_t>(set_mask >> 32);
    const uint32_t clear_high = static_cast<uint32_t>(clear_mask >> 32);
    if (clear_high != 0)
    {
        GPIO.out1_w1tc.data = clear_high;
    }
    if (set_high != 0)
    {
        GPIO.out1_w1ts.data = set_high;
    }
}

inline void FastWritePin(int pin, bool level)
{
    const uint64_t mask = PinMask(pin);
    FastWriteMask(level ? mask : 0, level ? 0 : mask);
}

inline int FastReadPin(int pin)
{
    if (pin < 32)
    {
        return (GPIO.in >> pin) & 0x1;
    }

    return (GPIO.in1.data >> (pin - 32)) & 0x1;
}

void InitKeyboardLedSpi()
{
    PERIPH_RCC_ATOMIC()
    {
        spi_ll_enable_bus_clock(kKeyboardLedSpiHost, true);
        spi_ll_reset_register(kKeyboardLedSpiHost);
    }

    spi_dev_t* const hw = spi_periph_signal[kKeyboardLedSpiHost].hw;
    spi_ll_enable_clock(kKeyboardLedSpiHost, true);
    spi_ll_master_init(hw);
    spi_ll_set_clk_source(hw, SPI_CLK_SRC_APB);
    spi_ll_master_set_clock(hw, kKeyboardLedSpiSourceHz, kKeyboardLedSpiClockHz, 128);
    spi_ll_master_set_mode(hw, 0);
    spi_ll_set_half_duplex(hw, true);
    spi_ll_set_sio_mode(hw, 0);
    spi_ll_set_tx_lsbfirst(hw, false);
    spi_ll_set_rx_lsbfirst(hw, false);
    spi_ll_master_select_cs(hw, -1);
    spi_ll_master_keep_cs(hw, 0);
    spi_ll_master_set_cs_setup(hw, 0);
    spi_ll_master_set_cs_hold(hw, 0);
    spi_ll_set_command_bitlen(hw, 0);
    spi_ll_set_addr_bitlen(hw, 0);
    spi_ll_set_dummy(hw, 0);
    spi_ll_enable_miso(hw, 0);
    spi_ll_enable_mosi(hw, 1);
    spi_ll_set_mosi_bitlen(hw, 8);
    spi_ll_set_mosi_free_level(hw, false);
    spi_ll_cpu_tx_fifo_reset(hw);
    spi_ll_cpu_rx_fifo_reset(hw);
    spi_ll_apply_config(hw);

    const spi_signal_conn_t& spi_signal = spi_periph_signal[kKeyboardLedSpiHost];
    esp_rom_gpio_pad_select_gpio(led_pin_list[1]);
    esp_rom_gpio_pad_select_gpio(led_pin_list[2]);
    esp_rom_gpio_connect_out_signal(led_pin_list[1], spi_signal.spiclk_out, false, false);
    esp_rom_gpio_connect_out_signal(led_pin_list[2], spi_signal.spid_out, false, false);

    gKeyboardLedSpiHw = hw;
    gKeyboardLedSpiReady = true;
}

bool KeyboardScanTimerCallback(gptimer_handle_t,
                               const gptimer_alarm_event_data_t*,
                               void* user_ctx)
{
    return static_cast<KeyboardDriver*>(user_ctx)->handleTimerTickFromIsr();
}
#endif

}  // namespace

#if defined(ESP_PLATFORM)

void KeyboardDriver::_set_output(const std::vector<int>& pinList, uint8_t output)
{
    output = output & 0B00000111;

    uint64_t set_mask = 0;
    if (output & 0B00000001)
    {
        set_mask |= PinMask(pinList[0]);
    }
    if (output & 0B00000010)
    {
        set_mask |= PinMask(pinList[1]);
    }
    if (output & 0B00000100)
    {
        set_mask |= PinMask(pinList[2]);
    }

    FastWriteMask(set_mask, kKeyboardOutputMask & ~set_mask);
}

uint8_t KeyboardDriver::_get_input(const std::vector<int>& pinList)
{
    uint8_t buffer = 0x00;
    uint8_t pin_value = 0x00;

    for (int i = 0; i < 7; i++)
    {
        pin_value = (FastReadPin(pinList[i]) == 1) ? 0x00 : 0x01;
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

    InitKeyboardLedSpi();
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

#if portNUM_PROCESSORS > 1
    xTaskCreatePinnedToCore(_scan_task_entry,
                            "kb_scan",
                            4096,
                            this,
                            kScanTaskPriority,
                            &_scan_task_handle,
                            kScanTaskPinnedCore);
#else
    xTaskCreate(_scan_task_entry, "kb_scan", 4096, this, kScanTaskPriority, &_scan_task_handle);
#endif

    _commit_led_frame();

    gptimer_config_t timer_config = {};
    timer_config.clk_src = GPTIMER_CLK_SRC_DEFAULT;
    timer_config.direction = GPTIMER_COUNT_UP;
    timer_config.resolution_hz = 1000000;
    gptimer_handle_t timer = nullptr;
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &timer));

    gptimer_event_callbacks_t callbacks = {};
    callbacks.on_alarm = KeyboardScanTimerCallback;
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(timer, &callbacks, this));

    gptimer_alarm_config_t alarm_config = {};
    alarm_config.reload_count = 0;
    alarm_config.alarm_count = kLedRowPeriodUs;
    alarm_config.flags.auto_reload_on_alarm = true;
    ESP_ERROR_CHECK(gptimer_set_alarm_action(timer, &alarm_config));
    ESP_ERROR_CHECK(gptimer_enable(timer));
    ESP_ERROR_CHECK(gptimer_start(timer));

    _scan_timer = timer;
    _scan_running = true;
}

void KeyboardDriver::_commit_led_frame()
{
    portENTER_CRITICAL(&kKeyboardStateMux);
    memcpy(_scan_led_level, _led_level, sizeof(_led_level));
    _scan_global_brightness = _global_brightness;
    _scan_led_enabled = _led_enabled;
    for (uint8_t phase = 0; phase < kLedPwmPhaseCount; ++phase)
    {
        for (uint8_t scan_index = 0; scan_index < 8; ++scan_index)
        {
            _scan_led_col_mask[phase][scan_index] = _scan_led_enabled ? _compute_led_col_mask(scan_index, phase) : 0;
        }
    }
    portEXIT_CRITICAL(&kKeyboardStateMux);
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
    if (_scan_timer && _scan_running)
    {
        ESP_ERROR_CHECK(gptimer_stop(reinterpret_cast<gptimer_handle_t>(_scan_timer)));
        _scan_running = false;
        FastWritePin(led_pin_list[0], true);
    }
}

void KeyboardDriver::resumeScan()
{
    if (_scan_timer && !_scan_running)
    {
        _scan_row = 0;
        _diag_scan_row = 0;
        _commit_led_frame();
        ESP_ERROR_CHECK(gptimer_start(reinterpret_cast<gptimer_handle_t>(_scan_timer)));
        _scan_running = true;
    }
}

void KeyboardDriver::setDiagDirect(bool enable, int en_level, uint8_t col, uint8_t row)
{
    _diag_en_level = en_level;
    _diag_col = col;
    _diag_row = row;
    _diag_scan_row = 0;
    _diag_direct_mode = enable;
}

void KeyboardDriver::_shift_led_columns(uint8_t value)
{
    if (gKeyboardLedSpiReady && gKeyboardLedSpiHw != nullptr)
    {
        while (spi_ll_get_running_cmd(gKeyboardLedSpiHw) != 0)
        {
        }

        spi_ll_write_buffer_byte(gKeyboardLedSpiHw, 0, &value, 1);
        spi_ll_set_mosi_bitlen(gKeyboardLedSpiHw, 8);
        spi_ll_user_start(gKeyboardLedSpiHw);

        while (spi_ll_get_running_cmd(gKeyboardLedSpiHw) != 0)
        {
        }
        return;
    }

    for (int bit = 7; bit >= 0; --bit)
    {
        const bool bit_set = ((value >> bit) & 0x01) != 0;
        FastWriteMask(bit_set ? kKeyboardLedDataMask : 0,
                      bit_set ? 0 : kKeyboardLedDataMask);
        FastWriteMask(kKeyboardLedClockMask, 0);
        FastWriteMask(0, kKeyboardLedClockMask);
    }
}

uint8_t KeyboardDriver::_compute_led_col_mask(uint8_t scan_index, uint8_t phase) const
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
    const uint8_t global_br = _scan_global_brightness;

    for (int col = 0; col < 7; col++)
    {
        const int x = x_base + col * 2;
        const uint8_t level = _scan_led_level[y][x];
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
    _commit_led_frame();
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
    if (_led_enabled)
    {
        _commit_led_frame();
    }
}

void KeyboardDriver::clearAllLEDs()
{
    memset(_led_level, 0, sizeof(_led_level));
    if (_led_enabled)
    {
        _commit_led_frame();
    }
}

void KeyboardDriver::setAllLEDs(uint8_t brightness)
{
    if (brightness > LED_BRIGHTNESS_MAX)
    {
        brightness = LED_BRIGHTNESS_MAX;
    }
    memset(_led_level, brightness, sizeof(_led_level));
    if (_led_enabled)
    {
        _commit_led_frame();
    }
}

void KeyboardDriver::setLEDGlobalBrightness(uint8_t level)
{
    if (level > LED_BRIGHTNESS_MAX)
    {
        level = LED_BRIGHTNESS_MAX;
    }
    _global_brightness = level;
    if (_led_enabled)
    {
        _commit_led_frame();
    }
}

void KeyboardDriver::_scan_task_entry(void* arg)
{
    static_cast<KeyboardDriver*>(arg)->_scan_task_loop();
}

bool KeyboardDriver::handleTimerTickFromIsr()
{
    BaseType_t high_task_woken = pdFALSE;

    FastWritePin(led_pin_list[0], true);

    if (_diag_direct_mode)
    {
        const uint8_t scan_row = _diag_scan_row;
        _set_output(output_list, scan_row);
        esp_rom_delay_us(kLedRowSelectSettleUs);
        _scan_raw_key_scan[scan_row] = _get_input(input_list);

        _diag_scan_row = static_cast<uint8_t>((scan_row + 1) & 0x07);
        if (_diag_scan_row == 0)
        {
            _pwm_phase = static_cast<uint8_t>((_pwm_phase + 1) % kLedPwmPhaseCount);
            vTaskNotifyGiveFromISR(_scan_task_handle, &high_task_woken);
        }

        _set_output(output_list, _diag_row);
        _shift_led_columns(_diag_col);
        FastWritePin(led_pin_list[0], _diag_en_level != 0);
        return high_task_woken == pdTRUE;
    }

    const uint8_t row = _scan_row;
    _scan_raw_key_scan[row] = _get_input(input_list);

    const uint8_t next_row = static_cast<uint8_t>((row + 1) & 0x07);
    _scan_row = next_row;
    if (next_row == 0)
    {
        _pwm_phase = static_cast<uint8_t>((_pwm_phase + 1) % kLedPwmPhaseCount);
        vTaskNotifyGiveFromISR(_scan_task_handle, &high_task_woken);
    }

    _set_output(output_list, next_row);
    const uint8_t next_col_mask = _scan_led_enabled ? _scan_led_col_mask[_pwm_phase][next_row] : 0;
    _shift_led_columns(next_col_mask);
    if (_scan_led_enabled && next_col_mask)
    {
        FastWritePin(led_pin_list[0], false);
    }

    return high_task_woken == pdTRUE;
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
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        portENTER_CRITICAL(&kKeyboardStateMux);
        memcpy(_raw_key_scan, _scan_raw_key_scan, sizeof(_raw_key_scan));
        portEXIT_CRITICAL(&kKeyboardStateMux);

        rebuild_debounced_key_list();

        xSemaphoreTake(_mutex, portMAX_DELAY);
        _task_key_list = local_key_list;
        xSemaphoreGive(_mutex);
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
    _scan_timer = nullptr;
    _scan_running = false;
    _scan_row = 0;
    _diag_scan_row = 0;
    _led_enabled = false;
    _scan_led_enabled = false;
    _global_brightness = LED_BRIGHTNESS_MAX;
    _scan_global_brightness = LED_BRIGHTNESS_MAX;
    _pwm_phase = 0;
    _diag_direct_mode = false;
    _diag_en_level = 1;
    _diag_col = 0;
    _diag_row = 0;
    memset(_raw_key_scan, 0, sizeof(_raw_key_scan));
    memset(_scan_raw_key_scan, 0, sizeof(_scan_raw_key_scan));
    memset(_debounced_key_scan, 0, sizeof(_debounced_key_scan));
    memset(_candidate_key_scan, 0, sizeof(_candidate_key_scan));
    memset(_debounce_count, 0, sizeof(_debounce_count));
    memset(_led_level, 0, sizeof(_led_level));
    memset(_scan_led_level, 0, sizeof(_scan_led_level));
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

bool KeyboardDriver::handleTimerTickFromIsr()
{
    return false;
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
