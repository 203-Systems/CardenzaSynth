# ComputeCardOS Cleanup Todo

This is the execution todo for the repository cleanup/refactor. Commit after each completed task.

## Rules For This Pass

- Prefer real ESP32-S3 firmware code over defensive host fallbacks.
- Delete prototype surfaces that no longer match the product model, starting with WebUI.
- Prefer deeper folders with owned modules over a flat `Library/Synth` pile.
- Keep behavior-preserving mechanical splits separate from semantic rewrites.
- Avoid new generic frameworks unless they remove concrete coupling.

## Task 0 - Planning Baseline

- [x] Write the cleanup review in `.agent/cleanup-plan.md`.
- [x] Replace the old bring-up todo with this cleanup todo.

## Task 1 - Delete Browser Prototype

- [x] Remove `Devices/ComputeCardSim/WebUI`.
- [x] Remove WebUI config flags and documentation references.
- [x] Keep desktop simulator as the only simulator surface for now.

## Task 2 - Formalize Build Entrypoints

- [x] Remove duplicate/obsolete ESP-IDF component paths.
- [x] Replace hard-coded local ESP-IDF paths with environment-driven commands.
- [x] Make the official build commands explicit in repo docs.

## Task 3 - Split Synth UI Renderer

- [ ] Move shared UI rendering helpers under `Library/Synth/UI/Render`.
- [ ] Move DX7 rendering into `Library/Synth/UI/Dx7`.
- [ ] Move Juno rendering into `Library/Synth/UI/Juno`.
- [ ] Keep `SynthUiRenderer.h` as the public app-facing facade.

## Task 4 - Split Synth UI Model

- [ ] Move generic UI types under `Library/Synth/UI/Model`.
- [ ] Move menu grammar under `Library/Synth/UI/Menu`.
- [ ] Move DX7/Juno menu semantics beside their engine UI modules.
- [ ] Keep `SynthUiModel` focused on state-machine orchestration.

## Task 5 - Split Synth Runtime

- [ ] Move engine interfaces and rack code under `Library/Synth/Core`.
- [ ] Move AMY-backed audio runtime under `Library/Synth/Audio/Amy`.
- [ ] Remove device audio types from the synth engine public interface.
- [ ] Replace device-to-synth render calls with a registered audio provider/callback.

## Task 6 - Tighten ComputeCard HAL

- [ ] Remove non-ESP fallback bodies from production HAL modules.
- [ ] Split keyboard into public types, layout, key scan, LED scan, diagnostics, and ESP implementation files.
- [ ] Move keyboard pin lists to `computecard/board/pins.h`.
- [ ] Replace loose audio codec probe flags with an explicit output status contract.

## Task 7 - Simplify Simulator

- [ ] Split `DesktopHost.cpp` into window, renderer, input, and layout modules.
- [ ] Stop calling `apps::SynthApp` internals from the simulator.
- [ ] Drive simulator input through the same system/device event path as firmware.

## Task 8 - Architecture Guardrails

- [ ] Add a short architecture doc with allowed dependency directions.
- [ ] Shrink or retire `cmake/ComputeCardOSSourceLists.cmake` after local CMake targets own sources.
- [ ] Add minimal validation targets for synth UI behavior and firmware smoke builds.
