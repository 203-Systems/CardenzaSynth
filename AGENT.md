# CardenzaSynth Agent Notes

## Cardenza HAL

- `Devices/Cardenza` sources are ESP-IDF-only. Do not add `ESP_PLATFORM` preprocessor guards or fallback typedefs in that tree.
- Keep `CardenzaHAL` lightweight and generic. Expose native ESP-IDF handles rather than app-level wrappers.
- `CardenzaHAL::Audio` should expose the TX `i2s` channel and volume only. Do not add output-mode abstractions.
- `CardenzaHAL::Mic` should expose the RX `i2s` channel only. Device-level input start/read/stop wrappers belong above the HAL.
- `CardenzaHAL::Display` should expose native panel and panel-IO handles.
- `CardenzaHAL::System::GetI2CBus()` belongs in the public `system.h` surface.

## Workflow

- Do not start builds from the agent unless the user explicitly asks for one.
- Prefer static validation such as `get_errors`, targeted searches, and diff inspection when editing the Cardenza HAL layer.

## Groovebox UI Rules

The spec is `docs/groovebox-design.md`; keep it in sync with behaviour changes.

- Fixed key roles: left column ESC / TAB / FN / CTRL (no on-screen labels, only four colored dither strips), right column ADD / SUB / ENTER / PLAY, columns 1–8 always play the current track, columns 9–12 mirror the on-screen 4x4 cell grid.
- Holding a left key turns the GRID into a map of what that key switches (ESC scenes by column, TAB tracks laid out like MIX, CTRL every page of the scene, FN functions); choices go on the GRID, not on the pads or number keys. Keep each scene at 16 pages or fewer so the CTRL map fits.
- Colors carry meaning: ESC red-orange = scenes / back, TAB cyan = tracks, FN yellow = second functions, CTRL violet = pages, green = playing, red = recording / danger. Selection is terminal inverse (ink background, black text). Area fills use the 4x4 Bayer dither, not flat blocks.
- Every new Fn shortcut must appear in `Controller::FnEntries()` (pads, right column) or `Controller::FnGridEntries()` (GRID) so the Fn map documents it.
- Track effects are `fx.*` engine parameters registered by `PagedSynthEngine::RegisterTrackFxParameters()`; the master bus is `Host::MasterFx` (project data). Both are defined in `Library/Synth/Audio/SynthFx.h`.
- Danger (overwrite, delete, discard) always confirms with ENTER = yes, ESC = no.
- The UI must not include ESP-IDF headers or device HAL code (enforced by ValidateArchitecture); use `groove::Host`.
- Check visual changes with `CardenzaSimShot` screenshots before flashing.
