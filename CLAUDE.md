# CardenzaSynth

The project rules are in AGENT.md; read and follow them: @AGENT.md

## Hard rule: CardenzaHAL is read-only

`Devices/Cardenza/CardenzaHAL` is the git submodule https://github.com/203-Systems/CardenzaHAL, a dependency we
do not maintain.

- Never edit, add or delete anything under `Devices/Cardenza/CardenzaHAL`, never commit inside it, and never
  push to github.com/203-Systems/CardenzaHAL (no fixes, branches, tags or pull requests), even if the
  account could.
- Pull updates only: `make hal-update`, then commit the new submodule pointer in this repository.
- Reuse the HAL's API instead of re-implementing drivers here. If the HAL lacks something or has a bug,
  work above it in `Devices/Cardenza` and tell the user what the HAL maintainer would need to change.
- `make hal-check` (run by every firmware build) and the CMake configure step fail on a modified HAL and
  block pushes from it.
