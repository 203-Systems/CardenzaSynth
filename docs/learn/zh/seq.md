# 2 SEQ 音序器 {#seq}

每条轨道最多 16 个 Pattern。每个 Pattern 有自己的长度，1–256 步（开始是 16 步），GRID 一次显示其中一页 16 步。每步最多 8 个音（和弦），每个音有自己的长度、力度和微小的时间偏移（每步 6 个微步）。SEQ 有三页：

| 页 | 做什么 |
|---|---|
| {{page:ROLL}} / {{page:STEP}} 音符页 | 写音符、改音符、录音。两种视图：ROLL 钢琴卷帘（默认）和 STEP 步进录制；鼓轨上是 {{page:DRUMS}} 鼓机 |
| {{page:PATTERN}} | 选择、新建、排队切换这一轨的 Pattern |
| {{page:TRACK}} | 这一轨的音序开关、当前 Pattern 的长度，全局 Swing 和 BPM |

<!-- keywords: 音序器, 编曲, 节奏, 循环, pattern, sequencer, sequence, steps, loop, beat -->

## 音符页的两种视图 {#seq-notes}

{{key:OK}} 是这一页的 {{icon:rec}} **录音键**：单击在两种视图之间切换，按住打开 RECORD 面板。有选中的步时，单击 {{key:OK}} 是换编辑项（见 [[#seq-edit]]）。

{{strip
- > seq-notes @footer | {{icon:roll}} **ROLL 钢琴卷帘**（默认）：时间 × 音高，音符画成条。
OK > seq-step-view @footer | {{icon:step}} 单击 {{key:OK}}：**STEP 步进录制**，4×4 步格写着音名。
OK > seq-notes @footer | 再单击：回到 ROLL。屏幕上的 OK 色块 ROLL 时 {{led:half}}、STEP 时 {{led:full}}。
}}

两种视图编辑同一个 Pattern，GRID 的 16 个键都对应屏幕上这一页的 16 步（第 1 行是第 1–4 步，第 2 行是 5–8 步……）。

| | {{icon:roll}} ROLL | {{icon:step}} STEP |
|---|---|---|
| 屏幕 | 时间 × 音高，音符画成条 | 4×4 步格，写着音名或和弦 |
| GRID 单击 | 走到这一步（再点一次放开）；SELECT MULTI 时可多选 | 把写入光标移到这一步 |
| 弹 PADS | 有选中的步时加 / 删音符；没有时就是弹奏（REC 打开时实时录音） | 写进光标所在的步，松开后光标前进 |
| {{key:ADD}} / {{key:SUB}} | 调整选中的步；没有选中时切换下一个 / 上一个 Pattern | 光标移动一步 |
| {{key:FN+ADD}} / {{key:FN+SUB}} | 选中的步大步调整；没有选中时长 Pattern 翻到下一页 / 上一页 | 光标移动一页（只有一页的 Pattern 是 4 步） |

<!-- keywords: 视图, 模式切换, 钢琴卷帘, 步进, 卷帘窗, view, piano roll, step view, mode -->

## ROLL 钢琴卷帘 {#seq-roll}

{{shot:seq-notes|ROLL：T2 上的几个音。底部一排小格就是 GRID 的 16 步。
mark 28,78,15,10 = 一个音符：长度 = 时值
mark 8,77,15,11 = 每个 C 有标记
mark 28,110,195,11 = 16 步 = GRID
mark footer = ROLL 视图
key G1 = 有音符：半亮
key G2 = 空步：灭
}}

横向是这一页的 16 步，纵向是音高（自动显示用到的音域，至少一个八度，黑键行带底纹）。音符越实力度越大；绿色竖线是播放头；底部小格：有音符的步带点阵，选中的步白色，播放头所在的步绿色。空的 Pattern 上写着 HOLD A PAD + TAP STEPS。

**写音符：按住一个或几个 PADS，再点 GRID 上的步。**

{{strip
hold r2c1~C > seq-hold-pad | 按住一个音（C）。
hold r2c1~C + G1 > seq-hold-tap @28,57,15,10 | 不松手，点 GRID 1：第 1 步写进 C。
hold r2c1~C + G5, G9 > seq-notes @28,110,195,11 | 继续点 5、9……换个音再点几步。
}}

- **走到一步**：单击 GRID 走到那一步，那一步的音在演奏区全亮，一眼就知道这一步有没有音；再点同一步放开。见 [[#seq-edit]]。
- **实时录音**：打开 REC（{{key:FN+PLAY}} 或 RECORD 面板），播放，然后弹，见 [[#seq-transport]]。
- 这一轨的音序关掉时（TRACK 页），这一页上写着 SEQUENCE OFF。

<!-- keywords: 钢琴卷帘, 卷帘窗, 音高, 时值, 音符长度, piano roll, roll, pitch, note length, duration -->

## STEP 步进录制 {#seq-step-rec}

一步一步把音写进去，不用跟着节拍：

{{strip
OK > seq-step-view @cell:1,1 @footer | 单击 {{key:OK}} 进入 STEP 视图。红色蚂蚁线框是**写入光标**。
G2, r2c2~D > seq-step @cell:1,2 | 点 GRID 2 把光标放到那里，再按一个 PAD：D 写进第 2 步。同时按几个就是和弦。
r2c3~E > seq-step @cell:1,3 @cell:1,4 | 松开后光标自动前进：E 写进第 3 步，光标停在第 4 步（角上的「+1」）。
ADD > - | {{key:ADD}} 跳过一步（休止），{{key:SUB}} 往回。
hold r2c1~C + G16 > - | 按着 PADS 点 GRID：直接写到那一步。
}}

{{shot:seq-step|STEP 视图：每格一步，写着音名；红框是光标。
mark cell:1,4 = 写入光标：闪烁
mark 212,30,17,9 = 每次前进几步
mark cell:1,1 = 写好的步
mark footer = STEP 视图
key G4 = 光标：闪烁
key G1 G2 G3 = 有音符：半亮
}}

- 按下去会替换光标那一步原来的音。
- 每次前进几步在 RECORD 面板的 STEP 里设（1 / 2 / 4）。
- 写进去的音用 SETUP KEYS 的力度（VELO），长度一步。之后可以切回 ROLL 细调。
- 长 Pattern 里光标走到哪一页，GRID 就跟到哪一页；{{key:FN+ADD}} / {{key:FN+SUB}} 一次移动一整页。
- 播放时，播放头走进的那一步绿色闪一下，然后保持绿框直到下一步；走过的步不留拖尾。
- FN 菜单里的 VIEW 也能切换视图，SEQ 的每一页都行。

<!-- keywords: 步进录制, 一步一步, 逐步输入, 输入音符, 休止, step record, step input, step entry, rest, cursor -->

## 选中与编辑 {#seq-edit}

在 ROLL 视图里单击 GRID **走到一步**：只选中这一步，换一步就跟着走过去，再点同一步放开（演奏区又回到弹奏）。走到的那一步，它的音在演奏区全亮；没有音的步，演奏区只有很暗的键盘。想一次选几步，在 {{scene:SETUP}} 的 KEYS 页把 **SELECT** 改成 MULTI（见 [[#setup-keys]]）。

{{shot:seq-selected|走到第 5 步：这一列高亮，这一步的音在演奏区全亮。
mark 76,15,14,96 = 走到的这一步
key G5 = 当前步：全亮
key r2c1 = 这一步的音：全亮
key PADS = 别的键：半亮
}}

播放时（没有选中步），播放头走到的那一步，它的音也在演奏区亮起来。走到一步之后：

{{strip
G5 > seq-selected @76,15,14,96 | 单击 GRID 5：第 5 步选中，这一列高亮。
OK, OK > seq-lane @footer | {{key:OK}} 循环编辑项 **NOTE → LEN → VEL → TIME**，底行是正在改的项和数值。
ADD > seq-lane @footer | {{key:ADD}} / {{key:SUB}} 调整，按住连发。{{key:ESC}} 取消选择。
}}

| 键 | 作用 |
|---|---|
| PADS | 在选中的步上加上或去掉这个音（和弦里的每个音都能单独加减）；播放中只编辑，不发声 |
| {{key:OK}} | 编辑项 NOTE（音高）→ LEN（长度）→ VEL（力度）→ TIME（微小的时间偏移） |
| {{key:ADD}} / {{key:SUB}} | 调整，按住连发；加 {{key:FN}} 是大步 |
| {{key:FN+OK}} | 「和弦里所有的音」（ALL）和「其中一个音」（底行显示音名）之间切换编辑对象 |
| {{key:ESC}} | 取消选择（再按 ESC 才换场景） |

选中时，演奏区上这一步的音 {{led:full|全亮}}。

<!-- keywords: 编辑音符, 改音高, 改长度, 改力度, 时间偏移, 微调, edit notes, note length, velocity, timing, nudge, microtiming, select steps -->

## DRUMS 鼓机页 {#seq-drums}

鼓轨（默认 {{track:1}}）的音符页是 {{page:DRUMS}} 鼓机，没有 ROLL / STEP。

{{shot:seq-drums|T1 的 DRUMS：左边是每种鼓的节奏，右边是选中的鼓的 16 步。
mark 9,15,95,108 = 八种鼓 × 16 步
mark 106,15,125,108 = 选中的鼓在 GRID 上
mark footer = DRUMS 页
key G1 G3 G5 = 选中的鼓（CH）的击打：全亮
key G2 = 这一步什么都没有：灭
}}

- **PADS** 选鼓和力度：每一列是一种鼓，每一行是力度（最上面一行最重）。按下会发声，之后 GRID 就用这个鼓、这个力度来写。
- **GRID**：点空的步立刻加一击（停止时听得到）；点已有的一击，松手时去掉。
- **按住一击**：屏幕显示它的值，{{key:ADD}} / {{key:SUB}} 改力度；{{key:OK}} 切换成改它在这一步里的时间（6 个微步），再按切回。改过或按得久的一击会留下。

{{strip
hold G1 > seq-drums @110,26,28,24 | 按住 CH 第 1 步的那一击（不要松手）。
hold G1 + ADD > seq-drum-edit @12,96,213,25 | 再按 {{key:ADD}}：力度变大，屏幕显示 CH VELOCITY。松手后这一击留下。
}}

- GRID 灯：选中的鼓的击打全亮，只有别的鼓的步半亮，空步灭。
- 鼓轨没有 ROLL / STEP 视图；实时录音和 RECORD 面板和别的轨一样用。

<!-- keywords: 鼓机, 打鼓, 节奏, 底鼓, 军鼓, 踩镲, drums, drum machine, beat, kick, snare, hi hat, drum sequencer -->

## 播放与实时录音 {#seq-transport}

{{strip
PLAY > seq-playing @strip:PLAY @77,15,4,96 | {{key:PLAY}}：播放，绿线是播放头。再按停止，播放头倒带回第 1 步。
FN+PLAY > seq-recording @strip:PLAY @footer | {{key:FN+PLAY}}：录音待命（REC）。PLAY 键灯闪烁，状态行里 REC 闪烁，播放时四周有红框。
r2c1~C > seq-recording @0,13,240,110 | 播放中弹 PADS：音符按弹的位置写进 Pattern（叠录）。
}}

- REC 也可以在 RECORD 面板里点开，或用 FN 菜单的 REC。录音待命时屏幕上的 PLAY 色块是红色。
- 录音是叠录，不会删掉已有的音，对齐到微步。ARP 打开时录的是 ARP 弹出的音。
- 实时录音在 ROLL 视图和 DRUMS 页上、没有选中步时进行；STEP 视图里 PADS 是写进光标那一步。
- 弹错了可以选中那一步用 {{key:FN+X}} 清除，或用 {{key:FN+Q}} 量化。

{{shot:seq-recording|播放中录音：四周红框，状态行里 REC 闪烁。
mark 167,0,21,11 = REC：闪烁
mark 77,15,4,96 = 播放头
mark strip:PLAY = PLAY：闪烁
mark footer = REC ARMED
key PLAY = 闪烁：录音中
}}

<!-- keywords: 播放, 停止, 录音, 实时录音, 录制, 叠录, play, stop, record, recording, live record, overdub, transport -->

## RECORD 面板 {#record-panel}

在音符页 **按住 {{key:OK}}**（约半秒）打开 RECORD 面板，也可以在 FN 菜单里选 RECORD；{{key:ESC}} 关闭。

{{shot:record|RECORD 面板：上面设置，中间 take，下面是要放进去的那一小节。
mark cells:1,1-1,4 = 设置：REC · SNAP · STEP · VIEW
mark cells:2,1-3,4 = 最近 8 段 take，最新在前
mark cell:2,2 = 选中的 take
mark cells:4,1-4,4 = 这一小节放进去的样子
mark footer = ±小节 · OK 新 Pattern · FN+OK 合并
key G1-G4 = 设置
key G5-G12 = take
}}

| GRID | 内容 |
|---|---|
| 第 1 行 | **REC** 实时录音开关 · **SNAP** take 放进 Pattern 时对齐整步（STEPS）还是保留原来的时间（FREE）· **STEP** 步进录制每次前进 1 / 2 / 4 步 · **VIEW** ROLL / STEP |
| 第 2–3 行 | 最近 8 段 take（无感录制），最新的在最前 |
| 第 4 行 | 选中的 take 那一小节放进 Pattern 后的样子 |

- 设置格：点一下切换，{{key:ADD}} / {{key:SUB}} 也可以调。
- take 格：点一下选中；{{key:ADD}} / {{key:SUB}} 选第几小节；{{key:OK}} 放进新 Pattern；{{key:FN+OK}} 合并进当前 Pattern。
- GRID 灯：设置半亮，REC、SNAP 打开时全亮，焦点格全亮，take 半亮。

<!-- keywords: 录音设置, 录制设置, 录音面板, 量化, 对齐, record settings, recording settings, record panel, snap, quantize -->

## 无感录制：把刚才弹的找回来 {#capture}

{{icon:capture}} 不用开 REC：你在演奏区弹的每一个音都会在后台记下来（Capture）。弹得好的那一段，事后放进音序器：

{{strip
r2c1~C, r2c3~E, r2c5~G > seq-keyboard | 随便弹。停顿超过 2.5 秒、换轨或超过 60 秒，就开始新的一段（take）。
hold OK > record @cells:2,1-3,4 | 按住 {{key:OK}}：中间两行是最近 8 段 take，编号、轨道、音符数。
G6 > record @cell:2,2 @cells:4,1-4,4 | 点一个 take：第 4 行是它这一小节放进去的样子。{{key:ADD}} / {{key:SUB}} 换小节。
OK > record-placed @14,94,212,28 | {{key:OK}}：放进这一轨的**新 Pattern**，原有的一个音都不动。
}}

- 8 段 take 一共能记 400 个音；记满时最旧的一段让出位置。
- 没有空 Pattern 时会提示，用 {{key:FN+OK}} 合并进当前 Pattern。
- 播放中弹的 take 按小节线对齐；停止时弹的 take 从第一个音开始，按当时的 BPM 计时。
- SNAP 打开时（默认）每个音对齐到最近的整步，长度也取整步；关掉则保留弹的细微时间。

<!-- keywords: 无感录制, 回溯录音, 找回, 刚才弹的, 随便弹, 捕捉, capture, retrospective record, take, takes, what i just played, recall, no record button -->

## 长 Pattern 与翻页 {#seq-long}

一个 Pattern 最长 256 步；GRID 和屏幕永远显示其中一页 16 步。页码在底行右端（例如 `2/4`，自己翻过页时是黄色）。

{{shot:fn-held|SEQ 菜单（按住 FN）：第 3 行翻页、设长度。
mark cells:3,1-3,2 = PAGE − / PAGE +：第几页 / 共几页
mark cells:3,3-3,4 = LEN − / LEN +：长度
}}

- **设长度**：TRACK 页的 STEPS（{{key:ADD}} / {{key:SUB}} 一步，{{key:FN+ADD}} / {{key:FN+SUB}} 跳到下一个整页），或 FN 菜单的 LEN − / LEN +：1、2、4、8、16、32、48 … 256。
- **翻页**：音符页上没有选中步时 {{key:FN+ADD}} / {{key:FN+SUB}}，或 FN 菜单的 PAGE − / PAGE +。
- 播放时 GRID 跟着播放头一页一页翻；自己翻过页后就停在那一页，直到下一次 PLAY。有选中的步时不翻页。
- 新建的 Pattern 沿用当前 Pattern 的长度；复制的 Pattern 保留自己的长度。超出长度的步在 GRID 上是灭的。
- 只有写了音符的页才占内存。内存不够时这个音不写入，屏幕提示 MEMORY FULL。

{{shot:seq-long|48 步的 Pattern，翻到第 2 页：GRID 现在是第 17–32 步。
mark footer = 页码 2/3
mark 12,96,213,25 = 翻页的提示
}}

<!-- keywords: 长 Pattern, 翻页, 步数, 长度, 32 步, 64 步, 小节, 超过 16 步, long pattern, pattern length, page, step page, bars -->

## PATTERN 页 {#seq-patterns}

{{shot:seq-pattern|PATTERN 页：这一轨的 Pattern 槽。
mark cell:1,1 = 已有的 Pattern：半亮
mark cell:1,2 = 正在用的：全亮
mark cell:1,3 = NEW：新建
mark footer = PATTERN 页
key G1 = 已有
key G2 = 当前
key G3 = NEW：灭
}}

- 单击一个槽：切到这个 Pattern。停止时立即切换；播放中会**排队**，等这一轨这一圈播完再切（格子显示 NEXT 和倒计时条，键灯 {{led:blink|闪烁}}）。
- 单击「NEW」槽：新建一个空 Pattern（每轨最多 16 个）。
- 正在用的 Pattern 上写着有几步写了音符。
- {{key:ADD}} / {{key:SUB}}：前后切换（播放中排队）；{{key:OK}} 回到音符页。
- {{key:FN+D}}：把当前 Pattern 复制到下一个空槽。{{key:FN+C}} / {{key:FN+V}} 复制粘贴整个 Pattern，{{key:FN+X}} 清空（要确认）。

<!-- keywords: pattern, 段落, 切换 pattern, 新建 pattern, 复制 pattern, 排队, new pattern, switch pattern, duplicate pattern, queue -->

## TRACK 页 {#seq-track}

{{shot:seq-track|TRACK 页：上面四个设置，下面是四轨总览。
mark cell:1,1 = SEQ 开关
mark cell:1,2 = STEPS 步数
mark cell:1,3 = SWING
mark cell:1,4 = BPM
mark 9,40,222,80 = 四轨：音源 · Pattern · 步数标尺
}}

| 设置 | 范围 | 说明 |
|---|---|---|
| SEQ | 开 / 关 | 这一轨的音序是否播放（单击或 {{key:OK}} 切换） |
| STEPS | 1–256 | 当前 Pattern 的长度；每个 Pattern 各有各的（例如 12 步对 16 步的复节奏） |
| SWING | 50–75 % | 全局摇摆 |
| BPM | 30–300 | 全局速度；也可以 {{key:FN+T}} 敲出来，或按住 FN 用 {{key:6}} / {{key:Y}} |

单击一格选中它，{{key:ADD}} / {{key:SUB}} 调整（加 {{key:FN}} 是大步）。总览里长 Pattern 压缩成 16 格。

<!-- keywords: 步数, 循环长度, 摇摆, 速度, 节拍, 复节奏, steps, length, loop length, swing, shuffle, tempo, bpm, polyrhythm -->

## 复制、粘贴、清除、量化 {#seq-tools}

按住 {{key:FN}} 用字母键，或在 FN 菜单里选（COPY、PASTE、CLEAR、QUANT、DUP，第二行写着 STEPS 还是 PATTERN）。音符页里作用于选中的步；没有选中时，以及在 PATTERN、TRACK 页，作用于整个 Pattern：

{{keymap:按住 FN 的编辑键
FN = 按住
C = 复制
V = 粘贴
X = 清除
Q = 量化
D = 复制 Pattern
T = 敲速度
}}

| 键 | 作用 |
|---|---|
| {{key:FN+C}} / {{key:FN+V}} | 复制 / 粘贴（复制过才能粘贴） |
| {{key:FN+X}} | 清除（整个 Pattern 时会先确认） |
| {{key:FN+Q}} | 量化到整步 |
| {{key:FN+D}} | 复制 Pattern 到下一个空槽 |
| {{key:FN+T}} | 敲击设定速度 |

GRID 上的 SEQ 菜单，三页都一样：

| 行 | 项 |
|---|---|
| 1 | COPY · PASTE · CLEAR · QUANT |
| 2 | DUP · VIEW（ROLL / STEP）· RECORD（RECORD 面板）· SCOPE（{{key:FN+OK}}，有选中的步时） |
| 3 | PAGE − · PAGE +（音符页，长 Pattern）· LEN − · LEN + |
| 4 | SAVE · TAP · REC · SETUP，每个画面都一样 |

菜单的更多说明见 [[#fn-menus]]。

> [!warn] 没有撤销，所以清除整个 Pattern 前一定会确认。

<!-- keywords: 复制, 粘贴, 清除, 删除音符, 量化, 对齐, 撤销, copy, paste, clear, delete notes, quantize, undo -->
