#include "Devices/Device.h"
#include "Devices/LedFrameBuffer.h"
#include "Devices/CardenzaSim/SimulatorInput.h"

#include <mutex>
#include <deque>
#include <string>
#include <algorithm>
#include <array>
#include <atomic>

namespace {

std::array<Device::Display::Pixel, Device::Display::kPixelCount> frameBuffer{};
Device::Keyboard::MatrixKeyState keyboardState{};
bool homeButtonPressed = false;
Device::LED::detail::LedFrameBuffer keyboardLedFrames;
bool audioInputEnabled = false;
// The simulator has no backlight; keep the level so settings round-trip like on hardware.
std::atomic<std::uint8_t> backlightLevel{Device::Display::kDefaultBacklight};

constexpr bool IsValidMatrixPosition(std::size_t row, std::size_t column) {
  return row < static_cast<std::size_t>(Device::LED::kRows) &&
         column < static_cast<std::size_t>(Device::LED::kColumns);
}

constexpr std::size_t MatrixIndex(std::size_t row, std::size_t column) {
  return row * static_cast<std::size_t>(Device::LED::kColumns) + column;
}

}  // namespace

namespace {

// A computer on the USB port (CardenzaSim::Input drives it, Device::USB is what the firmware sees).
std::mutex g_usbMutex;
bool g_usbConnected = false;
std::deque<std::array<std::uint8_t, 4>> g_toDevice, g_fromDevice;
// App messages (the guide): a page in the browser or a test is the computer. They need no cable.
std::deque<std::string> g_appToDevice, g_appFromDevice;
constexpr std::size_t kAppQueue = 64;

}  // namespace

namespace CardenzaSim::Input {

void SetMatrixKey(std::size_t row, std::size_t column, bool pressed) {
  if (!IsValidMatrixPosition(row, column)) {
    return;
  }

  keyboardState.set(MatrixIndex(row, column), pressed);
}

void SetHomeButton(bool pressed) {
  homeButtonPressed = pressed;
}

void ClearMatrixKeys() {
  keyboardState.reset();
}

void SetComputerConnected(bool connected)
{
  std::lock_guard<std::mutex> lock(g_usbMutex);
  g_usbConnected = connected;
  if (!connected) g_toDevice.clear();
}

void SendComputerMidi(const std::uint8_t packet[4])
{
  std::lock_guard<std::mutex> lock(g_usbMutex);
  if (g_usbConnected) g_toDevice.push_back({packet[0], packet[1], packet[2], packet[3]});
}

void SendComputerApp(const std::string& message)
{
  std::lock_guard<std::mutex> lock(g_usbMutex);
  if (g_appToDevice.size() < kAppQueue) g_appToDevice.push_back(message.substr(0, Device::USB::kAppMessageSize));
}

bool TakeDeviceApp(std::string& message)
{
  std::lock_guard<std::mutex> lock(g_usbMutex);
  if (g_appFromDevice.empty()) return false;
  message = std::move(g_appFromDevice.front());
  g_appFromDevice.pop_front();
  return true;
}

bool TakeDeviceMidi(std::uint8_t packet[4])
{
  std::lock_guard<std::mutex> lock(g_usbMutex);
  if (g_fromDevice.empty()) return false;
  std::copy(g_fromDevice.front().begin(), g_fromDevice.front().end(), packet);
  g_fromDevice.pop_front();
  return true;
}

}  // namespace CardenzaSim::Input

namespace Device {

namespace Display {

FrameBufferView CurrentFrameBuffer() {
  return FrameBufferView{frameBuffer.data(), frameBuffer.size(), kWidth, kHeight};
}

MutableFrameBufferView MutableFrameBuffer() {
  return MutableFrameBufferView{frameBuffer.data(), frameBuffer.size(), kWidth, kHeight};
}

void Clear(Pixel color) {
  frameBuffer.fill(color);
}

void Present() {}

void SetBacklight(std::uint8_t level) {
  backlightLevel.store(level, std::memory_order_relaxed);
}

std::uint8_t Backlight() {
  return backlightLevel.load(std::memory_order_relaxed);
}

}  // namespace Display

namespace Keyboard {

const MatrixKeyState& State() {
  return keyboardState;
}

}  // namespace Keyboard

namespace USB {

bool Connected()
{
  std::lock_guard<std::mutex> lock(g_usbMutex);
  return g_usbConnected;
}

bool ReadMidi(std::uint8_t packet[4])
{
  std::lock_guard<std::mutex> lock(g_usbMutex);
  if (g_toDevice.empty()) return false;
  std::copy(g_toDevice.front().begin(), g_toDevice.front().end(), packet);
  g_toDevice.pop_front();
  return true;
}

bool WriteMidi(const std::uint8_t packet[4])
{
  std::lock_guard<std::mutex> lock(g_usbMutex);
  if (!g_usbConnected) return false;
  g_fromDevice.push_back({packet[0], packet[1], packet[2], packet[3]});
  return true;
}

bool ReadAppMessage(AppMessage& message)
{
  std::lock_guard<std::mutex> lock(g_usbMutex);
  if (g_appToDevice.empty()) return false;
  const std::string& text = g_appToDevice.front();
  message.length = std::min(text.size(), kAppMessageSize);
  std::copy_n(text.data(), message.length, message.data);
  g_appToDevice.pop_front();
  return true;
}

bool WriteAppMessage(const char* data, std::size_t length)
{
  std::lock_guard<std::mutex> lock(g_usbMutex);
  // Nobody reads: the oldest goes.
  if (g_appFromDevice.size() >= kAppQueue) g_appFromDevice.pop_front();
  g_appFromDevice.emplace_back(data, std::min(length, kAppMessageSize));
  return true;
}

}  // namespace USB

namespace Buttons {

bool HomePressed() {
  return homeButtonPressed;
}

}  // namespace Buttons

namespace LED {

void BeginFrame() { keyboardLedFrames.BeginFrame(); }
void Present() { (void)keyboardLedFrames.Present(); }

std::uint8_t Brightness(std::size_t row, std::size_t column) {
  if (!IsValidMatrixPosition(row, column)) {
    return kBrightnessOff;
  }

  return keyboardLedFrames.Current().levels[MatrixIndex(row, column)];
}

void SetKey(std::size_t row, std::size_t column, std::uint8_t brightness) {
  if (!IsValidMatrixPosition(row, column)) {
    return;
  }

  (void)keyboardLedFrames.SetKey(row, column, brightness);
}

void Clear() {
  (void)keyboardLedFrames.Fill(kBrightnessOff);
}

void Fill(std::uint8_t brightness) {
  (void)keyboardLedFrames.Fill(brightness);
}

void SetGlobalBrightness(std::uint8_t brightness) {
  (void)keyboardLedFrames.SetMaster(brightness);
}

std::uint8_t GlobalBrightness() {
  return keyboardLedFrames.Current().master;
}

void SetEnabled(bool enabled) {
  (void)keyboardLedFrames.SetEnabled(enabled);
}

bool Enabled() {
  return keyboardLedFrames.Current().enabled;
}

}  // namespace LED

std::string Name() {
#if defined(__EMSCRIPTEN__)
  return "CardenzaWeb";  // Devices/CardenzaWeb reuses this facade in the browser.
#else
  return "CardenzaSim";
#endif
}

std::string BackendName() {
#if defined(__EMSCRIPTEN__)
  return "sim-web";
#else
  return "sim-host";
#endif
}

bool Init() {
  // The card folders themselves belong to the OS storage protocol (os::storage).
  (void)Storage::Mount();
  return true;
}

void Start() {}

}  // namespace Device
