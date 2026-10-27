# FN menu and shortcuts {#fn}

{{key:FN}} is **the menu of the current screen**: hold it and the GRID becomes a menu of what this screen can do, with the second functions of the keys around it; tap it and the same menu stays open.

<!-- keywords: fn, function, second function, menu, shift -->

## The FN menu {#fn-menu}

{{shot:fn-held|Holding FN on SEQ ROLL: this page's menu on the GRID. Only keys with an FN function are lit.
mark header = FN MENU and whose menu it is: SEQ ROLL
mark cells:1,1-3,4 = Rows 1–3: this page's items
mark cells:4,1-4,4 = Row 4: the same on every screen
mark footer = Other FN combinations
key FN = Hold
key GRID = Menu items: FN + a GRID key runs it
key C X Q D S T = Letters of the items
key 6 7 8 Y U I = Quick controls
key PLAY = REC
}}

| | {{hold:FN}} Hold FN | {{icon:tap}} Tap FN: the menu stays open |
|---|---|---|
| run an item | {{key:FN}} + its GRID key, or {{key:FN}} + its letter | its GRID key; the menu closes |
| close | let go of FN | {{key:FN}} or {{key:ESC}} |
| pads | silent; only the letters and quick controls work | keep playing |
| {{key:ADD}} {{key:SUB}} {{key:OK}} | big steps and the second OK (see [[#fn-shortcuts]]) | do nothing |

- A hold is 250 ms; release sooner and FN counts as a tap.
- Rows 1–3 belong to the page you are on; row 4 is the same on every screen: **SAVE** the project, **TAP** tempo, **REC** arm, **SETUP** (in SETUP: **BACK**).
- The FN panel and its pages are gone: settings live in [[#setup]], and {{key:FN}} + {{key:1}}…{{key:5}} do nothing.

{{shot:fn-menu-open|A tap on FN: the same menu, kept open. The bottom line says whose menu it is and how to close it.
mark cell:1,1 = Name · what it acts on · FN letter
mark cell:1,2 = Not available: dotted
mark cell:2,2 = A switch: VIEW (ROLL / STEP)
mark 8,123,56,12 = Whose menu: SEQ ROLL
mark 141,123,90,12 = FN / ESC close
key GRID = Run an item
key PADS = Keep playing
}}

Each tile has:

- the **name** on top and, top right in yellow, its FN letter: {{key:FN+C}} runs COPY; `OK` means {{key:FN+OK}}, `PLAY` {{key:FN+PLAY}};
- a **second line**: what it acts on (PATTERN or STEPS, ROW 2, the engine) or its state (ROLL, 120 BPM, OFF);
- its **light**: {{led:half|half lit}} available, {{led:full|fully lit}} a switch that is on (shown inverse: VIEW STEP, REC ARMED, PLAY SONG), {{led:off|off}} and dotted when it cannot run now (PASTE with nothing copied, SCOPE without selected steps).

<!-- keywords: fn menu, menu, fn panel, 面板, context menu, menu of this screen, hold fn, tap fn, items, 菜单 -->

## The menu on each screen {#fn-menus}

| Screen | Rows 1–3 |
|---|---|
| INSTR (every page) | **PRESETS** browse · **SAVE** / **SAVE AS** the preset · **ENGINE** picker · **RESET** `OK`: the focused parameter back to default (parameter pages only); SAMPLER also **SAMPLES** (files), **RECORD** (a sample), **PREVIEW** |
| SEQ (every page) | **COPY** `C` · **PASTE** `V` · **CLEAR** `X` · **QUANT** `Q`: the selected steps, otherwise the whole pattern · **DUP** `D`: the pattern into the next free slot · **VIEW**: ROLL / STEP · **RECORD**: the RECORD panel and its takes · **SCOPE** `OK`: which chord note the step editor edits (with steps selected) |
| TAPE SESSION | **LAUNCH**: the cursor's row · **ADD** TO SONG `OK`: the patterns playing become a new SONG part |
| TAPE SONG | **COPY** `C` · **PASTE** `V` (after the cursor) · **DELETE** `X` · **DUP** `D`: the part under the cursor (ROW n) · **PLAY** `OK`: SONG / LOOP |
| TAPE MIX | **UNMUTE** ALL · **UNSOLO** ALL |
| TAPE SENDS · FX | **RESET** `OK` |
| SETUP PROJECT · FILES | **NEW** · **LOAD** · **SAVE AS** (the project) |
| SETUP KEYS · CHORD · ARP · SYSTEM | **RESET** `OK`: the focused setting back to default |
| **row 4, every screen** | **SAVE** `S`: the project (UNSAVED when there are changes) · **TAP** `T`: tap tempo, shows the BPM · **REC** `PLAY`: record arm, fully lit when armed · **SETUP**: to 4 SETUP; in SETUP **BACK** to the scene you came from |

{{shot:fn-menu-instr|Holding FN on INSTR VOICE (T2 Juno): the sound's items; RESET is dotted because VOICE has no parameter to reset.
mark cells:1,1-1,4 = PRESETS · SAVE · SAVE AS · ENGINE
mark cell:2,1 = RESET: parameter pages only
mark cells:4,1-4,4 = Row 4: SAVE · TAP · REC · SETUP
key G1-G4 = Available: half lit
key G5 = Not available: off
}}

<!-- keywords: menu items, menus, per screen, copy, paste, launch, unmute all, unsolo all, reset, new, load, save as, back, 菜单项 -->

## Quick controls: BPM, volume, octave {#fn-quick}

The three most used settings need no page. Hold {{key:FN}}: the 2×3 block at the right end of the pads, next to the GRID, is three plus/minus pairs, **upper row up, lower row down** (like ADD above SUB):

{{keymap:Hold FN: 6 7 8 up, Y U I down
FN = Hold
6 Y = BPM + / −
7 U = Volume + / −
8 I = Octave + / −
}}

{{strip
hold FN + 6 > fn-quick @14,94,212,28 | BPM +1 ({{key:Y}} is −1). Hold to keep changing.
hold FN + 7 > fn-volume @14,94,212,28 | Volume: too quiet, press {{key:7}} (louder); too loud, press {{key:U}}.
hold FN + 8 > fn-octave @14,94,212,28 | Octave: {{key:8}} up, {{key:I}} down.
}}

{{ref:fn-quick}}

They work on every screen while FN is held, repeat while held, and pop the new value up at the bottom of the screen, over the menu. While FN is held these six keys are {{led:half|half lit}}, {{led:full|fully lit}} while pressed.

<!-- keywords: volume, bpm, octave, quick, shortcut, tempo, faster, slower, volume, louder, quieter, octave, turn up, turn down, 快捷, 音量 -->

## Second functions (letters and the keys around) {#fn-shortcuts}

{{keymap:Keys with a second function while FN is held
FN = Hold
ESC = Stop all
TAB = Previous track
CTRL = Previous page
ADD SUB = Big steps
OK = Second OK
PLAY = Record arm
C V = Copy, paste
X = Clear
Q = Quantize
D = Duplicate
T = Tap tempo
S = Save
}}

| FN + | Does |
|---|---|
| GRID | run that menu item |
| {{key:ESC}} | stop all (like HOME) |
| {{key:TAB}} | previous track |
| {{key:CTRL}} | previous page ({{key:CTRL}} alone: next page) |
| {{key:ADD}} / {{key:SUB}} | big steps: ×10 on wide parameters (BPM ±10), ×5 on settings; a page at a time in lists |
| {{key:OK}} | the screen's second OK: INSTR, SENDS, master FX and the SETUP settings pages: the focused value back to its default; SEQ note page: which chord note to edit (all / one); SESSION: ADD TO SONG; SONG: SONG / LOOP; RECORD panel: merge a take into the current pattern; sample list: rename / delete |
| {{key:PLAY}} | record arm (REC) |
| {{key:C}} / {{key:V}} | copy / paste (selected steps or the current pattern; a part in SONG) |
| {{key:X}} | clear (selected steps, or the current pattern after confirming; deletes the part in SONG) |
| {{key:Q}} | quantize (selected steps or the current pattern) |
| {{key:D}} | duplicate the current pattern into the next free slot; the part in SONG |
| {{key:T}} | tap tempo: tap two or more times, the interval sets the BPM |
| {{key:S}} | save the project (an untitled one becomes SONG 01, SONG 02…) |
| {{key:1}}–{{key:5}} | nothing |

> [!note] A letter works where the menu has its item (the pad is half lit then); {{key:S}} and {{key:T}} work everywhere. Each shows its name and result at the bottom of the screen. In the menu kept open (FN not held) the pads play notes, not letters.

<!-- keywords: shortcut, shortcuts, hotkeys, copy, paste, clear, delete, quantize, duplicate, tap tempo, save, previous page, big step, 快捷键 -->

## FN in input screens: the yellow legends {#fn-yellow}

In text entry, the preset and sample lists and confirmations, holding FN shows no menu; it uses the **yellow legends** printed on the key caps instead:

{{keymap:Hold FN: the four keys at the lower right of the GRID are arrows
FN = Hold
; = Up
. = Down
, = Left
/ = Right
ADD SUB = Page up / down
}}

<!-- keywords: arrows, arrow keys, cursor keys, up down left right, yellow legend, list, 方向键 -->

## Typing text {#fn-text}

When naming something (a project, a sound, a sample) the whole keyboard types its caps:

{{shot:text-entry|Typing a name: SAVE PROJECT AS.
mark 0,13,240,10 = What you are naming
mark 8,40,222,30 = Name and cursor
mark 203,13,28,10 = Characters / 24
mark footer = OK accept · DEL erase · ESC cancel
key ADD = Erase (del)
key OK = Accept
key ESC = Cancel
key PLAY = Space
}}

- Letters, digits, space (the PLAY key) and `-`; up to 24 characters.
- The top-right key (the del cap) erases before the cursor; {{key:OK}} accepts; {{key:ESC}} cancels.
- {{key:FN}} + {{key:,}} / {{key:/}} move the cursor, {{key:FN}} + {{key:;}} / {{key:.}} jump to the start / end, {{key:FN}} + del deletes after the cursor.
- The cursor is a bar between characters, a block at the end.

<!-- keywords: type, typing, name, rename, text entry, cursor, backspace, delete character, 打字, 起名 -->
