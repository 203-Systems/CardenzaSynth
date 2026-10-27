# Effects {#effects}

Every track has its own insert effects and two sends; the sends feed two buses (delay and reverb), and everything leaves through the master EQ, compressor and tape saturation.

```text
engine → DRIVE → CRUSH → FILTER → CHORUS → track fader ─┬─→ out
                                                       ├─ DELAY send ─→ master delay ─┐
                                                       └─ REVERB send → master reverb ┴→ EQ → COMP → TAPE → out
```

Three places to set effects:

{{strip
- > instr-juno-fx @cells:1,1-2,4 | {{scene:INSTR}} {{page:FX}}: the track's inserts and its two sends.
- > tape-sends @cells:1,1-4,4 | {{scene:TAPE}} {{page:SENDS}}: all four tracks' effects on one screen.
- > tape-fx @cells:1,1-3,4 | {{scene:TAPE}} {{page:FX}}: master delay, reverb, EQ, compressor, tape.
}}

<!-- keywords: effects, fx, signal chain, routing, 效果器 -->

## Track effects (inserts) {#fx-inserts}

On each engine's FX page (see [[#instr-fx]]), or all four tracks at once on TAPE's SENDS page:

{{shot:instr-juno-fx|The FX page: effect parameters on top, the track's signal chain below.
mark cell:1,1 = DRIVE: soft saturation, higher is dirtier
mark cell:1,2 = CRUSH: sample-rate and bit reduction
mark cell:1,3 = FILTER: 64 is off, low-pass below, high-pass above
mark cell:1,4 = RESO: filter resonance
mark cell:2,1 = CHORUS: Juno-style I / II / I+II
mark cells:2,2-2,3 = DELAY / REVERB sends
mark cells:3,1-4,4 = Signal chain: active effects light up
}}

They are sound parameters (`fx.*`), saved with the sound and the project; changing engine or sound resets them (Juno sounds bring their own chorus).

<!-- keywords: drive, distortion, saturation, bitcrush, lofi, filter, resonance, chorus, 失真 -->

## Sends: delay and reverb {#fx-sends}

Each track has two send amounts, DELAY and REVERB. Once sent to the buses, their RETURN levels set how much comes back overall.

{{shot:tape-sends|SENDS: one column per track. While a track sounds with a send on, that cell's right edge glows.
mark cells:3,1-3,4 = DELAY send
mark cells:4,1-4,4 = REVERB send
key G9-G16 = On: half lit · off: off
}}

<!-- keywords: send, sends, delay, echo, reverb, space, ambience, 发送 -->

## Master effects {#fx-master}

TAPE's FX page, part of the project (saved since project format v4):

{{shot:tape-fx|Master FX.
mark cells:1,1-1,4 = DELAY: tempo-synced tape delay
mark cells:2,1-2,4 = REVERB: 4-line FDN, on the dark side
mark cell:3,1 = COMP: bus compressor
mark cells:3,2-3,3 = EQ: low / high shelf ±12 dB
mark cell:3,4 = TAPE: tape saturation
mark cells:4,1-4,4 = Echoes at the current tempo, reverb decay
}}

- **DELAY**: its feedback loop has a TONE low-pass and an 80 Hz high-pass; changing the note value glides like tape; a note value longer than about 740 ms is halved automatically.
- **REVERB**: about 4.5 kHz wide, on the dark side.
- Both sends are decimated to 11.025 kHz before processing, so delay and reverb sound warm and a little lo-fi.

<!-- keywords: master, bus, tape delay, reverb, eq, compressor, saturation, 总线 -->

## Memory and CPU {#fx-cpu}

PSRAM holds the display framebuffer, sample PCM and sequencer project data. The audio working buffers stay in internal RAM; effect buffers are allocated the first time they are used:

| Part | Memory |
|---|---|
| Effect core | about 4.6 KB |
| Each track with effects / each chorus | 1 KB / 1 KB |
| Delay | 16 KB |
| Reverb | about 4.8 KB |
| Everything on | about 31 KB |

- If the delay or reverb cannot get memory, that bus is silent and the FX page and the signal chain show NO MEM. With every effect off, the output is bit-identical to having no effects.
- Audio has a budget of 5.8 ms per 256 frames. Typical effects use about 5.4–5.6 ms, so there is little headroom.

> [!warn] Very dense chords with many effects can crackle: play fewer notes at once, or turn an effect or two off.

<!-- keywords: memory, cpu, performance, crackle, glitch, dropout, no mem, 内存 -->
