# 3 TAPE {#tape}

TAPE is the whole song: which patterns play when, how the four tracks are mixed, the master effects and the project files. Seven pages ({{key:CTRL}} steps):

{{page:SESSION}} {{page:SONG}} {{page:MIX}} {{page:SENDS}} {{page:FX}} {{page:PROJECT}} {{page:FILES}}

<!-- keywords: tape, song, arrangement, mixer, project, whole song, 走带 -->

## SESSION: launch like Ableton {#tape-session}

One column per track (in its colour), one row per pattern slot; each cell draws the pattern's 16 steps.

{{shot:tape-session|SESSION: the four tracks side by side.
mark cells:1,1-4,1 = T1
mark cells:1,2-4,2 = T2
mark cells:1,3-4,3 = T3
mark cells:1,4-4,4 = T4
mark cell:2,2 = T2's second pattern
mark cell:2,1 = +: new pattern
key G1 G3 G4 = Current: fully lit
key G6 = Existing: half lit
}}

{{strip
PLAY, G6 > tape-session-queued @cell:2,2 | While playing, tap a cell: the track switches when its loop ends. The cell shows NXT, its light is {{led:blink|blinking}}.
- > tape-session @cell:2,2 | The loop ends and T2 switches to P02. When stopped, a tap switches at once.
}}

- Tap a cell: that track plays that pattern. The "+" after the last pattern creates a new one.
- {{key:ADD}} / {{key:SUB}}: move the cursor and scroll; {{key:OK}}: launch the cursor's whole row (every track that has that pattern).
- {{key:FN+OK}}: capture what every track plays (or has queued) as a new part in SONG.
- Launching from SESSION leaves SONG mode. A playing cell is green with ▶ and a moving playhead.

<!-- keywords: session, clip, launch, scene launch, ableton, live, grid launch -->

## SONG: arranging the song {#tape-song}

The song as a tape: each **column** is a part (time runs left to right), each **row** a track; a cell holds the pattern that plays in that part and the part's length in bars (xN).

{{shot:tape-song|SONG: each column a part, each row a track.
mark cells:1,1-4,1 = Part 1
mark cells:1,2-4,2 = Part 2
mark cell:1,1 = Selected cell
mark 34,15,26,9 = Bars: xN
mark cell:2,3 = Tap an empty column: add a part
mark 196,108,32,12 = LOOP / SONG mode
}}

- Tap a cell to select it; tap the same cell again to switch between "pattern" and "bars". `--` means the track is silent in that part.
- {{key:ADD}} / {{key:SUB}}: change the cell's pattern, or the part's length in bars (1–16).
- {{key:OK}}: play from this part (switches to SONG mode); {{key:FN+OK}}: SONG / LOOP.
- {{key:FN+C}} / {{key:FN+V}} / {{key:FN+X}} / {{key:FN+D}}: copy, paste, delete, duplicate a part.

In SONG mode each part plays xN bars (1 bar = 16 steps), then the next; after the last part it starts over from the first. LOOP mode: every track loops its current pattern.

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
key G5-G8 = Mute
}}

- One GRID column per track: row 1 select, row 2 mute, row 3 solo, row 4 sequencer on / off.
- {{key:ADD}} / {{key:SUB}} set the selected track's level; {{key:OK}} switches between the track and MASTER.

> [!tip] Holding {{key:TAB}} gives the same layout on any screen (the track map).

<!-- keywords: mix, mixer, fader, level, volume, mute, solo, master volume, 混音 -->

## SENDS: every track's effects at once {#tape-sends}

{{shot:tape-sends|SENDS: one column per track, one row per effect.
mark cells:1,1-1,4 = FILTER
mark cells:2,1-2,4 = CHORUS
mark cells:3,1-3,4 = DELAY send
mark cells:4,1-4,4 = REVERB send
mark cell:1,3 = Focus
key G1-G16 = On: half lit · off: off
}}

- Tap a cell to select it (and its track), {{key:ADD}} / {{key:SUB}} change it. Cells that are off are dimmed.
- {{key:OK}}: CHORUS changes mode, other cells turn off / back to their value; {{key:FN+OK}}: default.

<!-- keywords: sends, send, aux, reverb send, delay send, effects overview, 发送 -->

## FX: master effects {#tape-fx}

The master bus effects, part of the project:

{{shot:tape-fx|Master FX.
mark cells:1,1-1,4 = DELAY: time · feedback · TONE · return
mark cells:2,1-2,4 = REVERB: SIZE · DAMP · LOCUT · return
mark cells:3,1-3,4 = COMP · LOW · HIGH · TAPE
mark cells:4,1-4,4 = Echo train · reverb decay
}}

{{key:OK}} on DELAY steps the note value; {{key:FN+OK}} restores the default. See [[#fx-master]].

<!-- keywords: master fx, bus, compressor, eq, equalizer, tape saturation, master delay, master reverb, 总线 -->

## PROJECT {#tape-project}

{{shot:tape-project|PROJECT: the project name, whether it is unsaved, a summary of the four tracks.
mark cell:1,1 = SAVE
mark cell:1,2 = SAVE AS
mark cell:1,3 = LOAD
mark cell:1,4 = NEW
mark cell:2,1 = BPM
mark cell:2,2 = SWING
mark cell:2,3 = TAP: tap tempo
mark 100,85,48,10 = UNSAVED: unsaved changes
}}

- Row 1: SAVE, SAVE AS, LOAD (opens FILES), NEW (asks first if there are unsaved changes).
- Row 2: BPM, SWING, TAP (tap tempo).

<!-- keywords: project, save, save as, new, load, open, 工程 -->

## FILES: project list {#tape-files}

All projects on the SD card. {{key:ADD}} / {{key:SUB}} choose, {{key:OK}} loads; with unsaved changes it asks first.

{{shot:tape-files|FILES: no projects on the card yet.
mark content = Project list
mark footer = FILES page
}}

<!-- keywords: files, load project, open project, project list, 文件 -->
