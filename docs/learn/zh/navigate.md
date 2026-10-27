# 导航 {#navigate}

三个场景，每个场景几页；四条轨道随时切换。导航全靠左侧四个键：

{{keymap:左侧四键负责全部导航
ESC = 场景
TAB = 轨道
FN = FN 面板
CTRL = 页面
}}

<!-- keywords: 导航, 切换, 菜单, navigation, menu, switch, move around -->

## 场景：ESC {#nav-scenes}

| 场景 | 做什么 | 页面 |
|---|---|---|
| {{scene:INSTR}} 乐器 | 选音源、选音色、调参数和效果 | VOICE、音源参数页、FX |
| {{scene:SEQ}} 音序器 | 写音符、录音、管理 Pattern | ROLL / STEP、PATTERN、TRACK |
| {{scene:TAPE}} 走带 | 编排整首歌、混音、总线效果、工程 | SESSION、SONG、MIX、SENDS、FX、PROJECT、FILES |

**单击 {{key:ESC}}**：三个场景循环。开机进入 {{scene:SEQ}}。

{{strip
ESC > instr-voice @scene | {{scene:INSTR}}：音色和参数。
ESC > tape-session @scene | {{scene:TAPE}}：编排、混音、工程。
ESC > seq-notes @scene | 回到 {{scene:SEQ}}：写音符。
}}

**按住 {{key:ESC}}**：场景地图，点那一列的任意一个键直接跳过去。

{{strip
hold ESC > map-esc @cells:1,1-4,3 | 按住 ESC：三列 = 三个场景，第 4 列空着。
hold ESC + G1 > instr-voice @scene | 点第 1 列：直接到 {{scene:INSTR}}。松开 ESC 地图收起。
}}

> [!note] 弹窗打开时 {{key:ESC}} 是取消；SEQ 里有选中的步时，ESC 先取消选择。

<!-- keywords: 场景, 模式, 切换场景, 乐器, 音序器, 走带, scene, mode, instr, seq, tape, escape -->

## 轨道：TAB {#nav-tracks}

四条轨道，颜色到处一样：{{track:1}} 粉、{{track:2}} 蓝、{{track:3}} 黄绿、{{track:4}} 琥珀。

{{strip
TAB > tab-hint @strip:TAB | 单击 TAB：下一条轨道。约 0.8 秒里左侧只亮对应的那一行（T1 = ESC 行 … T4 = CTRL 行），一眼看出是第几轨。
hold TAB > map-tab @cells:1,1-4,4 | 按住 TAB：轨道地图，和 MIX 页同一布局。
}}

{{shot:map-tab|按住 TAB：每一列一条轨道，每一行一种开关。
mark cells:1,1-1,4 = 行 1：选中这条轨
mark cells:2,1-2,4 = 行 2：静音
mark cells:3,1-3,4 = 行 3：独奏
mark cells:4,1-4,4 = 行 4：音序开关
mark cell:1,2 = 当前 T2
key TAB = 按住
key G1-G4 = 选轨
}}

- {{key:TAB}} + {{key:ADD}} / {{key:SUB}}：前后切轨。
- {{key:FN+TAB}}：上一条轨道。

<!-- keywords: 轨道, 切轨, 换轨, 第几轨, track, switch track, channel, 通道 -->

## 页面：CTRL {#nav-pages}

页签在屏幕最下面，当前页紫底（如 {{page:ROLL}}）；页面太多时两端有 `<` `>`，右边是「当前 / 总数」。

{{strip
CTRL > seq-pattern @footer | 单击 CTRL：下一页（松开时生效）。
hold CTRL > map-ctrl @cells:1,1-1,3 | 按住 CTRL：页面地图，每格写着页签和一句说明。
hold CTRL + G3 > seq-track @footer | 点哪格跳哪页，松开后停在新页面。
}}

- {{key:CTRL}} + {{key:ADD}} / {{key:SUB}}：前后翻页。
- 每条轨道各自记住自己停在 INSTR 的哪一页。

<!-- keywords: 页面, 翻页, 下一页, 子页面, page, next page, tab, pages -->

## 按住左侧键 = 地图 {#nav-maps}

按住 {{key:ESC}}、{{key:TAB}}、{{key:CTRL}} 超过 250 毫秒，屏幕中间变成地图：**格子的位置就是 GRID 上对应的键**。按住 {{key:FN}} 则出现 FN 面板（见 [[#fn-panel]]）。

{{shot:map-ctrl|按住 CTRL：页面地图。
mark cell:1,1 = 当前页
mark cell:1,2 = 别的页
mark header = 地图标题 + 提示
key CTRL = 按住
key G1 = 全亮：当前
key G2 G3 = 半亮：可选
key PADS = 全灭：这时不发声
}}

| 灯 | 意思 |
|---|---|
| {{led:full|全亮}} | 当前项 |
| {{led:half|半亮}} | 能选 |
| {{led:off|灭}} | 没有东西；按住期间演奏区也是灭的（不发声） |

松开左侧键，地图收回去。

<!-- keywords: 地图, 按住, 长按, 覆盖层, map, overlay, hold, long press -->

## 返回与取消 {#nav-back}

{{shot:confirm|危险操作一定会问：后果用红字写出来。
mark 10,77,104,28 = OK 确认
mark 125,77,103,28 = ESC 取消
mark 8,49,172,14 = 后果（红字）
key OK = 闪烁：等你确认
key ESC = 取消
}}

- 弹窗（列表、确认框、文字输入、RECORD 面板、SETUP）里，{{key:ESC}} 关闭或取消。
- 确认框里 {{key:OK}} 确认、{{key:ESC}} 取消；覆盖、删除、丢弃未保存一律先确认。
- SEQ 里有选中的步时，{{key:ESC}} 先取消选择。

<!-- keywords: 返回, 取消, 退出, 关闭, back, cancel, close, exit -->

## 场景与页面地图 {#nav-scene-map}

所有场景和页面一览，点缩略图跳到说明：

{{map:scenes}}

<!-- keywords: 所有页面, 页面一览, 截图, overview, all pages, screens -->
