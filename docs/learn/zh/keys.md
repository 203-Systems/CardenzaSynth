# 按键 {#keys}

键盘 4 行 × 14 列，四块各有颜色和角色；每个键一颗单色灯，告诉你它**现在**能做什么。机身侧面还有一个 HOME 键。

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

实机同时检测到超过 **10 个矩阵按键**（包含 FN 等功能键）时，会触发数量异常保护：保留已确认的按键，继续处理松键，屏蔽可疑新按键。被屏蔽的键需要松开再按才恢复，避免串扰消退后突然补发音符。侧面 HOME 键不计入这个数量。

<!-- keywords: 键盘, 按键布局, keyboard, layout, keys, 键位, 串扰, 幻键, 同时按键, rollover, ghosting -->

## 按键地图 {#keys-map}

{{icon:map}} 点下图任意一个键，跳到它的说明。

{{map:device}}

<!-- keywords: 地图, 键位图, 示意图, map, diagram -->

## 左侧四个键：ESC TAB FN CTRL {#keys-left}

四个键都是 **单击一个动作，按住一张地图**：

| 键 | {{icon:tap}} 单击 | {{icon:hold}} 按住 |
|---|---|---|
| {{key:ESC}} | 循环场景 {{scene:INSTR}} → {{scene:SEQ}} → {{scene:TAPE}}；在 {{scene:SETUP}} 里是返回；弹窗里是取消 | 场景地图：GRID 四列 = 四个场景，第 4 列 = SETUP |
| {{key:TAB}} | 下一条轨道 {{track:1}} → {{track:2}} → {{track:3}} → {{track:4}} | 轨道地图：选轨、静音、独奏、音序开关 |
| {{key:FN}} | 打开 / 关闭当前画面的菜单 | FN 菜单 + 第二功能，见 [[#fn-menu]] |
| {{key:CTRL}} | 下一页 | 页面地图：当前场景的所有页面 |

{{shot:map-esc|按住 ESC：屏幕变成场景地图，GRID 的列就是场景。
mark cells:1,1-4,1 = 1 INSTR
mark cells:1,2-4,2 = 2 SEQ（当前）
mark cells:1,3-4,3 = 3 TAPE
mark cells:1,4-4,4 = 4 SETUP：只能从这里进
mark strip:ESC = 按住的 ESC
key ESC = 按住
key GRID = 场景：当前全亮、其它半亮
}}

屏幕左边缘的四个色块就是这四个键（从上到下 ESC 红橙、TAB 青、FN 黄、CTRL 紫）。色块中间的小点：

| 色块 | 小点 |
|---|---|
| {{key:ESC}} | 4 点 = 4 个场景，白点是当前场景 |
| {{key:TAB}} | 4 点 = 4 条轨道（轨道色），白点是当前轨 |
| {{key:FN}} | 1 点，录音待命时变红 |
| {{key:CTRL}} | 点数 = 当前场景的页数，白点是当前页 |

<!-- keywords: 左键, 功能键, 修饰键, escape, tab, fn, ctrl, function keys, modifier -->

## 右侧四个键：ADD SUB OK PLAY {#keys-right}

{{keymap:右列：上面两个调值，下面两个执行
ADD SUB = 加 / 减（按住连发）
OK = 确认 · 录音键
PLAY = 播放 / 停止
}}

| 键 | 键帽 | 作用 |
|---|---|---|
| {{key:ADD}} | del | 加 / 列表下一项；按住连续加，越按越快 |
| {{key:SUB}} | `\` | 减 / 列表上一项 |
| {{key:OK}} | ↵ Enter | 确认、执行、开关；在 SEQ 音符页是 {{icon:rec}} **录音键**：单击切换 ROLL / STEP，按住打开 RECORD 面板 |
| {{key:PLAY}} | 空格 | 播放 / 停止；{{key:FN+PLAY}} 开关实时录音 |

> [!note] 文本输入时，最上面那个键（键帽印 del）是删除。

<!-- keywords: 加, 减, 确认, 回车, 播放, 停止, add, sub, enter, ok, play, stop, plus, minus, increase, decrease -->

## 演奏区 PADS {#keys-pads}

中间 4×8 的键永远在弹**当前轨**，不管你在哪个场景、哪一页（按住 ESC / TAB / CTRL 看地图时除外）。

{{strip
hold r3c1~底鼓 > seq-drum-held | 鼓轨 {{track:1}}：每一**列**是一个鼓：底鼓、军鼓、闭镲、开镲、拍手、通鼓、牛铃、沙锤。
hold r3c1~C > seq-hold-pad | 旋律轨：最下面一行白键，上一行黑键，再上两行高一个八度。详见 [[#play-keyboard]]。
- > text-entry | 输入文字时，整个键盘按键帽上的字符打字。
}}

<!-- keywords: 演奏键, 打击垫, 键盘, 弹奏, pads, keyboard, play notes -->

## GRID 4×4 {#keys-grid}

右边 4×4 的 16 个键 = 屏幕中间的 4×4 格子。**屏幕上画的是什么，按下去就是什么：**

{{strip
G1 > seq-step @cell:1,1 | 音序器：16 格 = 16 步。
G1 > instr-juno-vcf @cell:1,1 | 乐器页：16 格 = 16 个参数，点一个就选中它。
G6 > tape-mix @cells:1,2-4,2 | MIX：每一列是一条轨道的选中、静音、独奏、音序开关。
}}

按住左侧任一键时，GRID 变成那个键的「地图」，见 [[#nav-maps]]。

<!-- keywords: 网格, 方阵, 4x4, grid, matrix, 直达键 -->

## HOME 侧键 {#keys-home}

{{icon:stop}} 机身侧面的 HOME 键是 **全部停止**：停止音序器、关掉所有正在发的声音（包括卡住的音、ARP、录音）。所有按键灯闪一下再恢复。

> [!note] HOME 只属于当前应用；切换应用由设备自带的启动器负责。

<!-- keywords: 全部停止, 急停, 卡音, panic, stop all, home, all notes off -->

## 一条总规则 {#keys-rules}

| | 规则 |
|---|---|
| {{hold:ESC}} {{hold:TAB}} {{hold:CTRL}} | **按住左侧任一键时，用右侧 GRID 选择**。地图上的每一格就在它对应的那个键的位置 |
| {{hold:FN}} | 按住 FN 时，**任何键都不做平时的事**：GRID 是菜单，有 FN 功能的键只做 FN 功能（灯亮），没有的什么也不做（灯灭） |
| {{icon:tap}} | 修饰键按住期间被组合用过，松开时就不再触发它的单击动作 |
| {{icon:hold}} | 按住的判定是 250 毫秒；{{key:ADD}} / {{key:SUB}} 按住 0.4 秒后开始连发，连发 12 次后步长 ×5 |

<!-- keywords: 规则, 组合键, 按住, 长按, 连发, rules, hold, long press, repeat, combination -->
