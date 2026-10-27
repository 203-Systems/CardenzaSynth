# Playing {#play}

The PADS always play the current track. Keyboard layout, scale, octave, velocity, chords and the arpeggiator (ARP) are shared by all four tracks and set on the FN panel's **1 KEYS**, **2 CHORD** and **3 ARP** pages (see [[#fn-pages]]). They are app settings: restored at power-on, never changed by loading or creating a project.

{{keymap:PADS: the 4×8 in the middle, always playing the current track
PADS = PADS
}}

<!-- keywords: play, perform, keyboard, how to play, 演奏 -->

## Keyboard layouts {#play-keyboard}

Melodic tracks have three layouts (LAYOUT on the KEYS page); the key lights show which keys can sound:

{{shot:seq-keyboard|The PIANO layout (default): two rows per octave; the gaps are off and silent.
key r3c1-r3c8 = White keys C D E F G A B C
key r2c2 r2c3 r2c5 r2c6 r2c7 = Black keys
key r2c1 r2c4 r2c8 = Gaps: off
key r0c1-r1c8 = One octave up
}}

| Layout | Arrangement | Lights |
|---|---|---|
| **PIANO** (default) | the lower row is the white keys C D E F G A B C, the row above the black keys; the gaps (before C#, between E and F, after A#) are silent. The top two rows are one octave higher | gaps {{led:off}} |
| **SCALE** | only the notes of the scale, left to right up the scale, each row one octave up. No wrong notes | all {{led:half}} |
| **CHROMA** | one semitone per key, left to right and bottom to top | all {{led:half}} |

{{strip
hold FN + 1, G1 > keys-scale @cell:1,1 | FN panel 1 KEYS: tap LAYOUT to switch. SCALE: every key is in the scale.
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

SCALE and ROOT on the KEYS page choose the scale and its root note (C–B). The scale decides which notes the SCALE layout holds, how chords stack and which keys LOCK acts on.

{{shot:setup-keys|The FN panel's 1 KEYS page.
mark cell:1,1 = LAYOUT
mark cell:1,2 = SCALE
mark cell:1,3 = ROOT note
mark cell:1,4 = OCTAVE
mark cell:2,1 = VELO (velocity)
mark cell:2,2 = LOCK (scale lock)
mark cells:3,1-4,4 = The current scale on the keyboard
}}

There are 14 scales:

{{ref:lists}}

<!-- keywords: scale, mode, key, root, tonic, major, minor, pentatonic, blues, dorian, 音阶, 根音 -->

## Octave {#play-octave}

OCTAVE on the KEYS page (−3 to +3) moves the whole keyboard up or down. The quickest way:

{{strip
hold FN + 8 > fn-octave @14,102,212,26 | Hold {{key:FN}} and press {{key:8}} for an octave up. The new value pops up at the bottom of the screen.
hold FN + I > fn-held @cell:1,4 | {{key:I}} for an octave down. You can also set OCTAVE on the KEYS page.
}}

<!-- keywords: octave, transpose, shift, higher, lower, 八度 -->

## Velocity {#play-velocity}

The pads are not velocity sensitive; every key plays at the same velocity: **VELO** on the KEYS page (1–127, default 110). Notes recorded into the sequencer get it too, and you can change it step by step on the note page later (VEL, see [[#seq-edit]]).

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

The FN panel's **2 CHORD** page turns one key into a chord:

{{strip
hold FN + 2 > setup-chord @cell:1,1 | {{key:FN+2}}: the CHORD page. CHORD is OFF.
G1 > chord-on @cell:1,1 @14,48,200,20 | Tap CHORD: TRIAD. The picture shows which notes the key on the root plays (C3 PLAYS C3 E3 G3).
hold r3c1~C > chord-held @cells:3,1-4,2 | Press a key: three notes sound together, and the chord tones light up on the screen and on the pads.
}}

- **CHORD**: OFF / TRIAD / 7TH / MAJ / MIN / SUS4 / MAJ7 / MIN7 / DOM7 / POWER / OCTAVE. TRIAD and 7TH stack thirds in the current scale (keys outside it borrow a major triad / dominant seventh); the others are fixed chords.
- **VOICING**: ROOT (root position) / INV1 (first inversion: lowest note up an octave) / INV2 (second inversion) / SPREAD (second note up an octave).
- The second line shows the triad on every degree of the key (the font is upper case only: minor is written `D-`, diminished `BDIM`).

{{shot:chord-held|Holding C: the three chord tones C E G are fully lit.
mark cell:1,1 = CHORD
mark cell:1,2 = VOICING
mark 9,48,140,9 = Notes the root key plays
mark 9,58,180,9 = Every degree of the key
mark cells:3,1-4,4 = Chord tones (blue)
key r3c1 r3c3 r3c5 = Chord tones: fully lit
}}

<!-- keywords: chord, chords, triad, seventh, inversion, voicing, harmony, one finger chord, 和弦 -->

## Arpeggiator {#play-arp}

The FN panel's **3 ARP** page: hold a few keys and they sound one after another.

{{shot:arp-running|ARP UP: holding C E G, the three notes play in turn from low to high. The white bar is the step sounding now.
mark cell:1,1 = Mode
mark cell:1,2 = Rate
mark cell:1,3 = Range
mark cell:1,4 = GATE
mark cell:2,1 = LATCH: keeps going after release
mark cells:3,1-4,4 = Playing order of 16 steps
key r3c1 r3c3 r3c5 = Held keys
}}

| Setting | Options |
|---|---|
| ARP | OFF / UP / DOWN / UP-DN / RANDOM / ORDER (in the order pressed) |
| RATE | 1/4, 1/8, 1/8T, 1/16, 1/16T, 1/32 |
| RANGE | 1–4 octaves |
| GATE | 10–100 % (how much of each note value sounds) |
| LATCH | keeps going after you let go; pressing again after releasing all starts a new set |

- Held keys (every tone of a chord counts) sound one at a time in order, up to 16 notes. The first note sounds at once.
- Stopped, it runs freely at the BPM; playing, it locks to the beat (1/16 follows swing), and PLAY / stop never breaks its rhythm.
- UP / DOWN / UP-DN continue from the last note played and never repeat at the turns.
- With REC on while playing, the ARP's notes are recorded into the pattern with the GATE length.
- Changing track, stop all and loading a project stop the ARP. Drum tracks do not arpeggiate.

<!-- keywords: arp, arpeggiator, arpeggio, latch, gate, rate, up, down, random, 琶音 -->

## Drums {#play-drums}

On a drum track ({{track:1}} by default) each pad **column** is one drum, and all four rows of a column are the same drum. Drums do not arpeggiate and ignore scale and chords. Each drum's level is on the INSTR KIT page (see [[#engine-drums]]).

{{keymap:Drum track: one drum per column
r0c1-r3c1 = Kick
r0c2-r3c2 = Snare
r0c3-r3c3 = Closed hat
r0c4-r3c4 = Open hat
r0c5-r3c5 = Clap
r0c6-r3c6 = Tom
r0c7-r3c7 = Cowbell
r0c8-r3c8 = Maracas
}}

<!-- keywords: drums, kick, snare, hi hat, clap, tom, beat, rhythm, percussion, 鼓 -->
