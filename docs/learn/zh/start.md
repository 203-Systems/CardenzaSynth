# 入门 {#start}

Cardenza Groovebox 是一台口袋音乐工作站：4 条轨道（鼓、Juno 合成器、DX7 FM 合成器、采样器），每条轨道有自己的音序器 Pattern，再加上编排、混音、效果器和工程存档。

键盘分成四块，**颜色就是角色**：这本说明书里每个键都用它的颜色画出来，和机器屏幕上用的颜色一样。

{{keymap:四块键盘：左边导航，中间演奏，右边 GRID 对应屏幕格子，最右调值和播放
ESC TAB FN CTRL = 左侧四键：去哪儿
PADS = 演奏区：弹当前轨
GRID = GRID：按屏幕上的格子
RIGHT = 右侧四键：调值 · 播放
}}

| 颜色 | 键 | 管什么 |
|---|---|---|
| {{key:ESC}} 红橙 | 场景 | 1 INSTR / 2 SEQ / 3 TAPE，还有 4 SETUP |
| {{key:TAB}} 青 | 轨道 | {{track:1}} {{track:2}} {{track:3}} {{track:4}} |
| {{key:FN}} 黄 | 第二功能 | 当前画面的菜单、快捷键 |
| {{key:CTRL}} 紫 | 页面 | 屏幕最下面的页签，如 {{page:ROLL}} |
| {{key:PLAY}} 绿 | 走带 | 播放 / 停止 |

这本说明书怎么用：{{icon:map}} 右上角 **地图** 点按键或页面直接跳过去；{{icon:search}} 顶部搜索任何词（「音量」「volume」「声音太小」都能找到同一处）；{{icon:play}} 每张截图下面都能 **在这里跟着操作**，换成一台运行中的设备（见 [[#web-live]]）。

<!-- keywords: 介绍, 概览, 是什么, overview, introduction, 开始, 新手 -->

## 一分钟上手 {#quick-start}

跟着这几格做一遍，就会了一大半：

{{strip
- > instr-boot @8,0,41,11 @51,0,15,11 @8,40,200,26 | 开机停在 {{scene:INSTR}} 的 VOICE 页，当前轨是 {{track:2}} 和它的音色（LEAD I）：演奏区就是钢琴，下面一行白键、上面一行黑键，缺口（Shift、D、J）不亮；没按的键半亮。
ESC > seq-empty @scene | 单击 {{key:ESC}}：到 {{scene:SEQ}}，在这里写这一轨的音符。还是 {{track:2}}。
hold r3c1~C + G1 > seq-hold-tap @27,52,14,10 | 按住一个音，点一步：音符写进去了。按住的键全亮。
hold TAB + G1 > seq-drum-held @track | 按住 {{key:TAB}} 点 GRID 1：换到 {{track:1}} 鼓，每一列是一个鼓。
hold r3c1~底鼓 + G1, G5, G9, G13 > seq-drums @26,119,198,9 | 按住底鼓键，点 GRID 1、5、9、13：四拍底鼓写好了。
PLAY > seq-drums-playing | 播放！绿线走到哪一步，哪一步就响。再按一次停止。
hold OK > record | 刚才随便弹过的不会丢：按住 {{key:OK}}，把它放进 Pattern。
}}

再记住四件事：

{{strip
hold ESC > map-esc @cells:1,1-4,4 | 单击 {{key:ESC}} 在三个音乐场景里循环：{{scene:INSTR}} 音色 → {{scene:SEQ}} 音符 → {{scene:TAPE}} 编排混音。按住它是这张地图：第 4 列是 {{scene:SETUP}}，工程和设置都在那里。
hold FN > fn-held @cells:1,1-4,4 | 按住 {{key:FN}}：GRID 上是当前画面的菜单（复制、粘贴、保存、录音……），FN + GRID 键执行。
hold FN + 7 > fn-volume @14,94,212,28 | 按住 FN 时：{{key:6}} {{key:Y}} 调 BPM，{{key:7}} {{key:U}} 调音量，{{key:8}} {{key:I}} 调八度。
FN+S > save-name @footer | {{key:FN+S}} 保存工程；第一次保存时名字是 SONG 01。
}}

<!-- keywords: 快速开始, 上手, 第一次, 教程, quick start, tutorial, getting started, 入门教程 -->

## 屏幕上有什么 {#screen-layout}

屏幕 240×135 像素，分成五块，和键盘的位置一一对应：

{{shot:seq-notes|SEQ 音符页（ROLL），T2 上的几个音。下面是同一时刻的按键灯。
mark scene = 场景
mark track = 轨道（轨道色）
mark bpm = BPM · 播放状态
mark strip:ESC = ESC
mark strip:TAB = TAB
mark strip:FN = FN
mark strip:CTRL = CTRL
mark content = 内容区 = GRID
mark strip:PLAY = PLAY
mark footer = 页签：CTRL 换页
key PADS = 能发声：半亮
key G1 = 有音符：半亮
key G2 = 空步：灭
}}

| 区域 | 在哪 | 内容 |
|---|---|---|
| 状态行 | 最上面 | 场景、当前轨（轨道色）、音源和音色、BPM、播放 / 录音 |
| 左侧四个色块 | 左边缘 | 就是 {{key:ESC}} {{key:TAB}} {{key:FN}} {{key:CTRL}} 四个键的灯；小点 = 场景数、轨道、录音、页数 |
| 内容区 | 中间 | 4×4 格子 = 右边 GRID 4×4：按哪个键就是哪一格 |
| 页签 | 最下面 | 当前场景的页面，当前页紫底 {{page:ROLL}}；{{key:CTRL}} 换页 |
| 右侧四个色块 | 右边缘 | {{key:ADD}} {{key:SUB}} {{key:OK}} {{key:PLAY}} 的灯 |

> [!tip] 屏幕上第 2 行第 3 格，就是 GRID 第 2 行第 3 个键（键帽 `[`）。所有页面都是这条规则：**屏幕格子在哪，键就在哪。**

两侧色块和按键灯同步：灯 {{led:off|灭}} 时色块很淡，{{led:half|半亮}} 时是 30% 点阵，{{led:full|全亮}} 时变成实色。详见 [[#lights-edges]]。

<!-- keywords: 界面, 屏幕布局, 状态栏, 页签, 内容区, screen, display, layout, status line, tabs, header, footer -->
