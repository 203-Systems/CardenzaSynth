# 2 SEQ 音序器 {#seq}

每条轨道最多 16 个 Pattern，每个 Pattern 最多 16 步，每步最多 8 个音（和弦），每个音有自己的长度、力度和微小的时间偏移（每步 6 个微步）。SEQ 有三页：

| 页 | 做什么 |
|---|---|
| {{page:ROLL}} / {{page:STEP}} 音符页 | 写音符、改音符、录音。两种视图：ROLL 钢琴卷帘（默认）和 STEP 步进录制 |
| {{page:PATTERN}} | 选择、新建、排队切换这一轨的 Pattern |
| {{page:TRACK}} | 这一轨的音序开关和步数，全局 Swing 和 BPM |

<!-- keywords: 音序器, 编曲, 节奏, 循环, pattern, sequencer, sequence, steps, loop, beat -->

## 音符页的两种视图 {#seq-notes}

{{key:OK}} 是这一页的 {{icon:rec}} **录音键**：单击在两种视图之间切换，按住打开 RECORD 面板。

{{strip
- > seq-notes @footer | {{icon:roll}} **ROLL 钢琴卷帘**（默认）：时间 × 音高，音符画成条。
OK > seq-step-view @footer | {{icon:step}} 单击 {{key:OK}}：**STEP 步进录制**，4×4 步格写着音名。
OK > seq-notes @footer | 再单击：回到 ROLL。{{key:OK}} 的灯 ROLL 时 {{led:half}}、STEP 时 {{led:full}}。
}}

两种视图编辑同一个 Pattern，GRID 的 16 个键都对应 16 步（第 1 行是第 1–4 步，第 2 行是 5–8 步……）。

| | {{icon:roll}} ROLL | {{icon:step}} STEP |
|---|---|---|
| 屏幕 | 时间 × 音高，音符画成条 | 4×4 步格，写着音名或和弦 |
| GRID 单击 | 选中 / 取消选中这一步 | 把写入光标移到这一步 |
| 弹 PADS | 有选中的步时加 / 删音符；没有时就是弹奏（REC 打开时实时录音） | 写进光标所在的步，松开后光标前进 |
| {{key:ADD}} / {{key:SUB}} | 调整选中的步 | 移动光标 |

<!-- keywords: 视图, 模式切换, 钢琴卷帘, 步进, 卷帘窗, view, piano roll, step view, mode -->

## ROLL 钢琴卷帘 {#seq-roll}

{{shot:seq-notes|ROLL：T2 上的几个音。底部一排小格就是 GRID 的 16 步。
mark 26,52,14,10 = 一个音符：长度 = 时值
mark 8,50,14,12 = 每个 C 有标记
mark 26,119,198,9 = 16 步 = GRID
mark footer = ROLL 视图
key G1 = 有音符：半亮
key G2 = 空步：灭
}}

横向是这一轨的 16 步，纵向是音高（自动显示用到的音域，至少一个八度，黑键行带底纹）。音符越实力度越大；绿色竖线是播放头；底部小格：有音符的步亮，选中的步白色。

**写音符：按住一个或几个 PADS，再点 GRID 上的步。**

{{strip
hold r3c1~C > seq-hold-pad | 按住一个音（C）。
hold r3c1~C + G1 > seq-hold-tap @27,52,14,10 | 不松手，点 GRID 1：第 1 步写进 C。
hold r3c1~C + G5, G9 > seq-notes @26,119,198,9 | 继续点 5、9……换个音再点几步。
}}

- **选中编辑**：单击 GRID 选中一步（可以多选），见 [[#seq-edit]]。
- **实时录音**：打开 REC（{{key:FN+PLAY}} 或 RECORD 面板），播放，然后弹，见 [[#seq-transport]]。

<!-- keywords: 钢琴卷帘, 卷帘窗, 音高, 时值, 音符长度, piano roll, roll, pitch, note length, duration -->

## STEP 步进录制 {#seq-step-rec}

一步一步把音写进去，不用跟着节拍：

{{strip
OK > seq-step-view @footer | 单击 {{key:OK}} 进入 STEP 视图。
G2 > seq-step-view @cell:1,1 | 红色蚂蚁线框是**写入光标**；点 GRID 把它放到任意一步。
r3c2~D > seq-step @cell:1,2 | 按一个 PADS：这个音写进光标那一步。同时按几个就是和弦。
- > seq-step @cell:1,4 | 松开后光标自动前进（角上的「+1」）。
ADD > seq-step @cell:1,4 | {{key:ADD}} 跳过一步（休止），{{key:SUB}} 往回。
hold r3c1~C + G16 > seq-step @cell:4,4 | 按着 PADS 点 GRID：直接写到那一步。
}}

{{shot:seq-step|STEP 视图：每格一步，写着音名；红框是光标。
mark cell:1,4 = 写入光标：闪烁
mark 213,30,15,8 = 每次前进几步
mark cell:1,1 = 写好的步
mark footer = STEP 视图
key G4 = 光标：闪烁
key G1 G2 G3 = 有音符：半亮
key OK = STEP：全亮
}}

- 每次前进几步在 RECORD 面板的 STEP 里设（1 / 2 / 4）。
- 写进去的音用 KEYS 页的力度（VELO），长度一步。之后可以切回 ROLL 细调。

<!-- keywords: 步进录制, 一步一步, 逐步输入, 输入音符, 休止, step record, step input, step entry, rest, cursor -->

## 选中与编辑 {#seq-edit}

在 ROLL 视图里单击 GRID 选中一步或几步，然后：

{{strip
G5 > seq-selected @75,15,14,104 | 单击 GRID 5：第 5 步选中，这一列高亮。
OK, OK > seq-lane @footer | {{key:OK}} 循环编辑项 **NOTE → LEN → VEL → TIME**，底行是正在改的项和数值。
ADD > seq-lane @footer | {{key:ADD}} / {{key:SUB}} 调整，按住连发。{{key:ESC}} 取消选择。
}}

| 键 | 作用 |
|---|---|
| PADS | 在选中的步上加上或去掉这个音（和弦里的每个音都能单独加减） |
| {{key:OK}} | 编辑项 NOTE（音高）→ LEN（长度）→ VEL（力度）→ TIME（微小的时间偏移） |
| {{key:ADD}} / {{key:SUB}} | 调整，按住连发 |
| {{key:FN+OK}} | 「和弦里所有的音」和「其中一个音」之间切换编辑对象 |
| {{key:ESC}} | 取消选择 |

选中时，演奏区上这一步的音 {{led:full|全亮}}。

<!-- keywords: 编辑音符, 改音高, 改长度, 改力度, 时间偏移, 微调, edit notes, note length, velocity, timing, nudge, microtiming, select steps -->

## 播放与实时录音 {#seq-transport}

{{strip
PLAY > seq-playing @strip:PLAY @73,15,4,104 | {{key:PLAY}}：播放，绿线是播放头。再按停止，播放头倒带回第 1 步。
FN+PLAY > seq-recording @strip:PLAY @footer | {{key:FN+PLAY}}：录音待命（REC），PLAY 键灯闪烁，四周红框。
r3c1~C > seq-recording @0,13,240,110 | 播放中弹 PADS：音符按弹的位置写进 Pattern（叠录）。
}}

- REC 也可以在 RECORD 面板里点开。录音待命时左侧 FN 色块里的点变红。
- 录音是叠录，不会删掉已有的音，对齐到微步。ARP 打开时录的是 ARP 弹出的音。
- 弹错了可以选中那一步用 {{key:FN+X}} 清除，或用 {{key:FN+Q}} 量化。

{{shot:seq-recording|播放中录音：PLAY 键灯闪烁，状态行写着 REC。
mark 166,0,22,11 = REC
mark 75,13,4,108 = 播放头
mark strip:PLAY = PLAY：闪烁
mark footer = REC ARMED
key PLAY = 闪烁：录音中
}}

<!-- keywords: 播放, 停止, 录音, 实时录音, 录制, 叠录, play, stop, record, recording, live record, overdub, transport -->

## RECORD 面板 {#record-panel}

在音符页 **按住 {{key:OK}}**（约半秒）打开 RECORD 面板，{{key:ESC}} 关闭。

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

<!-- keywords: 录音设置, 录制设置, 录音面板, 量化, 对齐, record settings, recording settings, record panel, snap, quantize -->

## 无感录制：把刚才弹的找回来 {#capture}

{{icon:capture}} 不用开 REC：你在演奏区弹的每一个音都会在后台记下来（Capture）。弹得好的那一段，事后放进音序器：

{{strip
r3c1~C, r3c3~E, r3c5~G > seq-keyboard | 随便弹。停顿超过 2.5 秒、换轨、超过 48 个音或 60 秒，就开始新的一段（take）。
hold OK > record @cells:2,1-3,4 | 按住 {{key:OK}}：中间两行是最近 8 段 take，编号、轨道、音符数。
G6 > record @cell:2,2 @cells:4,1-4,4 | 点一个 take：第 4 行是它这一小节放进去的样子。{{key:ADD}} / {{key:SUB}} 换小节。
OK > record-placed @14,102,212,26 | {{key:OK}}：放进这一轨的**新 Pattern**，原有的一个音都不动。
}}

- 没有空 Pattern 时会提示，用 {{key:FN+OK}} 合并进当前 Pattern。
- 播放中弹的 take 按小节线对齐；停止时弹的 take 从第一个音开始，按当时的 BPM 计时。
- SNAP 打开时（默认）每个音对齐到最近的整步，长度也取整步；关掉则保留弹的细微时间。

<!-- keywords: 无感录制, 回溯录音, 找回, 刚才弹的, 随便弹, 捕捉, capture, retrospective record, take, takes, what i just played, recall, no record button -->

## PATTERN 页 {#seq-patterns}

{{shot:seq-pattern|PATTERN 页：这一轨的 Pattern 槽。
mark cell:1,1 = 已有的 Pattern：半亮
mark cell:1,2 = 正在用的：全亮
mark cell:1,3 = NEW：新建
mark footer = PATTERN 页
key G1 = 已有
key G2 = 当前
}}

- 单击一个槽：切到这个 Pattern。停止时立即切换；播放中会**排队**，等这一轨这一圈播完再切（格子显示 NEXT 和倒计时条，键灯 {{led:blink|闪烁}}）。
- 单击「NEW」槽：新建一个空 Pattern。
- {{key:ADD}} / {{key:SUB}}：前后切换；{{key:OK}} 回到音符页。
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
| SEQ | 开 / 关 | 这一轨的音序是否播放 |
| STEPS | 1–16 | 这一轨的循环长度；每轨可以不同（例如 12 步对 16 步的复节奏） |
| SWING | 50–75 % | 全局摇摆 |
| BPM | 30–300 | 全局速度；也可以 {{key:FN+T}} 敲出来，或按住 FN 用 {{key:6}} / {{key:Y}} |

<!-- keywords: 步数, 循环长度, 摇摆, 速度, 节拍, 复节奏, steps, length, loop length, swing, shuffle, tempo, bpm, polyrhythm -->

## 复制、粘贴、清除、量化 {#seq-tools}

按住 {{key:FN}} 用字母键（音符页里作用于选中的步；没有选中时作用于整个 Pattern）：

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
| {{key:FN+C}} / {{key:FN+V}} | 复制 / 粘贴 |
| {{key:FN+X}} | 清除（整个 Pattern 时会先确认） |
| {{key:FN+Q}} | 量化到整步 |
| {{key:FN+D}} | 复制 Pattern 到下一个空槽 |
| {{key:FN+T}} | 敲击设定速度 |

> [!warn] 没有撤销，所以清除整个 Pattern 前一定会确认。

<!-- keywords: 复制, 粘贴, 清除, 删除音符, 量化, 对齐, 撤销, copy, paste, clear, delete notes, quantize, undo -->
