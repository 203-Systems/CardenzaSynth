# 3 TAPE {#tape}

TAPE is all four tracks together: which patterns play when, the song's arrangement, the mix, the sends and the master effects. Five pages ({{key:CTRL}} steps):

{{page:SESSION}} {{page:SONG}} {{page:MIX}} {{page:SENDS}} {{page:FX}}

Saving, loading and the project's BPM are global and live in {{scene:SETUP}}: see [[#setup-project]] and [[#setup-files]]. Everything on these five pages except the master volume is part of the project. The bottom row of the FN menu is the same on every TAPE page: SAVE ({{key:FN+S}}), TAP ({{key:FN+T}}), REC ({{key:FN+PLAY}}) and SETUP.

<!-- keywords: tape, song, arrangement, mixer, whole song, four tracks, 走带 -->

## SESSION: launch like Ableton {#tape-session}

One column per track (in its colour), one row per pattern slot; each cell draws the pattern's 16 steps.

{{shot:tape-session|SESSION: the four tracks side by side.
mark cells:1,1-4,1 = T1
mark cells:1,2-4,2 = T2
mark cells:1,3-4,3 = T3
mark cells:1,4-4,4 = T4
mark cell:2,2 = T2's second pattern
mark cell:2,1 = +: new pattern
key G1-G4 = Current pattern: fully lit
key G6 = Another pattern: half lit
}}

{{strip
PLAY, G6 > tape-session-queued @cell:2,2 | While playing, tap a cell: the track switches when its loop ends. The cell shows NXT, its light is {{led:blink|blinking}}.
- > - | The loop ends and T2 switches to P02. When stopped, a tap switches at once.
}}

- Tap a cell: that track plays that pattern (and becomes the current track). The "+" after a track's last pattern creates a new, empty one and launches it.
- {{key:ADD}} / {{key:SUB}}: move the cursor row up / down and scroll; {{key:OK}}: launch the cursor's whole row (every track that has that pattern).
- {{key:FN+OK}}: capture what every track plays (or has queued) as a new part at the end of SONG; the part lasts as long as its longest pattern.
- FN menu: **LAUNCH** (the cursor's row) and **ADD** TO SONG (the same as {{key:FN+OK}}).
- Launching from SESSION leaves SONG mode. A playing cell is green with ▶ and a moving playhead.

<!-- keywords: session, clip, launch, scene launch, ableton, live, grid launch -->

## SONG: arranging the song {#tape-song}

The song as a tape: each **column** is a part (time runs left to right), each **row** a track; a cell holds the pattern that plays in that part and the part's length in bars (xN).

{{shot:tape-song|SONG: each column a part, each row a track.
mark cells:1,1-4,1 = Part 1
mark cells:1,2-4,2 = Part 2
mark cell:1,1 = Selected cell
mark 47,15,17,10 = Bars: xN
mark cells:1,3-4,3 = + column: add a part
mark 199,111,31,12 = LOOP / SONG mode
}}

- Tap a cell to select it; tap the same cell again to switch between "pattern" and "bars" (the bars of the part light up yellow). `--` means the track is silent in that part.
- Tap the + column after the last part: a new part, a copy of the last one (on an empty song: what every track plays now).
- {{key:ADD}} / {{key:SUB}}: change the cell's pattern (`--` included), or the part's length in bars (1–16); {{key:FN+ADD}} / {{key:FN+SUB}} always change the bars.
- {{key:OK}}: play from this part (switches to SONG mode; while playing it jumps there); {{key:FN+OK}}: SONG / LOOP.
- {{key:FN+C}} / {{key:FN+V}} / {{key:FN+X}} / {{key:FN+D}}: copy, paste (after the cursor), delete, duplicate a part. The FN menu has them too (COPY, PASTE, DELETE, DUP, each marked ROW n), with PLAY for SONG / LOOP.
- The PLAY switch on [[#setup-project]] is the same SONG / LOOP setting. It is a performance control and is not saved in the project; launching from SESSION switches back to LOOP.

In SONG mode each part plays xN bars (1 bar = 16 steps), then the next; after the last part it starts over from the first. LOOP mode: every track loops its current pattern. The playing part has a green tape head; a part waiting to start is framed in yellow.

> [!tip] Put together a combination in SESSION and {{key:FN+OK}} records it as the next SONG part: arrange while you jam.

<!-- keywords: song, arrangement, song mode, chain, order, bars, structure, loop, part, 编排 -->

## MIX: the mixer {#tape-mix}

{{shot:tape-mix|MIX: four faders, MUTE / SOLO / SEQ switches, the master volume at the bottom.
mark cells:1,1-4,1 = T1's column
mark cell:1,2 = Selected track
mark cells:2,1-2,4 = MUTE
mark cells:3,1-3,4 = SOLO
mark cells:4,1-4,4 = SEQ: sequencer on / off
mark footer = MASTER volume · output level
key G2 = Selected: fully lit
key G5-G12 = MUTE / SOLO off: half lit
key G13-G16 = SEQ on: fully lit
}}

- One GRID column per track: row 1 select, row 2 mute, row 3 solo, row 4 sequencer on / off.
- {{key:ADD}} / {{key:SUB}} set the selected track's level (0–100, default 100; {{key:FN+ADD}} / {{key:FN+SUB}} ±10); {{key:OK}} switches between the track and MASTER. Hold a track's row-1 key: the pads are its levels.
- MASTER is the device's output volume, the same as VOLUME on [[#setup-system]]: it belongs to the device, not to the project. Track levels, mute, solo and SEQ are saved with the project.
- FN menu: **UNMUTE** ALL and **UNSOLO** ALL (available when a track is muted / soloed).

> [!tip] Holding {{key:TAB}} gives the same layout on any screen (the track map).

<!-- keywords: mix, mixer, fader, level, volume, mute, solo, master volume, 混音 -->

## SENDS: every track's effects at once {#tape-sends}

{{shot:tape-sends|SENDS: one column per track, one row per effect.
mark cells:1,1-1,4 = FILTER
mark cells:2,1-2,4 = CHORUS
mark cells:3,1-3,4 = DELAY send
mark cells:4,1-4,4 = REVERB send
mark cell:1,3 = Focus
key G3 = Focus: fully lit
key G1 G2 G4-G16 = Changed from the default: half lit · at the default (off): off
}}

- Tap a cell to select it (and its track), {{key:ADD}} / {{key:SUB}} change it ({{key:FN+ADD}} / {{key:FN+SUB}} 10 at a time); hold a cell and the pads are its values. Cells that are off are dimmed.
- {{key:OK}}, or a second tap on a CHORUS cell: CHORUS changes mode, other cells turn off / back to their value; {{key:FN+OK}} or RESET in the FN menu: default.
- These are the same `fx.*` values as the track's INSTR FX page ([[#fx-sends]]).

<!-- keywords: sends, send, aux, reverb send, delay send, effects overview, 发送 -->

## FX: master effects {#tape-fx}

The master bus effects, part of the project:

{{shot:tape-fx|Master FX.
mark cells:1,1-1,4 = DELAY: time · feedback · TONE · return
mark cells:2,1-2,4 = REVERB: SIZE · DAMP · LOCUT · return
mark cells:3,1-3,4 = COMP · LOW · HIGH · TAPE
mark cells:4,1-4,4 = Echo train · reverb decay
key G1 = Focus: fully lit
key G2-G12 = Parameters: half lit
}}

Tap a cell to focus it, {{key:ADD}} / {{key:SUB}} change it ({{key:FN+ADD}} / {{key:FN+SUB}} 10 at a time on wide ranges), hold a cell and the pads are its values. {{key:OK}} on DELAY steps the note value (the time follows the BPM); {{key:FN+OK}} (or RESET in the FN menu) restores the default. See [[#fx-master]].

<!-- keywords: master fx, bus, compressor, eq, equalizer, tape saturation, master delay, master reverb, 总线 -->
