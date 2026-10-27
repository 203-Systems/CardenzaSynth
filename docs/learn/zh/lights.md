# 按键灯 {#lights}

每个键有一颗单色灯，告诉你这个键现在能做什么。只有三种状态，加一种闪烁：

| | | |
|---|---|---|
| {{led:off}} | **灭** | 这里什么都没有 |
| {{led:half}} | **半亮** | 有东西，没选中 |
| {{led:full}} | **全亮** | 选中，或者正在发生 |
| {{led:blink}} | **闪烁** | 等待中 |

<!-- keywords: 灯, 灯光, 指示灯, 背光, led, lights, key lights, backlight -->

## 灭、半亮、全亮、闪烁 {#lights-states}

{{shot:seq-selected|SEQ 音符页选中第 5 步：一张图里四种状态都有。
mark 75,15,14,104 = 选中的步
key G2 G4 = 空步：灭
key G1 G3 = 有音符：半亮
key G5 = 选中：全亮
key r3c1 = 这一步的音：全亮
key PADS = 能发声：半亮
}}

| 状态 | 意思 | 例子 |
|---|---|---|
| {{led:off|灭}} | 这里什么都没有 | 空的步、发不出声的键（钢琴缺口、被锁定的音阶外音）、当前画面没有的功能、关着的发送 |
| {{led:half|半亮}} | 有东西，没选中 | 有音符的步、能发声的键、选项、参数、能执行的动作、已有的 Pattern |
| {{led:full|全亮}} | 选中，或者正在发生 | 选中的步、焦点参数、当前的页 / 场景 / 轨道 / Pattern、按住或正在发声的键、播放头、开着的开关（静音、独奏、音序开） |
| {{led:blink|闪烁}} | 等待中 | 排队的 Pattern、提示跳转的编排行、确认框的 OK、录音待命时的 PLAY、STEP 视图的写入光标 |

{{strip
- > seq-step @cell:1,4 | {{led:blink}} STEP 视图：写入光标在闪。
- > tape-session-queued @cell:2,2 | {{led:blink}} 排队的 Pattern：等这一圈播完。
- > seq-recording @strip:PLAY | {{led:blink}} 录音待命：PLAY 在闪。
}}

> [!note] 每个键有 16 档真实亮度（由设备的硬件层 CardenzaHAL 驱动），人眼对亮度不是线性的，所以固件先做伽马校正再点灯；KEY LED 总亮度另外再整体调暗。界面只用三档：「半亮」是键能发出的最暗的光（1/15 的点亮时间），「全亮」是 100 %。不用中间亮度，也不让动画把该灭的键点亮。

<!-- keywords: 灯的意思, 亮度含义, 半亮, 全亮, 闪烁, light meaning, half lit, fully lit, blinking, dim, bright -->

## 每个画面的灯 {#lights-screens}

{{strip
- > seq-notes | **SEQ 音符页**：空步灭，有音符半亮，选中全亮，播放头全亮走过。
- > seq-pattern | **PATTERN**：已有半亮，正在用的全亮，排队的闪烁。
- > instr-juno-vcf @cell:1,1 | **INSTR 参数页**：参数半亮，焦点参数全亮，空格灭。
- > tape-session | **SESSION**：每轨当前 Pattern 全亮，其它半亮，排队的闪烁。
- > tape-mix | **MIX**：选中轨全亮；静音 / 独奏 / 音序开着全亮、关着半亮。
- > map-esc | **按住 ESC / TAB / CTRL**：当前项全亮，其它半亮；演奏区全灭。
}}

| 画面 | GRID 上的灯 |
|---|---|
| SEQ 音符页 | 空步灭，有音符的半亮，选中的全亮，播放头全亮走过；STEP 视图里写入光标闪烁 |
| SEQ PATTERN | 已有的 Pattern 半亮，正在用的全亮，排队的闪烁 |
| INSTR 参数页 | 参数和动作半亮，焦点参数全亮，空格灭 |
| TAPE SESSION | 每轨当前的 Pattern 全亮，其它半亮，排队的闪烁 |
| TAPE SONG | 有 Pattern 的格子半亮，`--` 灭，选中的格子和正在播的那一行全亮 |
| TAPE MIX | 选中轨全亮；静音 / 独奏 / 音序开着全亮、关着半亮 |
| TAPE SENDS | 开着的发送半亮，关着的灭，焦点全亮 |
| 按住 ESC / TAB / CTRL | 地图上能选的键：当前项全亮，其它半亮；演奏区全灭 |
| 按住 FN | 有 FN 功能的键半亮（当前页、按下的快捷键全亮），其它全灭 |
| 文字输入 | 字符键半亮，有字时删除和 OK 半亮 |

- **演奏区**永远是当前轨的键盘：能发声的半亮，按住和正在发声的全亮，发不出声的灭（见 [[#play-keyboard]]）。
- **左侧四个键**：ESC、TAB、FN 半亮，CTRL 在有多页时半亮；按住的键全亮。
- **右侧**：ADD / SUB 有东西可调时半亮，OK 有动作时半亮（SEQ STEP 视图全亮），PLAY 停止半亮、播放全亮、录音待命闪烁。

<!-- keywords: 灯光规则, 哪些亮, 为什么亮, 为什么不亮, light rules, which keys light, why is it lit, why dark -->

## 屏幕两侧和按键灯同步 {#lights-edges}

屏幕左右两侧的 4 + 4 个色块**就是两侧 8 个键的灯**：每个色块读取对应键灯此刻的亮度。

{{shot:tab-hint|刚按完 TAB：TAB 键全亮，屏幕上 TAB 色块也变成实色。
mark strip:ESC = ESC 色块
mark strip:TAB = TAB：全亮 = 实色
mark strip:FN = FN 色块
mark strip:CTRL = CTRL 色块
mark strip:ADD = ADD
mark strip:PLAY = PLAY
key TAB = 全亮
key ESC FN CTRL = 灭
}}

| 键灯 | 屏幕色块 |
|---|---|
| {{led:off|灭}} | 很淡 |
| {{led:half|半亮}} | 30 % 点阵 |
| {{led:full|全亮}} | 实色（右侧的符号变黑） |

按住、切轨提示、地图、闪烁，屏幕和键上永远一致。

> [!tip] 没人操作时（4 秒内没按键），每 10 秒有一道光从上到下扫过两侧：左右同一行的键灯和屏幕色块一起亮起再落下，让你一眼看出屏幕两边的色块对应的是两侧的按键。

<!-- keywords: 侧边, 色块, 同步, 扫光, edge, strips, sync, sweep -->

## 亮度设置与灯光测试 {#lights-ramp}

FN 面板 5 SYSTEM 页的 **KEY LED**（0–15）是所有键灯的总亮度。

{{shot:setup-keyled|选中 KEY LED：演奏区从左到右是从暗到亮的亮度梯。
mark cell:1,2 = KEY LED 总亮度
mark cell:1,1 = 主音量
mark cell:1,3 = 屏幕亮度
key r0c1-r3c1 = 灭
key r0c2-r3c2 = 半亮：最暗的一档
key r0c3-r3c7 = 一档比一档亮
key r0c8-r3c8 = 全亮
}}

{{strip
FN, FN+5 > setup-system @footer | 单击 {{key:FN}} 打开 SETUP，{{key:FN+5}} 到 SYSTEM 页。
G2 > setup-keyled @cell:1,2 | 点 KEY LED：演奏区变成亮度梯。{{key:ADD}} / {{key:SUB}} 调总亮度。
}}

> [!warn] 如果亮度梯每一列看起来都一样亮，说明灯的调光不正常。

<!-- keywords: 灯亮度, 调暗, 调亮, 灯光测试, 测试, led brightness, dim lights, brighter, test, light test -->
