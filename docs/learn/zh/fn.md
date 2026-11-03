# FN 菜单与快捷键 {#fn}

{{key:FN}} 是**当前画面的菜单**：按住它，GRID 变成这个画面能做的事的菜单，四周的键是第二功能；单击它，同一个菜单常驻打开。

<!-- keywords: fn, 功能键, 第二功能, 菜单, function, menu, shift -->

## FN 菜单 {#fn-menu}

{{shot:fn-held|在 SEQ ROLL 按住 FN：这一页的菜单铺在 GRID 上。只有有 FN 功能的键亮着。
mark header = FN MENU 和这是哪个画面的菜单：SEQ ROLL
mark cells:1,1-3,4 = 行 1–3：这一页的菜单项
mark cells:4,1-4,4 = 行 4：所有画面都一样
mark footer = 快捷调节：BPM、音量、八度
key FN = 按住
key GRID = 菜单项：FN + GRID 执行
key C X Q D S T = 菜单项的字母键
key 6 7 8 Y U I = 快捷调节
key PLAY = 录音待命
}}

| | {{hold:FN}} 按住 FN | {{icon:tap}} 单击 FN：菜单常驻 |
|---|---|---|
| 执行一项 | {{key:FN}} + 它的 GRID 键，或 {{key:FN}} + 它的字母 | 它的 GRID 键；执行后菜单关闭 |
| 关闭 | 松开 FN | {{key:FN}} 或 {{key:ESC}} |
| 演奏区 | 不发声；只有字母键和快捷调节起作用 | 照常弹 |
| {{key:ADD}} {{key:SUB}} {{key:OK}} | 大步调整（SEQ 音符页：GRID 翻页）和第二个 OK（见 [[#fn-shortcuts]]） | ADD / SUB 不起作用；{{key:FN+OK}} 仍是第二个 OK |

- 按住的判定是 250 毫秒；更快松开就算单击。
- 行 1–3 属于你所在的页面；行 4 在所有画面都一样：**SAVE** 保存工程、**TAP** 敲速度、**REC** 录音待命、**SETUP**（在 SETUP 里是 **BACK**）。
- 原来的 FN 面板和它的五页没有了：设置都在 [[#setup]]，{{key:FN}} + {{key:1}}…{{key:5}} 不再有作用。

{{shot:fn-menu-open|单击 FN：同一个菜单常驻。底行写着这是哪个画面的菜单、怎么关闭。
mark cell:1,1 = 名称 · 作用对象 · FN 字母
mark cell:1,2 = 用不了：虚线
mark cell:2,2 = 开关：VIEW（ROLL / STEP）
mark 8,123,56,12 = 哪个画面的菜单：SEQ ROLL
mark 141,123,90,12 = FN / ESC 关闭
key GRID = 执行一项
key PADS = 照常弹
}}

每一格：

- 上面是**名称**，右上角黄色的是它的 FN 字母：{{key:FN+C}} 就是 COPY；`OK` 表示 {{key:FN+OK}}，`PLAY` 表示 {{key:FN+PLAY}}；
- **第二行**写作用对象（PATTERN 或 STEPS、ROW 2、音源）或状态（ROLL、120 BPM、OFF）；
- **灯**：{{led:half|半亮}} 能用，{{led:full|全亮}} 是开着的开关（反显：VIEW STEP、REC ARMED、PLAY SONG），{{led:off|灭}} 加虚线是现在用不了（没复制过东西时的 PASTE、没有选中步时的 SCOPE）。

<!-- keywords: fn 菜单, 菜单, fn 面板, 面板, 当前画面的菜单, 上下文菜单, 按住 fn, 单击 fn, 菜单项, fn menu, menu, fn panel, context menu -->

## 每个画面的菜单 {#fn-menus}

| 画面 | 行 1–3 |
|---|---|
| INSTR（所有页） | **PRESETS** 浏览音色 · **SAVE** / **SAVE AS** 保存 / 另存音色 · **ENGINE** 选音源 · **RESET** `OK`：焦点参数恢复默认（参数页，包括 CHORD 和 ARP）；SAMPLER 另有 **SAMPLES**（采样文件）、**RECORD**（录采样）、**PREVIEW**（试听） |
| SEQ（所有页） | **COPY** `C` · **PASTE** `V` · **CLEAR** `X` · **QUANT** `Q`：对选中的步，没有选中时对整个 Pattern · **DUP** `D`：Pattern 复制到下一个空位（最多 16 个）· **VIEW**：ROLL / STEP（鼓轨是虚线）· **RECORD**：RECORD 面板和 take · **SCOPE** `OK`：步编辑改和弦里的哪个音（有选中的步时）· **PAGE −** / **PAGE +**：长 Pattern 在 GRID 上显示哪一页（音符页，超过 16 步时）· **LEN −** / **LEN +**：Pattern 长度，1 到 256 步（1、2、4、8、16，之后每次 16 步） |
| TAPE SESSION | **LAUNCH**：发射光标所在的行 · **ADD** TO SONG `OK`：正在播的 Pattern 记成 SONG 的新一段 |
| TAPE SONG | **COPY** `C` · **PASTE** `V`（插在光标之后）· **DELETE** `X` · **DUP** `D`：光标所在的一段（ROW n）· **PLAY** `OK`：SONG / LOOP |
| TAPE MIX | **UNMUTE** ALL 全部取消静音 · **UNSOLO** ALL 全部取消独奏 |
| TAPE SENDS · FX | **RESET** `OK` |
| SETUP PROJECT · FILES | **NEW** · **LOAD** · **SAVE AS**（工程） |
| SETUP KEYS · SYSTEM | **RESET** `OK`：焦点设置恢复默认 |
| **行 4，所有画面** | **SAVE** `S`：保存工程（有改动时写 UNSAVED）· **TAP** `T`：敲速度，显示 BPM · **REC** `PLAY`：录音待命，开着时全亮 · **SETUP**：到 0 SETUP；在 SETUP 里是 **BACK**，回到进来之前的场景 |

{{shot:fn-menu-instr|在 INSTR VOICE（T2 Juno）按住 FN：音色相关的几项；VOICE 页没有参数可恢复，所以 RESET 是虚线。
mark cells:1,1-1,4 = PRESETS · SAVE · SAVE AS · ENGINE
mark cell:2,1 = RESET：只在参数页
mark cells:4,1-4,4 = 行 4：SAVE · TAP · REC · SETUP
key G1-G4 = 能用：半亮
key G5 = 用不了：灭
}}

<!-- keywords: 菜单项, 每个画面, 复制, 粘贴, 发射, 全部取消静音, 取消独奏, 恢复默认, 新建, 载入, 另存, 返回, 翻页, 长度, Pattern 长度, 长 Pattern, 256 步, menu items, menus, per screen, launch, reset, back, length -->

## 快捷调节：BPM、音量、八度 {#fn-quick}

最常用的三项不用找页面。按住 {{key:FN}}，演奏区最右边紧挨 GRID 的 2×3 块就是三对加减键，**上排加、下排减**（和右侧 ADD 在 SUB 上面一样）：

{{keymap:按住 FN：6 7 8 加，Y U I 减
FN = 按住
6 Y = BPM + / −
7 U = 音量 + / −
8 I = 八度 + / −
}}

{{strip
hold FN + 6 > fn-quick @14,94,212,28 | BPM +1（{{key:Y}} 是 −1）。按住连续调。
hold FN + 7 > fn-volume @14,94,212,28 | 音量：声音太小就按 {{key:7}}，太大按 {{key:U}}。
hold FN + 8 > fn-octave @14,94,212,28 | 八度：{{key:8}} 升、{{key:I}} 降。
}}

{{ref:fn-quick}}

按住 FN 时任何画面都能用，按住连续调，新值在屏幕底部弹出，盖在菜单上面。这 6 个键在按住 FN 时 {{led:half|半亮}}，按下时 {{led:full|全亮}}。

<!-- keywords: 音量, bpm, 八度, 快捷键, 调速度, 调节拍, 调音量, 声音大小, 大声, 小声, 升八度, quick, shortcut, tempo, bpm, volume, louder, quieter, octave -->

## 第二功能（字母键与四周的键） {#fn-shortcuts}

{{keymap:按住 FN 时有第二功能的键
FN = 按住
ESC = 全部停止
TAB = 上一轨
CTRL = 上一页
ADD SUB = 大步调整
OK = 第二个 OK
PLAY = 录音待命
C V = 复制 粘贴
X = 清除
Q = 量化
D = 复制一份
T = Tap tempo
S = 保存
}}

| FN + | 作用 |
|---|---|
| GRID | 执行菜单里这一格 |
| {{key:ESC}} | 全部停止：音序器、所有正在响的音和所有 ARP（ALL STOP） |
| {{key:TAB}} | 上一条轨道 |
| {{key:CTRL}} | 上一页（单按 {{key:CTRL}} 是下一页） |
| {{key:ADD}} / {{key:SUB}} | 大步调整：宽范围参数 ×10（BPM 一次 10），窄范围 ×3，SETUP 设置 ×5，VOICE 页一次 10 个音色，列表一次 8 行；SEQ 音符页：长 Pattern 的 GRID 翻页（有选中的步时：大步改它们的值） |
| {{key:OK}} | 这个画面的第二个 OK：INSTR、SENDS、总线 FX 和 SETUP 设置页：焦点值恢复默认；SEQ 音符页：和弦里的编辑对象（全部 / 单音）；SESSION：ADD TO SONG；SONG：SONG / LOOP 切换；RECORD 面板：把 take 合并进当前 Pattern；采样列表：改名 / 删除 |
| {{key:PLAY}} | 录音待命（REC） |
| {{key:C}} / {{key:V}} | 复制 / 粘贴（选中的步或当前 Pattern；SONG 里是一段） |
| {{key:X}} | 清除（选中的步，或当前 Pattern 需确认；SONG 里删除这一段） |
| {{key:Q}} | 量化（选中的步或当前 Pattern） |
| {{key:D}} | 把当前 Pattern 复制到下一个空位；SONG 里复制这一段 |
| {{key:T}} | Tap tempo：连按两次以上，按间隔设 BPM |
| {{key:S}} | 保存工程（还没起名时不问名字，直接用第一个空着的 SONG 01、SONG 02……） |
| {{key:F}} | 音色和采样列表里：搜索，见 [[#fn-yellow]] |
| {{key:1}}–{{key:5}} | 没有作用 |

> [!note] 字母键只在菜单里有对应项的画面起作用（那时它半亮）；{{key:S}} 和 {{key:T}} 到处都能用。执行后屏幕底部弹出名称和结果。菜单常驻（没有按住 FN）时演奏区弹的是音，不是字母功能。

<!-- keywords: 快捷键, 组合键, 复制, 粘贴, 清除, 删除, 量化, 保存, 上一页, 大步, tap tempo, shortcut, copy, paste, clear, delete, quantize, duplicate, save -->

## 输入画面里的 FN：黄色丝印 {#fn-yellow}

在所有弹窗里（文字输入、音色 / 采样列表、音源选择、RECORD 面板、确认框），按住 FN 不出菜单，而是用键帽上的**黄色丝印**：

{{keymap:按住 FN：GRID 右下角四个键是方向键
FN = 按住
; = 上
. = 下
, = 左
/ = 右
ADD SUB = 翻页
F = 搜索（列表）
}}

- 音色和采样列表里演奏区不打字：{{key:FN}} + {{key:F}} 打开搜索框（最多 16 个字符），{{key:OK}} 按它筛选列表。{{key:ESC}} 先清掉搜索，再按一次 {{key:ESC}} 关闭列表。
- {{key:FN+ESC}} 在这里同样是全部停止；文字输入里除外，那里 ESC 是取消。

<!-- keywords: 方向键, 上下左右, 黄色, 丝印, 列表, 搜索, 查找, 筛选, arrows, cursor keys, up down left right, yellow legend, search, fn f -->

## 输入文字 {#fn-text}

起名字（工程、音色、采样）时整个键盘按键帽打字：

{{shot:text-entry|输入名字：SAVE PROJECT AS。
mark 0,13,240,10 = 要起名的东西
mark 8,40,222,30 = 名字和光标
mark 203,13,28,10 = 字数 / 24
mark footer = OK 确定 · DEL 删除 · ESC 取消
key ADD = 删除（del）
key OK = 确定
key ESC = 取消
key PLAY = 空格
}}

- 字母、数字、空格（PLAY 键，不能放在第一个）、`-`；最多 24 个字符（搜索 16 个）。SAVE AS 从当前名字开始，没有名字时是 SONG。
- 右上角键（键帽 del）删除光标前的字符；{{key:OK}} 确定；{{key:ESC}} 取消。
- {{key:FN}} + {{key:,}} / {{key:/}} 左右移动光标，{{key:FN}} + {{key:;}} / {{key:.}} 跳到开头 / 结尾，{{key:FN}} + del 删除光标后的字符。
- 光标是字符之间的竖线，在末尾时是方块。

<!-- keywords: 打字, 起名, 重命名, 文本, 光标, 删除字符, type, name, rename, text entry, cursor, backspace -->
