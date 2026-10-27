# FN 面板与快捷键 {#fn}

{{key:FN}} 是第二功能键：**按住**它，屏幕变成 FN 面板，有 FN 功能的键亮起；**单击**它，同一个面板常驻打开（SETUP）。

<!-- keywords: fn, 功能键, 第二功能, 设置, setup, settings, function -->

## FN 面板 {#fn-panel}

{{shot:fn-held|按住 FN：FN 面板（这里在 1 KEYS 页）。只有有 FN 功能的键亮着。
mark 10,0,100,11 = 页签：FN + 1…5 换页
mark cells:1,1-2,4 = 这一页的设置：FN + GRID 选
mark footer = 快捷键提示
mark strip:FN = 按住的 FN
key FN = 按住
key 1 2 3 4 5 = 换页
key 6 7 8 Y U I = 快捷调节
key G1-G8 = 设置
key ADD SUB = 调值
}}

| 操作 | {{hold:FN}} 按住 FN 时 | {{icon:tap}} SETUP 打开时 |
|---|---|---|
| 换页 | {{key:FN+1}} … {{key:FN+5}}，或 {{key:FN+CTRL}} | 同左，或单击 {{key:CTRL}} |
| 选设置 | {{key:FN}} + GRID 键 | GRID 键 |
| 调整 | {{key:FN+ADD}} / {{key:FN+SUB}}（可连发） | {{key:ADD}} / {{key:SUB}} |
| 切换选项 | 再按一次同一个 GRID 键 | 同左，或 {{key:OK}} |
| 恢复默认 | — | {{key:FN+OK}} |

- **按住 {{key:FN}}**：显示 FN 面板，松开就收起。
- **单击 {{key:FN}}**：打开 SETUP，就是常驻的 FN 面板；再单击 FN 或按 ESC 关闭。SETUP 打开时演奏区照常发声，改了马上能听到。
- 面板记住上次停在哪一页，以及每页选中的是哪个设置。

<!-- keywords: fn 面板, 设置面板, setup, 常用设置, panel, settings page -->

## 五页设置 {#fn-pages}

{{strip
FN, FN+1 > setup-keys @footer | **1 KEYS 音阶**：排列、音阶、根音、八度、力度、锁定。
FN+2 > setup-chord @footer | **2 CHORD 和弦**：和弦、转位。
FN+3 > setup-arp @footer | **3 ARP 琶音**：模式、速度、范围、GATE、LATCH。
FN+4 > setup-project @footer | **4 PROJECT 工程**：BPM、Swing、LOOP / SONG、步数、录音待命。
FN+5 > setup-system @footer | **5 SYSTEM 系统**：主音量、按键灯、屏幕亮度；设备状态。
}}

| 页 | 存在哪 |
|---|---|
| 1 KEYS · 2 CHORD · 3 ARP | 应用设置（开机恢复，换工程不变） |
| 4 PROJECT | BPM、Swing、步数属于工程 |
| 5 SYSTEM | 设备设置（所有应用共用） |

下面是每一页的全部设置、范围和默认值（直接从固件代码生成）：

{{ref:fn}}

> [!tip] 在 SETUP 里选中 5 SYSTEM 的 KEY LED 时，演奏区从左到右显示按键灯能给出的每一档亮度，方便判断亮度设置和灯本身是否正常（见 [[#lights-ramp]]）。

<!-- keywords: keys, chord, arp, project, system, 主音量, 屏幕亮度, 按键灯亮度, bpm, swing, 设置项 -->

## 快捷调节：BPM、音量、八度 {#fn-quick}

最常用的三项不用找页面。按住 {{key:FN}}，演奏区最右边紧挨 GRID 的 2×3 块就是三对加减键，**上排加、下排减**（和右侧 ADD 在 SUB 上面一样）：

{{keymap:按住 FN：6 7 8 加，Y U I 减
FN = 按住
6 Y = BPM + / −
7 U = 音量 + / −
8 I = 八度 + / −
}}

{{strip
hold FN + 6 > fn-quick @14,102,212,26 | BPM +1（{{key:Y}} 是 −1）。按住连续调。
hold FN + 7 > fn-volume @14,102,212,26 | 音量：声音太小就按 {{key:7}}，太大按 {{key:U}}。
hold FN + 8 > fn-octave @14,102,212,26 | 八度：{{key:8}} 升、{{key:I}} 降。
}}

{{ref:fn-quick}}

任何 FN 页都能用，按住连续调，屏幕底部弹出新值。这 6 个键在按住 FN 时 {{led:half|半亮}}，按下时 {{led:full|全亮}}。

<!-- keywords: 音量, bpm, 八度, 快捷键, 调速度, 调节拍, 调音量, 声音大小, 大声, 小声, 升八度, quick, shortcut, tempo, bpm, volume, louder, quieter, octave -->

## 第二功能（字母键与四周的键） {#fn-shortcuts}

{{keymap:按住 FN 时有第二功能的键
FN = 按住
ESC = 全部停止
TAB = 上一轨
CTRL = 下一页
C V = 复制 粘贴
X = 清除
Q = 量化
D = 复制 Pattern
T = Tap tempo
S = 保存
OK = 第二功能
PLAY = 录音
}}

| FN + | 作用 |
|---|---|
| {{key:1}}–{{key:5}} | FN 面板换页 |
| GRID | 选 FN 面板上的设置 |
| {{key:ADD}} / {{key:SUB}} | 调整选中的设置（可连发） |
| {{key:CTRL}} | FN 面板下一页 |
| {{key:ESC}} | 全部停止（同 HOME） |
| {{key:TAB}} | 上一条轨道 |
| {{key:OK}} | 乐器 / 发送 / 总线效果：当前参数恢复默认；SEQ：和弦里的编辑对象（全部 / 单音）；SESSION：把正在播的组合记成 SONG 的新一行；SONG：SONG / LOOP 切换；RECORD 面板：把 take 合并进当前 Pattern；采样列表：改名 / 删除 |
| {{key:PLAY}} | 实时录音开关（REC） |
| {{key:C}} / {{key:V}} | 复制 / 粘贴（选中的步或当前 Pattern；SONG 里是整行） |
| {{key:X}} | 清除（选中的步，或当前 Pattern 需确认；SONG 里删除这一行） |
| {{key:Q}} | 量化（选中的步或当前 Pattern） |
| {{key:D}} | 把当前 Pattern 复制到下一个空位；SONG 里复制这一行 |
| {{key:T}} | Tap tempo：连按两次以上，按间隔设 BPM |
| {{key:S}} | 保存工程（没有名字时先起名） |

> [!note] SETUP 打开时字母快捷键不生效（面板只管设置）。字母功能执行后屏幕底部弹出名称和结果。

<!-- keywords: 快捷键, 组合键, 复制, 粘贴, 清除, 删除, 量化, 保存, tap tempo, shortcut, copy, paste, clear, delete, quantize, duplicate, save -->

## 输入画面里的 FN：黄色丝印 {#fn-yellow}

在文字输入、预设 / 采样列表、确认框这些弹窗里，按住 FN 不出面板，而是用键帽上的**黄色丝印**：

{{keymap:按住 FN：GRID 右下角四个键是方向键
FN = 按住
; = 上
. = 下
, = 左
/ = 右
ADD SUB = 翻页
}}

<!-- keywords: 方向键, 上下左右, 黄色, 丝印, 列表, arrows, cursor keys, up down left right, yellow legend -->

## 输入文字 {#fn-text}

起名字（工程、音色、采样）时整个键盘按键帽打字：

{{shot:text-entry|输入名字：SAVE AS。
mark 0,13,240,10 = 要起名的东西
mark 8,40,222,30 = 名字和光标
mark 222,13,16,9 = 字数 / 24
mark footer = OK 确定 · DEL 删除 · ESC 取消
key ADD = 删除（del）
key OK = 确定
key ESC = 取消
key PLAY = 空格
}}

- 字母、数字、空格（PLAY 键）、`-`；最多 24 个字符。
- 右上角键（键帽 del）删除光标前的字符；{{key:OK}} 确定；{{key:ESC}} 取消。
- {{key:FN}} + {{key:,}} / {{key:/}} 左右移动光标，{{key:FN}} + {{key:;}} / {{key:.}} 跳到开头 / 结尾，{{key:FN}} + del 删除光标后的字符。
- 光标是字符之间的竖线，在末尾时是方块。

<!-- keywords: 打字, 起名, 重命名, 文本, 光标, 删除字符, type, name, rename, text entry, cursor, backspace -->
