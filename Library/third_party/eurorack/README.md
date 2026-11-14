# Plaits DSP (vendored)

The sound engine of the Plaits macro oscillator by Emilie Gillet, copied from her Eurorack
module sources. It is what the `Plaits` track engine plays
(`Library/Synth/Audio/Plaits/PlaitsTrack.cpp`, `Library/Synth/Core/Engines/Plaits`).

| Directory | Upstream | Commit |
|---|---|---|
| `plaits/` | https://github.com/pichenettes/eurorack (`plaits/dsp`, `plaits/resources.*`) | `f4cfa7697095a28d292bff758beb51deb49b5a86` |
| `stmlib/` | https://github.com/pichenettes/stmlib (the commit `eurorack` pins) | `e3bd7c9cc00e4364166f9905c0509b6ffd0535ec` |

## License

MIT, Copyright Emilie Gillet: the notice is at the top of every file and in `stmlib/LICENSE`.
Only MIT-licensed files are here: of `stmlib` just `stmlib.h` and what Plaits includes from
`dsp/` and `utils/` (the ST and GPL code that `stmlib/LICENSE` also mentions is not copied).

Upstream's guidelines for derivative works: "Mutable Instruments" is a registered trademark and
must not name a derivative work, and keeping a module's original name is not recommended.

## What differs from upstream

- `plaits/user_data.h` is ours: no user data (upstream keeps it in the STM32's flash, this
  firmware keeps nothing in internal flash).
- Everything else is byte for byte upstream's. Do not edit it here: what the firmware needs
  differently is done around it.

## How it is built

- With `TEST` defined (`cardenza_collect_plaits_cpp_sources` in
  `cmake/CardenzaSynthSourceLists.cmake`): upstream's own switch for building off the STM32,
  which replaces the ARM instructions in `stmlib/dsp/dsp.h` with plain C++.
- `plaits/dsp/fm/algorithms.cc` is built through `Library/Synth/Audio/Plaits/PlaitsFmAlgorithms.cpp`
  (another object name: AMY has an `algorithms` object too, and ESP-IDF's linker fragments name
  objects by base name).
- Warnings off for these sources (third-party code under the firmware's `-Werror=all`).
- The code is written for 48 kHz (`plaits::kSampleRate`) and runs here at 44.1 kHz: pitch is
  corrected where notes are sent (`PlaitsTrack.cpp`); time constants run about 8% slow.

## Updating

Copy the same files from a newer upstream commit, keep `plaits/user_data.h`, update the table
above, then run the synth tests (`Library/Synth/tests`) and the on-board stress test.
