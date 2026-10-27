# 演奏 {#play}

演奏区（PADS）永远在弹当前轨。键盘排列、音阶、八度、力度、和弦和琶音（ARP）四条轨道通用，在 FN 面板的 **1 KEYS**、**2 CHORD**、**3 ARP** 三页里设置（见 [[#fn-pages]]）。这些是应用设置：开机恢复，载入或新建工程都不会改变它们。

{{keymap:PADS：中间 4×8，永远弹当前轨
PADS = 演奏区
}}

<!-- keywords: 演奏, 弹奏, 键盘, 怎么弹, play, perform, keyboard -->

## 键盘排列 {#play-keyboard}

旋律轨有三种排列（KEYS 页的 LAYOUT），看键灯就知道哪些键能发声：

{{shot:seq-keyboard|PIANO 钢琴排列（默认）：两行一个八度，缺口不亮也不发声。
key r3c1-r3c8 = 白键 C D E F G A B C
key r2c2 r2c3 r2c5 r2c6 r2c7 = 黑键
key r2c1 r2c4 r2c8 = 缺口：灭
key r0c1-r1c8 = 高一个八度
}}

| 排列 | 怎么排 | 灯 |
|---|---|---|
| **PIANO 钢琴**（默认） | 下面一行白键 C D E F G A B C，上面一行黑键；缺口（C# 之前、E–F 之间、A# 之后）不发声。上面两行高一个八度 | 缺口 {{led:off}} |
| **SCALE 音阶** | 只放音阶里的音，从左到右按音阶走，每往上一行高一个八度。不会弹错音 | 全部 {{led:half}} |
| **CHROMA 半音** | 每个键一个半音，从左到右、从下到上 | 全部 {{led:half}} |

{{strip
hold FN + 1, G1 > keys-scale @cell:1,1 | FN 面板 1 KEYS：点 LAYOUT 切换。SCALE：每个键都在音阶里。
G1 > keys-chroma @cell:1,1 | 再点：CHROMA，每个键一个半音。
G1 > setup-keys @cell:1,1 | 再点：回到 PIANO。
}}

| 灯 | 演奏区上的意思 |
|---|---|
| {{led:half|半亮}} | 能发声 |
| {{led:full|全亮}} | 按住的键、和弦的其它组成音、ARP 正在弹的音、音序器在这一轨弹出的音 |
| {{led:off|灭}} | 发不出声 |

<!-- keywords: 键盘排列, 钢琴, 黑键, 白键, 音阶模式, 半音, layout, piano, black keys, white keys, chromatic, scale layout, isomorphic -->

## 音阶和根音 {#play-scale}

KEYS 页的 SCALE 和 ROOT 选音阶和根音（C–B）。音阶决定 SCALE 排列里放哪些音、和弦怎么叠、LOCK 管哪些键。

{{shot:setup-keys|FN 面板 1 KEYS 页。
mark cell:1,1 = LAYOUT 排列
mark cell:1,2 = SCALE 音阶
mark cell:1,3 = ROOT 根音
mark cell:1,4 = OCTAVE 八度
mark cell:2,1 = VELO 力度
mark cell:2,2 = LOCK 锁定
mark cells:3,1-4,4 = 当前音阶在键盘上
}}

音阶有 14 种：

{{ref:lists}}

<!-- keywords: 音阶, 调式, 根音, 调, 大调, 小调, 五声, 布鲁斯, scale, mode, key, root, major, minor, pentatonic, blues, dorian -->

## 八度 {#play-octave}

KEYS 页的 OCTAVE（−3 到 +3）把整个键盘上下移动。最快的方法：

{{strip
hold FN + 8 > fn-octave @14,102,212,26 | 按住 {{key:FN}}，{{key:8}} 升八度。屏幕底部弹出新值。
hold FN + I > fn-held @cell:1,4 | {{key:I}} 降八度。也可以在 KEYS 页直接调 OCTAVE。
}}

<!-- keywords: 八度, 移调, 升高, 降低, 高音, 低音, octave, transpose, shift, higher, lower -->

## 力度 {#play-velocity}

演奏区不感应力度，每个键用同一个力度发声：KEYS 页的 **VELO**（1–127，默认 110）。录进音序器的音也用这个力度，之后可以在音符页逐步改（VEL，见 [[#seq-edit]]）。

<!-- keywords: 力度, 轻重, 音量大小, velocity, dynamics, how hard -->

## 音阶锁定 LOCK {#play-lock}

PIANO 和 CHROMA 排列下，音阶外的键怎么办：

| LOCK | 音阶外的键 | 灯 |
|---|---|---|
| **OFF** | 照常发声 | {{led:half}} |
| **MUTE** | 不发声 | {{led:off}} |
| **SNAP** | 落到下方最近的音阶音上 | {{led:half}} |

<!-- keywords: 锁定, 音阶锁定, 防错音, 吸附, lock, scale lock, snap, mute, wrong notes -->

## 和弦 {#play-chords}

FN 面板的 **2 CHORD** 页让一个键弹出一个和弦：

{{strip
hold FN + 2 > setup-chord @cell:1,1 | {{key:FN+2}}：CHORD 页。CHORD 是 OFF。
G1 > chord-on @cell:1,1 @14,48,200,20 | 点 CHORD：TRIAD。图上写着根音上的键会弹哪几个音（C3 PLAYS C3 E3 G3）。
hold r3c1~C > chord-held @cells:3,1-4,2 | 按一个键：三个音一起响，组成音在屏幕和演奏区上都亮。
}}

- **CHORD**：OFF / TRIAD / 7TH / MAJ / MIN / SUS4 / MAJ7 / MIN7 / DOM7 / POWER / OCTAVE。TRIAD 和 7TH 在当前音阶里叠三度（音阶外的键借用大三和弦 / 属七），其余是固定和弦。
- **VOICING**：ROOT 原位 / INV1 第一转位（最低音升八度）/ INV2 第二转位 / SPREAD 展开（第二个音升八度）。
- 第二行写着这个调里每一级的三和弦（字体只有大写：小三和弦写成 `D-`，减三和弦写成 `BDIM`）。

{{shot:chord-held|按住 C：和弦 C E G 三个音全亮。
mark cell:1,1 = CHORD 和弦
mark cell:1,2 = VOICING 转位
mark 9,48,140,9 = 根音上的键弹出的音
mark 9,58,180,9 = 这个调的每一级
mark cells:3,1-4,4 = 组成音（蓝）
key r3c1 r3c3 r3c5 = 和弦音：全亮
}}

<!-- keywords: 和弦, 和声, 转位, 三和弦, 七和弦, 一键和弦, chord, chords, triad, seventh, inversion, voicing, harmony -->

## 琶音 ARP {#play-arp}

FN 面板的 **3 ARP** 页：按住几个键，它们按顺序轮流发声。

{{shot:arp-running|ARP UP：按住 C E G，三个音从下往上轮流弹。白色的柱是正在发声的那一步。
mark cell:1,1 = 模式
mark cell:1,2 = 速度
mark cell:1,3 = 范围
mark cell:1,4 = GATE
mark cell:2,1 = LATCH 松手继续
mark cells:3,1-4,4 = 16 步的演奏顺序
key r3c1 r3c3 r3c5 = 按住的键
}}

| 设置 | 选项 |
|---|---|
| ARP | OFF / UP / DOWN / UP-DN / RANDOM / ORDER（按按下的顺序） |
| RATE | 1/4、1/8、1/8T、1/16、1/16T、1/32 |
| RANGE | 1–4 个八度 |
| GATE | 10–100 %（每个音占音符时值的多少） |
| LATCH | 松手后继续；全部松开后再按会换成新的一组 |

- 按住的键（和弦展开后的每个音都算）按顺序轮流发声，最多 16 个音。第一下立刻发声。
- 停止时按 BPM 自由运行；播放时对齐节拍（1/16 跟随 Swing），按 PLAY / 停止不会断拍。
- UP / DOWN / UP-DN 从刚弹过的音继续走，到两端不重复。
- REC 打开且在播放时，ARP 弹出的音按 GATE 长度录进 Pattern。
- 换轨、全部停止、载入工程时 ARP 停下。鼓轨不走 ARP。

<!-- keywords: 琶音, 琶音器, 分解和弦, 自动弹, arp, arpeggiator, arpeggio, latch, gate, rate, up, down -->

## 鼓 {#play-drums}

鼓轨（默认 {{track:1}}）的演奏区每一**列**是一个鼓，同一列四行都是同一个鼓。鼓不走 ARP，也不受音阶和和弦影响。每个鼓的音量在 INSTR 的 KIT 页（见 [[#engine-drums]]）。

{{keymap:鼓轨：一列一个鼓
r0c1-r3c1 = 底鼓
r0c2-r3c2 = 军鼓
r0c3-r3c3 = 闭镲
r0c4-r3c4 = 开镲
r0c5-r3c5 = 拍手
r0c6-r3c6 = 通鼓
r0c7-r3c7 = 牛铃
r0c8-r3c8 = 沙锤
}}

<!-- keywords: 鼓, 打鼓, 底鼓, 军鼓, 镲, 节奏, drums, kick, snare, hi hat, clap, beat, rhythm -->
