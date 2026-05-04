#pragma once

#include <cstdint>

#if __has_include(<esp_err.h>)
#include <esp_err.h>
#else
using esp_err_t = int;
inline constexpr esp_err_t ESP_OK = 0;
inline constexpr esp_err_t ESP_FAIL = -1;
inline constexpr esp_err_t ESP_ERR_INVALID_ARG = -2;
#endif

#if __has_include(<driver/i2c_master.h>)
#include <driver/i2c_master.h>
#else
struct i2c_master_bus_t;
using i2c_master_bus_handle_t = i2c_master_bus_t*;
#endif

#if __has_include(<driver/i2s_std.h>)
#include <driver/i2s_std.h>
#elif __has_include(<driver/i2s_common.h>)
#include <driver/i2s_common.h>
#elif __has_include(<driver/i2s.h>)
#include <driver/i2s.h>
#else
struct i2s_channel_t;
using i2s_chan_handle_t = i2s_channel_t*;
#endif

#if __has_include(<esp_lcd_panel_io.h>)
#include <esp_lcd_panel_io.h>
#else
struct esp_lcd_panel_io_t;
using esp_lcd_panel_io_handle_t = esp_lcd_panel_io_t*;
#endif

#if __has_include(<esp_lcd_panel_ops.h>)
#include <esp_lcd_panel_ops.h>
#else
struct esp_lcd_panel_t;
using esp_lcd_panel_handle_t = esp_lcd_panel_t*;
#endif

#if __has_include(<sdmmc_cmd.h>)
#include <sdmmc_cmd.h>
#else
struct sdmmc_card_t;
#endif