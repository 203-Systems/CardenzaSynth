# ComputeCardOS Agent Notes

## ComputeCard HAL

- `Devices/ComputeCard` sources are ESP-IDF-only. Do not add `ESP_PLATFORM` preprocessor guards or fallback typedefs in that tree.
- Keep `ComputeCardHAL` lightweight and generic. Expose native ESP-IDF handles rather than app-level wrappers.
- `ComputeCardHAL::Audio` should expose the TX `i2s` channel and volume only. Do not add output-mode abstractions.
- `ComputeCardHAL::Mic` should expose the RX `i2s` channel only. Device-level input start/read/stop wrappers belong above the HAL.
- `ComputeCardHAL::Display` should expose native panel and panel-IO handles.
- `ComputeCardHAL::System::GetI2CBus()` belongs in the public `system.h` surface.

## Workflow

- Do not start builds from the agent unless the user explicitly asks for one.
- Prefer static validation such as `get_errors`, targeted searches, and diff inspection when editing the ComputeCard HAL layer.