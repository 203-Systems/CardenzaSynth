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

## Sampler UI Rules

- Use `FILE` for sample file management. Put load/save/rename/delete entry points under `FILE`, not under a page titled `Sample`.
- Expose `FILE` and `REC` directly on the sampler root, including loaded state. Do not hide recording behind an intermediate file-management page.
- `FILE` should open the sample browser directly. Keep empty and loaded browser views on the same `1x4` load/search flow; do not add a loaded-only `EDIT` branch.
- In the sample browser, typing should filter immediately, and the visible `BACK` action should switch to `DEL` while a search string is non-empty.
- Do not add a dedicated `PLAY` action to the sampler file-management page. Preview belongs in context-specific flows like trim, not as a top-level file action.
- Do not expose a dedicated sampler `TUNE` control in the UI. Performance pitch comes from the music keyboard; keep `ROOT` only as the sample's base note.
- For `1x4` right-side layouts, put `DONE` / `YES` / `NEXT` / `START` / `SAVE` on the bottom slot and `BACK` / `RESET` on the slot above it when both are present.
- For `2x4` right-side layouts, put `BACK` on the bottom-right by default. If there is also a `NEXT` / `YES` / `DONE` / `SAVE` style forward action, place that on the bottom-right and move `BACK` to the bottom-left.
- Prefer `1x4` layouts when the action count is small and the page does not need a dense grid. Empty sampler entry and record-confirm pages should stay `1x4`.
