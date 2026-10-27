# 3 TAPE 走带 {#tape}

TAPE 是整首歌：哪一段放哪些 Pattern、四轨怎么混、总线效果和工程文件。七页（{{key:CTRL}} 翻）：

{{page:SESSION}} {{page:SONG}} {{page:MIX}} {{page:SENDS}} {{page:FX}} {{page:PROJECT}} {{page:FILES}}

<!-- keywords: 走带, 整首歌, 编排, 混音, 工程, tape, song, arrangement, mixer, project -->

## SESSION：像 Ableton 那样发射 {#tape-session}

每列一条轨道（轨道色），每行一个 Pattern 槽；每格画出这个 Pattern 的 16 步。

{{shot:tape-session|SESSION：四轨并排。
mark cells:1,1-4,1 = T1
mark cells:1,2-4,2 = T2
mark cells:1,3-4,3 = T3
mark cells:1,4-4,4 = T4
mark cell:2,2 = T2 的第二个 Pattern
mark cell:2,1 = +：新建
key G1 G3 G4 = 当前：全亮
key G6 = 已有：半亮
}}

{{strip
PLAY, G6 > tape-session-queued @cell:2,2 | 播放中点一格：这一轨在这一圈结束时切换。格子写着 NXT，键灯 {{led:blink|闪烁}}。
- > tape-session @cell:2,2 | 一圈播完，T2 换成 P02。停止时点格子立即切换。
}}

- 点格子：让这一轨播这个 Pattern。最后一个 Pattern 之后的「+」新建一个。
- {{key:ADD}} / {{key:SUB}}：上下移动光标并滚动；{{key:OK}}：发射光标所在的整行（所有有这个 Pattern 的轨道）。
- {{key:FN+OK}}：把每轨正在播（或排队）的 Pattern 记成 SONG 的新一行。
- 从 SESSION 发射会退出 SONG 模式。正在播的格子是绿色底、▶ 和走动的播放头。

<!-- keywords: session, 片段, 发射, 场景发射, ableton, live, clip, launch, scene launch -->

## SONG：编排整首歌 {#tape-song}

整首歌像一段磁带：每**列**是一段（从左往右是时间），每**行**是一条轨道，格子里是这一段要播的 Pattern 和小节数（xN）。

{{shot:tape-song|SONG：每列是一段，每行是一轨。
mark cells:1,1-4,1 = 第 1 段
mark cells:1,2-4,2 = 第 2 段
mark cell:1,1 = 选中的格子
mark 34,15,26,9 = 小节数 xN
mark cell:2,3 = 点空列：追加一段
mark 196,108,32,12 = LOOP / SONG 模式
}}

- 点格子选中，再点一次同一格在「Pattern」和「小节数」之间切换。`--` 表示这一轨在这一段不出声。
- {{key:ADD}} / {{key:SUB}}：换这一格的 Pattern，或改这一段的小节数（1–16）。
- {{key:OK}}：从这一段开始播（自动进入 SONG 模式）；{{key:FN+OK}}：切换 SONG / LOOP。
- {{key:FN+C}} / {{key:FN+V}} / {{key:FN+X}} / {{key:FN+D}}：复制、粘贴、删除、复制一段。

SONG 模式下每段播放 xN 小节（1 小节 = 16 步）后进入下一段，结尾回到第一段。LOOP 模式即各轨循环自己的当前 Pattern。

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
key G5-G8 = 静音
}}

- GRID 每列一条轨道：行 1 选中、行 2 静音、行 3 独奏、行 4 音序开关。
- {{key:ADD}} / {{key:SUB}} 调选中轨的音量；{{key:OK}} 在选中轨和 MASTER 之间切换。

> [!tip] 按住 {{key:TAB}} 在任何画面都能用同一个布局（轨道地图）。

<!-- keywords: 混音, 推子, 音量, 静音, 独奏, 主音量, mix, mixer, fader, level, mute, solo, master volume -->

## SENDS：四轨效果总览 {#tape-sends}

{{shot:tape-sends|SENDS：每列一条轨道，每行一种效果。
mark cells:1,1-1,4 = FILTER
mark cells:2,1-2,4 = CHORUS
mark cells:3,1-3,4 = DELAY 发送
mark cells:4,1-4,4 = REVERB 发送
mark cell:1,3 = 焦点
key G1-G16 = 开着：半亮 · 关着：灭
}}

- 点格子选中（同时选中那一轨），{{key:ADD}} / {{key:SUB}} 调整。关着的格子变暗。
- {{key:OK}}：CHORUS 换模式，其它格子关闭 / 恢复原值；{{key:FN+OK}}：恢复默认。

<!-- keywords: 发送, 效果发送, 混响发送, 延迟发送, sends, send, aux, reverb send, delay send -->

## FX：总线效果 {#tape-fx}

属于工程的总线效果：

{{shot:tape-fx|总线效果 FX。
mark cells:1,1-1,4 = DELAY：时值 · 反馈 · TONE · 返回
mark cells:2,1-2,4 = REVERB：SIZE · DAMP · LOCUT · 返回
mark cells:3,1-3,4 = COMP · LOW · HIGH · TAPE
mark cells:4,1-4,4 = 回声序列 · 混响衰减
}}

{{key:OK}} 在 DELAY 上切换时值；{{key:FN+OK}} 恢复默认。详见 [[#fx-master]]。

<!-- keywords: 总线, 主效果, 压缩, 均衡, 饱和, master fx, bus, compressor, eq, tape saturation, master delay, master reverb -->

## PROJECT：工程 {#tape-project}

{{shot:tape-project|PROJECT：工程名、是否未保存、四轨摘要。
mark cell:1,1 = SAVE
mark cell:1,2 = SAVE AS
mark cell:1,3 = LOAD
mark cell:1,4 = NEW
mark cell:2,1 = BPM
mark cell:2,2 = SWING
mark cell:2,3 = TAP 敲速度
mark 100,85,48,10 = UNSAVED：有没保存的改动
}}

- 行 1：SAVE（保存）、SAVE AS（另存为）、LOAD（打开 FILES）、NEW（新建，有未保存的改动时先确认）。
- 行 2：BPM、SWING、TAP（敲击速度）。

<!-- keywords: 工程, 保存, 另存, 新建, 打开, project, save, save as, new, load, open -->

## FILES：工程列表 {#tape-files}

SD 卡上的所有工程。{{key:ADD}} / {{key:SUB}} 选择，{{key:OK}} 载入；有未保存的改动时先问。

{{shot:tape-files|FILES：卡上还没有工程时。
mark content = 工程列表
mark footer = FILES 页
}}

<!-- keywords: 文件, 工程列表, 载入工程, 打开工程, files, load project, open project, project list -->
