# 3 TAPE 走带 {#tape}

TAPE 是四轨合在一起：哪一段放哪些 Pattern、整首歌的编排、混音、发送和总线效果。五页（{{key:CTRL}} 翻）：

{{page:SESSION}} {{page:SONG}} {{page:MIX}} {{page:SENDS}} {{page:FX}}

保存、载入和工程的 BPM 属于全局，在 {{scene:SETUP}} 里：见 [[#setup-project]] 和 [[#setup-files]]。这五页上除了主音量，全部属于工程。每一页 FN 菜单的最下面一行都一样：SAVE（{{key:FN+S}}）、TAP（{{key:FN+T}}）、REC（{{key:FN+PLAY}}）和 SETUP。

<!-- keywords: 走带, 整首歌, 编排, 混音, 四轨, tape, song, arrangement, mixer -->

## SESSION：像 Ableton 那样发射 {#tape-session}

每列一条轨道（轨道色），每行一个 Pattern 槽；每格画出这个 Pattern 的 16 步。

{{shot:tape-session|SESSION：四轨并排。
mark cells:1,1-4,1 = T1
mark cells:1,2-4,2 = T2
mark cells:1,3-4,3 = T3
mark cells:1,4-4,4 = T4
mark cell:2,2 = T2 的第二个 Pattern
mark cell:2,1 = +：新建
key G1-G4 = 当前 Pattern：全亮
key G6 = 另一个 Pattern：半亮
}}

{{strip
PLAY, G6 > tape-session-queued @cell:2,2 | 播放中点一格：这一轨在这一圈结束时切换。格子写着 NXT，键灯 {{led:blink|闪烁}}。
- > - | 一圈播完，T2 换成 P02。停止时点格子立即切换。
}}

- 点格子：让这一轨播这个 Pattern（同时这一轨成为当前轨）。一轨最后一个 Pattern 之后的「+」新建一个空的并开始播。
- {{key:ADD}} / {{key:SUB}}：光标行上移 / 下移并滚动；{{key:OK}}：发射光标所在的整行（所有有这个 Pattern 的轨道）。
- {{key:FN+OK}}：把每轨正在播（或排队）的 Pattern 记成 SONG 末尾的新一段；这一段的长度等于其中最长的 Pattern。
- FN 菜单：**LAUNCH**（发射光标所在的行）和 **ADD** TO SONG（同 {{key:FN+OK}}）。
- 从 SESSION 发射会退出 SONG 模式。正在播的格子是绿色底、▶ 和走动的播放头。

<!-- keywords: session, 片段, 发射, 场景发射, ableton, live, clip, launch, scene launch -->

## SONG：编排整首歌 {#tape-song}

整首歌像一段磁带：每**列**是一段（从左往右是时间），每**行**是一条轨道，格子里是这一段要播的 Pattern 和小节数（xN）。

{{shot:tape-song|SONG：每列是一段，每行是一轨。
mark cells:1,1-4,1 = 第 1 段
mark cells:1,2-4,2 = 第 2 段
mark cell:1,1 = 选中的格子
mark 47,15,17,10 = 小节数 xN
mark cells:1,3-4,3 = + 列：追加一段
mark 199,111,31,12 = LOOP / SONG 模式
}}

- 点格子选中，再点一次同一格在「Pattern」和「小节数」之间切换（编辑小节数时这一段的 xN 变黄）。`--` 表示这一轨在这一段不出声。
- 点最后一段之后的 + 列：追加一段，内容复制最后一段（歌曲还是空的时候：每轨正在播的 Pattern）。
- {{key:ADD}} / {{key:SUB}}：换这一格的 Pattern（包括 `--`），或改这一段的小节数（1–16）；{{key:FN+ADD}} / {{key:FN+SUB}} 总是改小节数。
- {{key:OK}}：从这一段开始播（自动进入 SONG 模式；正在播时直接跳过去）；{{key:FN+OK}}：切换 SONG / LOOP。
- {{key:FN+C}} / {{key:FN+V}} / {{key:FN+X}} / {{key:FN+D}}：复制、粘贴（插在光标之后）、删除、复制一段。FN 菜单里也有（COPY、PASTE、DELETE、DUP，写着 ROW n），另有 PLAY 切换 SONG / LOOP。
- [[#setup-project]] 上的 PLAY 开关就是同一个 SONG / LOOP 设置。它是演奏控制，不存进工程；在 SESSION 里发射会切回 LOOP。

SONG 模式下每段播放 xN 小节（1 小节 = 16 步）后进入下一段，结尾回到第一段。LOOP 模式即各轨循环自己的当前 Pattern。正在播的一段有绿色的磁头扫过；等着开始的一段有黄色虚线框。

> [!tip] 在 SESSION 里搭好一个组合，{{key:FN+OK}} 就把它记成 SONG 的下一段：边玩边编排。

<!-- keywords: 编排, 歌曲模式, 段落顺序, 小节, 循环, song, arrangement, song mode, chain, order, bars -->

## MIX：混音 {#tape-mix}

{{shot:tape-mix|MIX：四个推子，MUTE / SOLO / SEQ 开关，底部是主音量。
mark cells:1,1-4,1 = T1 一列
mark cell:1,2 = 选中轨
mark cells:2,1-2,4 = MUTE 静音
mark cells:3,1-3,4 = SOLO 独奏
mark cells:4,1-4,4 = SEQ 音序开关
mark footer = MASTER 主音量 · 输出电平
key G2 = 选中：全亮
key G5-G12 = MUTE / SOLO 没开：半亮
key G13-G16 = SEQ 开着：全亮
}}

- GRID 每列一条轨道：行 1 选中、行 2 静音、行 3 独奏、行 4 音序开关。
- {{key:ADD}} / {{key:SUB}} 调选中轨的音量（0–100，默认 100；{{key:FN+ADD}} / {{key:FN+SUB}} 一次 10）；{{key:OK}} 在选中轨和 MASTER 之间切换。按住某一轨第 1 行的键：演奏键就是它的音量值。
- MASTER 是设备的输出音量，和 [[#setup-system]] 的 VOLUME 是同一个：属于设备，不属于工程。各轨音量、静音、独奏和 SEQ 开关存在工程里。
- FN 菜单：**UNMUTE** ALL 全部取消静音、**UNSOLO** ALL 全部取消独奏（有轨道被静音 / 独奏时才能用）。

> [!tip] 按住 {{key:TAB}} 在任何画面都能用同一个布局（轨道地图）。

<!-- keywords: 混音, 推子, 音量, 静音, 独奏, 主音量, mix, mixer, fader, level, mute, solo, master volume -->

## SENDS：四轨效果总览 {#tape-sends}

{{shot:tape-sends|SENDS：每列一条轨道，每行一种效果。
mark cells:1,1-1,4 = FILTER
mark cells:2,1-2,4 = CHORUS
mark cells:3,1-3,4 = DELAY 发送
mark cells:4,1-4,4 = REVERB 发送
mark cell:1,3 = 焦点
key G3 = 焦点：全亮
key G1 G2 G4-G16 = 改过默认值：半亮 · 默认（关）：灭
}}

- 点格子选中（同时选中那一轨），{{key:ADD}} / {{key:SUB}} 调整（{{key:FN+ADD}} / {{key:FN+SUB}} 一次 10）；按住格子，演奏键就是它的取值。关着的格子变暗。
- {{key:OK}}，或在 CHORUS 格子上再点一次：CHORUS 换模式，其它格子关闭 / 恢复原值；{{key:FN+OK}} 或 FN 菜单的 RESET：恢复默认。
- 这些就是这一轨 INSTR FX 页上的那几个 `fx.*` 值（[[#fx-sends]]）。

<!-- keywords: 发送, 效果发送, 混响发送, 延迟发送, sends, send, aux, reverb send, delay send -->

## FX：总线效果 {#tape-fx}

属于工程的总线效果：

{{shot:tape-fx|总线效果 FX。
mark cells:1,1-1,4 = DELAY：时值 · 反馈 · TONE · 返回
mark cells:2,1-2,4 = REVERB：SIZE · DAMP · LOCUT · 返回
mark cells:3,1-3,4 = COMP · LOW · HIGH · TAPE
mark cells:4,1-4,4 = 回声序列 · 混响衰减
key G1 = 焦点：全亮
key G2-G12 = 参数：半亮
}}

点格子选中，{{key:ADD}} / {{key:SUB}} 调整（范围大的参数 {{key:FN+ADD}} / {{key:FN+SUB}} 一次 10），按住格子演奏键就是它的取值。{{key:OK}} 在 DELAY 上切换时值（时间跟着 BPM）；{{key:FN+OK}}（或 FN 菜单的 RESET）恢复默认。详见 [[#fx-master]]。

<!-- keywords: 总线, 主效果, 压缩, 均衡, 饱和, master fx, bus, compressor, eq, tape saturation, master delay, master reverb -->
