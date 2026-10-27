# 4 SETUP {#setup}

SETUP is the fourth scene, and the only one outside the {{key:ESC}} ring: the **global** pages (the project and its files) and the **settings** (keyboard, chords, arpeggiator, system). Six pages ({{key:CTRL}} steps, hold CTRL for the page map):

{{page:PROJECT}} {{page:FILES}} {{page:KEYS}} {{page:CHORD}} {{page:ARP}} {{page:SYSTEM}}

| Pages | Kind | Stored |
|---|---|---|
| PROJECT · FILES | global | projects on the SD card; BPM, swing and LOOP / SONG belong to the project |
| KEYS · CHORD · ARP | settings | app settings: restored at power-on, kept when you load or create a project |
| SYSTEM | settings | device settings, shared by every app |

<!-- keywords: setup, settings, global, settings page, preferences, options, project, files, keys, chord, arp, system, 设置, 全局 -->

## Getting in and out {#setup-open}

{{strip
hold ESC > map-esc @cells:1,4-4,4 | Hold {{key:ESC}}: column 4 of the scene map is 4 SETUP, set apart by a dotted rule and a dim number.
hold ESC + G4 > setup-project @8,0,41,11 | Press any key of column 4: SETUP opens on the page you were last on.
ESC > seq-notes @scene | Tap {{key:ESC}}: back to the scene you came from (SEQ here).
}}

- A tap on {{key:ESC}} only walks the three music scenes 1 INSTR → 2 SEQ → 3 TAPE; it never enters SETUP (see [[#nav-scenes]]).
- The FN menu has a SETUP item on every screen (the bottom-right GRID key, see [[#fn-menus]]). Inside SETUP the same item reads BACK and the scene you came from.
- The pads keep playing the current track in SETUP, so you hear every change at once.

<!-- keywords: open setup, enter setup, leave setup, back, scene 4, column 4, 进入设置 -->

## PROJECT {#setup-project}

{{shot:setup-project|SETUP PROJECT: what you can do with the project, its tempo and play mode, its name and the four tracks.
mark cells:1,1-1,4 = SAVE · SAVE AS · LOAD · NEW
mark cell:2,1 = BPM (focused)
mark cell:2,2 = SWING
mark cell:2,3 = TAP: tap tempo
mark cell:2,4 = PLAY: LOOP / SONG
mark 8,69,150,18 = Project name · UNSAVED / SAVED
mark 8,89,224,25 = The four tracks: engine and sound
key G1-G4 = Actions: half lit
key G5 = Focus: fully lit
key G8 = PLAY: fully lit in SONG
}}

| GRID | Does |
|---|---|
| row 1 | **SAVE**: save now · **SAVE AS**: type a name and save · **LOAD**: go to FILES · **NEW**: a new project with the default four tracks (asks first when there are unsaved changes) |
| row 2 | **BPM** (30–300) and **SWING** (50–75 %): tap to focus, {{key:ADD}} / {{key:SUB}} change it · **TAP**: tap it in time, the interval sets the BPM · **PLAY**: switches LOOP ↔ SONG (fully lit in SONG, see [[#tape-song]]) |

- {{key:OK}} saves the project; with TAP focused it taps the tempo instead.
- {{key:FN+ADD}} / {{key:FN+SUB}} take big steps: BPM ±10, swing ±5.
- An untitled project is saved as the first free name SONG 01, SONG 02…; SAVE AS gives it your own name (see [[#fn-text]]).
- The FN menu here has NEW, LOAD and SAVE AS; {{key:FN+S}} saves from any screen (see [[#project-save]]).

<!-- keywords: project, save, save as, load, new, bpm, swing, tap tempo, loop, song mode, project page, untitled, unsaved, 工程, 保存 -->

## FILES {#setup-files}

All projects on the SD card, by name.

{{shot:setup-files|SETUP FILES: no projects on the card yet.
mark content = Project list
mark footer = FILES: page 2 of 6
}}

- {{key:ADD}} / {{key:SUB}} choose ({{key:FN+ADD}} / {{key:FN+SUB}} 8 at a time), {{key:OK}} loads.
- With unsaved changes it asks first: {{key:OK}} loads anyway, {{key:ESC}} keeps your work.
- LOAD on the PROJECT page and LOAD in the FN menu lead here too.

<!-- keywords: files, load project, open project, project list, load, open, 载入工程, 打开工程, 工程列表 -->

## KEYS {#setup-keys}

How the pads play, for all four tracks:

{{shot:setup-keys|SETUP KEYS: seven settings and a two-octave preview of the keyboard.
mark cell:1,1 = LAYOUT (focused)
mark cell:1,2 = SCALE
mark cell:1,3 = ROOT
mark cell:1,4 = OCTAVE
mark cell:2,1 = VELO
mark cell:2,2 = LOCK
mark cell:2,3 = SELECT
mark cells:3,1-4,4 = Preview: scale notes dotted, root underlined
key G1 = Focus: fully lit
key G2-G7 = Settings: half lit
}}

| Setting | Options | More |
|---|---|---|
| LAYOUT | PIANO / SCALE / CHROMA | [[#play-keyboard]] |
| SCALE · ROOT | 14 scales · C–B | [[#play-scale]] |
| OCTAVE | −3 … +3 | [[#play-octave]] |
| VELO | 1–127, the velocity of every pad | [[#play-velocity]] |
| LOCK | OFF / MUTE / SNAP | [[#play-lock]] |
| SELECT | ONE: a GRID tap in ROLL goes to one step · MULTI: it selects several | [[#seq-edit]] |

The four settings pages (KEYS, CHORD, ARP, SYSTEM) all work the same way:

| To | Do |
|---|---|
| pick a setting | tap its GRID key (the focused cell is inverse) |
| change it | {{key:ADD}} / {{key:SUB}}, held they repeat; {{key:FN+ADD}} / {{key:FN+SUB}} 5 at a time |
| step a choice or a switch | tap the focused key again, or {{key:OK}} |
| restore the default | {{key:FN+OK}}, or RESET in the FN menu |

Each page remembers which setting was focused.

<!-- keywords: keys, keyboard settings, layout, scale, root, octave, velocity, lock, select, multi select, settings page, 键盘设置 -->

## CHORD and ARP {#setup-chord-arp}

{{strip
hold CTRL + G4 > setup-chord @cells:1,1-1,2 | **CHORD**: CHORD and VOICING; below, the notes the root key plays and the triad on every degree of the key. See [[#play-chords]].
hold CTRL + G5 > setup-arp @cells:1,1-2,1 | **ARP**: ARP (mode), RATE, RANGE, GATE / LATCH; below, the playing order, dimmed while ARP is OFF. See [[#play-arp]].
}}

<!-- keywords: chord page, arp page, chord settings, arpeggiator settings, 和弦设置, 琶音设置 -->

## SYSTEM {#setup-system}

{{shot:setup-system|SETUP SYSTEM: three device settings and a short report of the device.
mark cell:1,1 = VOLUME (focused)
mark cell:1,2 = KEY LED
mark cell:1,3 = SCREEN
mark 8,47,190,52 = Device · audio backend · SD card · memory · build
key G1 = Focus: fully lit
key G2 G3 = Settings: half lit
}}

| Setting | Range | |
|---|---|---|
| VOLUME | 0–100 | master volume; also {{key:FN}} + {{key:7}} / {{key:U}} anywhere ([[#fn-quick]]) |
| KEY LED | 0–15 | overall brightness of the key lights; while it is focused the pads show the brightness ramp ([[#lights-ramp]]) |
| SCREEN | shown in % | screen brightness |

The report below is typed out line by line each time you come in: the device, the audio backend, the SD card (READY or missing, see [[#storage-card]]), internal RAM, its largest free block, PSRAM and the firmware's build date. RAM shows the current free space and the lowest free space since boot (MIN); PSRAM shows free space separately, so it does not hide a shortage of internal RAM.

<!-- keywords: system, volume, master volume, key led, screen brightness, brightness, sd card, device info, build, 屏幕亮度, 系统 -->

## Every setting {#setup-ref}

Every setting of the four settings pages with its range and default (generated from the firmware code):

{{ref:settings}}

> [!note] The old FN panel's PROJECT page is gone: BPM, SWING and PLAY are on SETUP PROJECT, the track's STEPS on SEQ TRACK ([[#seq-track]]), REC in the FN menu, {{key:FN+PLAY}} and the RECORD panel.

<!-- keywords: settings list, all settings, defaults, ranges, setting reference, 设置一览 -->
