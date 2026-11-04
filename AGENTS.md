# CardenzaSynth agent instructions

Read and follow `AGENT.md` (project rules) and `docs/AGENT.md` before working in this repository.

## CardenzaHAL: changed upstream, consumed here

`Devices/Cardenza/CardenzaHAL` is the git submodule https://github.com/203-Systems/CardenzaHAL, the hardware layer every
Cardenza firmware builds on (this one, Cardenza-MPE, ...). Drivers belong there, not in a firmware.

- Since 2026-11-02 (user decision) we change the HAL ourselves: in a separate clone `~/projects/CardenzaHAL`, pushed
  straight to upstream `main`, then `make hal-update` here and commit the new submodule pointer.
- The submodule itself stays a clean consumer copy: never edit, commit or push inside `Devices/Cardenza/CardenzaHAL`.
- Reuse the HAL's API instead of re-implementing drivers here. If the HAL lacks something or has a bug, fix it
  in the HAL clone; work around it in `Devices/Cardenza` only for the time until that change is on `main`.
- `make hal-check` (run by every firmware build) and the CMake configure step fail on a modified submodule or
  one not on upstream `main`. `make HAL_DEV=~/projects/CardenzaHAL DEVICE=Cardenza build` builds against the clone for testing
  on the board; never ship that (no Launcher bin for users).
