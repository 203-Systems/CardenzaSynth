# Braids (Mutable Instruments), the DSP only

The oscillator code of Mutable Instruments' Braids macro oscillator by Emilie Gillet, for the Braids
engine of CardenzaSynth (`Library/Synth/Audio/Braids`, `Library/Synth/Core/Engines/Braids`). MIT
(`LICENSE`; every file keeps its own notice). "Mutable Instruments" is a registered trademark of its
owner and is not the name of anything here.

## Where it comes from

| Here | Upstream | Commit |
|---|---|---|
| `braids/` | https://github.com/pichenettes/eurorack, `braids/` | `f4cfa7697095a28d292bff758beb51deb49b5a86` |
| `stmlib/` | https://github.com/pichenettes/stmlib | `e3bd7c9cc00e4364166f9905c0509b6ffd0535ec` |
| `resources/data/` | eurorack `braids/data/` (`waves.bin`, `map.bin`) | the same |

Only what renders sound is here: the module's main loop, its user interface, drivers, bootloader,
settings storage, quantizer, internal envelope, VCO jitter and signature waveshaper are not.

## What differs from upstream

Unchanged: `braids/analog_oscillator.{cc,h}`, `braids/macro_oscillator.cc`, `braids/excitation.h`,
`braids/svf.h`, `braids/parameter_interpolation.h`, `stmlib/stmlib.h`, `stmlib/utils/dsp.h`,
`stmlib/utils/random.{cc,h}`, `resources/data/waves.bin`, `resources/data/map.bin`.

Changed (each place is marked `CardenzaSynth:` in the source):

- **`braids/resources.{cc,h}` are generated for 44100 Hz**, the rate CardenzaSynth renders at (the
  module runs at 96000 Hz), by `resources/generate.py`: upstream's `braids/resources/*.py` and
  `stmlib/tools/resources_compiler.py` in one Python 3 script with the sample rate as an argument. Only
  the tables the code here reads are written. At 96000 Hz the script gives upstream's `resources.cc`
  value for value: `python3 resources/generate.py --sample-rate 96000 --compare <upstream resources.cc>`.
  `resources/data/code.bin` is the table upstream's `waveforms.py` calls `random_data`.
- **`braids/digital_oscillator.{cc,h}`: the delay lines are lent by the host.** The 16 KB union of
  delay lines (comb filter, plucked, bowed, blown, fluted) was a member of `DigitalOscillator`; it is
  now `union DelayLines`, set with `set_delay_lines`, so a voice playing any other model is 16 KB
  smaller. Without them those models render silence. `braids/macro_oscillator.h` passes the pointer on
  and says which shapes need it (`MacroOscillator::NeedsDelayLines`).
- **`braids/digital_oscillator.cc`: the kick's and the snare's delays in milliseconds** counted samples
  at 48000 Hz (half the module's rate); they count them at 22050 Hz now (`kDrumSampleRate`).
- **`braids/digital_oscillator.cc`: `RenderWaveLine` read one entry past `wave_line`** with TIMBRE at
  its maximum; the index stops at the last entry.
- **`braids/settings.h`** keeps only the `MacroOscillatorShape` enumeration.

Not changed, and different from the module because of the sample rate: the models that compute every
other sample (VFOF, HARM, PLUCK, BOWED, WT X4, TWINQ, PRTCL, KICK, SNARE) do so at 22050 Hz instead
of 48000 Hz, and decays counted in samples (the drums' resonators, the snare's noise) last about
twice as long at the same setting. The string and bore models' tuning is corrected outside this
directory (`DelayTuning` in `Library/Synth/Audio/Braids/BraidsVoice.cpp`).

## Building

`cmake/CardenzaSynthSourceLists.cmake` (`cardenza_collect_braids_cpp_sources`) lists the sources and
compiles them with `-fwrapv`: the fixed-point arithmetic counts on signed overflow wrapping, as it
does with the module's compiler. Include from this directory (`braids/...`, `stmlib/...`); the
headers define unprefixed macros (`CLIP`, `CONSTRAIN`), so only `BraidsVoice.cpp` includes them.

## Updating

Copy the files again from the upstream commit, re-apply the changes above (`git diff` against the
upstream files shows them), run `python3 resources/generate.py`, then `Library/Synth/tests`
(`SynthRegression`: every model sounds, the tuning, the levels; `PRINT_BRAIDS_LEVELS=1` regenerates
`Library/Synth/Audio/Braids/ModelLevels.inc` when a model's level changes).
