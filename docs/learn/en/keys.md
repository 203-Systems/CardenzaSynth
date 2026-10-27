# Keys {#keys}

The keyboard has 4 rows × 14 columns in four blocks, each with its own colour and role; every key has a single-colour light that tells you what it can do **right now**. There is also a HOME key on the side.

{{keymap:The left column navigates, the 4×8 in the middle plays, the 4×4 on the right is the screen's cells, the last column sets values and plays
ESC = ESC: scenes
TAB = TAB: tracks
FN = FN: the menu of this screen
CTRL = CTRL: pages
PADS = PADS: play
GRID = GRID 4×4
ADD SUB OK = Plus, minus, confirm
PLAY = Play
}}

<!-- keywords: keyboard, layout, keys, key map, 键盘, 按键 -->

## Key map {#keys-map}

{{icon:map}} Click any key in the picture to jump to its explanation.

{{map:device}}

<!-- keywords: map, diagram, picture, 地图, 示意图 -->

## Left keys: ESC TAB FN CTRL {#keys-left}

All four keys work the same way: **a tap does one thing, a hold shows a map**:

| Key | {{icon:tap}} Tap | {{icon:hold}} Hold |
|---|---|---|
| {{key:ESC}} | walks the scenes {{scene:INSTR}} → {{scene:SEQ}} → {{scene:TAPE}}; in {{scene:SETUP}} goes back; cancels in a pop-up | scene map: the GRID's four columns = the four scenes, column 4 = SETUP |
| {{key:TAB}} | next track {{track:1}} → {{track:2}} → {{track:3}} → {{track:4}} | track map: select, mute, solo, sequencer on |
| {{key:FN}} | opens / closes the menu of this screen | the FN menu and second functions, see [[#fn-menu]] |
| {{key:CTRL}} | next page | page map: every page of the current scene |

{{shot:map-esc|Holding ESC: the screen becomes the scene map, and the GRID's columns are the scenes.
mark cells:1,1-4,1 = 1 INSTR
mark cells:1,2-4,2 = 2 SEQ (current)
mark cells:1,3-4,3 = 3 TAPE
mark cells:1,4-4,4 = 4 SETUP: only from here
mark strip:ESC = ESC held
key ESC = Hold
key GRID = Scenes: current fully lit, others half lit
}}

The four strips on the screen's left edge are these four keys (top to bottom: ESC red-orange, TAB cyan, FN yellow, CTRL violet). The small marks inside the strips:

| Strip | Marks |
|---|---|
| {{key:ESC}} | 4 marks = 4 scenes, the white one is the current scene |
| {{key:TAB}} | 4 marks = 4 tracks (in track colours), the white one is the current track |
| {{key:FN}} | 1 mark, turns red while recording is armed |
| {{key:CTRL}} | one mark per page of the current scene, the white one is the current page |

<!-- keywords: left keys, function keys, modifiers, escape, tab, fn, ctrl, 左键, 功能键 -->

## Right keys: ADD SUB OK PLAY {#keys-right}

{{keymap:The right column: the top two set values, the bottom two act
ADD SUB = Plus / minus (hold to repeat)
OK = Confirm · record key
PLAY = Play / stop
}}

| Key | Cap | Does |
|---|---|---|
| {{key:ADD}} | del | increase / next in a list; held it repeats, faster and faster |
| {{key:SUB}} | `\` | decrease / previous in a list |
| {{key:OK}} | ↵ Enter | confirm, run, switch; on the SEQ note page it is the {{icon:rec}} **record key**: a tap switches ROLL / STEP, a hold opens the RECORD panel |
| {{key:PLAY}} | space | play / stop; {{key:FN+PLAY}} arms / disarms live recording |

> [!note] In text entry the top key (the del cap) erases.

<!-- keywords: add, sub, plus, minus, increase, decrease, enter, ok, confirm, play, stop, 加, 减, 确认, 播放 -->

## The PADS {#keys-pads}

The 4×8 keys in the middle always play the **current track**, whatever scene or page you are on (except while you hold ESC, TAB or CTRL to see a map).

{{strip
hold r3c1~Kick > seq-drum-held | Drum track {{track:1}}: each **column** is one drum: kick, snare, closed hat, open hat, clap, tom, cowbell, maracas.
hold r3c1~C > seq-hold-pad | Melodic tracks: the bottom row is white keys, the row above black keys, the two rows above those one octave up. See [[#play-keyboard]].
- > text-entry | In text entry the whole keyboard types the characters on its caps.
}}

<!-- keywords: pads, play keys, keyboard, play notes, drums, 演奏键, 打击垫 -->

## The GRID {#keys-grid}

The 16 keys of the 4×4 on the right = the 4×4 cells in the middle of the screen. **What is drawn on the screen is what the key does:**

{{strip
G1 > seq-step @cell:1,1 | Sequencer: 16 cells = 16 steps.
G1 > instr-juno-vcf @cell:1,1 | Instrument pages: 16 cells = 16 parameters; tap one to select it.
G6 > tape-mix @cells:1,2-4,2 | MIX: each column is one track's select, mute, solo and sequencer switches.
}}

While a left key is held the GRID becomes that key's "map", see [[#nav-maps]].

<!-- keywords: grid, 4x4, matrix, cells, direct keys, 网格 -->

## HOME side key {#keys-home}

{{icon:stop}} The HOME key on the side is **stop all**: it stops the sequencer and silences everything that is sounding (stuck notes, the ARP and recording included). All key lights flash once and settle back.

> [!note] HOME belongs to the running app; switching apps is the job of the device's own launcher.

<!-- keywords: stop all, panic, all notes off, stuck notes, silence, home, 全部停止 -->

## One rule for everything {#keys-rules}

| | Rule |
|---|---|
| {{hold:ESC}} {{hold:TAB}} {{hold:CTRL}} | **While a left key is held, choose on the GRID.** Each cell of a map sits on the key that picks it |
| {{hold:FN}} | While FN is held, **no key does its usual job**: the GRID is the menu, keys with an FN function do only that (lit), the others do nothing (off) |
| {{icon:tap}} | A modifier that was used in a combination does not also fire its tap when released |
| {{icon:hold}} | A hold is 250 ms; {{key:ADD}} / {{key:SUB}} start repeating after 0.4 s and move 5 at a time after 12 repeats |

<!-- keywords: rules, hold, long press, combination, chord, repeat, auto repeat, 规则, 长按 -->
