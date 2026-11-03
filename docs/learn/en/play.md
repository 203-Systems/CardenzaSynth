# Playing {#play}

The PADS always play the current track. Keyboard layout, scale, root, octave, velocity and scale lock are shared by all four tracks and set on the **KEYS** page of {{scene:SETUP}} (see [[#setup-keys]]). Chords and the arpeggiator (ARP) belong to each track: every melodic track has its own **CHORD** and **ARP** pages in {{scene:INSTR}}. All of them are saved with the project; NEW starts from PIANO, C MAJOR, no chords and every ARP off.

{{keymap:PADS: the 4×8 in the middle, always playing the current track
PADS = PADS
}}

<!-- keywords: play, perform, keyboard, how to play, 演奏 -->

## Keyboard layouts {#play-keyboard}

Melodic tracks have three layouts (LAYOUT on the KEYS page); the key lights show which keys can sound:

{{shot:seq-keyboard|The PIANO layout (default): C sits in the middle, white keys and black keys row by row; the gaps are off and silent.
key r0c1-r0c8 = White keys, one octave up
key r1c2 r1c3 r1c5 r1c6 r1c7 = Black keys
key r2c1-r2c8 = White keys C D E F G A B C
key r3c2 r3c3 r3c5 r3c6 r3c7 = Black keys, one octave down
key r1c1 r1c4 r1c8 r3c1 r3c4 r3c8 = Gaps: off
}}

| Layout | Arrangement | Lights |
|---|---|---|
| **PIANO** (default) | from the top: white keys one octave up, black keys, white keys C D E F G A B C (second row from the bottom), black keys of the octave below. The gaps (before C#, between E and F, after A#) are silent | gaps {{led:off}} |
| **SCALE** | only the notes of the scale, from the root left to right up the scale, each row one octave up. No wrong notes | all {{led:half}} |
| **CHROMA** | one semitone per key from the root, left to right and bottom to top | all {{led:half}} |

{{strip
G1 > keys-scale @cell:1,1 | SETUP KEYS: tap LAYOUT to switch. SCALE: every key is in the scale.
G1 > keys-chroma @cell:1,1 | Tap again: CHROMA, one semitone per key.
G1 > setup-keys @cell:1,1 | Tap again: back to PIANO.
}}

| Light | On the pads it means |
|---|---|
| {{led:half|half lit}} | can sound |
| {{led:full|fully lit}} | held keys, the other tones of a chord, the note the ARP is playing, notes the sequencer plays on this track |
| {{led:off|off}} | cannot sound |

<!-- keywords: layout, piano, black keys, white keys, chromatic, scale layout, isomorphic, 键盘排列, 钢琴 -->

## Scale and root {#play-scale}

SCALE and ROOT on the KEYS page choose the scale and its root note (C–B). The scale decides which notes the SCALE layout holds, how TRIAD and 7TH chords stack and which keys LOCK acts on.

{{shot:setup-keys|The KEYS page of SETUP.
mark cell:1,1 = LAYOUT
mark cell:1,2 = SCALE
mark cell:1,3 = ROOT note
mark cell:1,4 = OCTAVE
mark cell:2,1 = VELO (velocity)
mark cell:2,2 = LOCK (scale lock)
mark cell:2,3 = SELECT (ROLL: one step / several)
mark cells:3,1-4,4 = The current scale on the keyboard
}}

There are 14 scales:

{{ref:lists}}

<!-- keywords: scale, mode, key, root, tonic, major, minor, pentatonic, blues, dorian, 音阶, 根音 -->

## Octave {#play-octave}

OCTAVE on the KEYS page (−3 to +3) moves the whole keyboard up or down. The quickest way:

{{strip
hold FN + 8 > fn-octave @14,94,212,28 | Hold {{key:FN}} and press {{key:8}} for an octave up, {{key:I}} for an octave down (held, it repeats). The new value pops up at the bottom of the screen.
G4 > setup-keys @cell:1,4 | Or on SETUP KEYS: tap OCTAVE (GRID 4) and change it with {{key:ADD}} / {{key:SUB}}.
}}

<!-- keywords: octave, transpose, shift, higher, lower, 八度 -->

## Velocity {#play-velocity}

The pads are not velocity sensitive; every key of a melodic track plays at the same velocity: **VELO** on the KEYS page (1–127, default 110). Notes recorded into the sequencer get it too, and you can change it step by step on the note page later (VEL, see [[#seq-edit]]). On a drum track the row sets how hard instead (see [[#play-drums]]).

<!-- keywords: velocity, dynamics, how hard, loudness of a note, 力度 -->

## Scale lock {#play-lock}

In the PIANO and CHROMA layouts, what keys outside the scale do:

| LOCK | Keys outside the scale | Light |
|---|---|---|
| **OFF** | sound as usual | {{led:half}} |
| **MUTE** | are silent | {{led:off}} |
| **SNAP** | play the nearest scale note below | {{led:half}} |

<!-- keywords: lock, scale lock, snap, mute, wrong notes, 锁定 -->

## Chords {#play-chords}

The **CHORD** page in {{scene:INSTR}} turns one key into a chord. Each track has its own chord; it stays when you change the sound or the engine. Drum tracks have no CHORD page.

{{strip
hold CTRL + G2 > instr-chord @cell:1,1 | In {{scene:INSTR}} hold {{key:CTRL}} and press G2: the track's CHORD page. CHORD is OFF.
G1 > chord-on @cell:1,1 @9,43,170,20 | Tap CHORD: TRIAD. The picture shows which notes the key on the root plays (C3 PLAYS C3 E3 G3).
hold r2c1~C > chord-held @cells:3,1-4,4 | Press a key: three notes sound together, and the chord tones light up on the screen and on the pads.
}}

- **CHORD**: OFF / TRIAD / 7TH / MAJ / MIN / SUS4 / MAJ7 / MIN7 / DOM7 / POWER / OCTAVE. TRIAD and 7TH stack thirds in the current scale (keys outside it borrow a major triad / dominant seventh); the others are fixed chords.
- **VOICING**: ROOT (root position) / INV1 (first inversion: lowest note up an octave) / INV2 (second inversion: the two lowest notes up an octave) / SPREAD (second note up an octave).
- The second line shows the triad on every degree of the key (the font is upper case only: minor is written `D-`, diminished `BDIM`).
- Chord tones are recorded into the sequencer like separate notes, and the ARP plays them one by one.

{{shot:chord-held|Holding C: the three chord tones C E G are fully lit.
mark cell:1,1 = CHORD
mark cell:1,2 = VOICING
mark 9,43,140,10 = Notes the root key plays
mark 9,53,170,10 = Every degree of the key
mark cells:3,1-4,4 = Chord tones (blue)
key r2c1 r2c3 r2c5 = Chord tones: fully lit
}}

<!-- keywords: chord, chords, triad, seventh, inversion, voicing, harmony, one finger chord, per track, midi fx, 和弦 -->

## Arpeggiator {#play-arp}

The **ARP** page in {{scene:INSTR}}: hold a few keys and they sound one after another. Every melodic track has its own ARP with its own settings, and several can run at once (a HOLD arpeggio on T2 keeps going while you play T3).

{{strip
hold CTRL + G3 > instr-arp @cell:1,1 | In {{scene:INSTR}} hold {{key:CTRL}} and press G3: the track's ARP page. ARP OFF dims the picture.
G1, hold r2c1~C + r2c3~E + r2c5~G > arp-running @cells:3,1-4,4 | Tap ARP: UP. Hold C E G: the notes play in turn from low to high.
}}

{{shot:arp-running|ARP UP: holding C E G, the three notes play in turn from low to high. Each bar is a step at its pitch, as wide as GATE; the white bar is the step sounding now.
mark cell:1,1 = ARP mode
mark cell:1,2 = RATE
mark cell:1,3 = RANGE
mark cell:1,4 = GATE
mark cell:2,1 = HOLD: keeps going after release
mark cells:3,1-4,4 = Playing order of 16 steps
key r2c1 r2c3 r2c5 = Held keys
}}

| Setting | Options |
|---|---|
| ARP | OFF / UP / DOWN / UP-DN / RANDOM / ORDER (in the order pressed) |
| RATE | 1/4, 1/8, 1/8T, 1/16 (default), 1/16T, 1/32 |
| RANGE | 1–4 octaves (upwards) |
| GATE | 10–100 % (how much of each note value sounds, default 50 %) |
| HOLD | keeps going after you let go; pressing again after releasing all starts a new set. One tap on the cell switches it, no need to select it first |

- Held keys (every tone of a chord counts) sound one at a time in order, up to 16 notes. The first note sounds at once.
- Stopped, it runs freely at the BPM; playing, it locks to the beat (1/16 follows swing), and PLAY / stop never breaks its rhythm.
- UP / DOWN / UP-DN continue from the last note played and never repeat at the turns.
- With REC on while playing, the ARP's notes are recorded into its track's pattern with the GATE length.
- Changing track does not stop the other tracks' ARPs. Turning HOLD off stops it at once (only keys still held keep playing). {{key:FN+ESC}} (stop all) and loading a project stop every ARP.

<!-- keywords: arp, arpeggiator, arpeggio, latch, hold, gate, rate, up, down, random, per track, 琶音 -->

## Drums {#play-drums}

On a drum track ({{track:1}} by default) each pad **column** is one drum and the **row** is how hard: the top row hits hardest (velocity 127), then 100, 70, and the bottom row softest (40). A hit lights its column from the bottom up to the row it was played on. Drums do not arpeggiate and ignore scale, chords and VELO. Each drum's level is on the INSTR KIT page (see [[#engine-drums]]).

{{keymap:Drum track: one drum per column, harder towards the top
r0c1-r3c1 = Kick
r0c2-r3c2 = Snare
r0c3-r3c3 = Closed hat
r0c4-r3c4 = Open hat
r0c5-r3c5 = Clap
r0c6-r3c6 = Tom
r0c7-r3c7 = Cowbell
r0c8-r3c8 = Maracas
}}

<!-- keywords: drums, kick, snare, hi hat, clap, tom, beat, rhythm, percussion, drum velocity, how hard, 鼓 -->
