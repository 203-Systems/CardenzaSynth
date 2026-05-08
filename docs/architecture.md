# ComputeCardOS Architecture

ComputeCardOS is organized around a small set of ownership boundaries. The firmware target is the source of truth for ComputeCard hardware behavior; desktop code exists to simulate that device boundary, not to keep alternate hardware implementations alive.

## Dependency Direction

Allowed high-level direction:

1. `Applications` depend on `OS`, `Library`, and the public `Devices/Device.h` facade.
2. `OS` depends on `Devices/Device.h` and application interfaces.
3. `Library` owns reusable domain code. Synth engine/core code must not include concrete device HAL headers.
4. `Devices/ComputeCard` owns ESP-IDF firmware and hardware HAL code.
5. `Devices/ComputeCardSim` owns desktop simulation of the public device facade.

Concrete rules:

- Hardware HAL code under `Devices/ComputeCard/computecard` is ESP-IDF-only. Do not add non-ESP fallback bodies there.
- Host simulation should go through `Devices/ComputeCardSim` and mutate/query the public device facade.
- Application input should flow through `Device::Keyboard::State()` and `os::system::RunFrame()` dispatch, not through app-specific key-handler calls from simulator code.
- Synth engines expose synth concepts. Device audio transport is connected by provider/callback registration rather than engine public interfaces.
- Board pins live in `Devices/ComputeCard/computecard/board/pins.h`.

## Build Entrypoints

- ComputeCard firmware: `make DEVICE=ComputeCard build`, which runs ESP-IDF through `tools/run_idf.ps1` on Windows.
- Desktop simulator: `make DEVICE=ComputeCardSim build`.
- Architecture guardrail check: `make validate-architecture`.

The root CMake project is for desktop-style targets such as `ComputeCardSim`. The ComputeCard hardware target is firmware-only and should be configured through ESP-IDF.

## Current Module Shape

- `Library/Synth/Core`: engine interfaces, engine implementations, rack, and factory.
- `Library/Synth/Audio`: runtime audio bridge, with AMY-specific code under `Audio/Amy`.
- `Library/Synth/UI`: renderer/model helpers split by shared primitives, menu behavior, and engine-specific DX7/Juno modules.
- `Devices/ComputeCard/computecard`: ESP-IDF HAL modules for system, display, audio, keyboard, storage, mic, and IR.
- `Devices/ComputeCardSim/DesktopHost`: Win32 simulator layout, drawing, window lifecycle, input, and rendering modules.

`cmake/ComputeCardOSSourceLists.cmake` is still the compatibility source registry for ESP-IDF and root CMake collection. Prefer adding owned CMake targets or local source lists as modules mature instead of growing unrelated flat lists.
