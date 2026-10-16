## Synth Sequencer Behavior and Design Constraints

The Synth app contains a four-track step sequencer aligned with its four synth tracks. The current implementation uses 16 steps per pattern, 1–16 patterns per track, up to eight notes per step, 96 PPQN, and six microsteps per step. Sequence state is saved with the project.

The September 22 update adds distinct Home editing, Transport, Tempo, Track and Pattern presentations. The September 23 update adds a persistent on-screen 4x4 Sequencer Home view alongside the default performance keyboard. These are focused usability updates; a complete new visual design language, Song/Chain arrangement, recording/overdub, swing and variable pattern lengths remain outside the implemented scope. Build and hardware evidence is recorded separately in [the implementation record](implementation-2026-09-22.md).

### Reuse Strategy

- Reuse the MatrixOS Sequencer data and editing ideas from `Sequence`, `SequenceData`, and `SequenceEvent` as much as practical: tracks, clips, patterns, timestamped events, note length, note-off scheduling, and helpers such as add/clear/has-event-in-range.
- Do not copy MatrixOS UI components wholesale. `PatternPad`, `NotePad`, and `ControlBar` are behavioral references only; ComputeCardOS needs UI behavior in `Applications/Synth/UI` and runtime behavior in `Applications/Synth/Runtime`.
- Do not reuse MatrixOS `SequenceMeta` directly. ComputeCardOS needs a smaller four-track metadata model with project-level defaults, active pattern state, track enable/mute state, and per-track edit defaults.
- Replace MatrixOS-specific dependencies during any port: `MatrixOS::SYS::Micros/Millis`, `MatrixOS::MIDI::Send`, MatrixOS `File`/CBOR wrappers, `Color`, FreeRTOS mutex handles, and MatrixOS application/UI classes.

### Home Layout

- Home starts in the performance keyboard view. Press both right-bar `+` and `-` buttons together to toggle between that view and the persistent on-screen 4x4 Sequencer grid. The combination also ends a held-step edit on Home before switching; it does not stop sequence playback or change octave/keyboard settings. Physical HOME on neutral Home remains an alternative view toggle.
- If a step is held, a Pattern/shift layer is active, or a Context menu is open, HOME only cancels the gesture or returns Home; it preserves the chosen keyboard/grid view. A further HOME press on neutral Home toggles the view.
- `SYS > SEQ` also offers `GRID` / `KEYS` in the bottom-left cell of its 2x4 action bar. The label names the destination view; selecting it toggles that view and returns Home immediately.
- Preserve the existing keyboard layout boundaries:
	- column 0: left bar / track buttons by default
	- columns 1-8: performance keyboard
	- columns 9-12: sequencer 4x4 step grid
	- column 13: right bar / context buttons
- Both the physical keyboard/LED grid and the on-screen Sequencer grid represent 16 steps for the active track's current pattern, in row-major order: top row steps 1-4, then 5-8, 9-12, and bottom row 13-16. Switching the screen view does not remap any physical keys.
- Track selection remains owned by the left bar when no step is held. The LED grid always reflects the active track.
- Empty steps should be dim, steps with events should be brighter, held/selected steps should be brightest, and the playback cursor should be visually distinct without hiding event state.
- The performance view shows the keyboard visualization, or named drum pads for Drums. The Sequencer view shows a persistent 4x4 grid with empty, filled, held/selected and playing states. Releasing a step leaves this view open.
- Holding steps in the performance view replaces its overview with the selected step count, note names and scope, or the active value lane. In the Sequencer view, the grid stays visible during NOTE/LEN/VEL/STEP editing, with compact details for the current selection and values. The footer keeps pattern, transport and BPM context visible.
- Home view choice is UI session state. It does not change Project v1, sequence data, track selection, or the physical performance/step boundaries.
- Keep the grid screen focused on musical state: the shared title bar (TRACK1, engine in cyan, patch in grey), numbered steps with the lowest note (`C3+2` for chords), cyan selection, a green playhead, and the shared Loop Bar below. Do not show persistent instruction text or pattern/transport/BPM status. While steps are held the title bar becomes SEQUENCE with the held steps in cyan and, on the grid, the note or lane value in grey; the scope already shown in the left bar is not repeated. Grid geometry does not change while editing.

### Step Editing Gestures

- Pressing performance notes without a held step remains normal live playing.
- Pressing a step selects or holds that step for editing.
- Holding a note and pressing a step, or holding a step and pressing a note, toggles that note event in that step. If the note already exists in the step, remove it; otherwise add it using the current default length and velocity.
- Chords are supported by holding multiple notes and pressing a step. Repeating the same gesture removes the matching notes.
- The sequencer should store note events per track/pattern/step, not as UI-only state.
- Copying a step clamps each copied note's duration to the destination pattern boundary, including its microstep offset. Copying a long note into step 16 cannot make it extend beyond the pattern.
- Changing the active track ends scope audition and clears held steps and edit-lane keys, so input from the previous track cannot edit the next track.

### Step-Held Left Bar

- While one or more steps are held, the left bar changes from track selection to step-edit controls:
	- `NOTE`
	- `LEN`
	- `VEL`
	- `STEP`
- Releasing all held steps restores the normal left-bar track selection behavior.
- Releasing a `LEN`, `VEL` or `STEP` button keeps that lane selected while any step remains held. This selection is only an editing gesture state, not a saved project setting; releasing all steps returns to `NOTE` / `ALL`.
- These controls edit the held step selection and should not navigate away from Home unless a later design explicitly introduces a full-screen editor.

### Shift Layer And Pattern Selector

- The Home right-bar octave buttons double as sequencer shift keys. A single short tap changes the melodic keyboard octave; holding either `+` or `-` alone displays the Pattern selector. Drums retains its fixed-pad octave behavior.
- Pressing the other octave button while the first is held toggles the keyboard/Sequencer Home view and ends the current sequencer editing gesture. Releasing the combination does not transpose the keyboard or stop playback.
- Pattern selection is momentary: it stays visible only while a single octave key or an activated `CLR` / `COPY` / `QNT` key remains held. Release all those keys to return to the selected Home view. There is no Pattern-lock gesture or `LOCK` action.
- While shift is active, the left bar becomes sequencer modifiers:
	- `CLR` clears held steps immediately, or clears a pressed step while held. Pressing a populated pattern opens a confirmation naming that exact pattern; `BACK` cancels and `YES` deletes it. An empty pattern can be deleted immediately. Deleting the only populated pattern requires the same confirmation and clears its notes while retaining one slot.
	- `COPY` captures the first pressed step or pattern as a copy source, then copies it to subsequent step or pattern targets while held.
	- `QNT` quantizes already-held steps immediately. With no held steps it only arms target selection: hold `QNT` and press a step to quantize that step, or press an existing pattern in the upper 2x8 selector to quantize that pattern without selecting it. Pressing and releasing `QNT` alone changes nothing; unavailable pattern slots do not create patterns or affect timing.
- After pressing a shift modifier, the user may release the octave shift key; the modifier stays active until its left-bar key is released.
- While shift is active, the upper 2x8 performance area is the pattern selector for up to 16 patterns. The lower 2x8 area remains available to the keyboard surface.
- The pattern selector can choose existing patterns, create the next pattern slot, delete patterns in `CLR`, and copy patterns in `COPY`.
- Deleting a pattern before the active pattern preserves the same selected musical content at its new index. Removed slots are cleared rather than retained invisibly.
- Home pattern deletion returns to Home after confirmation or cancellation. The keys that opened the confirmation can be released safely; deletion requires a new, explicit `YES` press. The equivalent `SYS > SEQ > PATT > CNT−` confirmation returns to Pattern settings.

### NOTE Control

- `NOTE` does not mean "add a note". Note add/remove is done by the note-plus-step gesture.
- `NOTE` cycles the edit scope for chord steps.
- The default scope is `ALL`, meaning length, velocity, and step-offset edits apply to all notes in the held step selection.
- If a held step contains a chord, pressing the top-left `ALL` scope button cycles `ALL -> 1/N -> 2/N -> ... -> ALL`.
- Individual scopes use pitch-low-to-high order. Home shows the actual selected pitch alongside its scope index; `ALL` shows the primary held step's note names.
- If the selected scope note is removed, fall back to `ALL` or the nearest valid note scope.
- Cycling to an individual note scope should audition that note until the scope button is released.
- The screen and LEDs should make the current scope clear enough for editing, but avoid text-heavy explanations on the device UI.

### LEN Control

- `LEN` edits note length for the current scope: all notes when scope is `ALL`, or the scoped `#/N` note when an individual note is selected.
- The current length buckets are 1, 2, 3, 4, 6, 8, 12 and 16 steps, clamped to the remaining pattern duration.
- The eight performance columns act as direct length buckets while `LEN` is selected. LEDs and the Home value display identify the current bucket; the Sequencer view keeps the step grid visible beside its compact edit details.
- Length edits should preserve the note pitch and velocity.
- A length may extend past the current step, but it should be clamped to the pattern boundary unless a later tie/legato design explicitly supports crossing boundaries.
- Value lanes address physical columns, including positions that are empty black-key gaps in Piano mode. Those same gaps remain inactive when editing notes.
- When selected notes across the held steps have different values, Home displays `MIXED` and does not highlight one value bucket as if it described the entire selection.

### VEL Control

- `VEL` edits velocity for the current scope.
- The eight performance columns select velocity 24, 40, 56, 72, 88, 104, 120 or 127.
- The eight performance columns act as direct velocity buckets while `VEL` is selected. LEDs and the Home value display identify the nearest current bucket, including in the Sequencer grid's compact edit details.
- Velocity edits should optionally audition the held step only if doing so does not cause stuck notes or disrupt live playing.
- Keep a per-track default velocity for newly inserted notes.

### STEP Control

- `STEP` edits microtiming inside the held step. It is the fourth step-edit lane in the first sequencer UI.
- Each normal step is divided into 6 microstep slots. Display them as 1-6 for users, but store them as zero-based slots 0-5 or equivalent pulse offsets internally.
- At 96 PPQN with 24 pulses per 16th step, 6 microsteps means 4 pulses per microstep. This fits the MatrixOS-derived timing model without fractional pulses.
- New note events default to microstep slot 0, meaning exactly on the step grid.
- `STEP` follows the same edit scope rule: `ALL` applies the microstep slot to the whole held step/chord, while `#/N` moves one note within the chord.
- Microstep edits should move event start time only. They must preserve pitch, velocity, and length unless a later design explicitly adds linked behavior.
- The visible control should be labeled `STEP` even though the implementation stores microstep/microtiming internally.
- The first six performance columns act as direct microstep buckets while `STEP` is selected. The remaining columns should be dim or inactive.

### Page-Level UX

- Sequencer features should live inside the Synth app, not as a separate application.
- Home remains the playable surface in both keyboard and Sequencer grid views. Settings pages configure the sequencer; step programming stays Home-first.
- Non-Home pages show their own compact sequencer summaries. Home's performance and step input mapping stays tied to the same physical columns in both views.

#### Home: Performance View

- Shows the active track identity, patch context, performance keyboard, and bottom pattern bar.
- Supports live playing on columns 1-8.
- Supports note-plus-step event toggling on columns 9-12.
- Shows pattern count, active pattern, and playback progress via the bottom bar; the physical LED step grid remains available for editing.
- Pressing `+` and `-` together switches to the Sequencer grid view; physical HOME on neutral Home also switches views. File management and detailed tempo setup remain in their Context menus; destructive pattern gestures use the momentary Pattern layer and confirmation.

#### Home: Persistent Sequencer Grid

- Replaces the performance visualization with a 4x4 view of the active track's current 16-step Pattern. Empty, filled, selected and playing states remain distinguishable; playback does not hide the event state.
- Keeps columns 1-8 as the live performance/note-value area and columns 9-12 as physical step keys. A visible cell is feedback for that same physical step, not a remapped input surface.
- Keeps the grid visible during NOTE, LEN, VEL and STEP edits. Compact details show the held step or selection, note names/scope and values or `MIXED`; releasing all steps returns to the grid overview.
- Pressing `+` and `-` together switches back to the performance view, ending a held-step edit if necessary while preserving sequence playback and keyboard settings. Physical HOME during an edit only cancels it and keeps the grid selected; HOME on neutral grid Home switches views. Context/Pattern exits preserve the current view; `SYS > SEQ > KEYS` explicitly switches back.
- This is a persistent Home presentation within the current UI session. It adds neither Song/Chain arrangement nor a Project v1 field.

#### Home Step Edit Layer

- Step editing is a temporary layer while one or more steps are held, available in either Home view. The underlying Sequencer grid view itself remains selected after release.
- Left bar becomes the current note scope (`ALL` or `#/N`), `LEN`, `VEL`, `STEP`.
- The left bar, LEDs and screen make the active lane and held steps clear. In grid view, the 4x4 grid stays visible with compact edit details.
- It should be safe to release the step at any time; releasing all steps returns to the selected Home view.

#### `SYS > SEQ` Root

- Implemented navigation is `SYS > SEQ`; `KEY` remains keyboard settings. Project management is under `SYS > PROJ`.
- The root is the transport overview: playing/stopped state, BPM, active track/pattern, and track enable/mute state. Its actions are `PLAY`/`STOP`, `STOP`, `RES`, `BPM`, `TRK`, `PATT`, `GRID`/`KEYS` and `BACK`.
- The bottom-left `GRID`/`KEYS` action switches to the named Home view and returns there immediately; bottom-right remains `BACK`. Step programming stays on Home, where grid view keeps the 16 steps visible during editing and playback.

#### Transport and Tempo

- Transport is on the SEQ root. `BPM` opens a distinct page with a large value, 30–300 range, playing/stopped status and −10 / +10 / −1 / +1 actions.
- Stop must flush queued note-offs for all tracks.
- Play starts at the beginning; Resume continues from the stopped position. There are no sequence recording/overdub controls in this version.

#### Pattern Page

- Shows active pattern / count, populated steps out of 16, and the pattern slots. `PREV` / `NEXT` select a pattern; `CNT−` / `CNT+` change the count between 1 and 16.
- `CNT−` removes the last pattern. If it contains notes, a separate confirmation identifies the pattern and warns that notes will be lost; `BACK` cancels. An empty last pattern can be removed directly.
- Reducing the count clears removed slots. Increasing it again creates empty patterns, without reviving deleted notes.
- Clear/copy gestures remain in Home's explicit shift layer. Pattern length is fixed at 16 steps; variable lengths are not exposed.

#### Track Page

- Shows all four tracks with their active patterns and ON / MUTE / OFF states. `T−` / `T+` change the active synth track while keeping this page open; `MUTE` and `ON` / `OFF` apply to that selected track.
- OFF skips sequence playback for the track. Track selection is shared with the Synth app rather than maintained as a separate hidden selection.
- Muting a sequencer track must send note-offs for currently held sequencer notes on that track.
- Track settings are sequencer/project state, not synth engine preset state.

#### Project Page

- Owns new, load, save, save as, rename, and delete project operations when project management exists.
- Keep project operations out of the Home step grid and out of individual synth preset pages.
- `SAVE` should write the current synth/sequencer/sampler references into one project file or project folder.
- `LOAD` should restore sequence state and track synth state together so playback sounds like the saved project.
- New/delete/overwrite operations need confirmation.
- `SYS > PROJ` now performs `NEW`, `SAVE`, `SAVEAS`, and `LOAD` using versioned project files. Loading preflights synth state and sample files before replacing the live session; New and Load require confirmation and reject unsaved sampler recordings. See [the implementation record](implementation-2026-09-22.md) for the current verified scope.

### Drums Tracks

Drums is engine kind 4 and reuses AMY's built-in 808 PCM kit. Every row in Home's eight-column performance zone repeats this mapping, independent of octave, root, scale or Piano mode:

| Column | Sound | GM note |
| --- | --- | --- |
| 1 | Kick | 36 |
| 2 | Snare | 38 |
| 3 | Closed Hat | 42 |
| 4 | Open Hat | 46 |
| 5 | Clap | 39 |
| 6 | Tom | 43 |
| 7 | Cowbell | 56 |
| 8 | Maraca | 70 |

The sequencer stores these GM notes directly. Each Drums track has six voices. Its eight sound levels affect subsequent hits, while master level changes the whole track immediately; all nine parameters are 0–127 with default 100. They are available through SND and saved by the existing preset workflow (`drums.808` / `808 Kit`, user bank in `Preset/Drums`).

Ordinary key or sequencer note-off leaves the one-shot tail sounding, so a drum event's LEN does not gate the sample. Explicit Stop, preset replacement and engine replacement stop the voices. Closed Hat chokes Open Hat. Only the eight listed GM notes are supported; this is not a complete General MIDI drum module.

### Project Saving

- Sequencer state is project-level state. Do not store it in individual synth presets or sampler sample files.
- Store projects under `Device::Storage::ProjectDirectoryPath()` using paths relative to the ComputeCardOS storage root.
- Use a versioned project format so future synth, sampler, and sequencer state can evolve without breaking old projects.
- Project v1 keeps DX7=1, Juno=2 and Sampler=3 and adds Drums=4 without changing the existing field layout. The current reader accepts old projects and rejects unknown kinds; older software cannot load projects containing Drums. Drum preset identity and all nine parameters are embedded, with no sample-file dependency.
- Save at least:
	- project format version and app/build compatibility metadata
	- tempo, swing if present, transport defaults, and pattern length
	- active track and active pattern/clip selection
	- per-track sequencer metadata: enabled/muted state, default velocity, default length, edit defaults, and any future probability/ratchet defaults
	- sequence data: tracks, clips, patterns, steps, note events, note lengths, velocities, microstep offsets, and future control/modulation events
	- synth track references: engine kind, selected preset identity, edited parameter overrides, and any sampler sample path references needed to reproduce the project
- Store sample references as relative paths into the sample storage area. Do not duplicate sample audio into every project by default.
- Prefer explicit user save for project files. If autosave is added later, write through a temporary file and rename/replace atomically to avoid corrupting projects on power loss.
- Do not depend on MatrixOS `File` serialization directly. ComputeCardOS storage is path-based; implement a ComputeCard-specific serializer or a small portable serializer layer.

### Sequencer Tick And Timing

- Do not drive musical timing from the UI frame rate. `SynthApp::Tick()` can coordinate state, but it should not be the authoritative sequencer clock for playback.
- Keep the MatrixOS timing model where useful: 96 PPQN, 24 pulses per 16th step, absolute microsecond deadlines, and catch-up when a pulse is late.
- The ideal firmware implementation is a dedicated sequencer clock worker notified by a high-resolution timer. Timer/ISR code should only notify or enqueue; MIDI generation and sequence mutation should happen in task/thread context, not inside an ISR.
- On desktop, use the same clock abstraction backed by `std::chrono::steady_clock` and a worker thread or equivalent host timer.
- Playback events are sent directly to the audio runtime by stable track index. Live performance retains each track's MIDI port; sequence events do not wait in UI MIDI queues.
- Use bounded queues or preallocated event buffers for clock-to-runtime communication. Avoid heap allocation, file I/O, logging spam, or long locks in the timing path.
- Stop/all-notes-off handling must be robust. Stopping playback, changing track state, or muting a track must flush queued note-offs and send necessary note-off/control messages so voices do not hang.
- Host regressions and successful builds do not establish device timing. Real microphone recording, four-track maximum load, heap peaks, SD-write timing and audio deadline behavior still require hardware measurements.

### LED Task Safety

- The keyboard LED scan is a high-frequency device task/ISR path. Sequencer playback must not run inside that path and must not block it.
- Do not write directly to keyboard scan buffers from the sequencer clock worker.
- Sequencer runtime may publish a compact playback/edit snapshot, such as current step and per-step event flags. `Application::UpdateLeds()` should consume that snapshot and call the normal `Device::LED` API at the OS frame/update boundary.
- Keep LED rendering deterministic and cheap: no dynamic allocation, no storage access, no MIDI sends, and no deep sequence traversal inside `UpdateLeds()`.
- If firmware priorities are adjusted, keep keyboard scan/LED timing and audio rendering higher priority than UI rendering. The sequencer worker should be precise enough for MIDI but must not starve audio or LED multiplexing.

### Implementation Boundaries

- Put app-owned sequencer runtime code under `Applications/Synth/Runtime/Sequencer` or an equivalent Synth-owned runtime folder.
- Put reusable sequence data/model code under `Library/Synth/Core` only if it is genuinely engine-agnostic and free of app/UI/device dependencies.
- Add new source files to `cmake/ComputeCardOSSourceLists.cmake` until the build system has a more local source registry for the module.
- Keep UI snapshot additions small and explicit. The renderer should receive prepared step-grid state rather than searching the full sequence model during drawing.
