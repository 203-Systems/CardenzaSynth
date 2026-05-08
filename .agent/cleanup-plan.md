# ComputeCardOS Cleanup Plan

> Review log for turning the current ESP32-S3 synth project into a more formal, maintainable codebase. This file is intentionally incremental: write findings as they are confirmed, then evolve them into a migration plan.

## 2026-05-08 First Pass: Size And Boundary Smells

### Confirmed oversized project-owned files

These are the first breakup candidates, excluding `build/`, `out/`, `.git/`, and `third_party/`:

| File | Approx lines | First read |
|---|---:|---|
| `Library/Synth/SynthUiModel.cpp` | 2000+ | Menu grammar, preset browser, DX7/Juno parameter grouping, modifier state, transitions, snapshots, and input handling live together. |
| `Library/Synth/SynthUiRenderer.cpp` | 1800+ | Generic screen layout, marquee text, bar rendering, home rendering, context rendering, DX7 algorithm drawing, and Juno panel drawing live together. |
| `Devices/ComputeCard/computecard/keyboard/keyboard.cpp` | ~900 | Matrix scan, LED multiplexing, debounce/public state, ISR/timer/task logic, and hardware details are likely mixed. |
| `Devices/ComputeCardSim/DesktopHost.cpp` | ~750 | Host window, simulator shell, app/system bootstrapping, input mapping, and rendering bridge are likely mixed. |
| `Library/Synth/Audio/Amy/SynthAudioRuntimeAmy.cpp` | ~650 | AMY platform integration, voice/event translation, and runtime render path should be reviewed before adding more engines. |

`OS/UI/BuiltInFonts.cpp` is larger than all of these, but it is generated/static asset data, so it should be treated as a build/asset pipeline issue rather than hand-refactored application logic.

### Boundary smells already visible

- `cmake/ComputeCardOSSourceLists.cmake` is a central handwritten source registry for apps, OS, synth core, device HAL, firmware sources, and AMY C files. It works as glue today, but it is becoming the unofficial architecture.
- The intended MatrixOS-like seams are already present (`Devices`, `OS`, `Library`, `Applications`), but several files appear to cross them directly: synth app touches device display/LEDs, device audio includes synth runtime, and OS system owns both application registry and device lifecycle.
- `SynthUiModel.cpp` and `SynthUiRenderer.cpp` have become product-specific feature accumulators. The renderer especially contains synth-domain drawing code for DX7 and Juno rather than a small UI rendering surface plus engine-specific view modules.

### Initial cleanup direction

Do not start by rewriting everything. Start by making current boundaries explicit and moving code behind them in small, verifiable slices:

1. Split `Library/Synth` UI into model/menu grammar, engine-specific UI adapters, snapshot types, and renderer modules.
2. Split ComputeCard keyboard HAL into hardware scan, LED scan buffer/multiplexing, key state publishing, and public facade.
3. Replace the central CMake source registry with ownership-local targets/source lists where practical, while keeping ESP-IDF compatibility.
4. Convert app/device/audio reverse dependencies into interfaces owned by the layer above or a narrow shared runtime boundary.

### Next review slice

Compare MatrixOS module wiring in `Applications`, `OS`, and `Devices` against ComputeCardOS equivalents. Then inspect the exact include/dependency graph around `SynthApp`, `DeviceAudio`, `OS/System`, and `OS/UI/Canvas`.

## 2026-05-08 MatrixOS Comparison: Keep The Seams, Fix The Direction

### What MatrixOS does better structurally

- MatrixOS builds public seams as CMake targets: `DeviceInterface`, `MatrixOSInterface`, `MatrixOSAPPInterface`, and application libraries. That makes dependency intent visible in the build graph.
- Device families own their config and application list. Applications are added from `ApplicationList.txt`, and a generated `Applications.h` registers only enabled apps.
- The OS owns app supervision and lifecycle. Applications are instantiated through metadata/factories instead of hard-coded direct construction in the main system loop.

ComputeCardOS has the same top-level folders, but its actual source graph is less formal because `cmake/ComputeCardOSSourceLists.cmake` manually pulls large chunks of every layer into firmware/simulator targets.

### Confirmed dependency direction problems

- `Applications/Synth/SynthApp.cpp` directly drives `Device::LED`, `Device::Display`, `os::midi`, `SynthRack`, `SynthUiModel`, and `SynthUiRenderer`. That makes the app both product logic and hardware presentation coordinator.
- Before the runtime split, `Library/Synth/Core/ISynthEngine.h` included `Devices/DeviceAudio.h`. That board audio type has now been removed from the public synth engine interface; the remaining MIDI packet dependency should be reviewed separately.
- `Library/Synth/Core/PagedSynthEngine.cpp` directly creates `midi::MidiPort`, calls `device::ConnectSynthAudioPipeline()`, and syncs `synth::audio::*` runtime state. This is too much ownership for a base engine class.
- `Devices/ComputeCard/DeviceAudio.cpp` and `Devices/ComputeCardSim/DeviceAudio.cpp` call `synth::audio::RenderFrames()`. That creates a reverse dependency from device output back into synth runtime.
- `OS/System/System.cpp` hard-codes built-in app registration and directly owns device display/LED frame cadence. This is fine as a prototype loop, but should become a scheduler/runtime service with clearer extension points.
- `OS/UI/Canvas.h` aliases `ui::Color` to `Device::Display::Pixel` and stores `Device::Display::MutableFrameBufferView`. This is acceptable for a tiny embedded renderer short term, but it prevents the UI library from being tested without a device facade.

### Direction to formalize this

Treat the current code as having four runtimes that need explicit boundaries:

1. **System runtime**: boot, frame loop, app lifecycle, input dispatch, display present, LED update cadence.
2. **Application runtime**: synth app state, performance key state, track selection, command handling.
3. **Synth runtime**: engine registry, parameters/presets, MIDI consumption, audio events, render callback.
4. **Device runtime**: display framebuffer, keyboard scan, LED output, codec/I2S/waveOut transport, storage/mic/IR.

The cleanup should make data flow one-way where possible: `Device input -> System -> App -> Synth commands -> Audio runtime`, and `Audio transport -> render callback` rather than `DeviceAudio -> SynthAudioRuntime` hard-coded.

### Next review slice

Inspect `SynthUiModel.cpp` and `SynthUiRenderer.cpp` by responsibility. The goal is to propose concrete files/classes to extract without changing behavior first.

## 2026-05-08 Synth UI: Break Up By Responsibility First

### `SynthUiModel.cpp` responsibilities found

The model file currently contains at least these independent concerns:

- Generic matrix UI state machine: modifier hold timing, home/context transitions, latched context, active track, menu highlight, key press/release handling.
- Generic menu helpers: `ContextMenuCell`, paged menu construction, back/next-page behavior, context bar snapshots.
- Sound workflow: engine picker, preset browser, preset search keymap, preset load/save/copy actions.
- Parameter workflow: browse mode, adjust mode, discrete action pages, selected parameter state.
- DX7-specific UI semantics: operator groups, algorithm/global summaries, DX7 parameter label aliases, DX7 group parameter collection.
- Juno-specific UI semantics: section groups, direct-action groups, discrete parameters, Juno snapshot value population, Juno summaries.
- Snapshot construction: home snapshot, context snapshot, left/right bar snapshot, transition snapshots.

The safest first split is mostly mechanical:

| New file | Responsibility |
|---|---|
| `Library/Synth/SynthUiTypes.h` | `ContextMenuCell`, `UiBarSnapshot`, `UiScreenSnapshot`, transition snapshot, matrix handling enums. |
| `Library/Synth/SynthUiMenu.h/.cpp` | Generic paged-menu helpers, modifier labels, modifier row mapping, home/context bar construction helpers. |
| `Library/Synth/SynthUiSoundMenu.h/.cpp` | Engine picker, preset browser, preset search, sound menu actions/data helpers. |
| `Library/Synth/SynthUiDx7Menu.h/.cpp` | DX7 group IDs, labels, summaries, parameter collection and menu labels. |
| `Library/Synth/SynthUiJunoMenu.h/.cpp` | Juno section IDs, labels, summaries, discrete action metadata, snapshot value population. |
| `Library/Synth/SynthUiModel.cpp` | Keep only the state machine and orchestration methods after helpers move out. |

Do this before changing behavior. Compile after each extraction so regression surface stays small.

### `SynthUiRenderer.cpp` responsibilities found

The renderer mixes reusable drawing primitives with synth-engine-specific drawing:

- Generic renderer: colors, rect helpers, text truncation/marquee, fade/mix, bar layout, transition interpolation, frame composition.
- Home screen: track label, mini keyboard, looper bar, home content.
- Sound context: preset browser and sound status views.
- DX7 renderer: 32 algorithm visual table, operator graph routing, operator pane, DX7 parameter content.
- Juno renderer: overview, DCO/VCF/ENV/LFO/VCA/CHO graphics, value pages, section-specific focus rendering.

Recommended split:

| New file | Responsibility |
|---|---|
| `Library/Synth/SynthUiRenderPrimitives.h/.cpp` | Color constants, rect helpers, text fit/marquee, shape helpers, segmented bars. |
| `Library/Synth/SynthUiFrameRenderer.cpp` | `RenderSynthUiFrame`, layout, transition frame composition, home/context dispatch. |
| `Library/Synth/SynthUiHomeRenderer.cpp` | Home content, track label, mini keyboard, looper bar. |
| `Library/Synth/SynthUiSoundRenderer.cpp` | Sound context and preset browser rendering. |
| `Library/Synth/SynthUiDx7Renderer.cpp` | DX7 algorithm table/graph/operator pane/parameter content. |
| `Library/Synth/SynthUiJunoRenderer.cpp` | Juno overview and section graphics. |

The current public header can remain `SynthUiRenderer.h` initially. Internals can move into private headers under `Library/Synth/ui/` later if the folder starts to grow too much.

### Important cleanup notes

- Some formatting drift exists in both UI files, especially around long initializer lists and nested blocks. Run the repo formatter after mechanical moves, but avoid mixing formatting-only churn with behavior changes.
- `UiScreenSnapshot` currently carries many engine-specific Juno fields and DX7 values. Long term, consider `std::variant`-like typed content snapshots, but that should come after the mechanical split.
- Do not introduce a heavyweight widget framework yet. The current immediate-mode snapshot renderer fits the 240x135 embedded screen; the problem is file ownership, not the rendering model itself.

### Next review slice

Inspect ComputeCard HAL, especially keyboard/audio, and decide which parts are real hardware contracts versus bring-up scaffolding.

## 2026-05-08 ComputeCard HAL: Separate Contracts From Bring-Up Code

### Current hardware facade shape

`Devices/Device.h` is already a useful application-facing facade: display framebuffer, keyboard state, LED state, audio constants, storage placeholder, `Init()`, and `Start()`. This is close to the MatrixOS-style device seam and should be preserved.

The ComputeCard-specific HAL under `Devices/ComputeCard/computecard` is also mostly in the right place, but several modules still mix public contract, hardware implementation, diagnostic tooling, and host compatibility in the same files.

### Keyboard HAL findings

`Devices/ComputeCard/computecard/keyboard/keyboard.cpp` currently combines:

- physical pin setup and fast GPIO writes,
- low-level SPI/shift-register LED column output,
- GPTimer ISR row scanning,
- LED PWM phase and committed scan buffers,
- debounce and key list publishing task,
- public keyboard facade wrappers,
- diagnostic direct LED/row mode,
- non-ESP host fallback implementation.

Task 6 cleanup started by removing the keyboard non-ESP fallback implementation and keeping `keyboard/keyboard.cpp` in ESP-IDF/embedded source lists only. `keyboard/keyboard.h` now derives its matrix and LED pin lists from `computecard/board/pins.h` and no longer publishes fake FreeRTOS typedefs for host builds.

The remaining ComputeCard hardware HAL implementations now fail explicitly outside ESP-IDF instead of returning host placeholder values. Desktop validation should go through `ComputeCardSim`; the hardware target is firmware-only. Audio output state has also been collapsed into `Audio::OutputStatus`, replacing separate codec probe/output flags.

The keyboard HAL split is now in place: `keyboard.h` is the facade, `types.h` owns public data types, `layout.h` owns matrix/keymap data, and `keyboard/internal` owns the driver declaration plus hardware, key scan, LED scan, and diagnostics implementations.

Simulator cleanup has started by adding a simulator-only keyboard input bridge. The desktop host now mutates `Device::Keyboard` state and lets `os::system::RunFrame()` dispatch matrix key events, instead of calling `SynthApp::PressMatrixKey()`/`ReleaseMatrixKey()` directly.

Recommended split:

| New file | Responsibility |
|---|---|
| `keyboard/keyboard.h` | Public HAL contract only: key points, key list/state APIs, LED mutators. No FreeRTOS headers or task/semaphore typedefs. |
| `keyboard/keyboard_types.h` | `Point2D_t`, `KeyValue_t`, key state structs, matrix constants. |
| `keyboard/keyboard_layout.h` | Key label/HID mapping and matrix coordinate mapping. |
| `keyboard/keyboard_led_scan.cpp` | LED column mask, global brightness, frame commit, shift-register/SPI output. |
| `keyboard/keyboard_key_scan.cpp` | GPIO row/input scan, debounce, task/ISR coordination. |
| `keyboard/keyboard_diagnostics.cpp` | `setDiagDirect()` and any future test-only scan modes. Build-gate it if it should not ship. |
| `keyboard/keyboard_host_stub.cpp` | Non-ESP behavior, instead of keeping the host fallback inside the production ESP implementation file. |

Also move the hard-coded `output_list`, `input_list`, and `led_pin_list` out of `keyboard.h`; `computecard/board/pins.h` is already the board pin source of truth.

### Audio HAL findings

The audio HAL is closer to a formal module than keyboard:

- `audio/audio.cpp` owns I2S channel setup, output enable, frame writes, cached volume, output mode, and delayed codec setup after clocks start.
- `audio/codec/es8156.cpp` owns the codec register table and I2C device handle.

Cleanup still needed:

- Replace the loose `codecProbeAttempted` / `codecAvailable` state with an explicit output capability/status struct, e.g. `AudioOutputStatus { i2sReady, codecControlReady, nativeOutputReady, lastError }`.
- Keep codec control optional, but report it formally so UI/system diagnostics can show why speaker control is missing.
- Make sample format explicit in the public audio contract: current device audio render path writes 16-bit samples, while I2S slot width is configured to 32-bit left slot. This may be intentional for clocking, but the contract should name it.
- Move `DeviceAudio.cpp` out of the device layer's responsibility for choosing synth rendering. Device audio should run a registered render callback or stream provider, not include `Library/Synth/Audio/SynthAudioRuntime.h` directly.

### Build and tree hygiene findings

- The old `Devices/ComputeCard/main/hal` path is not actually present; `Devices/ComputeCard/main` is empty. Do not plan cleanup work around ghost search results.
- `Devices/ComputeCard/esp-idf/main/CMakeLists.txt` still duplicates the ESP-IDF component registration path already handled by `Devices/ComputeCard/CMakeLists.txt` when `COMPONENT_DIR` is defined. Decide which one is canonical and remove the duplicate path after validating firmware builds.
- `Devices/ComputeCard/CMakeLists.txt` hard-codes `C:/espressif/v5.3.4/export.ps1` for the `CardputerFirmware` target. That belongs in user environment config or a wrapper script, not project architecture.
- `Makefile` calls `idf.py` directly and documentation requires running `export.ps1` first. Previous repo notes mentioned `tools/run_idf.ps1`, but that file does not exist in this tree. Reintroduce a small wrapper or update the memory/docs so there is only one official Windows ESP-IDF invocation path.

### Next review slice

Inspect simulator and WebUI boundaries, then turn the findings into prioritized cleanup phases.

## 2026-05-08 Simulator And WebUI: Support Tools, Not Product Source Of Truth

### Desktop simulator findings

`Devices/ComputeCardSim/DesktopHost.cpp` is useful, but it currently mixes:

- Win32 window setup/message loop/timers,
- simulator visual design and GDI drawing helpers,
- keyboard geometry/legends,
- framebuffer blitting,
- LED visualization,
- direct synth app bootstrapping,
- direct calls into `apps::SynthApp` methods for key press/release and UI inspection.

The desktop simulator should not need to know `SynthApp` internals. It should drive the same `Device::Keyboard` state or system input event API that firmware uses, then render `Device::Display::CurrentFrameBuffer()` and `Device::LED` state. Keep it as a hardware shell around the shared runtime.

Recommended split:

| New file | Responsibility |
|---|---|
| `DesktopHost.cpp` | `WinMain`, window creation, high-level orchestration only. |
| `DesktopWindow.h/.cpp` | Win32 message loop, resize/scale handling, timer dispatch. |
| `DesktopRenderer.h/.cpp` | GDI drawing for chassis, display framebuffer, keyboard, LEDs. |
| `DesktopInput.h/.cpp` | Mouse/key mapping to matrix events. Should target a simulator device input buffer, not `SynthApp` directly. |
| `DesktopLayout.h/.cpp` | Screen/key geometry and scale calculations. |

### Browser WebUI findings

The Svelte WebUI is currently a separate illustrative simulator, not a faithful runtime simulator:

- It still models `T1-T4`, a 3x4 matrix rail, and an FN popup.
- It has its own hard-coded synth/preset/parameter matrices in JavaScript.
- It does not consume the firmware-style `UiScreenSnapshot`, `Device::LED`, or framebuffer output.
- It includes visible instructional/marketing copy and a dashboard-style inspector; useful for demos, but not authoritative for product behavior.

Direction:

- Mark WebUI as a prototype until it consumes shared data from the C++ runtime or a generated JSON snapshot schema.
- If kept, rename its role explicitly, e.g. `WebPrototype` versus `RuntimeWebSim`.
- Do not let WebUI behavior drive firmware architecture until hardware and desktop runtime paths are stable.

## Prioritized Cleanup Phases

### Phase 0: Freeze current behavior with validation targets

- Keep firmware build and desktop simulator build green before moving files.
- Add small smoke tests or host validation around `SynthUiModel` menu behavior before splitting it, if feasible.
- Document the official commands for: firmware build, host HAL validation, desktop simulator, WebUI build.

### Phase 1: Mechanical file splits, no behavior changes

1. Split `SynthUiRenderer.cpp` into renderer modules first. This is mostly pure drawing and table movement.
2. Split `SynthUiModel.cpp` helper/menu/engine-specific code next, keeping state machine methods in place.
3. Split keyboard HAL into public types, ESP scan, LED scan, diagnostics, and host stub.
4. Split `DesktopHost.cpp` into window/input/renderer/layout modules.

### Phase 2: Formalize dependency direction

1. Remove `Devices/DeviceAudio.h` from `Library/Synth/Core/ISynthEngine.h`.
2. Move audio binding status out of engine interface or replace it with a generic runtime status type owned by `Library/Synth`.
3. Replace `DeviceAudio -> SynthAudioRuntime` hard include with a registered render callback/stream provider.
4. Move MIDI port ownership out of `PagedSynthEngine` if the synth engine should stay reusable; let `SynthRack` or an app-level router bind ports.
5. Keep `OS/System` as the frame scheduler, but give it narrower services for display, input, LEDs, and application lifecycle.

### Phase 3: Build system cleanup

1. Make ownership-local CMake targets authoritative: `ComputeCardSynthCore`, `ComputeCardSynthUI`, `ComputeCardSynthAudioRuntime`, `ComputeCardHALKeyboard`, etc.
2. Retire or shrink `cmake/ComputeCardOSSourceLists.cmake` after targets own their sources.
3. Choose one ESP-IDF component registration path. Remove duplicate `esp-idf/main/CMakeLists.txt` if `Devices/ComputeCard/CMakeLists.txt` is the canonical project root/component.
4. Replace hard-coded `C:/espressif/v5.3.4/export.ps1` with an environment-driven wrapper or documented `IDF_PATH`/`IDF_PYTHON_ENV_PATH` invocation.

### Phase 4: Product model cleanup

1. Convert Layout/System menu placeholder actions into real services or hide them until implemented.
2. Introduce typed UI snapshot content so Juno/DX7 fields do not keep accumulating in the generic `UiScreenSnapshot`.
3. Decide whether WebUI is deprecated, prototype-only, or rebuilt as a runtime-backed simulator.
4. Add a short architecture document describing layer direction and allowed dependencies.

## Immediate Action Recommendation

Start with Phase 1 renderer split. It is the biggest readability win with the lowest behavioral risk. After that, split `SynthUiModel` engine-specific menu helpers, then address the audio dependency cycle before adding any new synth features.