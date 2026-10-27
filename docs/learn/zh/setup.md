# 4 SETUP 设置 {#setup}

SETUP 是第四个场景，也是唯一不在 {{key:ESC}} 循环里的场景：前两页是**全局**（工程和工程文件），后四页是**设置**（键盘、和弦、琶音、系统）。六页（{{key:CTRL}} 翻，按住 CTRL 看页面地图）：

{{page:PROJECT}} {{page:FILES}} {{page:KEYS}} {{page:CHORD}} {{page:ARP}} {{page:SYSTEM}}

| 页 | 类别 | 存在哪 |
|---|---|---|
| PROJECT · FILES | 全局 | 工程存在 SD 卡上；BPM、Swing、LOOP / SONG 属于工程 |
| KEYS · CHORD · ARP | 设置 | 应用设置：开机恢复，载入或新建工程都不变 |
| SYSTEM | 设置 | 设备设置，所有应用共用 |

<!-- keywords: 设置, 全局, 设置页, 偏好, 选项, 工程, 文件, 键盘, 和弦, 琶音, 系统, setup, settings, global, preferences, options -->

## 进出 SETUP {#setup-open}

{{strip
hold ESC > map-esc @cells:1,4-4,4 | 按住 {{key:ESC}}：场景地图的第 4 列就是 4 SETUP，左边一条虚线、编号是暗的。
hold ESC + G4 > setup-project @8,0,41,11 | 按第 4 列任意一个键：进入 SETUP，停在上次离开时的那一页。
ESC > seq-notes @scene | 单击 {{key:ESC}}：回到进来之前的场景（这里是 SEQ）。
}}

- 单击 {{key:ESC}} 只在三个音乐场景 1 INSTR → 2 SEQ → 3 TAPE 之间循环，永远不会进 SETUP（见 [[#nav-scenes]]）。
- 每个画面的 FN 菜单里都有 SETUP 一项（GRID 右下角那个键，见 [[#fn-menus]]）。在 SETUP 里这一项写着 BACK 和你来的那个场景。
- 在 SETUP 里演奏区照常弹当前轨，改了什么马上能听到。

<!-- keywords: 进入设置, 打开设置, 离开设置, 返回, 第四个场景, 第 4 列, open setup, enter setup, back -->

## PROJECT 工程 {#setup-project}

{{shot:setup-project|SETUP PROJECT：工程的操作、速度和播放模式，下面是工程名和四轨摘要。
mark cells:1,1-1,4 = SAVE · SAVE AS · LOAD · NEW
mark cell:2,1 = BPM（焦点）
mark cell:2,2 = SWING
mark cell:2,3 = TAP 敲速度
mark cell:2,4 = PLAY：LOOP / SONG
mark 8,69,150,18 = 工程名 · UNSAVED / SAVED
mark 8,89,224,25 = 四条轨道：音源和音色
key G1-G4 = 动作：半亮
key G5 = 焦点：全亮
key G8 = PLAY：SONG 时全亮
}}

| GRID | 作用 |
|---|---|
| 行 1 | **SAVE** 马上保存 · **SAVE AS** 输入名字另存 · **LOAD** 跳到 FILES · **NEW** 新建（默认四轨，有未保存的改动时先确认） |
| 行 2 | **BPM**（30–300）和 **SWING**（50–75 %）：点一下选中，{{key:ADD}} / {{key:SUB}} 调 · **TAP**：跟着节奏点，按间隔设 BPM · **PLAY**：LOOP ↔ SONG 开关（SONG 时全亮，见 [[#tape-song]]） |

- {{key:OK}} 保存工程；焦点在 TAP 上时 {{key:OK}} 是敲速度。
- {{key:FN+ADD}} / {{key:FN+SUB}} 大步调：BPM 一次 10，Swing 一次 5。
- 还没起名的工程保存时自动用第一个空着的名字 SONG 01、SONG 02……；想自己起名用 SAVE AS（见 [[#fn-text]]）。
- 这一页的 FN 菜单有 NEW、LOAD、SAVE AS；{{key:FN+S}} 在任何画面都能保存（见 [[#project-save]]）。

<!-- keywords: 工程, 保存, 另存, 载入, 新建, bpm, swing, 敲速度, 循环, 歌曲模式, 工程页, 未保存, project, save, save as, load, new, tap tempo, untitled, unsaved -->

## FILES 工程文件 {#setup-files}

SD 卡上的所有工程，按名字排列。

{{shot:setup-files|SETUP FILES：卡上还没有工程时。
mark content = 工程列表
mark footer = FILES：第 2 / 6 页
}}

- {{key:ADD}} / {{key:SUB}} 选择（{{key:FN+ADD}} / {{key:FN+SUB}} 一次 8 个），{{key:OK}} 载入。
- 有未保存的改动时先问：{{key:OK}} 照样载入，{{key:ESC}} 保留现在的。
- PROJECT 页的 LOAD 和 FN 菜单里的 LOAD 也会跳到这里。

<!-- keywords: 文件, 载入工程, 打开工程, 工程列表, 载入, 打开, 读档, files, load project, open project, project list, load -->

## KEYS 键盘 {#setup-keys}

演奏区怎么弹，四条轨道通用：

{{shot:setup-keys|SETUP KEYS：七项设置和两个八度的键盘预览。
mark cell:1,1 = LAYOUT 排列（焦点）
mark cell:1,2 = SCALE 音阶
mark cell:1,3 = ROOT 根音
mark cell:1,4 = OCTAVE 八度
mark cell:2,1 = VELO 力度
mark cell:2,2 = LOCK 锁定
mark cell:2,3 = SELECT 选择方式
mark cells:3,1-4,4 = 预览：音阶音带点，根音带横线
key G1 = 焦点：全亮
key G2-G7 = 设置：半亮
}}

| 设置 | 选项 | 详见 |
|---|---|---|
| LAYOUT | PIANO / SCALE / CHROMA | [[#play-keyboard]] |
| SCALE · ROOT | 14 种音阶 · C–B | [[#play-scale]] |
| OCTAVE | −3 … +3 | [[#play-octave]] |
| VELO | 1–127，演奏区每个键的力度 | [[#play-velocity]] |
| LOCK | OFF / MUTE / SNAP | [[#play-lock]] |
| SELECT | ONE：ROLL 里点 GRID 走到一步 · MULTI：可以多选 | [[#seq-edit]] |

四个设置页（KEYS、CHORD、ARP、SYSTEM）的操作都一样：

| 要做 | 怎么做 |
|---|---|
| 选设置 | 点它的 GRID 键（焦点格子反显） |
| 调整 | {{key:ADD}} / {{key:SUB}}，按住连发；{{key:FN+ADD}} / {{key:FN+SUB}} 一次 5 |
| 切换选项或开关 | 再点一次焦点那个键，或按 {{key:OK}} |
| 恢复默认 | {{key:FN+OK}}，或 FN 菜单里的 RESET |

每一页记住自己选中的是哪个设置。

<!-- keywords: 键盘设置, 排列, 音阶, 根音, 八度, 力度, 锁定, 多选, 选择方式, 设置页, keys, keyboard settings, layout, scale, root, octave, velocity, lock, select, multi select -->

## CHORD 和 ARP {#setup-chord-arp}

{{strip
hold CTRL + G4 > setup-chord @cells:1,1-1,2 | **CHORD 和弦**：CHORD、VOICING；下面写着根音上的键弹出哪几个音，以及这个调每一级的三和弦。详见 [[#play-chords]]。
hold CTRL + G5 > setup-arp @cells:1,1-2,1 | **ARP 琶音**：ARP（模式）、RATE、RANGE、GATE / LATCH；下面是演奏顺序，ARP 为 OFF 时整幅变暗。详见 [[#play-arp]]。
}}

<!-- keywords: 和弦页, 琶音页, 和弦设置, 琶音设置, chord page, arp page, chord settings, arpeggiator settings -->

## SYSTEM 系统 {#setup-system}

{{shot:setup-system|SETUP SYSTEM：三项设备设置和一份设备状态。
mark cell:1,1 = VOLUME 主音量（焦点）
mark cell:1,2 = KEY LED 按键灯
mark cell:1,3 = SCREEN 屏幕
mark 8,47,190,52 = 设备 · 音频后端 · SD 卡 · 内存 · 构建日期
key G1 = 焦点：全亮
key G2 G3 = 设置：半亮
}}

| 设置 | 范围 | |
|---|---|---|
| VOLUME | 0–100 | 主音量；任何画面也能用 {{key:FN}} + {{key:7}} / {{key:U}} 调（[[#fn-quick]]） |
| KEY LED | 0–15 | 所有按键灯的总亮度；选中它时演奏区显示亮度梯（[[#lights-ramp]]） |
| SCREEN | 以 % 显示 | 屏幕亮度 |

下面的状态每次进来逐行打出：设备、音频后端、SD 卡（READY 或没有卡，见 [[#storage-card]]）、内部 RAM、最大连续空闲块、PSRAM 和固件构建日期。RAM 显示当前剩余和开机以来的最低剩余（MIN）；PSRAM 单独显示，不会掩盖内部 RAM 不足。

<!-- keywords: 系统, 主音量, 音量, 按键灯亮度, 屏幕亮度, 亮度, sd 卡, 设备信息, 构建, system, volume, key led, screen brightness, brightness -->

## 全部设置 {#setup-ref}

四个设置页的每一项、范围和默认值（直接从固件代码生成）：

{{ref:settings}}

> [!note] 原来 FN 面板的 PROJECT 页没有了：BPM、SWING、PLAY 在 SETUP PROJECT，轨道的 STEPS 在 SEQ TRACK（[[#seq-track]]），REC 在 FN 菜单、{{key:FN+PLAY}} 和 RECORD 面板。

<!-- keywords: 设置一览, 全部设置, 默认值, 范围, 设置列表, settings list, all settings, defaults -->
