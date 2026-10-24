# CardenzaSynth

Cardenza 是一台四轨 groovebox：DX7、Juno、Sampler、Drums 四种音源，16 步音序器，整首歌的 Pattern 编排（走带），四轨混音，每轨效果（失真、降采样、DJ 滤波、合唱）与总线延迟 / 混响，工程与音色存储。屏幕 240×135，键盘 4×14 并带单色按键灯。

界面是黑底的终端风格，搭配新极简排版和 dither 填充。屏幕左右两边各有 4 个键帽，和键盘最左、最右两列逐行对应：左侧不写字，用颜色区分（ESC 红橙、TAB 青、FN 黄、CTRL 紫），键帽里的小点表示这个键管的对象（场景、轨道、录音、页），当前项是白点；右侧是 + − ↵ ▶。按键灯有一套语言：亮度表示数值大小，呼吸表示按下会执行动作，细闪表示焦点，闪烁表示等待中；当前画面在用的那一块键最亮。界面和按键灯都带动效，逐帧录像（含键盘灯光）见 [docs/groovebox/motion](docs/groovebox/motion)。完整规格见 [Groovebox 设计](docs/groovebox-design.md)。

## 按键

按住左侧任一键时，屏幕会变成一张和右侧 4×4（GRID）一一对应的地图，直接按 GRID 选择。

| 键 | 单击 | 按住 |
|---|---|---|
| ESC | 返回上一层 / 回到上一个场景 | 场景地图：GRID 三列 = 乐器 / 音序器 / 走带 |
| TAB | 下一轨 | 轨道地图：GRID 每列一轨，行 0 选轨、行 1 静音、行 2 独奏、行 3 音序开关；TAB + ADD / SUB 前后切换 |
| FN | — | GRID 上是功能地图（速度、八度、音色、Pattern、方向键等），PADS 上是字母快捷键 |
| CTRL | 下一页 | 页面地图：当前场景的所有页面排在 GRID 上，按哪格跳哪页；CTRL + ADD / SUB 翻页；Fn + CTRL 上一页 |
| ADD / SUB（右侧上两键） | 加 / 减，列表移动 | 连发并加速；Fn + ADD / SUB 粗调 |
| ENTER | 确认、进入、开关 | Fn + ENTER：恢复默认 / 和弦编辑对象 / SONG·LOOP / 文件选项 |
| PLAY | 播放 / 停止 | Fn + PLAY：实时录音开关 |
| HOME 侧键 | 全部停止 | — |

中间 4×8 是演奏键（PADS），任何场景都演奏当前轨；右侧 4×4（GRID）和屏幕中央的 4×4 格子一一对应。文本输入时，最上面那个右侧键（键帽印 del）是删除。

Fn + GRID：`9 0` 速度 −/+，`- =` 八度，`O P` 上 / 下一个音色，`[ ]` 上 / 下一个 Pattern，`K` 键盘布局，`L` SONG / LOOP，`M` 静音当前轨，`; , . /` 方向键。Fn + PADS：`1–4` 静音轨道，`C V` 复制粘贴，`X` 清除，`Q` 量化，`D` 复制 Pattern（走带页为复制整行），`T` Tap tempo，`S` 保存。

## 先做一个循环

1. 开机在 **2 SEQ**。T1 是 808 鼓组，每一列演奏键是一种鼓。
2. 按住一个演奏键，再点 GRID 上的几个 step：鼓点直接写入。
3. 点一下 step 会选中它（可多选），演奏区会亮出这一步里的音。选中后按演奏键加 / 删音符；ENTER 在 NOTE / LEN / VEL / TIME 之间切换，ADD / SUB 调整；ESC 取消选择。
4. PLAY 播放，演奏区会跟着音序器正在弹的音闪烁。按 TAB 换到 T2 Juno，在 STEPS 页写一段旋律。
5. CTRL 到 PATTERN 页：点空槽新建 Pattern；播放中点别的 Pattern 会在本轮循环结束时切换。Fn + D 把当前 Pattern 复制一份。
6. Fn + PLAY 打开录音，播放中直接弹演奏键即叠录进当前 Pattern。
7. 按住 ESC 再按 GRID 第三列进入走带：点 GRID 第一列的 + 追加编排行，选中一格用 ADD / SUB 选 Pattern（`--` 为静音），Fn + ADD / SUB 改小节数，ENTER 从这一行开始播放整首歌。
8. CTRL 翻到 MIX 调音量、静音、独奏；SENDS 页给每轨开延迟 / 混响发送，FX 页调总线；按住 CTRL 在地图里点 PROJECT，SAVE AS 输入名字保存。

## 场景

- **1 INSTR 乐器**：VOICE 页大字显示音色，ADD / SUB 直接切换音色；ENGINE、PRESET（可输入搜索）、SAVE、SAVE AS。之后是引擎参数页：GRID 选参数，ADD / SUB 调值，空格位置显示包络、滤波、算法、波形等图形。DX7 共 12 页（算法、算子电平、六个算子的音高与 R1–R4 / L1–L4 包络、LFO），Juno 7 页（DCO、VCF、ENV、LFO）。每个音源都有 FX 页（DRIVE、CRUSH、FILTER、RESO、CHORUS、DELAY、REVERB，附信号链图）。KEYS 页设置布局、音阶、根音、八度。Sampler 的 SAMPLE 页可浏览 WAV、录音、裁剪和保存。
- **2 SEQ 音序器**：STEPS、PATTERN、TRACK（序列开关、每轨步数、Swing、BPM）。
- **3 TAPE 走带**：TAPE（整首歌的 Pattern 编排，SONG / LOOP 模式）、MIX（四轨推子、MUTE / SOLO / SEQ、主音量）、SENDS（四轨的滤波、合唱、延迟与混响发送一屏总览）、FX（总线：按速度同步的磁带延迟、混响、EQ、压缩、TAPE 饱和）、PROJECT（保存 / 另存 / 载入 / 新建、BPM、Swing、Tap）、FILES（工程列表）、SYSTEM（按键灯、屏幕亮度、主音量、存储与内存）。

采样为单声道 PCM16 WAV，支持 8–96 kHz。工程文件为 v4（含编排与总线效果），可以读取 v1–v3 工程。延迟和混响的缓冲在第一次打开发送时才分配（板子没有 PSRAM）。

## Linux 截图与测试

Linux 上没有窗口版模拟器，但可以无窗口运行并把画面导出为 PNG：

```bash
cmake -S . -B build/linux -G Ninja -DDEVICE=CardenzaSim
cmake --build build/linux
ctest --test-dir build/linux --output-on-failure
build/linux/Devices/CardenzaSim/CardenzaSimShot --script tests/groove/tour.script --out build/linux/shots --scale 2
```

`tests/groove/tour.script` 走查全部场景、浮层和主要编辑状态。

## Windows 模拟器与测试

需要 CMake、Visual Studio C++ 工具链和仓库子模块。在仓库根目录执行：

```powershell
git submodule update --init --recursive
cmake -S . -B build/CardenzaSim -DDEVICE=CardenzaSim
cmake --build build/CardenzaSim --config Debug
ctest --test-dir build/CardenzaSim -C Debug --output-on-failure
cmake --build build/CardenzaSim --target ValidateArchitecture --config Debug
```

程序位于 `build/CardenzaSim/Devices/CardenzaSim/Debug/CardenzaSimDesktop.exe`。模拟器数据位于程序旁的 `sdcard/CardenzaSynth/`，使用 `Project`、`Sample`、`Preset` 子目录。脚本截图参数见 [模拟器说明](Devices/CardenzaSim/README.md)。

真实音源回归独立构建：

```powershell
cmake -S Library/Synth/tests -B build/synth-regression -G "Visual Studio 17 2022" -A x64
cmake --build build/synth-regression --config Debug
ctest --test-dir build/synth-regression -C Debug --output-on-failure
```

## 固件与当前边界

固件通过 ESP-IDF 5.3.5 构建，入口和烧录步骤见 [Cardenza 说明](Devices/Cardenza/README.md)。开发板连在远端 Mac 上时，`make DEVICE=Cardenza remote-flash` 会编译、推送、烧录并回传启动日志（见 `tools/remote-flash/`）。

- [Groovebox 设计](docs/groovebox-design.md)：当前交互、视觉与实现说明。
- 旧版界面的清单与改版记录（`docs/ui-inventory.*`、`docs/ui-redesign*`、`docs/sequencer.md`）只作历史参考。旧界面代码仍在仓库中，`-DCARDENZA_GROOVE_UI=OFF` 可以构建。

实机按键手感、屏幕实际观感、扬声器音量曲线、麦克风录音电平仍需在板子前确认。外部 MIDI / USB 同步尚未实现。两分钟 Electro House 示例的生成工具见 [Demo 说明](tools/demo/README.md)。
