# Keyboard LED Matrix Integration Plan

## Scope

This document explains:

- how the current keyboard HAL scans the existing matrix
- how the new LED matrix is wired
- how the logical key coordinates should map onto the LED hardware
- a practical implementation plan for another agent or developer

This is based on the current `Keyboard` HAL in:

- `main/hal/keyboard/keyboard.h`
- `main/hal/keyboard/keyboard.cpp`


## 1. Current Keyboard HAL

The current keyboard code already scans a matrix through a `74HC138`.

Relevant GPIO definitions in `keyboard.h`:

- `output_list = {8, 9, 11}`: `KEY_A0`, `KEY_A1`, `KEY_A2`
- `input_list = {13, 15, 3, 4, 5, 6, 7}`: 7 keyboard sense lines
- `led_pin_list = {21, 48, 47}`: `LED_EN`, `LED_CLK`, `LED_DAT`

The scan logic is in `Keyboard::_set_output()` and `Keyboard::updateKeyList()`.

Important detail:

- `_set_output()` does not use a straight binary row order.
- It contains a prototype-specific remap:

```cpp
0 -> 0
1 -> 2
2 -> 4
3 -> 6
4 -> 1
5 -> 3
6 -> 5
7 -> 7
```

That remap must be reused by the LED code, because the LED rows share the same row wiring.


## 2. Keyboard Matrix Model Used by the HAL

The public keyboard API uses a logical `Point2D_t` coordinate space:

- `x = 0..13`
- `y = 0..3`

So the firmware sees the keyboard as `4 x 14 = 56` keys.

The electrical matrix is actually `8 x 7`.

The current code compresses the 8 scan rows into 4 logical rows by splitting odd/even `x` positions across two scan rows:

- one scan row handles `x = 1, 3, 5, ... 13`
- another scan row handles `x = 0, 2, 4, ... 12`

The mapping implied by the current code is:

```cpp
scan_index = (3 - y) + ((x & 1) == 0 ? 4 : 0);
column_index = x >> 1;
```

Where:

- `scan_index` is the current logical 0..7 index used by `_set_output()`
- `column_index` is the 0..6 bit position inside the 7-bit column mask

This is the safest mapping to reuse for LEDs, because it exactly matches how keys are already decoded.


## 3. LED Matrix Wiring

### Row side

The LED matrix reuses the same `ROW1..ROW8` nets as the keyboard matrix.

Those rows are driven by the existing `74HC138`:

- `A0/A1/A2` come from `KEY_A0 / KEY_A1 / KEY_A2`
- enable pins are fixed so the decoder is always active
- outputs `Y0..Y7` go through series resistors into the row nets

Because a `74HC138` output is active-low:

- one selected row is driven low
- the other rows stay high

This means the LED matrix should be treated as a multiplexed row-sink design.


### Column side

The LED columns are driven by a `74HC595`:

- `LED_DAT` -> `DS`
- `LED_CLK` -> `SHCP`
- `STCP` is tied to the same clock net as `SHCP`
- `LED_EN` -> `OE#`
- `MR#` is tied high
- `Q0..Q6` drive `LED_COL1..LED_COL7`
- `Q7` is unused

Important behavior:

- because `STCP` and `SHCP` are tied together, outputs update on every shift clock
- therefore `OE#` must be disabled while shifting, otherwise visible garbage can appear
- the pull-up on `LED_EN` makes outputs disabled by default at boot

Recommended update sequence:

1. set `LED_EN` high to disable output
2. shift out the new 8-bit value on `LED_DAT` with `LED_CLK`
3. select the desired row through `_set_output()`
4. set `LED_EN` low to enable output


## 4. Logical LED Mapping

The cleanest software model is to expose LEDs with the same logical coordinates as keys:

- `Point2D_t { x: 0..13, y: 0..3 }`

Then each LED position is converted to one electrical row and one 7-bit column position.

Recommended mapping:

```cpp
scan_index = (3 - pos.y) + ((pos.x & 1) == 0 ? 4 : 0);
column_index = pos.x >> 1;
```

Then store the LED state into a scan buffer:

```cpp
row_mask[scan_index] |= (1 << column_index);
```

This avoids trying to reason directly from schematic labels like `ROW1..ROW8`, which are already hidden behind the keyboard remap logic.


## 5. Recommended Software Structure

The current header already declares:

- `enableLED(bool enabled)`
- `setLED(Point2D_t pos, uint8_t brightness)`
- `updateLED()`

So the HAL shape is already good. What is missing is the implementation and a persistent LED buffer.

Recommended additions inside `Keyboard`:

- `bool _led_enabled`
- `uint8_t _led_scan_buffer[8]`
- `uint8_t _active_led_scan_index`
- optional `uint8_t _led_level[4][14]` if brightness or logical state should be preserved
- optional `uint8_t _pwm_phase` if software PWM is added later

Recommended private helpers:

- `_shift_led_columns(uint8_t value)`
- `_rebuild_led_scan_buffer()`
- `_show_led_scan_row(uint8_t scan_index)`


## 6. Practical Implementation Plan

### Phase 0: fix the local header issue

In the current working tree, `keyboard.h` has a broken private declaration:

```cpp
void (const std::vector<int>& pinList, uint8_t output);
```

That should be restored to:

```cpp
void _set_output(const std::vector<int>& pinList, uint8_t output);
```

Do this first, otherwise the class definition is incomplete.


### Phase 1: basic LED bring-up

Goal:

- verify one LED can be turned on at a known logical coordinate
- verify row/column polarity
- verify no keyboard regression

Steps:

1. initialize `LED_EN`, `LED_CLK`, `LED_DAT` in `Keyboard::init()`
2. keep `LED_EN` high by default
3. implement `_shift_led_columns()`
4. implement `enableLED()`
5. implement `setLED()` with simple on/off semantics
6. implement `updateLED()` to display one scan row per call

At this stage, ignore real brightness and treat:

- `brightness == 0` as off
- `brightness > 0` as on


### Phase 2: stable refresh

There are two possible approaches.

Option A: quick integration

- call `updateLED()` from the same places that already call `updateKeyList()`
- this is easy, but brightness and flicker depend on app loop timing

Option B: production recommendation

- move row refresh into a dedicated periodic service owned by `Keyboard`
- for example a low-priority task or timer that cycles rows at a fixed rate
- protect shared row GPIO access if keyboard scanning and LED refresh can run concurrently

Recommendation:

- use Option A first to validate hardware
- move to Option B after the mapping is confirmed


### Phase 3: brightness support

Real brightness control is not free here.

Why:

- the `74HC595` outputs are digital only
- the matrix is multiplexed
- full `0..255` brightness requires software PWM over time

Recommended approach:

- start with binary on/off
- if needed later, add a small PWM phase counter and compare `brightness > pwm_phase`
- keep expectations realistic; 4-bit or low-resolution PWM is much easier than full 8-bit PWM


## 7. Notes About Bit Order

Because `Q0 -> LED_COL1`, `Q1 -> LED_COL2`, ... `Q6 -> LED_COL7`, a convenient shift format is:

- bit 0 = `LED_COL1`
- bit 1 = `LED_COL2`
- ...
- bit 6 = `LED_COL7`
- bit 7 = unused

Then shift 8 bits from MSB to LSB so the final register state matches the bit positions.

Example:

```cpp
for (int bit = 7; bit >= 0; --bit) {
    write_dat((value >> bit) & 0x01);
    pulse_clk();
}
```


## 8. Hardware Notes To Verify

These should be checked on the real board during bring-up:

- confirm LED polarity with a single-pixel test
- confirm whether `1` in the `74HC595` column mask means LED on or off
- confirm the row remap in `_set_output()` is still valid for the new board spin
- confirm `R33` being `22 ohm` while the other LED column resistors are `120 ohm` is intentional


## 9. Suggested First Test

A minimal hardware test is:

1. initialize the keyboard HAL
2. clear all LED states
3. light one LED at logical `(0, 0)`
4. repeatedly call `updateLED()`
5. move the lit pixel across a few known key positions
6. run `updateKeyList()` while LEDs are active and verify key reads still work

If the lit position appears mirrored or shifted:

- do not change the schematic interpretation first
- check the logical-to-scan mapping formula first
- then check 595 bit order
- then check whether the row remap table in `_set_output()` changed


## 10. Recommended Deliverable for the Implementer

A good first implementation target is:

- LED on/off only
- logical API uses the existing `Point2D_t`
- scan buffer stored as 8 rows x 7 columns
- no new public API beyond the three methods already declared
- no app-level animation yet

Once that works, add:

- refresh scheduling
- optional brightness
- optional per-key effects
