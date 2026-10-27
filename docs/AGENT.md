# CardenzaSynth

CardenzaSynth is organized around a small set of ownership boundaries. The firmware target is the source of truth for Cardenza hardware behavior; desktop code exists to simulate that device boundary, not to keep alternate hardware implementations alive.

## Dependency Direction

Allowed high-level direction:

1. `Applications` depend on `OS`, `Library`, and the public `Devices/Device.h` facade.
2. `Applications/Synth` owns the Cardenza groovebox product runtime, including `GrooveApp`, track rack wiring, mixer state and the groovebox UI.
3. `OS` depends on `Devices/Device.h` and application interfaces.
4. `Library` owns reusable domain code and vendor dependencies used by shared runtimes. Synth engine/core code must not include concrete device HAL headers.
5. `Devices/Cardenza` owns ESP-IDF firmware and hardware HAL code.
6. `Devices/CardenzaSim` owns desktop simulation of the public device facade.

Concrete rules:

- `Devices/Cardenza/cardenza` is the read-only CardenzaHAL submodule (see the hard rule at the top of `AGENT.md`): never edit, commit or push it; pull with `make hal-update`.
- Host simulation should go through `Devices/CardenzaSim` and mutate/query the public device facade.
- Application input should flow through `Device::Keyboard::State()` and `os::system::RunFrame()` dispatch, not through app-specific key-handler calls from simulator code.
- Synth engines expose synth concepts. Device audio transport is connected by provider/callback registration rather than engine public interfaces.
- App-specific synth UI/model/renderer code lives under `Applications/Synth/UI`, not under `Library/Synth`.
- Board pins live in the HAL's `board/pins.h` (read-only).
- Vendored AMY sources live under `Library/third_party/amy`.
- Files on the card follow storage protocol 2 (`docs/app-storage-protocol.md`): apps use `OS/Storage`, with the built-in Synth mapped directly to `/CardenzaSynth/{Settings,Projects,Presets,Samples,Session}` and other apps isolated under `/CardenzaSynth/Apps/<Author>/<Name>/` on demand. `Device::Storage` only provides card root, mounting, capacity and free space. Settings have no NVS fallback: changes stay dirty in RAM until written to SD, and existing NVS records are never read, written or erased. The synth library reaches its folders through `synth::storage` (`Library/Synth/Core/SynthStorage.h`), which the hosting app defines.
- Older storage locations are not automatically discovered, imported, moved or removed. Leave them untouched; discuss any required migration with the user before implementing or running it.

## Build Entrypoints

- Cardenza firmware: open an exported ESP-IDF shell, then run `make DEVICE=Cardenza build`. The Makefile changes into `Devices/Cardenza` and invokes `idf.py` directly.
- Desktop simulator: `make DEVICE=CardenzaSim build`. On Linux, `CardenzaSimShot` replays key scripts headlessly and writes PNG screenshots (`tests/groove/tour.script`).
- Browser simulator: `make web` (`DEVICE=CardenzaWeb`, Emscripten from `~/emsdk`), then `make web-serve`. `Devices/CardenzaWeb` hosts the CardenzaSim facade in a page; the build has no threads, so its one coupling to the app is pumping the groove sequencer clock (`SynthSequencer::PumpClock`, `__EMSCRIPTEN__` only).
- Architecture guardrail check: `make validate-architecture`.

The root CMake project is for desktop-style targets such as `CardenzaSim`. The Cardenza hardware target is firmware-only and should be configured through ESP-IDF.

## Current Module Shape

- `Applications/Synth/Runtime`: app-owned synth rack and track wiring.
- `Applications/Synth/UI/Groove`: the groovebox UI. `GrooveController` (input, navigation, scenes, modals, LEDs) talks to the runtime only through `groove::Host`; `Render/` draws straight into the framebuffer. The previous UI (`Applications/Synth/UI/*` outside `Groove`, `SynthApp`) remains buildable with `-DCARDENZA_GROOVE_UI=OFF` until it is removed.
- `OS/Storage`: the app storage protocol (current paths, settings files and device settings). `OS/System`: app lifecycle and switching. The firmware has no launcher of its own; do not add one (the device has its own Launcher).
- `Library/Synth/Core`: engine interfaces, engine implementations, and factory.
- `Library/Synth/Audio`: runtime audio bridge, with AMY-specific code under `Audio/Amy`.
- `Library/third_party/amy`: vendored AMY sources consumed by the shared synth audio runtime.
- `Devices/Cardenza/cardenza`: the CardenzaHAL submodule (read-only): ESP-IDF modules for system, display, audio, keyboard, storage, mic and IR.
- `Devices/CardenzaSim/DesktopHost`: Win32 simulator layout, drawing, window lifecycle, input, and rendering modules.

`cmake/CardenzaSynthSourceLists.cmake` is still the compatibility source registry for ESP-IDF and root CMake collection. Prefer adding owned CMake targets or local source lists as modules mature instead of growing unrelated flat lists.


## Cardenza HAL

- `Devices/Cardenza` sources are ESP-IDF-only. Do not add `ESP_PLATFORM` preprocessor guards or fallback typedefs in that tree.
- CardenzaHAL (`Devices/Cardenza/cardenza`) is an upstream submodule we only consume: never edit, commit or push it, reuse its API instead of re-implementing drivers, and pull updates with `make hal-update` (hard rule at the top of `AGENT.md`).

## Agent Workflow

- Prefer static validation (`get_errors`, targeted searches, diff inspection) when changing the Cardenza device layer.
