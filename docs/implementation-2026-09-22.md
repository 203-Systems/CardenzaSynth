# ComputeCardOS 实现与验收记录 — 2026-09-22

这份记录对应本轮补齐后的代码；`review-2026-09-22.md` 保留修改前的问题证据，不能再作为当前状态清单。代码仍在工作区，未提交或烧录设备。

## 已补齐

- DX7 / Juno 工厂参数从仓库内 AMY 数据解码，编辑值同步到实际音源。移除当前 AMY 无法按轨实现的参数入口。DX7 的四个简化包络编辑值按 Rate 1 / Rate 2 / Level 3 / Rate 4 命名。
- 新增 Drums 引擎（编号 4），复用 AMY 已内置的 808 PCM 音源；单轨 6 复音、6 个振荡器。八鼓按 Kick / Snare / Closed Hat / Open Hat / Clap / Tom / Cowbell / Maraca 对应 GM 36 / 38 / 42 / 46 / 39 / 43 / 56 / 70；只开放这八个音符。Home 每行八列重复同一映射，不随键盘音阶或八度改变。
- Drums 的八个鼓音量和 `master.level` 均为 0–127、默认 100。鼓音量作用于后续触发，master 实时调整整轨；普通松键保留一次性鼓声尾音，显式 Stop、换预设或换引擎会停止，闭镲截断开镲。工厂音色为 `drums.808` / `808 Kit`，用户音色保存在 `Preset/Drums`。
- 音频公共入口拒绝 MIDI note 128–255，防止访问 128 项音符登记数组时越界。
- 用户 Preset 保存基础工厂音色 ID、参数和名字。版本化 `.ccpreset` 文件使用临时文件与备份；写入失败不显示成功，不更新内存中的当前音色身份。
- Home 右侧 Stop / Play 已接线。SYS → SEQ 增加 Resume、30–300 BPM、音轨启用/静音、Pattern 选择与数量调整。播放事件直接送到同步音频运行时，避免 UI 帧队列和轨道对象替换造成的时序问题。
- Sequencer 的 MIDI 批次与 Stop 串行化；音符释放表满时拒绝无法跟踪的 NoteOn；ESP 时钟使用 timer 唤醒任务。
- Sequencer 的 Home 选步区显示音名、当前音符范围与数值；多步或和弦所选范围数值不同显示 MIXED。LEN / VEL / STEP 松开左键后保持所选 lane，全部步进键松开后恢复 NOTE；Piano 的空键位置也能选择数值档位，不会误插入音符。
- SYS → SEQ 的 Transport、BPM、TRK、PATT 有独立信息呈现。TRK 分别展示四轨输出 OUT/MUTE 与序列启用 ON/OFF，T− / T+ 改变当前轨；PATT 展示当前 Pattern、含音符步数和槽位，CNT− 删除含音符的末尾 Pattern 前要求确认，取消不改序列。缩减数量会清空移除槽位，重新增加不会恢复已删数据。
- Pattern 锁定后左栏保持 CLR / COPY / QNT，可直接执行而不误切轨。COPY 区分 Sxx 步进源与 Pxx Pattern 源；两位数 Pattern 10–16 已在真实屏幕检查。Home 保留物理步进网格，只显示编辑信息、键盘／鼓垫与播放状态，不重复画 4×4 步进格。
- 修复复制步到 Pattern 末尾时长度越界、删除当前 Pattern 之前的槽位后选中内容跳错，以及切轨遗留 held step／试听状态的问题。
- Sampler 空槽静音、One Shot 不回卷；导入检查 PCM16 单声道、采样率、RIFF/chunk 长度、奇数字节和容量。当前上限为 220,500 帧、8–96 kHz；并非通用 WAV 解码器。
- PCM 采用不可变共享缓冲。录音输入独立任务持续读取 DMA，用环形缓冲与溢出反馈交给 UI。内存不足时录音长度可缩短，录音时长以实际容量为准。
- 采样保存保留备份；裁剪保存后重置相对 trim，取消录音恢复原采样元数据；保留文件名真实大小写。
- Project 保存、另存、文件列表、命名和覆盖确认已连接真实文件操作；保存包含四轨引擎、基础音色与参数、采样文件引用、键盘设置、LED 亮度、BPM、Pattern 和音符。存在未保存录音时拒绝保存，提示先保存采样。
- Project 解析器有版本、字段长度、计数、音符、文件名与尾部校验；坏文件不替换输出对象；`.bak` 可以列出并读取。
- Project 加载 / 新建已接通：通过界面 YES 确认后执行。有录音正在进行或未保存录音时拒绝切换；先校验工程、恢复待用引擎并读取采样，准备失败保留原会话。成功后停止演奏、转移现有 MIDI 绑定并恢复全部工程设置；新建恢复默认四轨和空序列。
- 修复 Juno 工厂音色内嵌 EQ 修改全局输出的问题：加载音色后恢复平直全局 EQ，避免其他轨道音色及工程恢复顺序改变声音。
- 修复录音状态刷新吞掉菜单按键的问题，清理旧按键只在开始录音的状态跃迁执行。工程反馈和多行确认根据可用空间调整字号，避免只显示截断的提示。
- FILE 浏览器按 OPT 打开选中文件的 REN / DEL。直接管理目标文件，不加载它；取消和返回保留原搜索，删除固定目标并要求 YES 确认，改名失败保留输入及播放状态。空槽与已加载状态使用同一流程。
- 采样改名同步本次会话的共享引用；其他轨仍在使用的文件拒绝删除或覆盖。目录碰撞、文件操作失败和录音／草稿保护均有回归覆盖。已保存工程的引用不会自动迁移，操作页明确提示影响。
- 引擎切换先准备新引擎，再转移已有 MIDI 绑定；录音及草稿阻止切换。修复清空 Sampler 缓冲误清 AMY 音符登记的问题，避免新引擎无法收到正确停音。
- 重新录音覆盖草稿需要显式确认；后端默认拒绝覆盖，只有新缓冲和输入准备成功后才替换旧草稿。
- 固件 UI 任务栈改为实际 16 KB；原代码错误地将 ESP-IDF 的字节参数除以 `sizeof(StackType_t)`。架构检查改为验证真正禁止的 HAL fallback 条件，不再要求注释中出现固定标记。

## 剩余范围

- 完整视觉设计语言尚未实施；本轮是 FILE、Sequencer 和 Drums 的定向可用性升级。UI 清单现有 73 个页面／状态，其中 24 个包含真实模拟器截图；其余结构示意仍不能当成实机或模拟器验收。
- Juno Chorus / HPF、DX7 LFO Delay / operator velocity sensitivity 和 Sampler Pan 暂不暴露。AMY 中相关能力缺失或作用域是全局，不能当作四轨独立能力承诺。
- 外部 MIDI / USB 同步、固件更新、校准等 SYS 占位入口已移除；没有以状态文字冒充实现。
- 未做硬件录音、四轨最大负载、任务栈水位、音频欠载、SD 写入时微步抖动、低内存极限或突然断电测试。当前固件未启用 PSRAM；多采样工程峰值内存仍是重要限制。
- 临时文件/备份保护不是 FAT 掉电原子性证明。大块 PCM 使用可检查的分配，但关闭 C++ 异常后的普通 STL 小分配仍存在系统级 OOM 风险。

## 验证

在 Windows / MSVC 与本机 ESP-IDF 5.3.4、Xtensa GCC 13.2.0 上验证：

```powershell
cmake -S . -B build/ComputeCardSim -DDEVICE=ComputeCardSim
cmake --build build/ComputeCardSim --target ComputeCardSimDesktop ComputeCardCoreTests ComputeCardSamplerTests ComputeCardUiTests ComputeCardProjectTests ValidateArchitecture --config Debug
ctest --test-dir build/ComputeCardSim -C Debug --output-on-failure

cmake -S Library/Synth/tests -B build/synth-regression -G "Visual Studio 17 2022" -A x64
cmake --build build/synth-regression --config Debug
ctest --test-dir build/synth-regression -C Debug --output-on-failure

# 先进入本机 ESP-IDF 环境，再在 Devices/ComputeCard 下执行：
idf.py -B ../../build/ComputeCard build
```

- Core：Project round trip、备份恢复、截断/畸形输入保持旧数据、Windows 保留名、Stop 与 MIDI 批次的确定性并发测试。
- Sampler：坏 WAV、奇数填充块、data-before-fmt、覆盖保护、大小写文件名、输入积压、取消/空输入恢复、裁剪重置、溢出停止。
- Sampler 文件操作：共享引用改名、删除与覆盖保护、独立浏览目标、固定目标两次确认、取消、碰撞/目录拒绝、失败不改 PCM、未保存草稿及显式替换保护。
- UI：真实按键路由、Project 确认和取消、失败反馈、键盘恢复和菜单可达性。
- Project 应用集成：使用真实 App / Rack / 引擎 / MIDI 路由 / UI / 序列化，验证完整保存和恢复（含采样裁剪）、取消不改状态、坏工程和缺失采样保留旧对象、新建恢复默认，以及录音中和未保存草稿分别阻止 Load / New。仅设备 IO、像素和音频输出使用替身。
- 同一应用集成测试进一步覆盖 ENGINE 切换保护，播放 A 时浏览并改名／删除 B 不改变 A，取消和失败返回、搜索保留，以及草稿重录的确认／取消。
- Synth：256 个工厂 Preset 的恢复/渲染，DX7 降低全部 Operator 输出后实际 PCM 降幅，Juno VCA 静音，One Shot / Loop，磁盘失败、用户音色重建和 MIDI 绑定转移；真实 AMY 测试覆盖 Juno 跨轨 EQ 与加载顺序。
- 本轮独立 SynthRegression 已通过：八鼓 PCM 均非零且互不相同、每鼓音量衰减与零值、master 静音、松键尾音、强制 Stop、闭镲截断开镲、用户预设重建、工厂参数复原与非法音符边界。12 轮全 Drums / Juno / DX7 / Sampler 及混合切换还检查实际振荡器范围、归属和不重叠；这是主机证据，不代表 ESP32 音频截止时间或 heap 已验收。
- 本轮 Sequencer 和 App 回归通过：持久 lane、多步 MIXED、末步复制长度、Pattern 删除选中和槽位清空、Piano 空键档位、锁定后 COPY/QNT/CLR 及保存结果，以及 Drums 固定映射、九参数菜单调整、预设和 Project v1 往返。
- `tests/ui-preview.script` 在隐藏的实际桌面模拟器内生成六张 240×135 截图，检查 Home、SYS、Project、命名、Transport 与 Tempo。UI 清单中其余画板为源码结构示意，不能当成全部页面的运行截图。
- 实际按键另存 `Project201711.ccproj` 成功，生成 2,920 字节 v1 文件，含四轨实际工厂身份与参数，截图显示 `SAVED`；测试文件位于模拟器构建目录的独立 `sdcard` 下。
- `tests/project-preview.script` 通过实际模拟器按键检查 Project 浏览、Load 确认与成功反馈、New 确认与成功反馈，截图保存在 `tests/build/project-preview/`。
- `tests/sampler-file-preview.script` 用专门生成的 `qafile.wav` 测试采样检查搜索、OPT、改名成功、删除确认与取消；截图位于 `tests/build/sampler-file-preview/`，其中四张保留在 UI 清单。
- `tests/sequencer-preview.script` 和 `tests/drums-preview.script` 通过实际模拟器键盘输入生成 16 张 Sequencer 与 9 张 Drums 画面，涵盖 NOTE/LEN/VEL/STEP、Scope、COPY、16 Pattern 边界、SYS 子页、删除确认、鼓垫高亮、参数分页和固定 KEY。逐图检查后修正截断、重叠与菜单状态；UI 清单合并旧有系统／Sampler 截图后共 24 张，均为 240×135。
- 最终结果：模拟器编译、架构检查通过；Core / Sampler / UI / Project 四组 CTest 全部通过（13.10 秒）；独立真实 AMY SynthRegression 通过（0.60 秒），共五组。HTML 数据及交互逻辑检查通过：73 唯一节点、9 个家族、24 张合法尺寸截图、源码路径、筛选／搜索／详情／缩放。
- 最终固件编译通过，镜像为 `0x1864c0` 字节，4 MB App 分区余量 62%；Flash 分区余量不代表运行时 heap 余量。未烧录或进行实机验收。

`git diff --check` 仅保留原工作区 `AGENT.md` 与 `docs/AGENT.md` 的 EOF 空行提示；没有改动这些既有用户编辑来消除提示。

## 2026-09-23：量化目标与 Pattern 删除保护

- 修复 QNT 提前量化整段：无 held steps 时只进入目标选择，单按／释放不改数据；按住 QNT 后点 step 仅量化该步，点已有 Pattern 仅量化指定 Pattern，不切换或创建槽位。先选 step 再按 QNT 的快捷操作保留。
- Home 的 CLR 删除有音符 Pattern 复用确认页，固定轨道与目标编号；BACK、HOME 或换轨取消，YES 才执行。Home 来源返回 Home，SYS 来源返回 PATT。空 Pattern 仍可直接删除；唯一有内容 Pattern 确认后清空并保留一槽。进入确认时清理 Home 按住状态和试听，避免松键误触。
- 回归以真实 App 输入和工程保存回读核对未选中 step、Pattern 与轨道保持原样，并覆盖空目标、非末尾删除、唯一 Pattern、确认前按键释放及取消。四组 Core / Sampler / UI / Project 测试全部通过，15.71 秒；独立静态 review 未发现新的具体问题。
- 模拟器与架构检查通过；ESP-IDF 固件编译通过，镜像 `0x1868d0` 字节，4 MB App 分区剩余 62%。未烧录。
- `tests/sequencer-safety-preview.script` 生成并逐张核对 QNT 等待、Home 删除 P01、取消与删除后返回的四张实际画面；UI 清单 H07 / Y14 已替换为对应截图，仍为 73 个节点、24 张真实截图。

## 2026-09-23：独立 Sequencer Home

- 新增可常驻的 4×4 Sequencer 主屏，替换中心键盘图形；16 步按实体步进区的行列顺序排列，区分有事件、按住选中与当前播放步。NOTE / LEN / VEL / STEP 编辑时网格保留，底部显示紧凑音符或参数详情。
- 无临时编辑的 Home 按实体 HOME 可在演奏键盘与网格之间切换；SYS → SEQ 新增左下 GRID / KEYS 入口。处于菜单、按住步进编辑或 Pattern 操作层时 HOME 只退出操作，保持视图。切换不停止播放、不改变音符或键盘映射，跨菜单与换轨保留；视图为会话状态，未改变 Project v1。
- 模拟器和架构检查通过；四组 CTest 通过（17.48 秒）。真实 App 测试验证播放中的切换、轨道与键盘设置保留、选步编辑、HOME 取消和工程回读；独立静态 review 未发现具体问题。
- `tests/sequencer-grid-preview.script` 生成并检查九张真实模拟器画面，覆盖 GRID 入口、常驻网格、多步选择、力度、微步、播放光标、空轨与切回键盘。ESP-IDF 编译通过，固件镜像 `0x187650` 字节，4 MB App 分区剩余 62%；未烧录。
- UI 清单新增 H08 / H09，共 75 个页面、26 张实际截图；顶部入口及 `#sequencer` 集中展示 14 个 Sequencer 状态，默认选择新主屏。Node 检查截图数据、页面与源码链接、入口、搜索、空结果和详情逻辑通过；实际主屏画面依据原生模拟器截图验收。
- 小屏信息复审：删除网格的常驻 HOME 提示、空步教学语、SEQ 标题、分割线和重复 scope；轨道缩为 T1，Pattern 移至顶行，格子高度 16→18 px。保留步号、事件／选中／播放标记及传输状态、BPM；编辑详情只在选步时出现，格子几何位置不随编辑改变。新版模拟器、架构与固件编译通过，现有 UI 回归通过（0.71 秒），并核对空闲、播放、力度与空步截图；H08 / H09 已更新。当前固件 `0x1875c0` 字节，未烧录。

## 2026-09-23：双键切换 Home 视图

- 按用户选择，`+` 与 `−` 同按改为切换 Piano / Sequencer，移除旧 Pattern 锁定。单键短按仍调整八度；按住单个 OCT 进入临时 Pattern 层，OCT 与相关动作键全部松开后退出。HOME 与 SYS → SEQ 的 GRID / KEYS 入口保留。
- 组合键持续消费到两键都松开，覆盖不同按下／松开顺序、重复按下、仅重按一键、进入菜单后的松键和 HOME 取消，避免重复切换或误调八度。切换保持播放、音符、轨道及键盘配置。
- 模拟器、架构检查与 ESP-IDF 固件编译通过；Core / Sampler / UI / Project 四组 CTest 全部通过（17.67 秒）。三个原生模拟器按键回放脚本均成功，检查了两键保持按下时显示网格、播放中切回 Piano，以及 Pattern / QNT 的松键退出提示。UI 清单六张截图已同步。固件镜像 `0x1875d0` 字节，4 MB App 分区剩余 62%；未烧录。

## Project 文件格式 v1

文件位于 `ComputeCardOS/Project/<name>.ccproj`。名字为 1–24 个 ASCII 字母、数字、空格、下划线或短横线，不允许首尾空格、路径穿越或 Windows 设备保留名。

文本格式以 `CCOS_PROJECT 1` 和 `BUILD ComputeCardOS 1` 开头，依次写 `UI`、四组 `TRACK` / `PARAM`、`SEQ` 和四组 `T` / `NOTES`，最后为 `END`。Preset 参数通过带引号字符串保存；音符使用稀疏记录，含 Pattern、Step、Pitch、Velocity、Length 和 Microstep。采样保存相对文件名，不打包 PCM；搬移工程时应一并复制所引用的 Sample 文件。

引擎编号保留 DX7=1、Juno=2、Sampler=3，并追加 Drums=4；文本结构和已有字段语义保持 v1。新版继续读取旧工程，未知引擎编号仍被拒绝，Drums 不能携带 Sampler 文件引用。旧版软件不支持编号 4，因此不能读取含 Drums 的新工程。Drums 的九个参数和基础预设身份随工程保存，内置鼓样本不需要额外文件。

未来变更字段语义时应递增版本并显式迁移，不能默默把未知版本当 v1 读取。
