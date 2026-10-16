# ComputeCard UI · 第三版：标题栏写清在哪，按键旁写清是什么

[查看原生画面对照](ui-redesign.html) · [完整 UI 清单](ui-inventory.html#sequencer)

第三版全部落在 C++ 渲染器与 UI 快照上；键位、动作位置、工程格式与引擎均未改动。初稿（连续键条、1× 页头、主页 8×4 键阵、调整过的暗色）经过几轮 mock 评审后改为下面的定稿：v2 配色、NullPerator 式标题栏、四块描边按键、单八度小键盘。

## 改了什么，为什么

1. **标题栏（2026-10-13 定稿）。** 每屏顶部一条 24 px 标题栏，两侧按键和内容都在它下面。
   - 左侧是白色 2× 页面名，末尾编号与单词连写（`TRACK1`、`OPERATOR1`）。
   - 页面名右侧是两行 1× 小字：第一行青色，第二行灰色。
   - 额外信息放在右上角，例如八度、DX7 当前音色、`UNSAVED`。
   - 各页对应：主页是 `TRACK1` / 引擎 / 音色；按住 step 时是 `SEQUENCE` / `2 STEPS` / `VEL 88`（Piano 视图下方已有大字，所以灰色行留空）；Pattern 层是 `PATTERN` / `P16` / `16 IN USE`；DX7 总览是 `DX7` / `ALGORITHM 5` / `FEEDBACK 6`，右上角写音色；Operator 页在青色行写 `CARRIER` 或 `MODULATES OP2`；Sampler 把文件名和时长放进标题栏。
   - 确认页标题统一用白色；危险由红色后果文字和红色 YES 键提示。
2. **四块描边按键。** 侧栏每个键是独立的描边块（外边距 2 px、间隔 3 px），没有动作的键也保留淡轮廓，任何一页都能看出四个键。
   - 图标 9 px；右侧第三个键为圆形。它目前仍执行 STOP，录音模式尚未实现。
   - 状态分为：默认、锁定（暗青底青字）、按住（实填青色）、播放（绿）、禁用（暗字）、危险（红字）。v2 中锁定与按住都是整块青色。
   - 快照新增 `pressed` / `playing`，键 LED 亮度逻辑保持不变。
3. **v2 配色。** `kSurface (8,18,16)`、`kSelection (26,51,53)`、`kRule (40,52,50)`、暗字 `(128,145,140)` 沿用 v2；青色表示选择，绿色表示播放，红色表示破坏性操作或录音。
4. **参数值贴着按键。** DX7、Juno、Sampler FLT / ENV、Drums Kit、KEY、TRK、SYS、SEQ 的当前值排在对应按键行，连续参数附 2 px 电平条。图形只放在没有按键的行：Juno ENV / VCF / LFO 曲线、DX7 ALG 路由。
5. **主页。**
   - Piano：保留 v2 的单八度小键盘，但改为按实际音高点亮。v2 把“列号”当半音，按 C3 D#3 G3 会亮成 C D E。按下的音名用 2× 青色写在键盘下方。
   - Seq：空步只剩暗色编号，有音符的步亮底白字，和弦写成 `C3+2`；按住 step 时标题栏换成 `SEQUENCE`。
   - Loop Bar：当前 Pattern 为青色；播放时在青色底上显示绿色进度。
   - Drums：八鼓排在与演奏键相同的八列（BD SD CH OH CP TM CB MA），击打时下方写全名。
   - LEN 直接写实际长度 1 2 3 4 6 8 12 16；VEL 用递增的柱表示八档。
6. **一种字形与公共模板。**
   - 5×7 点阵没有下伸部，全部按大写字形绘制。
   - 浏览、搜索（搜索词作为标题栏青色行）、命名、确认、数值调整共用版式。
   - 删除 “Load from file or Record new”“8 DRUMS / MASTER LEVEL / Select a pad”“Project / Sequencer” 等占位或教学文字。

## 验证

- 模拟器（MSVC Debug）编译通过；架构检查（`ValidateArchitecture`）通过。
- Core / Sampler / UI / Project 四组回归全部通过。UI 测试新增三项检查：小写字形与大写逐像素一致；Piano 与音阶布局下的演奏键映射；按住与锁定状态分离（含 `TRACK 1` 标题与圆形图标）。
- 十份回放脚本共采集 92 张 240×135 原始 framebuffer。新增 `tests/ui-review.script`，覆盖 KEY、TRK、预设浏览、DX7 ALG / GLB / 第二页、Juno ENV / VCF / LFO / Range、Sampler 加载 / 裁剪 / 录音、16 个 Pattern 与按住帮助。
- 定稿前的 mock 在独立目录编译评审（已清理）。与评审通过的 mock 相比，35 张截图中 32 张逐像素一致；其余 3 张为播放进度时序，以及两处有意的波形位置调整。
- v2 对照截图来自本轮改动前的源码，在独立目录编译后用同一组脚本采集。截图保存在 `tests/build/ui-v2-reference-2026-10-11` 与 `tests/build/ui-v3-2026-10-13`。
- 固件（ESP-IDF 5.3.4，`build/ComputeCard-ui-redesign`）编译通过，镜像 `0x179ed0` 字节，4 MB App 分区剩余 63%。**未烧录。**

## 仍需实机确认

- 标题栏底色、按键描边、空键淡轮廓、空步编号这些暗色，在实际面板上的可辨性。
- 1× 青 / 灰两行小字与 2 px 电平条在实际阅读距离下是否清楚。
- 圆形键：录音模式实现前，它仍执行 STOP。
- 录音页的输入电平：模拟器没有麦克风输入，只验证了零电平与计时。
- 长音色名的滚动；16 个 Pattern 时 Loop Bar 每段只有 7–8 px 宽，是否看得清。

---

## 第二版记录（同日早些时候）

本轮已落到实际 C++ 渲染器。参考 2026-10-11 读取的 NullPerator UI2：

- [UiPalette.cpp](https://github.com/203-Systems/NullPerator/blob/main/sources/UI2/Theme/UiPalette.cpp)：深色背景与分区、青色选中、绿色播放、红色危险操作。为 240×135 屏幕适当提亮次级文字。
- [UiFont5x7.cpp](https://github.com/203-Systems/NullPerator/blob/main/sources/UI2/Text/UiFont5x7.cpp)：复用实际 5×7 字形，补齐可打印 ASCII 标点，保留 BSD 3-Clause 许可；整数像素放大，不增加运行时字体依赖。
- [UiInstrumentView.cpp](https://github.com/203-Systems/NullPerator/blob/main/sources/UI2/Views/Instrument/UiInstrumentView.cpp)：参考标题、分区与标签/值的对齐关系。

此前下载目录的 `picotracker-fx-sections.html` 当前内容已变为 203 Launcher UI sheets，本轮没有将它当作 NullPerator 音乐界面的唯一依据。

## 已应用

- 单列侧栏 46→40 px，双列 100→88 px；Home 中央可用区域加宽。保留全部物理按键位置和菜单动作。
- 侧栏改为低亮底色和细底线，选中项整块反白；无选中时降低边框噪声。
- Seq 保留 4×4 映射，格内显示最低音名，和弦另有 `+` 标记；空步、填入事件、按住选中和绿色播放线同时可辨。音符摘要只读当前 Pattern，不改变工程数据。
- Home、Sequencer 菜单与文件浏览使用紧凑标题区；Seq 菜单的 BPM、Pattern、轨道状态按字段对齐。常驻退出提示与重复教学文字移至文档。
- DX7 Operator 浏览页显示实际算法路由和当前 Operator，移除占位式的 “Parameters / Level, ratio…” 文案；进入参数后继续显示名称和值。
- 文件浏览器使用整行反白选中。保留搜索、OPT 文件动作、重命名及删除确认。
- 修复窄容器的截断问题：原实现至少留下三个字符再加省略号，可能越出新侧栏。现在连省略号自身也受可用宽度约束。

## Loop Bar 调整

- Piano 与 Seq 主页移除 P01/1、STOP / PLAY 与 BPM 文字行。
- 两个视图共用底部 4 px Loop Bar：按独立 Pattern 数量分段，1 个显示 1 格、4 个显示 4 格；当前 Pattern 高亮，格内显示播放进度。
- 通过 `tests/loop-bar-preview.script` 检查 1 / 4 / 16 格与播放状态，另外回放五份相关原生脚本。模拟器和固件均重新编译通过，固件镜像为 `0x179cc0` 字节；未烧录。
- 这些格代表独立 Pattern；播放仍循环当前 Pattern，不表示自动串行播放。

## 操作不变

`+` 与 `−` 同按切换 Piano / Seq；单独短按调整八度；按住单个进入临时 Pattern 层。原有 HOME / SYS → SEQ → GRID、四轨、音源、播放、工程格式均保留。未加入 Song / Chain 或新 Synth engine。

## 验证

- 模拟器与架构检查通过。
- Core / Sampler / UI / Project 四组回归通过，17.68 秒。新增检查覆盖最低音/和弦摘要、空步、换轨、复制后摘要及多字号窄侧栏文字宽度。
- 八份原生按键脚本成功运行，包含 `tests/ui-design-preview.script`；文件相关回放使用独立模拟器目录与复制的测试夹具。已检查 Seq 空闲/编辑/播放、Pattern、Piano、DX7、Juno、Sampler 浏览/重命名、工程确认、Drums 等画面。
- 固件编译通过，镜像 `0x17a130` 字节，4 MB App 分区剩余 63%。构建目录为 `build/ComputeCard-ui-redesign`；旧缓存引用已迁移的 ESP-IDF 路径，因此使用独立目录，保留旧产物。
- UI 清单的 32 张截图与对照页来自模拟器 240×135 原始 framebuffer；其余清单画板仍明确标为结构示意。

未烧录。实机阅读距离、屏幕亮度和按键手感仍待设备验收；这版用于确认视觉方向，不代表 75 个状态都完成了逐页实机验收。
