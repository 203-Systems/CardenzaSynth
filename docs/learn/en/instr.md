# 1 INSTR {#instr}

INSTR is the current track's sound: which engine, which sound (preset), every parameter of the engine and the track's effects. Each of the four tracks uses one engine and remembers which page it was on.

| Engine | Default track | Pages ({{key:CTRL}} steps) |
|---|---|---|
| DRUMS drum machine | {{track:1}} | VOICE, KIT, FX |
| JUNO analog synth | {{track:2}} | VOICE, DCO, VCF, ENV, LFO, FX |
| DX7 FM synth | {{track:3}} | VOICE, ALGO, LEVELS, OP1–OP6, LFO, FX |
| SAMPLER | {{track:4}} | VOICE, SAMPLE, TONE, FX |

<!-- keywords: instrument, synth, synthesizer, engine, sound, patch, 乐器, 音源 -->

## VOICE: engine and sound {#instr-voice}

The first page of every engine: the sound's name in large type, a live keyboard below it.

{{shot:instr-voice|INSTR VOICE (T2 Juno).
mark cell:1,1 = ENGINE: change engine
mark cell:1,2 = PRESET: browse
mark cell:1,3 = SAVE
mark cell:1,4 = SAVE AS
mark 8,40,200,26 = Sound name
mark 8,66,120,8 = Engine · scale · octave
mark 8,75,222,44 = Live keyboard
key ADD SUB = Next / previous sound
key OK = Sound browser
key G1-G4 = ENGINE PRESET SAVE SAVE AS
}}

- {{key:ADD}} / {{key:SUB}}: straight to the next / previous sound, ready to hear.
- {{key:OK}}: opens the sound browser.

Changing the engine gives the track the new engine's default sound; the track's sequence is not touched.

{{strip
G1 > engine-picker @cells:2,1-2,4 | ENGINE: the four engines, the current one marked NOW.
ADD > instr-voice @8,40,200,26 | {{key:ADD}} / {{key:SUB}} change the sound right on the VOICE page.
}}

<!-- keywords: change sound, next sound, preset, engine, choose synth, voice, engine picker, 换音色 -->

## Browsing and saving sounds {#instr-presets}

{{strip
G2 > preset-browser @0,13,240,10 | PRESET: the sound list, with "position / total" at the top right.
B, A, S, S > preset-filtered @0,13,240,10 | Type on the keyboard to filter (for example `BASS`).
OK > instr-voice @8,40,200,26 | {{key:ADD}} / {{key:SUB}} move, {{key:OK}} loads.
}}

- {{key:ESC}} clears the filter first, a second press closes.
- {{key:FN}} + {{key:;}} {{key:.}} move up and down, {{key:FN+ADD}} / {{key:FN+SUB}} a page at a time.
- SAVE overwrites the current sound (your own sounds), SAVE AS stores it under a new name. Your sounds live in this app's Presets folder on the SD card.

<!-- keywords: presets, preset, preset library, browse, search, search sound, save sound, save as, patch library, 音色库 -->

## Parameter pages {#instr-params}

The engine's other pages are parameter pages: **each GRID key picks one parameter**, {{key:ADD}} / {{key:SUB}} change it (held they repeat), {{key:FN+OK}} restores its default. Switches and choices step with a second press on the same key.

{{shot:instr-juno-vcf|Juno VCF page: parameters on the top two rows, the filter curve below.
mark cell:1,1 = Focused parameter: white
mark cell:1,2 = RESO
mark cell:1,3 = ENV
mark cell:1,4 = POL
mark cells:3,1-4,4 = The group's picture: filter curve
key G1 = Focus: fully lit
key G2-G6 = Parameters: half lit
key G9-G16 = Empty cells: off
key ADD SUB = Change value
}}

Cells without a parameter draw the group's picture: envelopes, filter curves, algorithms, waveforms, a scope… Every parameter's range, default and options are in the engine references below.

<!-- keywords: parameter, parameters, edit sound, knob, tweak, default, reset, 参数 -->

## FX page: the track's effects {#instr-fx}

The last page of every engine is FX:

{{shot:instr-juno-fx|The FX page and its signal chain.
mark cell:1,1 = DRIVE: saturation
mark cell:1,2 = CRUSH: bit reduction
mark cell:1,3 = FILTER: DJ filter
mark cell:1,4 = RESO
mark cell:2,1 = CHORUS
mark cell:2,2 = DELAY send
mark cell:2,3 = REVERB send
mark cells:3,1-4,4 = Signal chain: IN → … → OUT
}}

They are part of the sound and are saved with it and with the project. See [[#effects]].

<!-- keywords: fx, effects, drive, distortion, saturation, bitcrush, filter, chorus, delay, reverb, 效果 -->

## DRUMS {#engine-drums}

Eight drums (kick, snare, closed hat, open hat, clap, tom, cowbell, maracas), one pad column each. The KIT page sets each drum's level and the kit's master level.

{{shot:instr-drums-kit|DRUMS KIT page: the eight drums' levels, the pads drawn at the bottom right.
mark cells:1,1-2,4 = Eight drum levels
mark cell:3,1 = MASTER level
mark cells:3,2-4,4 = Pads (light up when hit)
}}

{{ref:engine:DRUMS}}

<!-- keywords: drums, drum machine, kit, 808, kick level, snare level, 鼓机 -->

## JUNO analog synth {#engine-juno}

Classic subtractive synthesis: DCO oscillator → VCF filter → VCA envelope, plus an LFO and the Juno chorus.

{{strip
CTRL > instr-juno-dco @cells:3,1-4,4 | **DCO**: RANGE, SAW, PULSE, PWM, PW SRC, SUB, NOISE, LFO, with a scope.
CTRL > instr-juno-vcf @cells:3,1-4,4 | **VCF**: CUTOFF, RESO, ENV, POL, LFO, KBD, with the filter curve.
CTRL > instr-juno-env @cells:3,1-4,4 | **ENV**: ADSR, VCA mode (ENV / GATE), LEVEL, with the envelope.
CTRL > instr-juno-lfo @cells:3,1-4,4 | **LFO**: RATE, DELAY (fade-in), with the waveform.
}}

The Chorus I / II that Juno presets carry maps to CHORUS on the FX page.

{{ref:engine:JUNO}}

<!-- keywords: juno, analog, subtractive, oscillator, dco, vcf, filter, juno filter, envelope, adsr, lfo, pwm -->

## DX7 FM synth {#engine-dx7}

Six-operator FM synthesis, compatible with DX7 sounds:

{{strip
CTRL > instr-dx7-algo @cells:3,1-4,4 | **ALGO**: 32 algorithms, feedback, transpose, with the algorithm diagram.
CTRL > instr-dx7-levels @cells:3,1-4,4 | **LEVELS**: the six operators' output levels. Solid bars are carriers you hear, dithered ones are modulators.
CTRL > instr-dx7-op1 @cell:2,3 | **OP1–OP6**: OUT, CRS, FINE, DTN, MODE, AMOD, the R1–R4 / L1–L4 envelope and a CAR / MOD mark.
CTRL > instr-dx7-lfo @cells:3,1-4,4 | **LFO**: SPEED, WAVE, PMD, AMD, P SENS, with the waveform.
}}

{{ref:engine:DX7}}

<!-- keywords: dx7, fm, frequency modulation, operator, algorithm, electric piano, epiano, yamaha -->

## SAMPLER {#engine-sampler}

Plays a WAV: the SAMPLE page picks the file, records, trims, sets the root note, loop and gain (with the waveform); TONE is filter, envelope and level. The full workflow is in [[#sampler]].

{{shot:instr-sampler-sample|SAMPLER SAMPLE page (no sample loaded yet).
mark cell:1,1 = FILE: load
mark cell:1,2 = REC: record
mark cell:1,3 = START
mark cell:1,4 = END
mark cells:2,1-2,4 = ROOT · TUNE · LOOP · GAIN
}}

{{ref:engine:SAMPLER}}

<!-- keywords: sampler, sample, wav, waveform, trim, loop, 采样器 -->
