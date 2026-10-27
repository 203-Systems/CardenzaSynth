# CardenzaSynth

Cardenza 是一台四轨 groovebox：DX7、Juno、Sampler、Drums 四种音源，16 步音序器，整首歌的 Pattern 编排（走带），四轨混音，每轨效果（失真、降采样、DJ 滤波、合唱）与总线延迟 / 混响，工程与音色存储。屏幕 240×135，键盘 4×14 并带单色按键灯。

界面是黑底的终端风格，搭配新极简排版和 dither 填充。屏幕左右两边各有 4 个键帽，和键盘最左、最右两列逐行对应：左侧不写字，用颜色区分（ESC 红橙、TAB 青、FN 黄、CTRL 紫），键帽里的小点表示这个键管的对象（场景、轨道、录音、页），当前项是白点；右侧是 + − ↵ ▶。按键灯有一套语言：亮度表示数值大小，呼吸表示按下会执行动作，细闪表示焦点，闪烁表示等待中；当前画面在用的那一块键最亮。界面和按键灯都带动效，逐帧录像（含键盘灯光）见 [docs/groovebox/motion](docs/groovebox/motion)。完整规格见 [Groovebox 设计](docs/groovebox-design.md)。

## 按键

按住左侧任一键时，屏幕会变成一张和右侧 4×4（GRID）一一对应的地图，直接按 GRID 选择。

| 键 | 单击 | 按住 |
|---|---|---|
| ESC | 在三个场景之间循环（乐器 → 音序器 → 走带）；弹窗里是取消 | 场景地图：GRID 三列 = 乐器 / 音序器 / 走带 |
| TAB | 下一轨 | 轨道地图：GRID 每列一轨，行 0 选轨、行 1 静音、行 2 独奏、行 3 音序开关；TAB + ADD / SUB 前后切换 |
| FN | 打开 / 关闭 SETUP（常驻的 FN 面板） | FN 面板：Fn + 1–5 或 Fn + CTRL 换页，Fn + GRID 选参数，Fn + ADD / SUB 调整；PADS 上是字母快捷键；按住 FN 时其它键只做 Fn 功能 |
| CTRL | 下一页 | 页面地图：当前场景的所有页面排在 GRID 上，按哪格跳哪页；CTRL + ADD / SUB 翻页 |
| ADD / SUB（右侧上两键） | 加 / 减，列表移动 | 连发并加速；Fn + ADD / SUB 调 FN 面板上选中的参数 |
| ENTER | 确认、进入、开关 | Fn + ENTER：恢复默认 / 和弦编辑对象 / 记进 SONG / SONG·LOOP / 文件选项 |
| PLAY | 播放 / 停止 | Fn + PLAY：实时录音开关 |
| HOME 侧键 | 全部停止 | — |

中间 4×8 是演奏键（PADS），任何场景都演奏当前轨；右侧 4×4（GRID）和屏幕中央的 4×4 格子一一对应。文本输入时，最上面那个右侧键（键帽印 del）是删除。

按住 FN 快捷调节：`6` / `Y` BPM + / −，`7` / `U` 主音量 + / −，`8` / `I` 八度 + / −（演奏区最右边的 2×3 块，上加下减，按住连续调）。Fn + PADS：`1–5` FN 面板换页，`C V` 复制粘贴，`X` 清除，`Q` 量化，`D` 复制 Pattern（走带页为复制整行），`T` Tap tempo，`S` 保存。Fn + ESC 全部停止，Fn + TAB 上一轨，Fn + PLAY 录音。字母操作会在屏幕底部弹出名称和新值。

在文字输入、列表这类输入画面里，按住 FN 用的是键帽上的黄色丝印：`; . , /` 是上下左右；输入文字时 Fn + `, /` 移动光标，Fn + `; .` 跳到开头 / 结尾，Fn + del 删除光标后的字符。

四条轨道各有颜色：T1 粉、T2 蓝、T3 黄绿、T4 琥珀，状态行、TAB 色条、混音、发送、走带里都一致。切轨时左列四个键只亮对应的那一行（T1 在最上面）。

## 先做一个循环

1. 开机在 **2 SEQ**。T1 是 808 鼓组，每一列演奏键是一种鼓。
2. 按住一个演奏键，再点 GRID 上的几个 step：鼓点直接写入。
3. 点一下 step 会选中它（可多选），演奏区会亮出这一步里的音。选中后按演奏键加 / 删音符；ENTER 在 NOTE / LEN / VEL / TIME 之间切换，ADD / SUB 调整；ESC 取消选择。
4. PLAY 播放，演奏区会跟着音序器正在弹的音闪烁。按 TAB 换到 T2 Juno，在 STEPS 页写一段旋律。
5. CTRL 到 PATTERN 页：点空槽新建 Pattern；播放中点别的 Pattern 会在本轮循环结束时切换。Fn + D 把当前 Pattern 复制一份。
6. Fn + PLAY 打开录音，播放中直接弹演奏键即叠录进当前 Pattern。
7. 按住 ESC 再按 GRID 第三列进入走带，先到 SESSION：每列一轨，点格子即时切换 Pattern，满意的组合按 Fn + ENTER 记进 SONG。CTRL 翻到 SONG：点 GRID 第一列的 + 追加编排行，选中一格用 ADD / SUB 选 Pattern（`--` 为静音），再点一次同一格后用 ADD / SUB 改小节数，ENTER 从这一行开始播放整首歌。
8. CTRL 翻到 MIX 调音量、静音、独奏；SENDS 页给每轨开延迟 / 混响发送，FX 页调总线；按住 CTRL 在地图里点 PROJECT，SAVE AS 输入名字保存。

## FN 面板（按住 FN，或单击 FN 打开 SETUP）

按住 FN 时屏幕显示 FN 面板，松开即收起；单击 FN 打开的 SETUP 是同一个面板，常驻到再单击 FN 或按 ESC。面板有 5 页，Fn + 1–5 直接跳页，Fn + CTRL 依次换页（SETUP 里单击 CTRL 即可），并记住上次停留的页和每页选中的参数。Fn + GRID 选参数，Fn + 右上角的 del / `\`（ADD / SUB）调整；SETUP 里不用按 FN，Fn + ENTER 恢复默认，打开时演奏区照常发声。

- **1 KEYS 音阶**：键盘布局（默认 PIANO 黑白键排列，另有 SCALE、CHROMA）、14 种音阶、根音、八度、演奏力度、音阶锁定（音阶外的键静音或落到音阶上）。
- **2 CHORD 和弦**（MIDI 效果）：和弦（TRIAD / 7TH 按音阶叠三度，以及 MAJ、MIN、SUS4、MAJ7、MIN7、DOM7、POWER、OCTAVE）与转位；图上显示一个键弹出的音和这个调的各级和弦。
- **3 ARP 琶音**（MIDI 效果）：UP / DOWN / UP-DN / RANDOM / ORDER，1/4–1/32，1–4 个八度，GATE，LATCH；图上是 16 步的演奏顺序。
- **4 PROJECT 工程**：BPM、Swing、LOOP / SONG、当前轨步数、录音待命；工程名、保存状态、四轨 Pattern 一览。
- **5 SYSTEM 系统**：主音量、按键灯、屏幕亮度，以及设备状态。

KEYS / CHORD / ARP 存在本应用的设置文件里，SYSTEM 存在设备设置里，开机自动恢复，改动约 1 秒后写入 SD 卡，载入或新建工程不会改变它们；PROJECT 的 BPM、Swing、步数属于工程。

## 场景

- **1 INSTR 乐器**：VOICE 页大字显示音色，ADD / SUB 直接切换音色；ENGINE、PRESET（可输入搜索）、SAVE、SAVE AS。之后是引擎参数页：GRID 选参数，ADD / SUB 调值，空格位置显示包络、滤波、算法、波形等图形。DX7 共 11 页（算法、算子电平、六个算子的音高与 R1–R4 / L1–L4 包络、LFO），Juno 6 页（DCO、VCF、ENV、LFO）。每个音源都有 FX 页（DRIVE、CRUSH、FILTER、RESO、CHORUS、DELAY、REVERB，附信号链图）。Sampler 的 SAMPLE 页可浏览 WAV、录音、裁剪和保存。
- **2 SEQ 音序器**：音符页有两种视图，默认 **ROLL 钢琴卷帘**（时间 × 音高，音符画成和时值一样长的条），单击 OK 切到 **STEP 步进录制**（按 PADS 写进光标所在的步，松开后光标前进）。OK 是录音键：按住打开 RECORD 面板，可以开关实时录音，并把「无感录制」记下的最近 8 段弹奏（take）中的一小节放进新 Pattern。之后是 PATTERN、TRACK（序列开关、每轨步数、Swing、BPM）。
- **3 TAPE 走带**：SESSION（类似 Ableton Live 的 Session 视图：四轨并排，每格一个 Pattern，点格子在本轮循环结束时切换，ENTER 发射整行，Fn + ENTER 把当前组合记录进 SONG）、SONG（整首歌的 Pattern 编排：按顺序播放，每行循环 N 小节，四轨同时显示；SONG / LOOP 模式）、MIX（四轨推子、MUTE / SOLO / SEQ、主音量）、SENDS（四轨的滤波、合唱、延迟与混响发送一屏总览）、FX（总线：按速度同步的磁带延迟、混响、EQ、压缩、TAPE 饱和）、PROJECT（保存 / 另存 / 载入 / 新建、BPM、Swing、Tap）、FILES（工程列表）。编排格子再点一次在 Pattern 与小节数之间切换。

采样为单声道 PCM16 WAV，支持 8–96 kHz。工程文件为 v4（含编排和总线效果），可以读取 v1–v3 以及开发版写的 v5 工程。延迟和混响的缓冲在第一次打开发送时才分配（板子没有 PSRAM）。

## 应用与存储

设备可以装多个应用（组件），一次运行一个，由设备自带的启动器选择；本固件不带自己的启动器。切换前应用会保存当前工作状态（session），回来时原样恢复。

SD 卡上每个应用有自己的目录，内置和用户自己写的应用都遵守同一份[存储协议](docs/app-storage-protocol.md)：

```text
/sdcard/Cardenza/
  System/settings.cfg        设备设置：主音量、按键灯、屏幕亮度、上次的应用、协议版本
  Shared/Samples/            所有应用共用的采样
  Apps/<作者>/<名称>/         settings.cfg、Projects/、Presets/<组>/、Session/
```

Groovebox 是 `Apps/Cardenza/Synth/`：工程在 `Projects/`，用户音色在 `Presets/<引擎>/`，SETUP 的演奏设置在 `settings.cfg`。设置文件是 `key=value` 文本，原子写入；没有 SD 卡时设置暂存在设备内部（NVS），插卡后自动写回卡上。旧版固件的 `/sdcard/CardenzaSynth/{Project,Preset,Sample}` 在第一次开机时自动搬到新位置，不会覆盖或丢失文件。

## Linux 截图与测试

Linux 上没有窗口版模拟器，但可以无窗口运行并把画面导出为 PNG：

```bash
cmake -S . -B build/linux -G Ninja -DDEVICE=CardenzaSim
cmake --build build/linux
ctest --test-dir build/linux --output-on-failure
build/linux/Devices/CardenzaSim/CardenzaSimShot --script tests/groove/tour.script --out build/linux/shots --scale 2
```

`tests/groove/tour.script` 走查全部场景、浮层和主要编辑状态。

## 浏览器模拟器

同一套 C++ 代码（GrooveApp、Groove 界面、AMY 音源与运行时、CardenzaSim 设备层）用 Emscripten 编译成 WebAssembly，在网页里显示同样的屏幕和 4×14 键盘（含按键灯），声音来自 `synth::audio::RenderFrames`。电脑键盘按物理位置映射到设备键（数字行 = 第 0 行，Q 行 = 第 1 行，A 行 = 第 2 行，Z 行 = 第 3 行；Esc = ESC，Tab = TAB，Caps / 左 Shift = FN，Ctrl = CTRL，Backspace = ADD，`\` = SUB，Enter = ENTER，空格 = PLAY，Home = HOME）。每个键帽上都标着对应的电脑键。设置、工程和音色保存在浏览器里（IndexedDB，目录结构同 SD 卡），「Load WAV…」可以把 WAV 放进共用采样目录 `Shared/Samples`。

```bash
git clone https://github.com/emscripten-core/emsdk.git ~/emsdk && ~/emsdk/emsdk install latest && ~/emsdk/emsdk activate latest
make web        # 即 make DEVICE=CardenzaWeb build，输出 build/web/site/
make web-serve  # http://localhost:8000/
```

输出目录可以放到任何静态服务器上（不需要 COOP/COEP 头，可以嵌入 iframe）。线上版本在 https://cardenza-synth.203.io（Vercel），`make web-deploy` 重新构建并发布（首次需要 `npx vercel login`，见 `tools/web-deploy.sh`）。详见 [CardenzaWeb 说明](Devices/CardenzaWeb/README.md)。

## Windows 模拟器与测试

需要 CMake、Visual Studio C++ 工具链和仓库子模块。在仓库根目录执行：

```powershell
git submodule update --init --recursive
cmake -S . -B build/CardenzaSim -DDEVICE=CardenzaSim
cmake --build build/CardenzaSim --config Debug
ctest --test-dir build/CardenzaSim -C Debug --output-on-failure
cmake --build build/CardenzaSim --target ValidateArchitecture --config Debug
```

程序位于 `build/CardenzaSim/Devices/CardenzaSim/Debug/CardenzaSimDesktop.exe`。模拟器的 SD 卡是程序旁的 `sdcard/`，目录结构见[存储协议](docs/app-storage-protocol.md)（`sdcard/Cardenza/...`）；没有卡时的设备内部存储是旁边的 `internal/`。脚本截图参数见 [模拟器说明](Devices/CardenzaSim/README.md)。

真实音源回归独立构建：

```powershell
cmake -S Library/Synth/tests -B build/synth-regression -G "Visual Studio 17 2022" -A x64
cmake --build build/synth-regression --config Debug
ctest --test-dir build/synth-regression -C Debug --output-on-failure
```

## 固件与当前边界

固件通过 ESP-IDF 5.3.5 构建，入口和烧录步骤见 [Cardenza 说明](Devices/Cardenza/README.md)。开发板连在远端 Mac 上时，`make DEVICE=Cardenza remote-flash` 会编译、推送、烧录并回传启动日志（见 `tools/remote-flash/`）。

- [Groovebox 设计](docs/groovebox-design.md)：当前交互、视觉与实现说明。
- [应用存储协议](docs/app-storage-protocol.md)：SD 卡目录、设置文件格式、应用生命周期。
- [Learn 说明书](https://cardenza-synth.vercel.app/learn/)：完整的双语使用说明，带截图、按键灯、地图导航和同义词搜索（`make learn` 从 `docs/learn` 生成，见 `tools/learn/build.py`）。
- 旧版界面的清单与改版记录（`docs/ui-inventory.*`、`docs/ui-redesign*`、`docs/sequencer.md`）只作历史参考。旧界面代码仍在仓库中，`-DCARDENZA_GROOVE_UI=OFF` 可以构建。

实机按键手感、屏幕实际观感、扬声器音量曲线、麦克风录音电平仍需在板子前确认。外部 MIDI / USB 同步尚未实现。两分钟 Electro House 示例的生成工具见 [Demo 说明](tools/demo/README.md)。
