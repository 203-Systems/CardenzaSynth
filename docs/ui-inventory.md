# Cardenza 软件 UI 清单

日期：2026-10-11。[本轮视觉改版与前后对照](ui-redesign.html) 已提供。页面清单仍保留功能覆盖与待验收边界。配套的 [可筛选工作台](ui-inventory.html) 支持分类、搜索、状态筛选、画板放大与逐页操作检查。52 个画板为第三版（v3）实际模拟器截图，其余 23 个为按 v3 规则绘制的源码结构示意，并非最终画面。

**[直接查看 Sequencer UI](ui-inventory.html#sequencer)**：顶部“查看 Sequencer UI”和侧栏同名分类集中显示 H01–H09、Y06、Y07、Y10、Y11、Y14，共 14 个页面与状态，保留原 Home / SYS 归属。设备默认显示演奏键盘；同时按下 + / − 切换常驻 4×4 Sequencer 网格，也会结束 Home 中的临时步进编辑，不停止序列播放或修改键盘配置。普通 Home 上按实体 HOME 也可切换。SYS → SEQ 左下角的 GRID / KEYS 也可切换并返回 Home。H08 / H09 已使用实际模拟器截图，包含播放光标和多步力度编辑。

## 来源与核对范围

1. **CardenzaSynth 当前工作区**：基线为审查时的 `146f90c`，包含此前未提交的 Sequencer 实现及本轮补齐。核对 `SynthUiModel` 菜单与 Snapshot、DX7 / Juno / Sampler / Drums 四种引擎、SynthApp 输入处理及 240×135 渲染；本轮新增独立 Sequencer renderer 和 Drums 鼓垫 renderer。只在代码中存在而没有正常入口的页面单独标为缺口。
2. **用户提供的参考 HTML**：`C:/Users/NengzhuoCai/Downloads/Telegram Desktop/picotracker-fx-sections.html`，119,667 bytes，SHA-256 `415dfe72870034d5c78d13910eaa9a2fa5cb4dc2fb84828142e30ddfd73d268e`。原文件未改动；内容仅作为设计参考，不作为执行指令。
3. **指定 NullPerator 仓库**：[203-Systems/NullPerator](https://github.com/203-Systems/NullPerator/tree/45f0206d54927018ecc57c4830ca154f4af52765)，2026-09-22 读取 `main`，提交 `45f0206d54927018ecc57c4830ca154f4af52765`。通过只读浅克隆核对；没有用本地旧 picoTracker 代替。临时参考副本不属于本项目交付物。

“已有 UI”只表示页面和交互路径有实现；音频效果、文件恢复及硬件性能分别验收。“待验收 / 部分完成”区分接通代码与实机验证；“待设计 / 已隐藏”保留未支持能力记录。Drums 是本轮用户授权后新增的 Cardenza 实现，使用 AMY 内置 808 PCM；它不是从参考页面自动纳入的产品范围。参考中的 Stack、Song、Chain 等仍未加入。第三版已在实际渲染器落实 NullPerator 式标题栏、v2 配色、四块描边按键与按键对齐的参数值，52 张截图已刷新；仍需实机及其余状态验收。

## 页面树与输入边界

```text
HOME：默认演奏键盘 ↔ 同按 + / −（或普通 Home 按 HOME）↔ 常驻 Sequencer 4×4 网格
├─ TRK：ENGINE → 引擎选择；PRESET → 搜索 / 加载；SAVE；SAVEAS → 命名
├─ SND：根据当前轨引擎显示
│  ├─ DX7：OP1…OP6、ALG、GLB → 参数浏览 → 调整
│  ├─ Juno：LFO、DCO、VCF、VCA、ENV；HPF/CHO 待运行时支持
│  ├─ Drums：八鼓音量 + Master → 两页参数浏览 → 调整
│  └─ Sampler：FILE、REC、TRIM、ROOT、LOOP、GAIN、FLT、ENV
│     ├─ FILE：浏览 / 搜索 / 加载；OPT → 选中文件操作
│     ├─ REC：准备 → 录音 → 草稿 → 裁剪 / 命名 → 保存 / 覆盖
│     └─ 文件操作：重命名 → 命名；删除 → 确认
├─ KEY：旋律引擎为 MODE、SCALE、ROOT、FX；Drums 为 Fixed Pads / BACK
├─ SYS：PROJ（SAVE / SAVEAS / LOAD / NEW）、SEQ、INFO
│  └─ SEQ：PLAY / STOP / RES、BPM、TRACK（T- / T+、MUTE、ON / OFF）、PATT、GRID / KEYS → Home
└─ Sequencer 交互层：按住步进 → NOTE / LEN / VEL / STEP
   └─ shift / Pattern → 选择 / 添加 / CLR / COPY / QNT
```

- 设备画布是 **240×135**，与参考和 NullPerator 的 **240×240** 不同；高度只有后者的 56.25%。来源：[Device.h](../Devices/Device.h)、[NullPerator UiTypes.h](https://github.com/203-Systems/NullPerator/blob/45f0206d54927018ecc57c4830ca154f4af52765/sources/UI2/Core/UiTypes.h)。
- 矩阵为 4×14：第 0 列为左侧 4 个模式键；第 1–8 列为 32 个演奏键；第 9–12 列为 16 个步进键；第 13 列为右侧动作。Context 的 2×4 动作占第 12–13 列。来源：[SynthTypes.h](../Library/Synth/Core/SynthTypes.h)。
- 当前显示标签是 **TRK / SND / KEY / SYS**。内部 `UiModifier::Sound` 指 TRK，`Parameter` 指 SND，不应直接照内部命名写用户文案。
- 当前右侧栏基准宽度为单列 40 px、双列 88 px；Home 左右各 40 px，Context 隐藏左栏、单列右栏放大至 60 px。结构预览使用这个分区比例，内容、字体与图形仍是示意，不宣称等同当前固件截图。来源：[SynthUiRenderPrimitives.h](../Applications/Synth/UI/Render/SynthUiRenderPrimitives.h)、`BuildScreenSnapshot()`。
- Home 左侧短按（少于 200 ms）切换到对应音轨，按住进入对应菜单；菜单按住帮助属于独立反馈状态，不能与短按操作混淆。
- Home 提供默认演奏键盘和常驻 Sequencer 网格两种视图；同按 + / − 切换两者，也会结束 Home 中的步进编辑，不停止序列播放、不修改八度或键盘配置。普通 Home 上按实体 HOME 也可切换。按住 step、显示 Pattern / shift 层或处于 Context 时，HOME 只取消手势或返回 Home，保留此前选择的视图。SYS → SEQ 的 GRID / KEYS 位于 2×4 操作栏左下角，切换后直接回 Home。
- Sequencer 视图以屏幕 4×4 网格持续显示 16 步的空白、已填、选中和播放位置；编辑音符、长度、力度或微步时保留网格，并显示紧凑详情。实际演奏仍用第 1–8 列，实际步进仍用第 9–12 列；切换显示不会改变输入映射。单独按住 + 或 − 打开的 2×8 槽位仍是 Pattern 选择层；只有 OCT 或 CLR / COPY / QNT 仍按住时才保持显示，全部松开即退出。单独短按 + / − 仍调整旋律键盘八度。
- Home 的音符编辑显示实际音名、单步编号或多选数量；LEN / VEL / STEP 的选中态在松开菜单键后保持，松开全部 step 后回到选定的 Home 视图。不同值显示 MIXED，屏幕和 LED 不伪造唯一选中档位。Home 视图选择属于当前 UI 会话，不加入 Project v1。

## 所有页面与独立状态

共 75 个页面或独立状态，52 个有实际模拟器截图。

| ID | 页面 / 路径 | 操作栏 | 状态 | 行为与边界 |
|---|---|---|---|---|
| [H01](ui-inventory.html#H01) | 演奏主屏<br>HOME | 1×4：OCT+ / OCT- / STOP / PLAY | 待验收 / 部分完成 | 默认演奏视图：标题栏 TRACK1，右侧青色引擎、灰色音色，八度放右上角；单八度小键盘按实际音高点亮，按下的音名写在键盘下方；Drums 显示同八列鼓垫；底部 Loop Bar 按独立 Pattern 数量显示分段，当前 Pattern 高亮并显示播放进度；不显示 P01/1、STOP 或 BPM 状态行。同按 + / − 切换 H08，不停止序列播放或修改键盘配置；普通 Home 上按实体 HOME 也可切换。第 1–8 列演奏、第 9–12 列步进的物理映射保持不变。 |
| [H02](ui-inventory.html#H02) | 步进选中 / NOTE<br>HOME → hold step | 1×4：ALL / LEN / VEL / STEP | 待验收 / 部分完成 | 显示单步编号或 held 步数、首个 held step 的实际音名；ALL 按音高顺序循环和弦单音并试听。选定 lane 持续高亮，松开所有 step 返回 Home；右侧仍为 Home 动作。 |
| [H03](ui-inventory.html#H03) | 音符长度<br>HOME → hold step → LEN | 1×4：ALL / LEN / VEL / STEP | 待验收 / 部分完成 | 演奏区八列选择 1/2/3/4/6/8/12/16 步长度；松开 LEN 键后仍保持该 lane。不同值显示 MIXED 且不高亮唯一档位；Piano 空黑键位置也可选档，长度在 Pattern 末尾裁剪。 |
| [H04](ui-inventory.html#H04) | 力度<br>HOME → hold step → VEL | 1×4：ALL / LEN / VEL / STEP | 待验收 / 部分完成 | 演奏区八列选择力度 24/40/56/72/88/104/120/127；作用于 ALL 或所选和弦单音，跨 held steps 应用。MIXED 不假定第一颗音的值，松开 VEL 后选中态保持。 |
| [H05](ui-inventory.html#H05) | 微步偏移<br>HOME → hold step → STEP | 1×4：ALL / LEN / VEL / STEP | 待验收 / 部分完成 | 96 PPQN，每十六分音步 24 pulses；前六列选显示值 1–6（内部 0–5，每格 4 pulses），末两列禁用。多值显示 MIXED，lane 保持；不改变音高或力度。 |
| [H06](ui-inventory.html#H06) | Pattern 选择 / 添加<br>HOME → shift / pattern | 1×4：CLR / COPY / QNT / · | 待验收 / 部分完成 | 单独按住 + 或 − 进入 Pattern 选择，上方 2×8 演奏键选择现有槽或下一个 + 槽，最多 16 个。Pattern 只在 OCT 或 CLR / COPY / QNT 仍按住时显示；全部松开即恢复所选 Home 视图。同按 + / − 切换键盘/网格视图。 |
| [H07](ui-inventory.html#H07) | 清除 / 复制 / 量化<br>HOME → shift action | 1×4：CLR / COPY / QNT / · | 待验收 / 部分完成 | 单独按住 OCT 后使用 CLR / COPY / QNT；按住操作键后可先松开 OCT，松开所有相关键即退出。COPY 显示 S（step）或 P（Pattern）源编号，再选同类目标。无 held steps 时按下 QNT 只进入等待目标；按住 QNT 再点物理 step 仅量化该步，先选 held steps 再按 QNT 则量化选中步。QNT + 上方两行的现有 Pattern 仅量化指定槽，不切换或创建槽。CLR 清除 held steps 或点选 step；点有内容 Pattern 进入 Y14 确认。 |
| [H08](ui-inventory.html#H08) | Sequencer 网格主屏<br>HOME → + 与 − / SYS → SEQ → GRID | 1×4：OCT+ / OCT- / STOP / PLAY | 待验收 / 部分完成 | 常驻 4×4 网格显示当前轨 Pattern 的 16 步、最低音名、和弦 +N 标记、选中状态和绿色播放位置；松开 step 后仍保留网格。标题栏与 Piano 相同（TRACK1 / 引擎 / 音色），按住 step 时换成 SEQUENCE；底部 Loop Bar 有几个 Pattern 就显示几格，当前 Pattern 高亮并显示播放进度。不常驻操作提示，编辑时才显示实际值，左栏已有 scope 不再重复。同按 + / − 回演奏键盘并结束临时步进编辑，不停止序列播放或改变键盘配置；普通 Home 上的实体 HOME 和 SYS → SEQ 左下角 KEYS 也可切换。视图切换不改变物理演奏/步进映射，也不改 Project v1。 |
| [H09](ui-inventory.html#H09) | 网格内步进编辑<br>HOME → GRID → hold step | 1×4：ALL / LEN / VEL / STEP | 待验收 / 部分完成 | 按住一个或多个 step 时保留 4×4 网格和选中/播放状态，紧凑详情显示音名、scope、LEN / VEL / STEP 值或 MIXED。物理第 1–8 列仍负责加减音符或选择数值档位；松开 step 回网格主屏；编辑期间按 HOME 只取消编辑并保留网格，同按 + / − 则结束编辑并切换演奏视图。 |
| [T01](ui-inventory.html#T01) | 音轨根页<br>TRK | 1×4：ENGINE / PRESET / SAVE / SAVEAS | 已有 UI | TRK 管音轨和音色文件；SND 管参数。 |
| [T02](ui-inventory.html#T02) | 选择引擎<br>TRK → ENGINE | 1×4：RESET / PREV / NEXT / LOAD | 已有 UI | DX7、Juno、Sampler、Drums 四种引擎。RESET 恢复初始选择，LOAD 提交；切引擎释放声音，未保存采样草稿会阻止替换。 |
| [T03](ui-inventory.html#T03) | 音色浏览 / 搜索<br>TRK → PRESET | 1×4：BACK / DEL / PREV / NEXT / LOAD | 待验收 / 部分完成 | 有搜索时 BACK 变为删字 DEL。工厂与用户音色恢复在本轮补齐。 |
| [T04](ui-inventory.html#T04) | 音色命名 / 另存<br>TRK → SAVEAS | 1×4：DEL / · / BACK / SAVE | 待验收 / 部分完成 | 命名 UI 已有；持久化与实际声音重建在本轮补齐。 |
| [D01](ui-inventory.html#D01) | DX7 分组总览<br>SND → DX7 | 2×4：OP1 / OP2 / OP3 / OP4 / OP5 / OP6 / ALG / GLB | 待验收 / 部分完成 | 标题栏 DX7 / ALGORITHM / FEEDBACK，右上角写音色；载波实底、调制器描边。暴露参数必须实际驱动音源。 |
| [D02](ui-inventory.html#D02) | Operator 1<br>SND → DX7 → OP1 | 2×4：OUT / CRS / FINE / DTN / R1 / R2 / BACK / 1/2 | 待验收 / 部分完成 | op1.output/coarse/fine/detune/attack/decay/sustain/release/amp_mod。包络为 Rate1/Rate2/Level3/Rate4，AMOD 为布尔值。velocity 未支持已移除。 |
| [D03](ui-inventory.html#D03) | Operator 2<br>SND → DX7 → OP2 | 2×4：OUT / CRS / FINE / DTN / R1 / R2 / BACK / 1/2 | 待验收 / 部分完成 | op2.output/coarse/fine/detune/attack/decay/sustain/release/amp_mod。包络为 Rate1/Rate2/Level3/Rate4，AMOD 为布尔值。velocity 未支持已移除。 |
| [D04](ui-inventory.html#D04) | Operator 3<br>SND → DX7 → OP3 | 2×4：OUT / CRS / FINE / DTN / R1 / R2 / BACK / 1/2 | 待验收 / 部分完成 | op3.output/coarse/fine/detune/attack/decay/sustain/release/amp_mod。包络为 Rate1/Rate2/Level3/Rate4，AMOD 为布尔值。velocity 未支持已移除。 |
| [D05](ui-inventory.html#D05) | Operator 4<br>SND → DX7 → OP4 | 2×4：OUT / CRS / FINE / DTN / R1 / R2 / BACK / 1/2 | 待验收 / 部分完成 | op4.output/coarse/fine/detune/attack/decay/sustain/release/amp_mod。包络为 Rate1/Rate2/Level3/Rate4，AMOD 为布尔值。velocity 未支持已移除。 |
| [D06](ui-inventory.html#D06) | Operator 5<br>SND → DX7 → OP5 | 2×4：OUT / CRS / FINE / DTN / R1 / R2 / BACK / 1/2 | 待验收 / 部分完成 | op5.output/coarse/fine/detune/attack/decay/sustain/release/amp_mod。包络为 Rate1/Rate2/Level3/Rate4，AMOD 为布尔值。velocity 未支持已移除。 |
| [D07](ui-inventory.html#D07) | Operator 6<br>SND → DX7 → OP6 | 2×4：OUT / CRS / FINE / DTN / R1 / R2 / BACK / 1/2 | 待验收 / 部分完成 | op6.output/coarse/fine/detune/attack/decay/sustain/release/amp_mod。包络为 Rate1/Rate2/Level3/Rate4，AMOD 为布尔值。velocity 未支持已移除。 |
| [D08](ui-inventory.html#D08) | 算法 / Feedback<br>SND → DX7 → ALG | 2×4：ALG / FB / · / · / · / · / BACK / · | 待验收 / 部分完成 | global.algorithm / global.feedback；须显示真实工厂音色值。 |
| [D09](ui-inventory.html#D09) | 全局 / LFO<br>SND → DX7 → GLB | 2×4：TRNS / SPD / PMOD / AMOD / · / · / BACK / · | 待验收 / 部分完成 | global.transpose/lfo_speed/lfo_pitch/lfo_amp；lfo_delay 已移除。Pitch 深度遵循工厂 PMS 灵敏度，PMS=0 时不会改变声音。 |
| [D10](ui-inventory.html#D10) | 数值调整模板<br>DX7 / Juno / Sampler → parameter | 2×4：-1 / +1 / -10 / +10 / MIN / MAX / RST / DONE | 待验收 / 部分完成 | RST 是恢复进入编辑时的值，不等于工厂默认。 |
| [J01](ui-inventory.html#J01) | Juno 分组总览<br>SND → Juno | 2×4：LFO / DCO / · / VCF / VCA / ENV / · / · | 待验收 / 部分完成 | HPF / CHO 入口已隐藏；当前 16 个参数进入运行时。 |
| [J02](ui-inventory.html#J02) | DCO<br>SND → Juno → DCO | 2×4：RNG / PWM / SAW / SUB / NOI / · / · / DONE | 待验收 / 部分完成 | dco.range/pwm/saw/sub/noise。Range 与 Saw 使用离散选择器。 |
| [J03](ui-inventory.html#J03) | Range / Saw 选择<br>Juno → DCO → RNG / SAW | 1×4：16' / 8' / 4' / DONE | 待验收 / 部分完成 | Saw 的动作是 ON / OFF / 空 / DONE。 |
| [J04](ui-inventory.html#J04) | VCF<br>SND → Juno → VCF | 2×4：CUT / RES / ENVM / LFOM / · / · / · / DONE | 待验收 / 部分完成 | vcf.cutoff/resonance/env/lfo。 |
| [J05](ui-inventory.html#J05) | VCA<br>SND → Juno → VCA | 2×4：LVL / · / · / · / · / · / · / DONE | 待验收 / 部分完成 | vca.level；未来可统一小参数组为直接调整。 |
| [J06](ui-inventory.html#J06) | Envelope<br>SND → Juno → ENV | 2×4：ATK / DEC / SUS / REL / · / · / · / DONE | 待验收 / 部分完成 | env.attack/decay/sustain/release。 |
| [J07](ui-inventory.html#J07) | LFO<br>SND → Juno → LFO | 2×4：RATE / DLY / · / · / · / · / · / DONE | 待验收 / 部分完成 | lfo.rate/delay。 |
| [J08](ui-inventory.html#J08) | HPF / Chorus 已隐藏<br>待支持 · 无正常入口 | 1×4：· / · / · / · | 待设计 / 已隐藏 | 旧分支还在源码，注册参数与根菜单入口已移除；当前不可操作。 |
| [B01](ui-inventory.html#B01) | Drums 鼓垫主屏<br>HOME → Drums | 1×4：OCT+ / OCT- / STOP / PLAY | 待验收 / 部分完成 | 八鼓排在与演奏键相同的八列（BD SD CH OH CP TM CB MA），击打时青色并在下方写出全名；物理四行的第 1–8 列重复同一 GM 映射。八度短按不改变共享键盘设置，长按仍进入 Sequencer shift。 |
| [B02](ui-inventory.html#B02) | Drums Kit · 第 1 页<br>SND → Drums → 1/2 | 2×4：Kick / Snare / C HAT / O HAT / Clap / Tom / BACK / 1/2 | 待验收 / 部分完成 | 第一页为 Kick、Snare、Closed Hat、Open Hat、Clap、Tom 音量；BACK 左下、页码右下。选择后进入 B04 数值调整。 |
| [B03](ui-inventory.html#B03) | Drums Kit · 第 2 页<br>SND → Drums → 2/2 | 2×4：BELL / MARA / MSTR / · / · / · / BACK / 2/2 | 待验收 / 部分完成 | 第二页包含 Cowbell、Maraca 和整轨 Master；仍为 BACK 左下、页码右下，翻页回到第一页，九个参数均可达。 |
| [B04](ui-inventory.html#B04) | Drums 音量调整<br>SND → Drums → level | 2×4：-1 / +1 / -10 / +10 / MIN / MAX / RST / DONE | 待验收 / 部分完成 | 九个 level 参数均为 0–127、默认 100，显示百分比。每鼓 level 影响后续触发，Master 控制整轨；RST 恢复入页值，DONE 返回同一 Kit 页。 |
| [B05](ui-inventory.html#B05) | Drums 固定键盘说明<br>KEY → Drums | 1×4：· / · / · / BACK | 待验收 / 部分完成 | 只显示 Fixed Pads 与 BACK。鼓音固定为 GM 36/38/42/46/39/43/56/70；不受 octave/root/scale/layout 影响，退出仍保留旋律引擎原键盘设置。 |
| [S01](ui-inventory.html#S01) | Sampler 空槽<br>SND → Sampler · empty | 1×4：FILE / · / · / REC | 待验收 / 部分完成 | 空槽静音已通过实际音频测试；FILE 与 REC 直接可达，保持 1×4；标题栏写 EMPTY，不放教学文字。 |
| [S02](ui-inventory.html#S02) | Sampler 已加载<br>SND → Sampler · loaded | 2×4：FILE / REC / TRIM / ROOT / LOOP / GAIN / FLT / ENV | 待验收 / 部分完成 | 加载后仍直接提供 FILE 与 REC。 |
| [S03](ui-inventory.html#S03) | 采样浏览 / 搜索<br>Sampler → FILE | 1×4：BACK / DEL / PREV / NEXT / LOAD | 待验收 / 部分完成 | 空槽与已加载均为 1×4 即时过滤；OPT 打开选中文件操作，不常驻底部教学提示。目前导入 mono PCM16 WAV；App 集成与模拟器验证通过，实机待验收。 |
| [S04](ui-inventory.html#S04) | 录音准备<br>Sampler → REC | 1×4：· / · / BACK / START | 待验收 / 部分完成 | 底部 START，BACK 在其上一格；已有草稿时明确警告替换，取消保留草稿，确认后才开始重录。该流程已通过应用集成测试，采集质量需实机验收。 |
| [S05](ui-inventory.html#S05) | 录音中<br>Sampler → REC → START | 1×4：· / · / · / REC | 待验收 / 部分完成 | REC 再按为停止；时长和容量取自实际状态。 |
| [S06](ui-inventory.html#S06) | 录音草稿<br>Sampler → stop recording | 1×4：REC / · / BACK / SAVE | 待验收 / 部分完成 | 已修复 BACK 不可见；草稿为 1×4，BACK 第三格、SAVE 最底。 |
| [S07](ui-inventory.html#S07) | 裁剪 / 预览<br>Sampler → TRIM | 2×4：LEFT / RGHT / BEG / END / PLAY / · / · / DONE | 待验收 / 部分完成 | 预览属于裁剪上下文；物理裁剪后须归一化边界。 |
| [S08](ui-inventory.html#S08) | 文件命名<br>Sampler → draft SAVE | 1×4：DEL / · / BACK / SAVE | 待验收 / 部分完成 | 成功必须来自真实写入结果。 |
| [S09](ui-inventory.html#S09) | 覆盖确认<br>Sampler → existing name | 1×4：· / · / BACK / RPLC | 待验收 / 部分完成 | BACK 已统一第三格；覆盖失败保留旧文件或备份。 |
| [S10](ui-inventory.html#S10) | 文件操作入口<br>Sampler → FILE → OPT | 1×4：REN / DEL / · / BACK | 待验收 / 部分完成 | OPT 单击进入 FILE ACTIONS，目标为浏览器选中文件；无需先加载。BACK 返回浏览器并保留搜索与选择；App 集成与模拟器验证通过，实机待验收。 |
| [S11](ui-inventory.html#S11) | 重命名<br>Sampler → FILE → OPT → REN | 1×4：DEL / · / BACK / SAVE | 待验收 / 部分完成 | 重命名选中文件；其他轨正在使用时拒绝操作。已保存工程中的旧文件引用不会自动迁移，需重新保存相关工程；App 集成与模拟器验证通过，实机待验收。 |
| [S12](ui-inventory.html#S12) | 删除确认<br>Sampler → FILE → OPT → DEL | 1×4：· / · / BACK / YES | 待验收 / 部分完成 | 确认页显示选中文件，BACK 取消、底部 YES 确认；其他轨正在使用时拒绝删除。删除会使已保存工程的对应引用失效；App 集成与模拟器验证通过，实机待验收。 |
| [S13](ui-inventory.html#S13) | ROOT 基准音<br>Sampler → ROOT | 2×4：-1 / +1 / OCT- / OCT+ / MIN / MAX / RST / DONE | 待验收 / 部分完成 | sample.root；演奏音高来自键盘，不公开独立 TUNE。 |
| [S14](ui-inventory.html#S14) | LOOP<br>Sampler → LOOP | 1×4：ON / OFF / RST / DONE | 待验收 / 部分完成 | sample.loop；One Shot 不回卷已通过音频回归测试。 |
| [S15](ui-inventory.html#S15) | GAIN<br>Sampler → GAIN | 2×4：-1 / +1 / -10 / +10 / MIN / MAX / RST / DONE | 待验收 / 部分完成 | 直接调整 sample.gain；amp.level 不在当前菜单，PAN 不承诺立体声。 |
| [S16](ui-inventory.html#S16) | Filter<br>Sampler → FLT | 1×4：CUT / RES / · / BACK | 待验收 / 部分完成 | filter.cutoff / filter.resonance；选参数后复用 2×4 调整。 |
| [S17](ui-inventory.html#S17) | Envelope<br>Sampler → ENV | 1×4：ATK / REL / · / BACK | 待验收 / 部分完成 | env.attack / env.release；选参数后复用 2×4 调整。 |
| [K01](ui-inventory.html#K01) | 键盘根页<br>KEY | 1×4：MODE / SCALE / ROOT / FX | 已有 UI | 旋律引擎按模式禁用不适用的 ROOT/SCALE，并随工程保存；Drums 使用独立 Fixed Pads 说明页 B05，不暴露无效音阶或根音控制。 |
| [K02](ui-inventory.html#K02) | 键盘布局<br>KEY → MODE | 1×4：PIANO / OCT / CHRM / DONE | 已有 UI | 键位提示与实际 MIDI note 一致。 |
| [K03](ui-inventory.html#K03) | 音阶<br>KEY → SCALE | 1×4：PREV / NEXT / · / DONE | 已有 UI | 名称来自源码 kKeyboardScales；详见文字清单。 |
| [K04](ui-inventory.html#K04) | 根音<br>KEY → ROOT | 1×4：PREV / NEXT / · / DONE | 已有 UI | 影响音阶映射，与 Sampler ROOT 不同。 |
| [K05](ui-inventory.html#K05) | 键盘 FX 预留<br>KEY → FX | 1×4：· / · / · / DONE | 入口或行为缺口 | 旧需求明确保留空页；未实现 Arp/Latch。 |
| [Y01](ui-inventory.html#Y01) | 系统菜单<br>SYS | 1×4：PROJ / SEQ / INFO / BACK | 已有 UI | 本轮根菜单收敛为真实入口，旧 MIDI/SYNC/USB/SD/维护占位已隐藏。 |
| [Y02](ui-inventory.html#Y02) | 项目管理<br>SYS → PROJ | 2×4：SAVE / AS... / LOAD / NEW / · / · / · / BACK | 待验收 / 部分完成 | SAVE/AS... 已真实写盘并通过模拟器按键验证；有草稿时拒绝保存。LOAD/NEW 执行已接通，待实机验收。 |
| [Y03](ui-inventory.html#Y03) | 项目浏览 / 加载<br>SYS → PROJ → LOAD | 1×4：PREV / NEXT / BACK / LOAD | 待验收 / 部分完成 | 文件列表与工程加载已接通；先准备并校验音源和采样，再替换当前工程。无名称搜索，待实机验收。 |
| [Y04](ui-inventory.html#Y04) | 项目命名<br>SYS → PROJ → SAVEAS | 1×4：DEL / · / BACK / SAVE | 待验收 / 部分完成 | 空名禁用 SAVE；重名进入独立覆盖确认。工程名上限 24 字符。 |
| [Y05](ui-inventory.html#Y05) | 新建 / 加载确认<br>SYS → PROJ → NEW / LOAD | 1×4：· / · / BACK / YES | 待验收 / 部分完成 | YES 已接通工程加载或新建，BACK 取消；确认后替换当前会话，未保存编辑会丢弃。待实机验收。 |
| [Y06](ui-inventory.html#Y06) | Sequencer 设置<br>SYS → SEQ | 2×4：PLAY / STOP / STOP / RES / BPM / TRK / PATT / GRID / KEYS / BACK | 待验收 / 部分完成 | Transport 概览显示播放状态、BPM、轨道与 Pattern，并提示 Home 音符+step 录入。PLAY 切换、STOP 释放声音、RES 恢复位置；BPM/TRK/PATT 各有独立信息画面。左下角 GRID / KEYS 与 Home 同按 + / − 使用同一视图切换，直接返回网格/演奏 Home，不停止序列播放或改变键盘配置；右下角仍为 BACK。 |
| [Y07](ui-inventory.html#Y07) | BPM<br>SYS → SEQ → BPM | 2×4：-10 / +10 / -1 / +1 / · / · / · / BACK | 待验收 / 部分完成 | 大字显示当前 BPM，保留 30–300 范围及 PLAYING/STOPPED；±10 与 ±1 直接调整全局速度，不重置序列位置。 |
| [Y08](ui-inventory.html#Y08) | 连接 / 存储占位已隐藏<br>待支持 · 无正常入口 | 1×4：· / · / · / · | 待设计 / 已隐藏 | 旧固定 LOCAL/INT/READY 文案入口已隐藏。外部 MIDI/Clock/USB 服务接通后再设计状态页面。 |
| [Y09](ui-inventory.html#Y09) | 维护占位已隐藏<br>待支持 · 无正常入口 | 1×4：· / · / · / · | 待设计 / 已隐藏 | 亮度、校准、测试、升级等旧占位入口隐藏，不再显示固定 TEST OK。 |
| [Y10](ui-inventory.html#Y10) | 轨道状态 / 切轨<br>SYS → SEQ → TRACK | 2×4：T- / T+ / MUTE / UNM / OFF / ON / · / · / · / BACK | 待验收 / 部分完成 | 2×4 操作栏，T- / T+ 立即切换实际编辑轨；四行分别显示各轨输出状态 OUT/MUTE 与序列启用 ON/OFF。静音或禁用释放该轨序列音符；禁用跳过该轨序列，不禁用实时演奏。 |
| [Y11](ui-inventory.html#Y11) | Pattern 控制<br>SYS → SEQ → PATT | 2×4：PREV / NEXT / CNT- / CNT+ / · / · / · / BACK | 待验收 / 部分完成 | 显示当前 Pattern、已填步数及槽位内容标记。CNT+ 添加末尾空槽；CNT- 删除最后一槽，有内容时进入 Y14，只有一槽时禁用。尚无 Song/Chain 编排。 |
| [Y12](ui-inventory.html#Y12) | 项目覆盖确认<br>SYS → PROJ → SAVE → existing name | 1×4：· / · / BACK / YES | 待验收 / 部分完成 | 已有工程文件不会直接截断；使用临时文件和可恢复备份，硬件掉电行为仍需验收。 |
| [Y13](ui-inventory.html#Y13) | 后端信息<br>SYS → INFO | 1×4：PROJ / SEQ / INFO / BACK | 已有 UI | 显示 Device::BackendName()；目前不含完整版本号或硬件诊断。 |
| [Y14](ui-inventory.html#Y14) | Pattern 删除确认<br>HOME → CLR + Pattern / SYS → SEQ → PATT → CNT- | 1×4：· / · / BACK / YES | 待验收 / 部分完成 | Home 的 CLR + 有内容 Pattern 确认删除所点槽；SYS 的 CNT- 仍针对末槽。确认页显示确切目标 Pxx，BACK 或 HOME 取消保留数据，YES 删除该目标。Home 来源的 BACK / YES 返回 Home，SYS 来源返回 PATT；HOME 始终退出至 Home。Home 仅剩一个有内容 Pattern 时确认后清空并保留槽；空槽删除无需确认。 |
| [C01](ui-inventory.html#C01) | 按住帮助<br>Context action → hold | 1×4：· / · / · / · | 已有 UI | 帮助来自菜单 fullName/description；标题栏写全称，正文 1×。 |
| [C02](ui-inventory.html#C02) | 菜单进退 / 过渡<br>HOME ↔ Context | 1×4：TRK / SND / KEY / SYS | 已有 UI | snapshot from/to 插值；保留实体键对应关系。 |
| [C03](ui-inventory.html#C03) | 文件与运行错误<br>Load / Save / Record failure | 1×4：· / · / BACK / RETRY | 待设计 / 已隐藏 | 统一错误语义；RETRY 是建议动作，依实际能力显示。 |

## 参数清单

### DX7

| 分组 | 参数 ID | 取值 / 说明 |
|---|---|---|
| ALG | `global.algorithm` | 1–32，显示 FM 路由 |
| ALG | `global.feedback` | 0–7 |
| GLB | `global.transpose` | -24…24 半音 |
| GLB | `global.lfo_speed / lfo_pitch / lfo_amp` | 0–99 |
| OP1…OP6 | `opN.output` | 0–99 |
| OP1…OP6 | `opN.coarse / fine / detune` | 0–31 / 0–99 / -7…7 |
| OP1…OP6 | `opN.attack / decay / sustain / release` | 0–99；实际标签为 Rate 1 / Rate 2 / Level 3 / Rate 4，不等同完整 ADSR 或完整 DX7 包络参数集 |
| OP1…OP6 | `opN.amp_mod` | 0 / 1 布尔值 |

共 60 个参数：6 个 Global 加 6×9 个 Operator。每个 Operator 两页各最多 6 个参数，底部保留 BACK / 页码。旧 `global.lfo_delay` 与 `opN.velocity` 没有完整运行时支持，已移除可操作参数。Pitch LFO 深度受原工厂 Patch 的 PMS 灵敏度影响，PMS=0 时改变深度仍不会改变声音；不应把这种真实条件误判为控件失效。来源：[Dx7UiModel.cpp](../Applications/Synth/UI/Dx7/Dx7UiModel.cpp)、[Dx7SynthEngine.cpp](../Library/Synth/Core/Engines/Dx7/Dx7SynthEngine.cpp)。

### Juno

| 分组 | 参数 ID | 控件 |
|---|---|---|
| DCO | `dco.range` | 16′ / 8′ / 4′ |
| DCO | `dco.pwm / sub / noise` | 0–127 |
| DCO | `dco.saw` | ON / OFF |
| VCF | `vcf.cutoff / resonance / env / lfo` | 0–127 |
| VCA | `vca.level` | 0–127 |
| ENV | `env.attack / decay / sustain / release` | 0–127 |
| LFO | `lfo.rate / delay` | 0–127 |
| 已隐藏 / 待支持 | `hpf.mode / chorus.mode` | 旧 0–3 / OFF-I-II；注册参数与根菜单入口已移除 |

当前 16 个可编辑参数。工厂音色如果关闭 Pulse 波形，PWM 本来就不会产生可听变化。来源：[JunoUiModel.cpp](../Applications/Synth/UI/Juno/JunoUiModel.cpp)、[JunoSynthEngine.cpp](../Library/Synth/Core/Engines/Juno/JunoSynthEngine.cpp)。

### Sampler

| 页面 | 参数 / 状态 | 控件与边界 |
|---|---|---|
| TRIM | `sample.start / end`，游标，预览播放头 | 0–127 比例；显示波形与有效区域 |
| ROOT | `sample.root` | MIDI 0–127，±1 半音 / ±12 半音 |
| LOOP | `sample.loop` | ON / OFF；OFF 为 One Shot |
| GAIN | `sample.gain` | 0–127，直接调整 |
| FLT | `filter.cutoff / resonance` | 0–127，选择后调数值 |
| ENV | `env.attack / release` | 0–127，选择后调数值 |
| 录音 | 真实输入电平、帧数、容量、波形、草稿 | 准备 / 录音 / 停止 / 命名 / 保存错误 |
| 内部字段 | `sample.tune / amp.level` | 当前不作为独立根页控件；不额外公开 TUNE |
| 已移除 | `amp.pan` | 当前单声道输出，不承诺 Pan |

来源：[SamplerUiModel.cpp](../Applications/Synth/UI/Sampler/SamplerUiModel.cpp)、[SamplerTrackState.h](../Applications/Synth/Runtime/Sampler/SamplerTrackState.h)。

### Drums

| 鼓垫 / 列 | GM 音符 | 参数 ID |
|---|---|---|
| 1 · Kick | 36 | `kick.level` |
| 2 · Snare | 38 | `snare.level` |
| 3 · Closed Hat | 42 | `closed_hat.level` |
| 4 · Open Hat | 46 | `open_hat.level` |
| 5 · Clap | 39 | `clap.level` |
| 6 · Tom | 43 | `tom.level` |
| 7 · Cowbell | 56 | `cowbell.level` |
| 8 · Maraca | 70 | `maraca.level` |
| Master | 整轨音量 | `master.level` |

九个参数均为 0–127、默认 100，显示百分比。每鼓音量作用于后续触发，Master 调整整轨；Home 四行的第 1–8 列重复同一鼓组，屏幕以 2×4 名称列表显示按键活动。固定 GM 映射不受 Octave / Root / Scale / Layout 影响；Drums 的八度短按也不改共享旋律键盘设置，长按仍可使用 Sequencer shift。KEY 只说明 Fixed Pads，并提供 BACK。

工厂音色为 `drums.808` / `808 Kit`，用户音色位于 `Preset/Drums`。工程仍用 v1：DX7=1、Juno=2、Sampler=3 保持不变，Drums=4；新版能读取原三种引擎工程，旧版无法读取含 Drums 的新工程。工程保存九个参数、预设身份及 GM 音符；内置鼓样本不需要额外采样文件。来源：[DrumsSynthEngine.h](../Library/Synth/Core/Engines/Drums/DrumsSynthEngine.h)、[DrumsSynthEngine.cpp](../Library/Synth/Core/Engines/Drums/DrumsSynthEngine.cpp)、[DrumsUiRenderer.cpp](../Applications/Synth/UI/Drums/DrumsUiRenderer.cpp)、[ProjectStorage.cpp](../Applications/Synth/Runtime/ProjectStorage.cpp)。

### Keyboard / Sequencer

- KEY MODE：Piano / Octave / Chromatic；ROOT：C–B；SCALE：Major、Minor、Dorian、Mixolydian、Minor Pentatonic、Major Pentatonic、Blues、Harmonic Minor。共 8 种音阶。
- Sequencer：4 轨；每轨最多 16 个 Pattern；每 Pattern 16 步；每步最多 8 音；NOTE / LEN / VEL / STEP；96 PPQN、每步 6 微步。
- 默认 Home 为演奏键盘；同按 + / − 切换常驻 4×4 Sequencer 网格，并结束 Home 中的临时步进编辑，不停止序列播放或改变键盘配置。普通 Home 上按实体 HOME 也可切换。按住 step、显示 Pattern / shift 层或处于 Context 时，HOME 只取消/返回并保留原视图。SYS → SEQ 左下角 GRID / KEYS 提供同一视图切换并回 Home。网格在 NOTE / LEN / VEL / STEP 编辑时保留，附紧凑音名、scope 与值信息；第 1–8 列演奏和第 9–12 列步进的输入映射不变。视图状态不加入 Project v1，也未新增 Song / Chain。
- SYS > SEQ、BPM、TRACK、PATT 已采用各自的状态画面；TRACK 显示四轨概览，T- / T+ 切换实际编辑轨。PATT 显示当前 Pattern、已填步数、槽位与内容标记；CNT- 删除最后一个 Pattern，有内容时先确认，BACK / YES 返回 PATT，HOME 取消并退出至 Home；只有一槽时 CNT- 禁用。
- Home 支持按住音符再点 step，以及按住 step 再点音符。LEN 的八列档位是 1/2/3/4/6/8/12/16 个十六分音步；VEL 为 24/40/56/72/88/104/120/127；STEP 的前六列为 1–6 微步，其余两列禁用。Piano 的空黑键位置可作为数值档位，NOTE 模式仍不产生音符。
- COPY 显示源类型与编号（S 为 step，P 为 Pattern），再选择同类型目标。QNT 无 held steps 时仅进入等待目标，不立即改写任何 Pattern；按住 QNT 再点物理 step 仅量化该步，先按住若干 steps 再按 QNT 则量化这些选中步。按住 QNT 点上方两行的现有 Pattern 时，量化明确指定的槽，保持当前 Pattern 和槽数量；点空的新增位置不会创建槽。
- Home 的 CLR + 有内容 Pattern 使用 Y14 确认所点槽，目标可为非当前、非末尾槽；确认页显示确切 Pxx。BACK 或 HOME 取消，YES 才删除该索引；从 Home 进入的确认返回 Home。只剩一个有内容 Pattern 时，确认后清空并保留这一槽；空 Pattern 删除直接生效。步骤清除仍直接作用于 held steps 或点选 step。自动串联、Song/Chain、外部 Clock、实时录入/overdub 尚未实现。来源：[SynthApp.cpp](../Applications/Synth/SynthApp.cpp)、[SynthUiModel.cpp](../Applications/Synth/UI/SynthUiModel.cpp)、[SequencerUiRenderer.cpp](../Applications/Synth/UI/Sequencer/SequencerUiRenderer.cpp)、[SynthSequencer.cpp](../Applications/Synth/Runtime/Sequencer/SynthSequencer.cpp)。

## NullPerator 的可复用规则

已核对的仓库 UI2 页面族：Song、Chain、Phrase、Table、Groove、Mixer、Instrument、Project、Device、Theme、Font、Browser、Sample、Record、Dialog。浏览器 / 手机外层使用 Svelte，主导航为 Tracker（内部 Device）、Files、MIDI、Settings，开发模式增加 Logs / Trace。来源：[UI2 Views](https://github.com/203-Systems/NullPerator/tree/45f0206d54927018ecc57c4830ca154f4af52765/sources/UI2/Views)、[web navigation.js](https://github.com/203-Systems/NullPerator/blob/45f0206d54927018ecc57c4830ca154f4af52765/web/src/navigation.js)。Cardenza 软件清单对应的是内层设备 UI，不需要把外层手机 App 的全部导航移植进 135 px 高的显示。

| 参考里的模式 | Cardenza 对应位置 | 下一轮设计决定 |
|---|---|---|
| 明确区分编辑光标与播放游标 | 步进、Pattern、当前轨 | 青色编辑、绿色播放可同时出现；不能只靠亮度判断 |
| 页面标题 + 范围 / 编号 + 操作帮助 | TRK / SND / 参数调整 | 固定“轨道 / 引擎 → 组 → 参数 → 值”的层级 |
| Action / Selector / Adjustment 三种操作条 | 1×4 与 2×4 实体键映射 | 复用语义，保留侧栏，不照搬底栏坐标 |
| FX 原始值 + 解码后的含义 | Transpose、Root、微步、参数值 | 显示单位、符号、枚举名；不引入无必要十六进制 |
| Browser 的选择、搜索、空态 | Preset / WAV / Project | 一套选择和回退规则，统一无结果和文件错误 |
| Record Armed/Recording/Saving/Unavailable | Sampler 录音全流程 | 录音、写入、不可用不能混成一个 REC 高亮 |
| 系统颜色与电源状态 | 保存、错误、设备状态 | 真实数据可获得再显示电池百分比和进度 |
| 用户名字保持原大小写 | Patch、Sample、Project 名称 | v3 改为全部按大写字形绘制：5×7 格无下伸部，小写 p/g/y 会抬离基线。数据仍保留原大小写，FAT 文件名本身不区分大小写 |

以上组件语义已由 [UiChromeModel.h](https://github.com/203-Systems/NullPerator/blob/45f0206d54927018ecc57c4830ca154f4af52765/sources/UI2/Chrome/UiChromeModel.h)、[UiFxParameterAdjustment.h](https://github.com/203-Systems/NullPerator/blob/45f0206d54927018ecc57c4830ca154f4af52765/sources/UI2/Views/Tracker/UiFxParameterAdjustment.h)、[Ui2RecordSnapshot.h](https://github.com/203-Systems/NullPerator/blob/45f0206d54927018ecc57c4830ca154f4af52765/sources/Application/Views/Ui2RecordSnapshot.h) 核对。

### 颜色语义建议

NullPerator 使用 20 个用户颜色 token，核心默认值如下。右列为 v3 固件实际 token（[SynthUiRenderPrimitives.h](../Applications/Synth/UI/Render/SynthUiRenderPrimitives.h)）。

| 语义 | NullPerator 默认色 | 当前 Cardenza |
|---|---|---|
| Background | `#030707` | kBackground `#030707` |
| Top / Bottom surface | `#081210` | kSurface `RGB(8,18,16)`：标题栏、按键块与空步（v2 数值） |
| Text | `#e8eeeb` | kText `#e8eeeb` |
| Dim text | `#596462` | kDim `RGB(128,145,140)`，为 135 px 屏提亮；标题栏第二行用它。kRule `RGB(40,52,50)` 用于按键描边与未选段 |
| Cursor / colored text | `#45dce8` | kAccent `#45dce8`：标题栏第一行、选择；锁定态底色 kSelection `RGB(26,51,53)` |
| Playback | `#68e69a` | kPlayback `#68e69a`：步进播放线、▶、Loop Bar 进度、预览播放头 |
| Information | `#00dc74` | 缺独立统一语义 |
| Warning | `#f0ce00` | 缺独立统一语义 |
| Error | `#f02e75` | kDanger `#f02e75`：破坏性确认、CLR、录音 |

来源：[NullPerator UiPalette.cpp](https://github.com/203-Systems/NullPerator/blob/45f0206d54927018ecc57c4830ca154f4af52765/sources/UI2/Theme/UiPalette.cpp)、[UiThemeSchema.h](https://github.com/203-Systems/NullPerator/blob/45f0206d54927018ecc57c4830ca154f4af52765/sources/UI2/Theme/UiThemeSchema.h)、[Cardenza primitives](../Applications/Synth/UI/Render/SynthUiRenderPrimitives.h)。用户参考 HTML 的 `cursor.selection` 是 `#123639`，实际仓库已是 `selection.active` / `#1a3335`，两者不能混称同一个版本。

## 下一轮需要固定的设计决策

1. **导航词汇**：保留 TRK / SND / KEY / SYS 与 Home；SYS 已收敛为 PROJ / SEQ / INFO，下一轮确认缩写和层级的可读性。
2. **操作位置**：1×4 的前进确认放最底，返回放其上；2×4 的返回默认右下，有前进时前进右下、返回左下。浏览器的搜索 DEL 是特定覆盖状态，需有清晰清空后返回路径。
3. **参数模板**：连续数值、离散选项、波形裁剪、音源路由图各用一个模板；不给每个参数发明一套操作。
4. **“Reset”含义**：参数 RST 恢复进入编辑时数值；Engine RESET 恢复原选择；Project NEW 丢弃当前作品。标签和确认文案必须区分。
5. **文件管理入口**：FILE 保留相同的 1×4 搜索列表；浏览器的 OPT 键打开选中文件的 Rename/Delete，底部提示 OPT: FILES。保留长按帮助与现有浏览器滚动语义；下一轮验收入口可发现性、跨轨使用保护，以及重命名 / 删除对已保存工程文件引用的说明。
6. **状态优先级**：错误和确认优先于一般提示；播放状态与编辑状态并行；切菜单不能让用户误以为停止播放。
7. **尚未支持能力**：HPF/Chorus、外部 MIDI/Clock、维护功能显示不可用原因或隐藏，不显示成功。KEY FX 保持有意预留。

下一版以实际 240×135 的 Home、参数调整、采样裁剪、浏览器 / 命名、Sequencer 设置和 Drums 画面为基础，统一字体层级、颜色语义与确认方式。是否完整采用 NullPerator 色板、字体大小和圆角风格仍需设计评审；本轮功能和信息反馈补齐不代表这项工作已完成。

## 本清单的验证

- 参考 HTML 原文件只读；指定 NullPerator 的 commit 和源码来源已经核实。
- HTML 无外部脚本、字体或网络依赖；卡片数据包含唯一 ID、路径、动作、状态和实现入口。
- Node 数据检查覆盖 75 条页面 / 状态、9 个实际家族与跨家族的 14 项 Sequencer 浏览集合；检查脚本语法、唯一 ID、动作数量边界、源码链接存在和 Markdown 对应。原导航的搜索、无结果、分类与状态联合筛选、选中详情检查使用最小 DOM stub，仅证明逻辑与输出结构，不代表浏览器渲染。
- 内置浏览器拒绝访问本地 `file:///` URL；依其安全策略没有通过其他浏览器或代理绕过。尚未完成浏览器截图 / 视觉验收；代码结构预览不能替代真实固件截图。


本轮实现、测试与剩余边界见 [实现与验收记录](implementation-2026-09-22.md)。HTML 已做脚本与数据检查；浏览器视觉验收因本地文件访问策略未完成。已内嵌 26 张实际模拟器截图，覆盖 Sequencer 网格、编辑与 Pattern 操作、16 Pattern 边界、Drums 鼓垫与参数、系统与采样文件操作流程。
