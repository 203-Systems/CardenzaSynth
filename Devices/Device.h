#pragma once

#include <bitset>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace Device {

namespace Display {

inline constexpr int kWidth = 240;
inline constexpr int kHeight = 135;
inline constexpr std::size_t kPixelCount = static_cast<std::size_t>(kWidth) * static_cast<std::size_t>(kHeight);

using Pixel = std::uint16_t;

struct FrameBufferView {
  const Pixel* pixels = nullptr;
  std::size_t pixelCount = kPixelCount;
  int width = kWidth;
  int height = kHeight;
};

struct MutableFrameBufferView {
  Pixel* pixels = nullptr;
  std::size_t pixelCount = kPixelCount;
  int width = kWidth;
  int height = kHeight;

  operator FrameBufferView() const {
    return FrameBufferView{pixels, pixelCount, width, height};
  }
};

FrameBufferView CurrentFrameBuffer();
MutableFrameBufferView MutableFrameBuffer();
void Clear(Pixel color = 0);
void Present();

// Power-on level of the firmware HAL's backlight PWM (the light UI needs less than full).
inline constexpr std::uint8_t kDefaultBacklight = 180;

// Backlight PWM level, 0 (off) .. 255 (full). Takes effect immediately, or when the display
// starts if it has not been initialized yet.
void SetBacklight(std::uint8_t level);
std::uint8_t Backlight();

}  // namespace Display

namespace Keyboard {

inline constexpr int kRows = 4;
inline constexpr int kColumns = 14;
inline constexpr int kKeyCount = kRows * kColumns;

using MatrixKeyState = std::bitset<kKeyCount>;

const MatrixKeyState& State();

}  // namespace Keyboard

namespace Buttons {

bool HomePressed();

}  // namespace Buttons

namespace LED {

inline constexpr int kRows = Keyboard::kRows;
inline constexpr int kColumns = Keyboard::kColumns;
inline constexpr int kLedCount = kRows * kColumns;
inline constexpr std::uint8_t kBrightnessOff = 0;
inline constexpr std::uint8_t kBrightnessMax = 15;
// Key levels are perceived brightness (0 off, 15 full); the device gamma-corrects them
// (Devices/LedLight.h). The global level is the KEY LED setting.

// Stage a complete update without blanking the currently displayed LED frame.
void BeginFrame();
void Present();
std::uint8_t Brightness(std::size_t row, std::size_t column);
void SetKey(std::size_t row, std::size_t column, std::uint8_t brightness);
void Clear();
void Fill(std::uint8_t brightness);
// Master intensity scales each key's level; it does not clip bright states together.
void SetGlobalBrightness(std::uint8_t brightness);
std::uint8_t GlobalBrightness();
void SetEnabled(bool enabled);
bool Enabled();

}  // namespace LED

namespace Audio {

inline constexpr int kOutputChannels = 2;
inline constexpr bool kHasInput = true;
inline constexpr std::uint32_t kDefaultInputSampleRate = 44100;
inline constexpr std::uint8_t kInputChannels = 1;
inline constexpr std::uint8_t kInputBitsPerSample = 16;

struct SampleBufferView {
  const std::int16_t* samples = nullptr;
  std::size_t frameCount = 0;
};

struct InputStreamInfo {
  std::uint32_t sampleRate = kDefaultInputSampleRate;
  std::uint8_t channelCount = kInputChannels;
  std::uint8_t bitsPerSample = kInputBitsPerSample;
};

enum class InputStatus {
  Ready,
  NotReady,
  InvalidArgument,
  Overflow,
};

inline constexpr std::uint8_t kDefaultOutputVolume = 100;

// Master output volume in percent; values above 100 are clamped. The device output stage applies
// it after the synth mix, so it never changes what the synth renders.
void SetOutputVolume(std::uint8_t percent);
std::uint8_t OutputVolume();

InputStreamInfo InputInfo();
InputStatus StartInput();
void StopInput();
InputStatus ReadInputFrames(std::int16_t* destination,
                            std::size_t frameCount,
                            std::size_t& framesRead,
                            std::uint32_t timeoutMs = 0);

}  // namespace Audio

namespace Storage {

struct FileHandleView {
  const char* path = nullptr;
  bool writable = false;
};

// Low-level storage only. Folders and files on the card belong to the OS storage protocol
// (OS/Storage, docs/app-storage-protocol.md); apps never call these functions directly.

// Root of the removable card: "/sdcard" on the device, "<working directory>/sdcard" in the
// simulator (beside the executable on Windows).
std::string RootPath();
// Mounts the card when it is not mounted. False when it is missing or cannot be mounted; the device
// may limit how often a missing card is probed.
bool Mount();
bool IsMounted();
// Size and free space of the mounted card in bytes (0 when unknown).
std::uint64_t CapacityBytes();
std::uint64_t FreeBytes();

// Small records in internal flash that work without a card: NVS on the device, files in
// "<working directory>/internal" in the simulator. Keys are 1..15 characters of [a-z0-9_].
inline constexpr std::size_t kInternalKeyMax = 15;
inline constexpr std::size_t kInternalValueMax = 4000;
bool ReadInternal(std::string_view key, std::string& value);
bool WriteInternal(std::string_view key, std::string_view value);
bool EraseInternal(std::string_view key);

}  // namespace Storage

std::string Name();
std::string BackendName();
bool Init();
void Start();

}  // namespace Device
