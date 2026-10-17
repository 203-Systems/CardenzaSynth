# CardenzaSynth

Cardenza 的四轨合成器固件与 Windows 桌面模拟器。当前包括 DX7、Juno、Sampler、Drums，步进序列器，以及工程和用户音色保存／恢复。显示画布为 240×135。

## 常用操作

- Home 左侧四键短按切换音轨，按住进入 TRK / SND / KEY / SYS 菜单；在按住期间选择项目可固定菜单。Home 右侧为八度、Stop、Play。
- Home 默认显示演奏键盘；**同时按住 + / −** 切换演奏与常驻 **Sequencer 4×4 网格**。在 Home 的步进编辑期间使用此组合也会结束编辑并切换视图，不停止序列播放、不改八度或键盘配置。网格显示 16 步的事件、选中与播放位置。实体 HOME 在普通 Home 也可切换；按住 step、Pattern / shift 层或 Context 菜单中按 HOME，只取消/返回并保留所选视图。
- TRK → ENGINE 选择音源，PRESET 浏览音色；SAVE / SAVEAS 保存用户音色。录音或未保存草稿会阻止切换音源。
- Drums 提供 8 音 808 鼓组。演奏区每行的八列依次为底鼓、军鼓、闭镲、开镲、拍手、通鼓、牛铃、沙锤；四行重复相同鼓种，不受八度、根音或键盘模式影响。SND 可调各鼓音量和总音量，支持保存音色。
- Sampler 的 SND → FILE 直接浏览 WAV，输入即搜索。选择文件后按 LOAD；按 **OPT**（矩阵零基 `(3,1)`）进入该文件的 REN / DEL。BACK 返回原搜索位置，删除需要 YES 确认。文件操作不会先加载目标采样。
- SND → REC → START 录音；停止后裁剪并保存。已有未保存录音时，重新录音会先显示替换警告，确认后才替换。
- SYS → PROJ 提供 SAVE / AS... / LOAD / NEW。Load 和 New 需要确认；准备失败保留当前会话，未保存录音会阻止切换。
- SYS → SEQ 提供播放、停止、继续，以及独立的 BPM、TRK、PATT 页面；左下角 **GRID / KEYS** 切换网格/演奏视图并直接返回 Home。TRK 显示四轨概览，T− / T+ 切轨后可静音或禁用序列；PATT 的 CNT− 删除含音符的末尾 Pattern 前会要求确认。
- Home 按住步进键显示音名和编辑范围；网格视图在编辑时保留 4×4 网格及紧凑详情。LEN / VEL / STEP 在松开对应左键后继续保持，直到全部步进键松开；多步数值不一致时显示 MIXED。两种视图均使用物理第 1–8 列演奏、第 9–12 列编辑步进；Piano 布局的空键位置也可选择数值档位。详细手势见 [Sequencer 说明](docs/sequencer.md)。
- 单独短按 + 或 − 调整旋律键盘八度，单独按住其中一键显示 Pattern 选择。Pattern 只在 OCT 或 CLR / COPY / QNT 仍按住时显示；按住操作键后可先松开 OCT，松开全部相关键即返回所选 Home 视图。QNT 先选好 step 时立即量化选步；否则按住 QNT 后点 step / 已有 Pattern 才量化指定目标，单按 QNT 不改数据。CLR 删除有内容的 Pattern 会显示目标编号并要求 YES，BACK 取消；仅剩一个 Pattern 时确认后清空音符并保留该槽。

采样为单声道 PCM16 WAV，支持 8–96 kHz，上限 220,500 帧；实际可用容量受设备内存限制。工程保存采样文件名引用，不包含音频数据。移动工程时一起复制 Sample 目录。改名／删除文件可能使以前保存的工程引用失效，界面会提示；其他轨道正在引用的文件不能删除或覆盖，改名会同步本次会话的各轨引用。

Drums 复用 AMY 内置样本，每轨 6 复音。各鼓音量作用于后续触发，总音量实时作用于整轨；松键保留鼓声尾音，Stop、换预设或引擎会停音，闭镲会截断开镲。Project v1 保留原有引擎编号并新增 Drums=4：新版可以载入旧工程，旧软件不支持含 Drums 的工程。

Home 网格属于当前 UI 会话的视图选择，不改变 Project v1 文件格式。当前仍为每轨 Pattern 编辑和播放，未加入 Song / Chain 编排。

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

固件通过 ESP-IDF 构建，入口和烧录步骤见 [Cardenza 说明](Devices/Cardenza/README.md)。当前验证环境为 ESP-IDF 5.3.4；构建通过不代表实机录音、四轨负载、内存峰值或掉电恢复已经验收。

- [可筛选 UI 清单](docs/ui-inventory.html)：页面／状态清单、实际模拟器截图，以及 NullPerator 设计参考。新增页面的运行截图以该清单实际收录内容为准。
- [实现与验收记录](docs/implementation-2026-09-22.md)：已完成工作、测试命令和剩余边界。
- [原始审查](docs/review-2026-09-22.md)：修改前问题快照，以实现记录为当前状态。

本轮完成的是 Sequencer 和 Drums 的定向可用性升级，完整视觉设计语言仍待更新。外部 MIDI / USB 同步、全局效果的按轨实现尚未完成；实机录音、heap 峰值和音频处理截止时间仍需独立验收，不以模拟器测试代替。
