# The drum kits in p90v2

The stock firmware has one drum bank: the TR-808 samples AMY ships. This fork appends two more,
side by side with the originals, so `T1 DRUMS` can be three different kits without touching
the sampler track or the SD card.

| Preset id | Menu name | PCM entries | What it is |
|---|---|---|---|
| `drums.808` | 808 Kit | 1, 2, 6, 7, 9, 8, 10, 0 | AMY's own TR-808 bank, untouched |
| `drums.crush` | Crush Kit | 16..23 | Bit-crushed synth kit: 7-bit, sample-and-hold, deliberately toy-like |
| `drums.lofi` | Lofi Kit | 24..31 | Acoustic-style kit in a small room, for chillhop / boom-bap |

Both new kits follow the pad order `BD SD CH OH CP TM CB MA`, so the drum machine layout never
changes.

## Why these are baked in, not loaded from a card

The drum track is a fixed sample player: eight pads, eight GM notes, no file access at runtime.
Loading a kit from the SD card would mean giving the drum engine the sampler's loading path —
a much larger change. Baking the samples into AMY's PCM bank costs flash and keeps the engine
untouched.

Cost: the crush kit is 34,618 samples (69 kB), the lofi kit is 45,200 samples (88 kB). Both are
22.05 kHz, int16, mono, matching `PCM_AMY_SAMPLE_RATE`. The app partition is 4 MB, so there is
room, but these two kits are now ~157 kB of the firmware.

## How the bank is put together

Three files carry it, and all three must agree:

1. **`Library/third_party/amy/src/pcm_samples_tiny.h`** — the sample data itself, appended to the
   `pcm[]` array inside the `AMY_WAVETABLE` guard. The comments mark which entries are which kit.
2. **`Library/third_party/amy/src/pcm_tiny.h`** — `PCM_CRUSH_LENGTH` and `PCM_LOFI_LENGTH` roll
   into `PCM_LENGTH`, and `PCM_MAP_ENTRIES` covers the extra 16 entries. Each new sample needs a
   `{ offset, length, 0, 0, 60 }` row in `pcm_map`; `offset + length` must stay inside
   `PCM_LENGTH`.
3. **`Library/Synth/Audio/Amy/SynthAudioRuntimeAmy.cpp`** — `kKits` maps each kit to eight PCM
   entries, and `engine.drumKit` selects a row. The preset text (`drums.crush`, `drums.lofi`)
   sets it.

`Library/Synth/Core/Engines/Drums/DrumsSynthEngine.cpp` registers the presets. The preset
carries the eight level knobs, not the samples: switching kits switches the mapping, and the
knobs come back at their defaults.

## Adding a fourth kit

1. Generate eight one-shots at 22,050 Hz, int16 mono (see `../_run/gen_realdrum.py` in the
   workspace this fork came from — `render_demo.py` next to it renders a loop for judging them).
2. Append the data to `pcm_samples_tiny.h`, adding `PCM_<NAME>_LENGTH` to `PCM_LENGTH`.
3. Add eight rows to `pcm_map` and bump `PCM_MAP_ENTRIES` by 8.
4. Add a row to `kKits`, extend the `clamp` bound, and extend the `drums.*` branch in
   `ApplyAmyEventNoLock`.
5. `RegisterPreset("drums.<name>", "<Name> Kit", preset)`.

The failure modes are quiet rather than loud: a wrong offset plays somebody else's sample, a
missing comma in `pcm_map` is a compile error, and a sample array longer than `PCM_LENGTH` is
"too many initializers".

## Naming move (p90v2, second pass)

The first fork kit was called `drums.lofi` / "Lofi Kit". That name now belongs to the acoustic
kit, because that is the one meant for lofi music; the bit-crushed one became `drums.crush` /
"Crush Kit". A project saved while the first kit was the only extra one would now resolve its
`drums.lofi` to the acoustic kit — pick CRUSH KIT once and save again.

## Where the sound came from

Both kits are synthesized, not sampled from hardware, so there is no licensing question:

- **Crush**: sine sweeps and noise, quantized to 7 bits and held at 2–3 samples, which is where
  the toy quality comes from.
- **Lofi**: the parts a synth kick does not have — a beater that drops 118 Hz → 48 Hz in 28 ms, a
  shell mode under the fundamental, snare wires that outlast the head, sheet-metal hats from six
  inharmonic squares, four early reflections under 32 ms and a little tape hiss. The kit is then
  balanced by RMS rather than by peak, so the tom does not sit louder than the kick.

Neither kit is bit-crushed on the way in. That is `fx.crush`'s job on the FX page, so the amount
of dirt stays a live decision instead of being baked into the samples.
