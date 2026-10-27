# 2 SEQ {#seq}

Every track has up to 16 patterns of up to 16 steps; each step holds up to 8 notes (a chord), each note with its own length, velocity and a small timing offset (6 microsteps per step). SEQ has three pages:

| Page | For |
|---|---|
| {{page:ROLL}} / {{page:STEP}} note page | writing, editing and recording notes, in two views: ROLL piano roll (default) and STEP step recording |
| {{page:PATTERN}} | choosing, creating and queueing the track's patterns |
| {{page:TRACK}} | the track's sequencer switch and steps, global swing and BPM |

<!-- keywords: sequencer, sequence, pattern, steps, loop, beat, arrangement, 音序器 -->

## The note page's two views {#seq-notes}

{{key:OK}} is this page's {{icon:rec}} **record key**: tap it to switch between the two views, hold it for the RECORD panel.

{{strip
- > seq-notes @footer | {{icon:roll}} **ROLL piano roll** (default): time × pitch, notes drawn as bars.
OK > seq-step-view @footer | {{icon:step}} Tap {{key:OK}}: **STEP recording**, a 4×4 of steps with note names.
OK > seq-notes @footer | Tap again: back to ROLL. The {{key:OK}} light is {{led:half}} in ROLL, {{led:full}} in STEP.
}}

Both views edit the same pattern, and the 16 GRID keys are the 16 steps in both (row 1 is steps 1–4, row 2 steps 5–8…).

| | {{icon:roll}} ROLL | {{icon:step}} STEP |
|---|---|---|
| Screen | time × pitch, notes as bars | a 4×4 of steps with note names or chords |
| GRID tap | selects / deselects the step | moves the write cursor to the step |
| Playing PADS | add / remove notes on selected steps; with none selected they just play (and record live with REC on) | write into the cursor step, the cursor moves on when released |
| {{key:ADD}} / {{key:SUB}} | change the selected steps | move the cursor |

<!-- keywords: view, mode, piano roll, step view, switch view, 视图 -->

## ROLL piano roll {#seq-roll}

{{shot:seq-notes|ROLL: a few notes on T2. The row of small cells at the bottom is the GRID's 16 steps.
mark 26,52,14,10 = A note: length = duration
mark 8,50,14,12 = Every C is marked
mark 26,119,198,9 = 16 steps = GRID
mark footer = ROLL view
key G1 = Has notes: half lit
key G2 = Empty step: off
}}

The track's 16 steps run left to right, pitch bottom to top (the range around the notes in use, shown automatically: at least an octave, black-key rows shaded). The more solid a note, the higher its velocity; the green line is the playhead. The small cells at the bottom: lit where a step holds notes, white where it is selected.

**Write notes: hold one or more PADS, then tap steps on the GRID.**

{{strip
hold r3c1~C > seq-hold-pad | Hold a note (C).
hold r3c1~C + G1 > seq-hold-tap @27,52,14,10 | Keep holding and tap GRID 1: C is written into step 1.
hold r3c1~C + G5, G9 > seq-notes @26,119,198,9 | Go on with 5, 9…, then switch to another note and tap a few more steps.
}}

- **Select and edit**: tap a GRID key to select a step (several at once work too), see [[#seq-edit]].
- **Record live**: turn REC on ({{key:FN+PLAY}} or the RECORD panel), start playback, then play the pads, see [[#seq-transport]].

<!-- keywords: piano roll, roll, pitch, note length, duration, 钢琴卷帘 -->

## STEP recording {#seq-step-rec}

Write notes one step at a time, without keeping time:

{{strip
OK > seq-step-view @footer | Tap {{key:OK}} for the STEP view.
G2 > seq-step-view @cell:1,1 | The red marching frame is the **write cursor**; tap the GRID to put it on any step.
r3c2~D > seq-step @cell:1,2 | Press a PAD: that note is written into the cursor step. Press several together for a chord.
- > seq-step @cell:1,4 | When you let go the cursor moves on (the "+1" in its corner).
ADD > seq-step @cell:1,4 | {{key:ADD}} skips a step (a rest), {{key:SUB}} goes back.
hold r3c1~C + G16 > seq-step @cell:4,4 | Hold PADS and tap a GRID key: they are written straight into that step.
}}

{{shot:seq-step|The STEP view: one cell per step, with note names; the red frame is the cursor.
mark cell:1,4 = Write cursor: blinking
mark 213,30,15,8 = How far it moves each time
mark cell:1,1 = A written step
mark footer = STEP view
key G4 = Cursor: blinking
key G1 G2 G3 = Has notes: half lit
key OK = STEP: fully lit
}}

- How far the cursor moves each time is STEP in the RECORD panel (1 / 2 / 4).
- Written notes use the KEYS page velocity (VELO) and are one step long. Switch back to ROLL later to fine-tune them.

<!-- keywords: step record, step input, step entry, rest, cursor, one step at a time, 步进录制 -->

## Selecting and editing {#seq-edit}

In the ROLL view, tap GRID keys to select one or more steps, then:

{{strip
G5 > seq-selected @75,15,14,104 | Tap GRID 5: step 5 is selected and its column highlighted.
OK, OK > seq-lane @footer | {{key:OK}} cycles what you edit, **NOTE → LEN → VEL → TIME**; the bottom line shows the lane and its value.
ADD > seq-lane @footer | {{key:ADD}} / {{key:SUB}} change it; held they repeat. {{key:ESC}} clears the selection.
}}

| Key | Does |
|---|---|
| PADS | add or remove that note on the selected steps (each tone of a chord separately) |
| {{key:OK}} | what you edit: NOTE (pitch) → LEN (length) → VEL (velocity) → TIME (small timing offset) |
| {{key:ADD}} / {{key:SUB}} | change it; held they repeat |
| {{key:FN+OK}} | switch between editing all notes of the chord and just one of them |
| {{key:ESC}} | clear the selection |

While steps are selected, their notes are {{led:full|fully lit}} on the pads.

<!-- keywords: edit notes, pitch, note length, velocity, timing, nudge, microtiming, select steps, 编辑音符 -->

## Play and live recording {#seq-transport}

{{strip
PLAY > seq-playing @strip:PLAY @73,15,4,104 | {{key:PLAY}}: play; the green line is the playhead. Press again to stop, and the playhead rewinds to step 1.
FN+PLAY > seq-recording @strip:PLAY @footer | {{key:FN+PLAY}}: record arm (REC). The PLAY light blinks and a red frame goes around the screen.
r3c1~C > seq-recording @0,13,240,110 | Play the PADS while it plays: the notes are written into the pattern where you play them (overdub).
}}

- REC can also be turned on in the RECORD panel. While recording is armed the dot in the FN strip on the left turns red.
- Recording is overdub: existing notes stay, and new ones land on the nearest microstep. With ARP on, the arpeggiator's notes are what gets recorded.
- A wrong note: select its step and clear it with {{key:FN+X}}, or tidy the timing with {{key:FN+Q}}.

{{shot:seq-recording|Recording while playing: the PLAY light blinks and the status line reads REC.
mark 166,0,22,11 = REC
mark 75,13,4,108 = Playhead
mark strip:PLAY = PLAY: blinking
mark footer = REC ARMED
key PLAY = Blinking: recording
}}

<!-- keywords: play, stop, record, recording, live record, overdub, transport, 播放, 录音 -->

## The RECORD panel {#record-panel}

On the note page **hold {{key:OK}}** (about half a second) for the RECORD panel; {{key:ESC}} closes it.

{{shot:record|The RECORD panel: settings on top, takes in the middle, the bar to place below.
mark cells:1,1-1,4 = Settings: REC · SNAP · STEP · VIEW
mark cells:2,1-3,4 = Last 8 takes, newest first
mark cell:2,2 = The chosen take
mark cells:4,1-4,4 = How this bar would land
mark footer = ±bar · OK new pattern · FN+OK merge
key G1-G4 = Settings
key G5-G12 = Takes
}}

| GRID | Holds |
|---|---|
| row 1 | **REC** live recording on / off · **SNAP** whether takes land on whole steps (STEPS) or keep their timing (FREE) · **STEP** how far step recording moves (1 / 2 / 4) · **VIEW** ROLL / STEP |
| rows 2–3 | the last 8 takes (capture), newest first |
| row 4 | the chosen take's bar as it would land in a pattern |

- Setting cells: a tap switches them; {{key:ADD}} / {{key:SUB}} work too.
- Take cells: a tap chooses; {{key:ADD}} / {{key:SUB}} choose the bar; {{key:OK}} puts it into a new pattern; {{key:FN+OK}} merges it into the current pattern.

<!-- keywords: record settings, recording settings, record panel, snap, quantize, 录音设置 -->

## Capture: get back what you just played {#capture}

{{icon:capture}} No need to turn on REC: every note you play on the pads is kept in the background (capture). When something good came out, put it into the sequencer afterwards:

{{strip
r3c1~C, r3c3~E, r3c5~G > seq-keyboard | Just play. A pause longer than 2.5 s, a track change, more than 48 notes or 60 seconds starts a new take.
hold OK > record @cells:2,1-3,4 | Hold {{key:OK}}: the two middle rows are the last 8 takes, with number, track and note count.
G6 > record @cell:2,2 @cells:4,1-4,4 | Tap a take: row 4 shows how its bar would land. {{key:ADD}} / {{key:SUB}} pick another bar.
OK > record-placed @14,102,212,26 | {{key:OK}}: into a **new pattern** on this track; not a single existing note is touched.
}}

- Without a free pattern slot it tells you; use {{key:FN+OK}} to merge into the current pattern instead.
- Takes played while the transport ran keep their place against the bar; takes played while stopped start with their first note, timed at the BPM of that moment.
- With SNAP on (default) every note lands on the nearest whole step with a whole-step length; off, your playing's fine timing is kept.

<!-- keywords: capture, retrospective record, take, takes, what i just played, recall, no record button, jam, noodle, 无感录制 -->

## PATTERN page {#seq-patterns}

{{shot:seq-pattern|The PATTERN page: this track's pattern slots.
mark cell:1,1 = Existing pattern: half lit
mark cell:1,2 = In use: fully lit
mark cell:1,3 = NEW: create one
mark footer = PATTERN page
key G1 = Existing
key G2 = Current
}}

- Tap a slot: switch to that pattern. Stopped, it switches at once; playing, it is **queued** until the track finishes its loop (the cell shows NEXT and a countdown bar, its key light is {{led:blink|blinking}}).
- Tap the "NEW" slot: a new empty pattern.
- {{key:ADD}} / {{key:SUB}}: next / previous; {{key:OK}} goes back to the note page.
- {{key:FN+D}}: duplicate the current pattern into the next free slot. {{key:FN+C}} / {{key:FN+V}} copy and paste a whole pattern, {{key:FN+X}} clears it (after confirming).

<!-- keywords: pattern, new pattern, switch pattern, duplicate pattern, queue, next pattern, clip, delete pattern, clear pattern -->

## TRACK page {#seq-track}

{{shot:seq-track|The TRACK page: four settings on top, an overview of the four tracks below.
mark cell:1,1 = SEQ on / off
mark cell:1,2 = STEPS
mark cell:1,3 = SWING
mark cell:1,4 = BPM
mark 9,40,222,80 = Four tracks: engine · pattern · step ruler
}}

| Setting | Range | Meaning |
|---|---|---|
| SEQ | on / off | whether this track's sequence plays |
| STEPS | 1–16 | this track's loop length; tracks can differ (12 against 16 for polyrhythms) |
| SWING | 50–75 % | global swing |
| BPM | 30–300 | global tempo; also tap it with {{key:FN+T}} or hold FN and use {{key:6}} / {{key:Y}} |

<!-- keywords: steps, length, loop length, swing, shuffle, tempo, bpm, polyrhythm, 步数, 速度 -->

## Copy, paste, clear, quantize {#seq-tools}

Hold {{key:FN}} with a letter (on the note page they act on the selected steps; with none selected, on the whole pattern):

{{keymap:Edit keys while FN is held
FN = Hold
C = Copy
V = Paste
X = Clear
Q = Quantize
D = Duplicate pattern
T = Tap tempo
}}

| Keys | Does |
|---|---|
| {{key:FN+C}} / {{key:FN+V}} | copy / paste |
| {{key:FN+X}} | clear (asks first for a whole pattern) |
| {{key:FN+Q}} | quantize to whole steps |
| {{key:FN+D}} | duplicate the pattern into the next free slot |
| {{key:FN+T}} | tap tempo |

> [!warn] There is no undo, so clearing a whole pattern always asks first.

<!-- keywords: copy, paste, clear, delete notes, quantize, undo, tidy timing, 复制, 量化 -->
