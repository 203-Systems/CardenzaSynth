# Navigation {#navigate}

Four scenes with a few pages each, and four tracks you can switch at any time. The four left keys do all the navigation:

{{keymap:The four left keys handle all the navigation
ESC = Scenes
TAB = Tracks
FN = The menu of this screen
CTRL = Pages
}}

<!-- keywords: navigation, switch, move around, where am i, 导航 -->

## Scenes: ESC {#nav-scenes}

Four scenes in two kinds, numbered as on the screen. 0 SETUP comes first and holds everything global; 1, 2 and 3 are the music itself:

| Scene | For | Pages |
|---|---|---|
| {{scene:SETUP}} setup | global: the project and its files, the keyboard for every track, the system | PROJECT, FILES, KEYS, SYSTEM |
| {{scene:INSTR}} instrument | what the current track sounds like: engine, sound, its chord and ARP, parameters, effects | VOICE, CHORD, ARP, engine pages, FX (drums: VOICE, KIT, FX) |
| {{scene:SEQ}} sequencer | what the current track plays: notes, recording, patterns | ROLL / STEP (DRUMS on a drum track), PATTERN, TRACK |
| {{scene:TAPE}} tape | all four tracks together: launching, the song, the mix, sends, master effects | SESSION, SONG, MIX, SENDS, FX |

**Tap {{key:ESC}}**: the three music scenes in turn, 1 → 2 → 3 → 1. The groovebox starts in {{scene:INSTR}} on the VOICE page, on track {{track:1}}.

{{strip
ESC > seq-notes @scene | {{scene:SEQ}}: write notes.
ESC > tape-session @8,0,41,11 | {{scene:TAPE}}: launch, arrange and mix.
ESC > instr-voice @8,0,41,11 | Back to {{scene:INSTR}}: sounds and parameters.
}}

**Hold {{key:ESC}}**: the scene map, four tall columns, one per scene; press any key of a column to jump straight there. The tap ring never enters SETUP: **column 1 is the way in**.

{{shot:map-esc|Holding ESC: four columns = four scenes, each listing its pages; the page you would land on is marked.
mark cells:1,1-4,1 = 0 SETUP: dotted rule, dim number
mark cells:1,2-4,2 = 1 INSTR
mark cells:1,3-4,3 = 2 SEQ (current)
mark cells:1,4-4,4 = 3 TAPE
mark 162,0,70,11 = Tap ESC = 1 > 2 > 3
key ESC = Hold
key G3 G7 G11 G15 = Current scene: fully lit
key G1 G5 G9 G13 = SETUP: half lit
}}

{{strip
hold ESC + G2 > instr-voice @8,0,41,11 | Tap column 2: straight to {{scene:INSTR}}. Release ESC and the map folds away.
hold ESC + G1 > setup-project @8,0,41,11 | Tap column 1: {{scene:SETUP}}, on the page you were last on.
ESC > seq-notes @scene | In SETUP a tap on {{key:ESC}} goes back to the scene you came from.
}}

> [!note] In a pop-up {{key:ESC}} cancels; in SEQ with steps selected, ESC clears the selection first. The FN menu also reaches SETUP (its bottom-right item, see [[#fn-menus]]).

<!-- keywords: scene, scenes, mode, instr, seq, tape, setup, instrument, sequencer, escape, four scenes, scene map, 场景 -->

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

- {{key:TAB}} + {{key:ADD}} / {{key:SUB}}: next / previous track (the map's title says so).
- Row 1 of the map shows each track's engine and sound; rows 2 to 4 switch at once, like on MIX.
- {{key:FN+TAB}}: previous track.

<!-- keywords: track, switch track, change track, channel, which track, 轨道 -->

## Pages: CTRL {#nav-pages}

The page tabs are at the bottom of the screen, the current one on violet (like {{page:ROLL}}); when there are too many you see `<` `>` at the ends and "current / total" on the right.

{{strip
CTRL > seq-pattern @footer | Tap CTRL: next page (on release).
hold CTRL > map-ctrl @cells:1,1-1,3 | Hold CTRL: the page map; each cell shows the tab and a short description.
hold CTRL + G3 > seq-track @footer | Tap a cell to jump to that page; you stay there after releasing.
}}

- {{key:CTRL}} + {{key:ADD}} / {{key:SUB}}: next / previous page; {{key:FN+CTRL}}: previous page.
- Each track remembers which INSTR page it was on.
- The page map's title: TAP CTRL = NEXT.

<!-- keywords: page, pages, next page, sub page, tabs, 页面, 翻页 -->

## Hold a left key = a map {#nav-maps}

Hold {{key:ESC}}, {{key:TAB}} or {{key:CTRL}} for more than 250 ms and the middle of the screen becomes a map: **each cell sits on the GRID key that picks it**. Holding {{key:FN}} shows the menu of the current screen instead, laid out the same way (see [[#fn-menu]]).

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
mark 10,70,104,26 = OK confirms
mark 125,70,104,26 = ESC cancels
mark 8,43,172,11 = Consequence (in red)
key OK = Blinking: waiting for you
key ESC = Cancel
}}

- In a pop-up (lists, confirmations, text entry, the RECORD panel, the open FN menu) {{key:ESC}} closes or cancels.
- In {{scene:SETUP}} {{key:ESC}} goes back to the scene you came from.
- In a confirmation {{key:OK}} confirms and {{key:ESC}} cancels; overwriting, deleting and discarding unsaved work always ask first.
- In SEQ with steps selected {{key:ESC}} clears the selection first.

<!-- keywords: back, cancel, close, exit, undo selection, 返回, 取消 -->

## Scene and page map {#nav-scene-map}

Every scene and page at a glance; click a picture to jump to its explanation:

{{map:scenes}}

<!-- keywords: all pages, overview, screens, screenshots, 页面一览 -->
