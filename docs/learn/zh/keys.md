# 按键 {#keys}

键盘 4 行 × 14 列，四块各有颜色和角色；每个键一颗单色灯，告诉你它**现在**能做什么。机身侧面还有一个 HOME 键，Groovebox 不用它。

{{keymap:最左列是导航，中间 4×8 弹琴，右边 4×4 是屏幕格子，最右列调值和播放
ESC = ESC 场景
TAB = TAB 轨道
FN = FN 当前画面的菜单
CTRL = CTRL 页面
PADS = 演奏区 PADS
GRID = GRID 4×4
ADD SUB OK = 加 减 确认
PLAY = 播放
}}

<!-- keywords: 键盘, 按键布局, keyboard, layout, keys, 键位 -->

## 按键地图 {#keys-map}

{{icon:map}} 点下图任意一个键，跳到它的说明。

{{map:device}}

<!-- keywords: 地图, 键位图, 示意图, map, diagram -->

## 左侧四个键：ESC TAB FN CTRL {#keys-left}

四个键都是 **单击一个动作，按住一张地图**：

| 键 | {{icon:tap}} 单击 | {{icon:hold}} 按住 |
|---|---|---|
| {{key:ESC}} | 循环场景 {{scene:INSTR}} → {{scene:SEQ}} → {{scene:TAPE}}；在 {{scene:SETUP}} 里是返回；弹窗里是取消 | 场景地图：GRID 四列 = 四个场景，第 1 列 = 0 SETUP |
| {{key:TAB}} | 下一条轨道 {{track:1}} → {{track:2}} → {{track:3}} → {{track:4}} | 轨道地图：选轨、静音、独奏、音序开关 |
| {{key:FN}} | 打开 / 关闭当前画面的菜单 | FN 菜单 + 第二功能，见 [[#fn-menu]]；输入画面里是黄色丝印，见 [[#fn-yellow]] |
| {{key:CTRL}} | 下一页 | 页面地图：当前场景的所有页面 |

{{shot:map-esc|按住 ESC：屏幕变成场景地图，GRID 的列就是场景。
mark cells:1,1-4,1 = 0 SETUP：只能从这里进
mark cells:1,2-4,2 = 1 INSTR
mark cells:1,3-4,3 = 2 SEQ（当前）
mark cells:1,4-4,4 = 3 TAPE
mark strip:ESC = 按住的 ESC
key ESC = 按住
key GRID = 场景：当前全亮、其它半亮
}}

屏幕左边缘的四个色块就是这四个键（从上到下 ESC 红橙、TAB 青、FN 黄、CTRL 紫），各带一个图标。你在哪里看状态行和页签，不看色块：

| 色块 | 图标 | 显示 |
|---|---|---|
| {{key:ESC}} | 返回箭头 | 半亮：随时可用（场景、返回、取消） |
| {{key:TAB}} | Tab 箭头 | 弹窗里灭；切轨后片刻只有新轨对应那一行的色块亮（T1 = ESC … T4 = CTRL） |
| {{key:FN}} | 菜单 | 菜单常驻打开时全亮 |
| {{key:CTRL}} | 页面 | 画面只有一页或弹窗打开时灭 |

按住哪个键，它的色块从下往上填满。

<!-- keywords: 左键, 功能键, 修饰键, escape, tab, fn, ctrl, function keys, modifier -->

## 右侧四个键：ADD SUB OK PLAY {#keys-right}

{{keymap:右列：上面两个调值，下面两个执行
ADD SUB = 加 / 减（按住连发）
OK = 确认 · 录音键
PLAY = 播放 / 停止
}}

| 键 | 键帽 | 作用 |
|---|---|---|
| {{key:ADD}} | del | 加；列表里往上；按住连续加，越按越快 |
| {{key:SUB}} | `\` | 减；列表里往下 |
| {{key:OK}} | ↵ Enter | 确认、执行、开关；在 SEQ 音符页是 {{icon:rec}} **录音键**：单击切换 ROLL / STEP，按住（0.45 秒）打开 RECORD 面板 |
| {{key:PLAY}} | 空格 | 播放 / 停止；{{key:FN+PLAY}} 开关实时录音 |

- 按住 {{key:TAB}} 时 {{key:ADD}} / {{key:SUB}} 是下一条 / 上一条轨道；按住 {{key:CTRL}} 时是下一页 / 上一页。
- 右侧色块上的图标说明 ADD、SUB 在这个画面做什么：+ −、列表里 ↑ ↓、横向 → ←（步、音源选择）。

> [!note] 文本输入时，最上面那个键（键帽印 del）是删除。

<!-- keywords: 加, 减, 确认, 回车, 播放, 停止, add, sub, enter, ok, play, stop, plus, minus, increase, decrease -->

## 演奏区 PADS {#keys-pads}

中间 4×8 的键永远在弹**当前轨**，不管你在哪个场景、哪一页。例外：按住 ESC / TAB / CTRL 看地图时（不发声），按住 FN 时（字母键和快捷调节，见 [[#fn-shortcuts]]），按住一个参数格时（演奏区是它的档位），以及在音色和采样列表里（不发声；搜索用 {{key:FN}} + {{key:F}}）。

{{strip
hold r3c1~底鼓 > seq-drum-held | 鼓轨 {{track:1}}：每一**列**是一个鼓：底鼓、军鼓、闭镲、开镲、拍手、通鼓、牛铃、沙锤。行表示力度：最上面一行最重，最下面一行最轻。
hold r3c1~C3 > seq-hold-pad | 旋律轨（PIANO 排列）：两个完整的八度，第 2、4 行白键，第 1、3 行黑键；C3 在左下角，上面两行高一个八度。详见 [[#play-keyboard]]。
- > text-entry | 输入文字时，整个键盘按键帽上的字符打字。
}}

<!-- keywords: 演奏键, 打击垫, 键盘, 弹奏, pads, keyboard, play notes -->

## GRID 4×4 {#keys-grid}

右边 4×4 的 16 个键 = 屏幕中间的 4×4 格子。**屏幕上画的是什么，按下去就是什么：**

{{strip
G1 > seq-step @cell:1,1 | 音序器：16 格 = 16 步。Pattern 最长 256 步，GRID 一次显示其中一页 16 步。
G1 > instr-juno-vcf @cell:1,1 | 乐器页：16 格 = 16 个参数，点一个就选中它。
G6 > tape-mix @cells:1,2-4,2 | MIX：每一列是一条轨道的选中、静音、独奏、音序开关。
}}

按住左侧任一键时，GRID 变成那个键的「地图」，见 [[#nav-maps]]。

<!-- keywords: 网格, 方阵, 4x4, grid, matrix, 直达键 -->

## HOME 侧键 {#keys-home}

机身侧面的 HOME 键在 Groovebox 里没有功能。{{icon:stop}} **全部停止** 是 {{key:FN+ESC}}：停止音序器、关掉所有正在发的声音（包括卡住的音和所有 ARP），屏幕显示 ALL STOP。

> [!note] 切换应用由设备自带的启动器负责。

<!-- keywords: 全部停止, 急停, 卡音, HOME 键, fn esc, panic, stop all, home, all notes off -->

## 一条总规则 {#keys-rules}

| | 规则 |
|---|---|
| {{hold:ESC}} {{hold:TAB}} {{hold:CTRL}} | **按住左侧任一键时，用右侧 GRID 选择**。地图上的每一格就在它对应的那个键的位置 |
| {{hold:FN}} | 按住 FN 时，**任何键都不做平时的事**：GRID 是菜单，有 FN 功能的键只做 FN 功能（灯亮），没有的什么也不做（灯灭） |
| {{icon:tap}} | 修饰键按住期间被组合用过，松开时就不再触发它的单击动作 |
| {{icon:hold}} | 按住的判定是 250 毫秒（SEQ 音符页的 OK 是 0.45 秒）；{{key:ADD}} / {{key:SUB}} 按住 0.4 秒后开始连发，连发 12 次后步长 ×5 |

<!-- keywords: 规则, 组合键, 按住, 长按, 连发, rules, hold, long press, repeat, combination -->
