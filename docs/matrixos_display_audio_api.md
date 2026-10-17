# MatrixOS-Compatible Display And Audio APIs

This document defines a forward-compatible display and audio API shape for CardenzaSynth. The intent is to keep new CardenzaSynth common APIs close to MatrixOS style so the same concepts can later be merged into MatrixOS as first-class `MatrixOS::Display` and `MatrixOS::Audio` modules.

## Goals

- Keep the public application-facing API in the `MatrixOS::` style: OS wrappers for applications, device contracts for hardware families.
- Use FreeRTOS primitives and scheduling concepts on firmware: tasks, queues, semaphores, timers, and bounded blocking with timeouts.
- Avoid exposing ESP-IDF, Win32, or board-specific handles through public application APIs.
- Support screen-first devices without weakening the existing MatrixOS LED grid model.
- Support low-latency audio output and optional audio input without tying audio timing to UI frame timing.
- Let CardenzaSynth implement the API first, then upstream the stable contract into MatrixOS.

## Non-Goals

- Do not rewrite CardenzaSynth into MatrixOS immediately.
- Do not force LED grid UI and screen UI into one renderer. They can share input and lifecycle but remain separate output surfaces.
- Do not make audio a normal foreground application loop callback.
- Do not expose native I2S, LCD, DMA, panel, codec, or simulator handles to applications.
- Do not require every MatrixOS target to implement display or audio.

## Layering Model

The API should follow the existing MatrixOS dependency direction:

1. `Device::Display` and `Device::Audio` are hardware contracts implemented by each target.
2. `MatrixOS::Display` and `MatrixOS::Audio` are OS wrappers used by applications and shared runtime code.
3. Applications should prefer `MatrixOS::Display` and `MatrixOS::Audio` over direct `Device::` calls.
4. Concrete hardware code remains under the selected device/family target.

CardenzaSynth can keep its current `Device::Display` and `Device::Audio` namespaces while shaping them toward this contract. Once stable, MatrixOS can add feature flags and matching OS modules.

## Feature Flags

MatrixOS currently uses compile-time device feature flags such as `DEVICE_STORAGE` and `DEVICE_BATTERY`. Display and audio should follow the same pattern.

Recommended flags:

```cmake
set(DEVICE_DISPLAY 1)
set(DEVICE_AUDIO_OUTPUT 1)
set(DEVICE_AUDIO_INPUT 1)
```

Recommended compile definitions:

```cpp
DEVICE_DISPLAY
DEVICE_AUDIO_OUTPUT
DEVICE_AUDIO_INPUT
```

The flags should mean:

- `DEVICE_DISPLAY`: target provides a framebuffer-like screen surface.
- `DEVICE_AUDIO_OUTPUT`: target can produce PCM output.
- `DEVICE_AUDIO_INPUT`: target can capture PCM input.

Targets without a feature should still compile. The OS wrapper should return `Unavailable` or `false` rather than requiring application-level `#ifdef` everywhere.

## Shared Types

Use small POD-style types in public headers. Avoid STL containers in real-time data paths.

```cpp
namespace MatrixOS::Display {

enum class PixelFormat : uint8_t {
  Rgb565,
};

struct Info {
  uint16_t width = 0;
  uint16_t height = 0;
  PixelFormat pixelFormat = PixelFormat::Rgb565;
  bool doubleBuffered = false;
  bool hasBacklight = false;
};

using Pixel = uint16_t;

struct FrameBufferView {
  const Pixel* pixels = nullptr;
  size_t pixelCount = 0;
  uint16_t width = 0;
  uint16_t height = 0;
  uint16_t stridePixels = 0;
};

struct MutableFrameBufferView {
  Pixel* pixels = nullptr;
  size_t pixelCount = 0;
  uint16_t width = 0;
  uint16_t height = 0;
  uint16_t stridePixels = 0;
};

enum class Status : uint8_t {
  Ready,
  Unavailable,
  Busy,
  InvalidArgument,
  Timeout,
  Error,
};

} // namespace MatrixOS::Display
```

```cpp
namespace MatrixOS::Audio {

enum class SampleFormat : uint8_t {
  Int16,
};

enum class Status : uint8_t {
  Ready,
  Unavailable,
  Busy,
  InvalidArgument,
  Timeout,
  Error,
};

struct StreamInfo {
  uint32_t sampleRate = 44100;
  uint8_t channelCount = 1;
  SampleFormat sampleFormat = SampleFormat::Int16;
  size_t preferredFrameCount = 0;
};

using RenderCallback = size_t (*)(int16_t* destination, size_t frameCount, void* userContext);

struct RenderProvider {
  StreamInfo stream;
  RenderCallback render = nullptr;
  void* userContext = nullptr;
};

} // namespace MatrixOS::Audio
```

Notes:

- RGB565 should be the initial display format because CardenzaSynth already uses it and it maps well to small TFT panels.
- Int16 PCM should be the initial audio format because the current synth/audio path uses signed 16-bit samples.
- `preferredFrameCount` lets the target describe its low-latency block size without making applications know DMA details.

## Device Display Contract

Device layer responsibilities:

- Own screen hardware initialization.
- Own framebuffer memory or expose a stable target-owned framebuffer view.
- Present pixels to the panel using the target's DMA/panel mechanism.
- Provide bounded blocking semantics for `Present()`.
- Keep panel callbacks and synchronization inside device code.

Proposed device-facing contract:

```cpp
namespace Device::Display {

using Pixel = MatrixOS::Display::Pixel;
using Info = MatrixOS::Display::Info;
using Status = MatrixOS::Display::Status;
using FrameBufferView = MatrixOS::Display::FrameBufferView;
using MutableFrameBufferView = MatrixOS::Display::MutableFrameBufferView;

bool Available();
const Info& GetInfo();

FrameBufferView CurrentFrameBuffer();
MutableFrameBufferView MutableFrameBuffer();

void Clear(Pixel color = 0);
Status Present(uint32_t timeoutMs = 1000);

Status SetBacklight(uint8_t brightness);
uint8_t Backlight();

} // namespace Device::Display
```

Compatibility with current CardenzaSynth:

- `Device::Display::CurrentFrameBuffer()` already exists.
- `Device::Display::MutableFrameBuffer()` already exists.
- `Device::Display::Clear()` already exists.
- `Device::Display::Present()` already exists but should return status and accept a timeout when the API is formalized.
- `kWidth`, `kHeight`, and `kPixelCount` should eventually move behind `Info` while remaining as convenience constants for fixed-size targets.

## MatrixOS Display API

Applications should use the OS wrapper:

```cpp
namespace MatrixOS::Display {

bool Available();
const Info& GetInfo();

FrameBufferView CurrentFrameBuffer();
MutableFrameBufferView MutableFrameBuffer();

void Clear(Pixel color = 0);
Status Present(uint32_t timeoutMs = 1000);

Status SetBacklight(uint8_t brightness);
uint8_t Backlight();

} // namespace MatrixOS::Display
```

The first implementation can be a thin wrapper over `Device::Display`. Later, MatrixOS may add display layers, damage rectangles, or a screen UI framework above this base API.

## Display Scheduling

Display rendering should stay outside interrupt context.

Recommended firmware model:

- App or UI code draws into a target-owned framebuffer.
- `MatrixOS::Display::Present(timeoutMs)` submits the buffer to the device layer.
- Device layer waits for DMA/panel completion using a FreeRTOS semaphore or task notification.
- Panel ISR/callback only gives the semaphore or sends a task notification.
- No application callbacks run from display ISR context.

For CardenzaSynth, the current `Device::Display::Present()` pattern already follows this shape: the panel callback gives a semaphore and the caller waits for completion.

## Device Audio Contract

Device layer responsibilities:

- Own codec, I2S, DMA, and mic hardware initialization.
- Own audio pump task creation and buffering.
- Pull samples from an OS-provided render callback in task context.
- Avoid heap allocation, file I/O, logging spam, and long locks in the audio render path.
- Provide optional input capture through bounded reads.

Proposed device-facing contract:

```cpp
namespace Device::Audio {

using Status = MatrixOS::Audio::Status;
using StreamInfo = MatrixOS::Audio::StreamInfo;
using RenderProvider = MatrixOS::Audio::RenderProvider;

bool OutputAvailable();
bool InputAvailable();

StreamInfo OutputInfo();
StreamInfo InputInfo();

Status SetOutputRenderProvider(RenderProvider provider);
Status StartOutput();
void StopOutput();

Status StartInput();
void StopInput();
Status ReadInputFrames(int16_t* destination,
                       size_t frameCount,
                       size_t* framesRead,
                       uint32_t timeoutMs = 0);

} // namespace Device::Audio
```

Compatibility with current CardenzaSynth:

- `Device::Audio::InputInfo()`, `StartInput()`, `StopInput()`, and `ReadInputFrames()` already exist.
- `device::SetSynthAudioRenderProvider()` should evolve toward `MatrixOS::Audio::SetOutputRenderProvider()`.
- `device::ConnectSynthAudioPipeline()` should evolve toward `MatrixOS::Audio::StartOutput()` or a small compatibility wrapper around it.
- The current Cardenza firmware audio pump task is already close to the target model: it renders fixed blocks, applies volume, writes I2S, and logs overruns outside interrupt context.

## MatrixOS Audio API

Applications and shared runtimes should use the OS wrapper:

```cpp
namespace MatrixOS::Audio {

bool OutputAvailable();
bool InputAvailable();

StreamInfo OutputInfo();
StreamInfo InputInfo();

Status SetOutputRenderProvider(RenderProvider provider);
Status StartOutput();
void StopOutput();

Status StartInput();
void StopInput();
Status ReadInputFrames(int16_t* destination,
                       size_t frameCount,
                       size_t* framesRead,
                       uint32_t timeoutMs = 0);

} // namespace MatrixOS::Audio
```

The API is intentionally stream-oriented rather than synth-specific. Synth engines, samplers, and future apps can all provide PCM without knowing about the selected device backend.

## Audio Scheduling

Audio must not be driven by UI frame timing.

Recommended firmware model:

- Device audio owns a high-priority FreeRTOS task.
- The task fills fixed-size PCM blocks by calling the registered render callback.
- The task writes blocks to I2S/DMA with bounded blocking appropriate for the backend.
- The render callback runs in task context, not ISR context.
- Start/stop/provider changes are synchronized with a FreeRTOS mutex or critical section small enough not to affect audio timing.
- Overrun logging should be rate-limited.

Priority guidance:

- Audio output task should be higher priority than application UI rendering.
- Keyboard scan and LED multiplex timing remain hardware-critical and must not be blocked by audio locks.
- Sequencer clock work should be precise, but it should emit MIDI/events from task context and not starve audio.

For MatrixOS upstream, avoid requiring ESP-IDF-only APIs such as `xTaskCreatePinnedToCore()` in the public design. Targets may optionally pin tasks internally when the port supports it.

## FreeRTOS Interface Rules

Use these rules for both CardenzaSynth and future MatrixOS code:

- Public APIs use `uint32_t timeoutMs` for bounded waits, matching existing MatrixOS API style.
- Internal firmware code converts to ticks with `pdMS_TO_TICKS()`.
- ISRs should only use ISR-safe FreeRTOS operations such as `xSemaphoreGiveFromISR()`, `xQueueSendFromISR()`, or task notifications.
- Callbacks registered by applications must not execute in ISR context.
- Do not allocate memory from the audio render callback or display ISR callback.
- Prefer static task allocation for always-on system tasks when stack sizing is known.
- Use queues or task notifications to cross ISR-to-task boundaries.
- Use mutexes only around short state changes. Do not hold a mutex while calling app render callbacks if that mutex is also needed by UI or control paths.

## Interaction With MatrixOS LED And Input

Display and audio should not replace MatrixOS LED and input APIs.

- `MatrixOS::LED` remains the API for grid LEDs and touchbar/key LEDs.
- `MatrixOS::Display` is a separate screen framebuffer API.
- `MatrixOS::Input` remains the source of input events and live input state.
- Devices with both screen and LEDs should update them through separate device contracts.
- Screen presentation must not happen from the high-frequency LED scan path.
- Audio rendering must not depend on LED update callbacks.

For CardenzaSynth, this means the keyboard LED scan buffer continues to be updated through the normal LED API and committed by the device keyboard driver, while display rendering uses `Display::Present()` and audio uses the audio pump task.

## App Lifecycle Policy

MatrixOS currently assumes one foreground app. Audio-capable screen devices need an explicit policy:

- A foreground app may register an audio render provider.
- Launching a new foreground app should either stop the previous provider or transfer ownership through a system service.
- Long-lived instrument behavior should eventually use a privileged audio service or system app if playback must survive normal app switches.
- The initial CardenzaSynth-compatible design may keep audio ownership with the active Synth app.

This policy should be decided before fully upstreaming a synth app into MatrixOS.

## CardenzaSynth Migration Plan

1. Add a local documentation-backed API target shape matching this document.
2. Rename or wrap `device::SetSynthAudioRenderProvider()` as `MatrixOS::Audio::SetOutputRenderProvider()` in CardenzaSynth.
3. Rename or wrap `device::ConnectSynthAudioPipeline()` as `MatrixOS::Audio::StartOutput()` while keeping the synth compatibility call if needed.
4. Add status-returning `Device::Display::Present(timeoutMs)` and `Device::Display::GetInfo()` while preserving existing convenience functions.
5. Keep the existing `ui::Canvas` framebuffer renderer, but make it consume `MatrixOS::Display::MutableFrameBuffer()` once the wrapper exists.
6. Keep simulator and firmware implementations behind the same Device contract.
7. When stable, port the same headers and thin OS wrappers into MatrixOS under `OS/Display` and `OS/Audio`.

## MatrixOS Upstream Plan

1. Add `DEVICE_DISPLAY`, `DEVICE_AUDIO_OUTPUT`, and `DEVICE_AUDIO_INPUT` feature flags.
2. Extend `Devices/Device.h` with optional `Device::Display` and `Device::Audio` contracts guarded by those flags.
3. Add `OS/Display` and `OS/Audio` modules.
4. Add their headers to `OS/MatrixOS.h` or keep declarations in the umbrella header, following existing MatrixOS conventions.
5. Link `MatrixOSDisplay` and `MatrixOSAudio` from the umbrella `MatrixOS` target when the feature flags are enabled.
6. Add no-op or unavailable wrappers for targets that do not support the feature.
7. Create a simple MatrixOS sample app that clears/presents the display and plays a generated tone without using device-specific APIs.

## Open Decisions

- Whether `Color` should be reused for screen pixels or whether screen pixels stay explicitly `uint16_t` RGB565.
- Whether MatrixOS should introduce a screen UI framework or keep only the framebuffer primitive initially.
- Whether audio provider ownership belongs to the foreground app or a privileged system audio service.
- Whether display presentation should support damage rectangles in the first upstream version.
- Whether audio input should expose blocking reads only or also callback/queue-based capture.

## Recommended First Version

For CardenzaSynth, keep the first version deliberately small:

- RGB565 framebuffer display.
- One mutable framebuffer owned by the device layer.
- Synchronous `Present(timeoutMs)` with device-internal DMA/panel completion sync.
- Int16 PCM output with one render provider.
- Optional int16 mono input with blocking reads.
- No display layers, no audio graph, no multi-client mixer.

This is enough for the Synth app, sampler, sequencer, simulator, and a future MatrixOS Cardenza target while keeping the API narrow enough to upstream cleanly.