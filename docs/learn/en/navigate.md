# Navigation {#navigate}

Three scenes with a few pages each, and four tracks you can switch at any time. The four left keys do all the navigation:

{{keymap:The four left keys handle all the navigation
ESC = Scenes
TAB = Tracks
FN = FN panel
CTRL = Pages
}}

<!-- keywords: navigation, menu, switch, move around, 导航 -->

## Scenes: ESC {#nav-scenes}

| Scene | For | Pages |
|---|---|---|
| {{scene:INSTR}} instrument | choosing the engine and sound, setting parameters and effects | VOICE, engine pages, FX |
| {{scene:SEQ}} sequencer | writing notes, recording, managing patterns | ROLL / STEP, PATTERN, TRACK |
| {{scene:TAPE}} tape | arranging the whole song, mixing, master effects, projects | SESSION, SONG, MIX, SENDS, FX, PROJECT, FILES |

**Tap {{key:ESC}}**: the three scenes in turn. The groovebox starts in {{scene:SEQ}}.

{{strip
ESC > instr-voice @scene | {{scene:INSTR}}: sounds and parameters.
ESC > tape-session @scene | {{scene:TAPE}}: arrangement, mixing, projects.
ESC > seq-notes @scene | Back to {{scene:SEQ}}: write notes.
}}

**Hold {{key:ESC}}**: the scene map; press any key of a column to jump straight there.

{{strip
hold ESC > map-esc @cells:1,1-4,3 | Hold ESC: three columns = three scenes, column 4 is empty.
hold ESC + G1 > instr-voice @scene | Tap column 1: straight to {{scene:INSTR}}. Release ESC and the map folds away.
}}

> [!note] In a pop-up {{key:ESC}} cancels; in SEQ with steps selected, ESC clears the selection first.

<!-- keywords: scene, mode, instr, seq, tape, instrument, sequencer, escape, 场景 -->

## Tracks: TAB {#nav-tracks}

Four tracks, with the same colour everywhere: {{track:1}} pink, {{track:2}} blue, {{track:3}} lime, {{track:4}} amber.

{{strip
TAB > tab-hint @strip:TAB | Tap TAB: next track. For about 0.8 s only the matching row of the left keys is lit (T1 = the ESC row … T4 = the CTRL row), so you see at a glance which track you are on.
hold TAB > map-tab @cells:1,1-4,4 | Hold TAB: the track map, laid out like the MIX page.
}}

{{shot:map-tab|Holding TAB: one column per track, one kind of switch per row.
mark cells:1,1-1,4 = Row 1: select this track
mark cells:2,1-2,4 = Row 2: mute
mark cells:3,1-3,4 = Row 3: solo
mark cells:4,1-4,4 = Row 4: sequencer on
mark cell:1,2 = Current: T2
key TAB = Hold
key G1-G4 = Select track
}}

- {{key:TAB}} + {{key:ADD}} / {{key:SUB}}: next / previous track.
- {{key:FN+TAB}}: previous track.

<!-- keywords: track, switch track, change track, channel, which track, 轨道 -->

## Pages: CTRL {#nav-pages}

The page tabs are at the bottom of the screen, the current one on violet (like {{page:ROLL}}); when there are too many you see `<` `>` at the ends and "current / total" on the right.

{{strip
CTRL > seq-pattern @footer | Tap CTRL: next page (on release).
hold CTRL > map-ctrl @cells:1,1-1,3 | Hold CTRL: the page map; each cell shows the tab and a short description.
hold CTRL + G3 > seq-track @footer | Tap a cell to jump to that page; you stay there after releasing.
}}

- {{key:CTRL}} + {{key:ADD}} / {{key:SUB}}: next / previous page.
- Each track remembers which INSTR page it was on.

<!-- keywords: page, pages, next page, sub page, tabs, 页面, 翻页 -->

## Hold a left key = a map {#nav-maps}

Hold {{key:ESC}}, {{key:TAB}} or {{key:CTRL}} for more than 250 ms and the middle of the screen becomes a map: **each cell sits on the GRID key that picks it**. Holding {{key:FN}} shows the FN panel instead (see [[#fn-panel]]).

{{shot:map-ctrl|Holding CTRL: the page map.
mark cell:1,1 = Current page
mark cell:1,2 = Another page
mark header = Map title and hint
key CTRL = Hold
key G1 = Fully lit: current
key G2 G3 = Half lit: can pick
key PADS = Off: silent while held
}}

| Light | Means |
|---|---|
| {{led:full|fully lit}} | the current item |
| {{led:half|half lit}} | can be picked |
| {{led:off|off}} | nothing there; while you hold, the pads are off too (silent) |

Release the left key and the map folds away.

<!-- keywords: map, overlay, hold, long press, 地图 -->

## Back and cancel {#nav-back}

{{shot:confirm|Anything destructive always asks first, with the consequence written in red.
mark 10,77,104,28 = OK confirms
mark 125,77,103,28 = ESC cancels
mark 8,49,172,14 = Consequence (in red)
key OK = Blinking: waiting for you
key ESC = Cancel
}}

- In a pop-up (lists, confirmations, text entry, the RECORD panel, SETUP) {{key:ESC}} closes or cancels.
- In a confirmation {{key:OK}} confirms and {{key:ESC}} cancels; overwriting, deleting and discarding unsaved work always ask first.
- In SEQ with steps selected {{key:ESC}} clears the selection first.

<!-- keywords: back, cancel, close, exit, undo selection, 返回, 取消 -->

## Scene and page map {#nav-scene-map}

Every scene and page at a glance; click a picture to jump to its explanation:

{{map:scenes}}

<!-- keywords: all pages, overview, screens, screenshots, 页面一览 -->
