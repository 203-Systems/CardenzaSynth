# Versions {#changelog}

The manual's version is at the top right (for example v1.2.0), followed by the firmware it describes (the code commit; a `+` means uncommitted changes are included) and the build date. The parameter tables, screenshots and key lights in this manual are generated from the firmware code, so they always match that firmware.

<!-- keywords: version, changelog, release notes, what's new, update, 版本 -->

## 1.2.0 · 2026-10-27 {#version-1-2-0}

Four scenes, and FN becomes the menu of the screen:

- **Four scenes in two kinds**: 1 INSTR (what the current track sounds like), 2 SEQ (what it plays), 3 TAPE (all four tracks together: SESSION, SONG, MIX, SENDS, FX) and the new **4 SETUP** (global and settings: PROJECT, FILES, KEYS, CHORD, ARP, SYSTEM). See [[#nav-scenes]] and [[#setup]].
- A tap on ESC walks only 1 → 2 → 3. SETUP is reached by holding ESC and pressing GRID column 4 (the scene map now has four columns), or with SETUP in the FN menu; in SETUP a tap on ESC goes back to where you came from.
- PROJECT and FILES moved from TAPE to SETUP. The project page also has PLAY (LOOP / SONG). FN+S never asks for a name: an untitled project is saved as SONG 01, SONG 02…
- **FN is the menu of the current screen** ([[#fn-menu]]): hold it and the GRID shows what this page can do (copy, paste, presets, launch, reset…), with SAVE, TAP, REC and SETUP on the bottom row of every screen; tap it and the menu stays open. The FN panel, its QUICK page, FN SETUP and FN + 1…5 are gone; the settings live on SETUP's KEYS, CHORD, ARP and SYSTEM pages. FN + CTRL is now the previous page, FN + ADD / SUB take big steps.
- Power-on starts on 1 INSTR, page VOICE (track T2 as before).
- STEP view while playing: the step the playhead enters flashes green once and keeps an outline; no trail behind it.

<!-- keywords: 1.2.0, setup, four scenes, fn menu, menu, settings moved, fn panel, boot -->

## 1.1.5 · 2026-10-27 {#version-1-1-5}

- In ROLL a GRID tap goes to that step: only it is selected, and tapping it again lets go. Its notes light up fully on the pads; on an empty step the pads stay a very dim keyboard. While playing, the notes of the step under the playhead light up too. For several steps at once, set SELECT to MULTI on the FN panel's 1 KEYS page.

<!-- keywords: 1.1.5, multi select, single select, current step, select -->

## 1.1.4 · 2026-10-26 {#version-1-1-4}

- Key lights are gamma-corrected: the hardware layer corrects only the overall brightness and drives each key linearly, so the firmware converts every level for the eye first. "Half lit" is now the dimmest glow a key has, clearly apart from fully lit.

<!-- keywords: 1.1.4, gamma, correction, half lit, too bright -->

## 1.1.3 · 2026-10-26 {#version-1-1-3}

- Power-on and new projects start on {{track:2}}: the pads show the piano layout from the start, white and black keys half lit, the gaps (Shift, D, J) dark, a pressed key fully lit.

<!-- keywords: 1.1.3, power-on, default track, piano layout -->

## 1.1.2 · 2026-10-26 {#version-1-1-2}

- The hardware layer, CardenzaHAL, now gives each key 16 real brightness levels and dims them all with a separate hardware control for KEY LED. "Half lit" is now 20 % on-time, about half as bright to the eye and clearly apart from fully lit; the KEY LED ramp shows 8 of the 16 levels.

<!-- keywords: 1.1.2, brightness, key lights, 16 levels -->

## 1.1.1 · 2026-10-26 {#version-1-1-1}

- Key lights work exactly as the device's hardware layer, CardenzaHAL, drives them: four real steps, 25 / 50 / 75 / 100 %, with "half lit" at 25 %. The key-light diagrams, the KEY LED brightness ramp and the web simulator follow this model.
- Volume is set once, by the audio chip (it was also scaled in software before, so low settings came out quieter than set).

<!-- keywords: 1.1.1, brightness, key lights, volume -->

## 1.1.0 · 2026-10-26 {#version-1-1}

The manual now looks like the product:

- The whole page is the device's dark interface: the simulator's panels, screen bezel and keys, and the screen's colours (ESC orange, TAB cyan, FN yellow, CTRL purple, PLAY green, track colours).
- What to look at on a screenshot is **highlighted**: the rest dims, each highlight gets the screen's yellow dashed frame and a number, and the list under the picture says what each number is. Keys on the key-light diagrams are highlighted the same way.
- Keys are drawn as the real key caps (the simulator's legends and colours); a held key says HOLD, and a GRID key shows its cap and its cell (for example 9 G1).
- Every procedure is a row of numbered steps: which key → what the screen shows → what happens. 21 new screenshots (a drum beat, the track hint, chords, the arpeggiator, a take put into a pattern, queued launches, saving…).
- Scenes, tracks, page tabs and key-light states in the text are drawn as the screen draws them.

<!-- keywords: 1.1, new look, highlights, numbered, redesign -->

## 1.0.0 · 2026-10-26 {#version-1-0}

The first manual, for these firmware features:

- Three scenes INSTR / SEQ / TAPE, four tracks (DRUMS, JUNO, DX7, SAMPLER), navigation by holding a left key; a tap on ESC walks the scenes.
- The FN panel: KEYS, CHORD, ARP, PROJECT and SYSTEM pages; hold FN for quick BPM, volume and octave; the yellow-legend arrows and text cursor in input screens.
- SEQ: two views, ROLL piano roll (default) and STEP recording; OK is the record key (tap: view, hold: the RECORD panel); capture keeps the last 8 phrases played, to put a bar at a time into a new pattern or merge it.
- Playing: piano / scale / chromatic layouts, 14 scales, scale lock, one-key chords and voicings, the arpeggiator.
- TAPE: SESSION (Ableton-style launching), SONG arrangement, MIX, SENDS, master effects, projects and files.
- Key lights: off / half / full / blinking, half being a dim pulse; the screen's edge strips follow the key lights; the KEY LED brightness ramp.
- Storage protocol: SD card folders, settings files, kept sessions; the web simulator (the same firmware code).
- The manual: every screenshot can become a running device to follow along on (one at a time, the section opened last); a Cardenza on USB plays it with its own keys.

<!-- keywords: 1.0, first version, feature list, release, 第一版 -->
