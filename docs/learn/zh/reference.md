# 速查 {#reference}

<!-- keywords: 速查, 参考, 一览, reference, cheat sheet, summary -->

{{keymap:全部按键一览
ESC = 场景 · 返回
TAB = 轨道
FN = FN 菜单 · 第二功能
CTRL = 页面
PADS = 演奏当前轨
GRID = 屏幕格子
ADD SUB = 加 / 减
OK = 确认 · 录音
PLAY = 播放 / 停止
}}

## 全部按键速查 {#ref-shortcuts}

**两侧按键**

| 按键 | 作用 |
|---|---|
| {{key:ESC}} 单击 | 循环场景 1 INSTR → 2 SEQ → 3 TAPE（在 SETUP 里回到进来之前的场景；弹窗里是取消；SEQ 有选中的步时先取消选择） |
| {{hold:ESC}} + {{key:GRID}} | 场景地图：四列依次是 0 SETUP、1 INSTR、2 SEQ、3 TAPE；第 1 列是进 SETUP 的路 |
| {{key:TAB}} 单击 | 下一轨 |
| {{hold:TAB}} + {{key:GRID}} | 轨道地图，和 MIX 一样排：每列一轨，各行是选中 / 静音 / 独奏 / 音序开关 |
| {{key:TAB}} + {{key:ADD}} / {{key:SUB}} | 下一轨 / 上一轨 |
| {{key:CTRL}} 单击 | 下一页 |
| {{hold:CTRL}} + {{key:GRID}} | 页面地图：这个场景的全部页面，按哪个去哪页 |
| {{key:CTRL}} + {{key:ADD}} / {{key:SUB}} | 下一页 / 上一页 |
| {{key:FN}} 单击 | 打开 / 关闭当前画面的菜单（常驻；按 GRID 执行一项后关闭） |
| {{hold:FN}} | 按住期间显示同一个菜单；下面的字母和两侧按键是它的第二功能 |
| {{key:ADD}} {{key:SUB}} | 加 / 减，在列表里移动；按住连发并加速 |
| {{key:OK}} | 确认、进入、开关；SEQ 音符页单击切换 ROLL / STEP，按住打开 RECORD，有选中的步时循环 NOTE → LEN → VEL → TIME |
| {{key:PLAY}} | 播放 / 停止 |
| HOME 侧键 | Groovebox 不用它（全部停止是 {{key:FN+ESC}}） |
| 按住 GRID 上的参数格 | 演奏区变成它的档位：按哪个设到哪一档 |

**FN + 一个键**

| 按键 | 作用 |
|---|---|
| {{key:FN+GRID}} | 执行菜单里这一格 |
| {{key:FN}} + {{key:6}} / {{key:Y}} | BPM + / −（任何画面，按住连发） |
| {{key:FN}} + {{key:7}} / {{key:U}} | 主音量 + / − |
| {{key:FN}} + {{key:8}} / {{key:I}} | 八度 + / − |
| {{key:FN+ESC}} | 全部停止：音序、所有 ARP 和所有发声 |
| {{key:FN+TAB}} | 上一轨 |
| {{key:FN+CTRL}} | 上一页 |
| {{key:FN+ADD}} / {{key:FN+SUB}} | 大步调整（BPM 一次 10，设置 ×5）；SEQ 音符页：长 Pattern 的下一页 / 上一页；列表里：一次一页 |
| {{key:FN+OK}} | 这个画面的第二个 OK：RESET（恢复默认）、SCOPE（SEQ）、ADD TO SONG（SESSION）、SONG / LOOP（SONG）；采样列表里：改名 / 删除 |
| {{key:FN+PLAY}} | 录音开关（REC） |
| {{key:FN+C}} {{key:FN+V}} | 复制 / 粘贴（选中的步、Pattern 或编排行） |
| {{key:FN+X}} | 清除（整个 Pattern 会先确认）/ 删除编排行 |
| {{key:FN+Q}} | 量化 |
| {{key:FN+D}} | 复制一份 Pattern / 编排行 |
| {{key:FN+T}} | 敲击速度 |
| {{key:FN+S}} | 保存工程（新工程自动起名 SONG 01、SONG 02……） |

**输入画面里的 FN**（列表、文字输入、确认框）

| 按键 | 作用 |
|---|---|
| {{key:FN}} + {{key:;}} {{key:.}} {{key:,}} {{key:/}} | 上 / 下 / 左 / 右（文字输入里：光标到开头 / 结尾、左移 / 右移） |
| {{key:FN+F}} | 在音色 / 采样列表里搜索 |
| {{key:FN+ADD}} / {{key:FN+SUB}} | 一次翻一页 |
| {{key:ADD~del}} / {{key:FN}} + {{key:ADD~del}} | 文字输入里（右上角的键）：删除光标前 / 光标后的字符 |
| {{key:OK}} / {{key:ESC}} | 是 / 否，确定 / 取消 |

<!-- keywords: 快捷键表, 全部快捷键, 按键表, 字母键, 按住地图, shortcut list, all shortcuts, key list, cheat sheet -->

## 全部页面 {#ref-pages}

{{ref:pages}}

<!-- keywords: 所有页面, 页面列表, all pages, page list -->

## 选项列表 {#ref-lists}

{{ref:lists}}

<!-- keywords: 音阶列表, 和弦列表, 选项, scale list, chord list, options -->

## 名词解释 {#glossary}

| 名词 | 意思 |
|---|---|
| 轨道 Track | 四条之一，每条有一个音源、自己的 Pattern、和弦与 ARP；开机在 T2 |
| 音源 Engine | 发声的合成器：DRUMS、JUNO、DX7、SAMPLER |
| 音色 Preset / Sound | 一个音源的一组参数 |
| 步 Step | 音序器的最小格子，1/16 音符 |
| 微步 Microstep | 一步里的 6 个细分，用来记录不完全对齐的时间 |
| Pattern | 一段 1 到 256 步的音序，一次显示 16 步；每轨最多 16 个 |
| 编排行 Song row | SONG 里的一段：每轨播哪个 Pattern、播几小节（SONG 页上的一列，FN 菜单里写作 ROW n） |
| 场景 Scene | 界面的四个部分：0 SETUP、1 INSTR、2 SEQ、3 TAPE |
| SETUP | 场景 0：工程、文件和所有轨道共用的设置；按住 ESC + GRID 第 1 列进入 |
| FN 菜单 | 当前画面能做的事，按住 FN 时铺在 GRID 上（单击 FN 常驻） |
| Take | 无感录制记下的一段弹奏 |
| 自动保存 Autosave | 工作状态存在设备自己的存储里，随时可以拔电 |
| GRID | 右侧 4×4，对应屏幕中间的 4×4 |
| PADS 演奏区 | 中间 4×8，永远弹当前轨 |
| 发送 Send | 送到总线延迟 / 混响的量 |
| 总线 Master bus | 四轨混在一起之后的效果 |

<!-- keywords: 名词, 术语, 解释, glossary, terms, definitions, meaning -->
