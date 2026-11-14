# CardenzaSynth

运行在 Cardenza 上的四轨 groovebox，配备 240×135 屏幕和 4×14 带灯键盘。

- DX7、Juno、Sampler、Drums 四种音源，每轨独立和弦与琶音器（鼓轨除外）。
- 每个 Pattern 1–256 步，支持实时录音、Pattern 切换和整曲编排。
- 四轨混音、轨道效果、延迟与混响发送及总线效果。
- 工程、音色、采样和自动保存均存于 SD 卡。

[教程与手册](wiki/README.md) · [浏览器模拟器](https://cardenza-synth.203.io) · [界面演示](docs/groovebox/motion)

## 基本操作

中间 4×8 的 PADS 演奏当前轨，右侧 4×4 的 GRID 对应屏幕格子。

| 键 | 单击 | 按住 |
|---|---|---|
| ESC | 乐器 → 音序器 → 走带；取消 / 返回 | GRID 选择场景，第一列进入 SETUP |
| TAB | 下一轨 | GRID 选轨、静音、独奏与音序开关 |
| FN | 当前页面菜单 | GRID 执行菜单项或使用快捷键 |
| CTRL | 下一页 | GRID 直接选页 |
| ADD / SUB | 调值或移动 | 连发；FN 组合为大步调整 |
| ENTER | 确认、进入、切换 | SEQ 中打开录音面板 |
| PLAY | 播放 / 停止 | FN + PLAY 开关实时录音 |

在 SEQ 中，按住演奏键再点 step 写入音符；点 step 选中后，用 PADS 加减音符、ENTER 切换编辑项、ADD / SUB 调整。FN + S 保存工程，FN + ESC 全部停止。

开机没插 SD 卡时，本次运行不保存数据；插卡后需重启。数据目录为 `CardenzaSynth/`，详见[存储协议](docs/app-storage-protocol.md)。

## 构建

先初始化子模块：

```sh
git submodule update --init --recursive
```

固件需要 ESP-IDF 5.3.5，在已加载 ESP-IDF 环境的终端运行：

```sh
make DEVICE=Cardenza build
```

主机模拟器与测试：

```sh
cmake -S . -B build/host -DDEVICE=CardenzaSim -DBUILD_TESTING=ON
cmake --build build/host --config Debug
ctest --test-dir build/host -C Debug --output-on-failure
```

浏览器模拟器需要 Emscripten SDK：

```sh
make web
make web-serve
```

- [固件构建与烧录](Devices/Cardenza/README.md)
- [桌面模拟器、截图与测试](Devices/CardenzaSim/README.md)
- [浏览器模拟器](Devices/CardenzaWeb/README.md)
- [开发规则与设计](AGENT.md)
- [Demo 生成工具](tools/demo/README.md)

## 做自己的版本

想在 CardenzaSynth 上改出自己的固件并分享，请用 203 社区的命令行 `203c`（需要 Node.js 18 以上）：

```sh
curl -fsSL https://cardenza.203.io/community/skill/install.sh | sh
203c login                       # 打开浏览器，在社区网站上批准（远程时加 --no-browser）
203c fork V6QK28 my-synth        # 克隆源码，在社区建 Fork，写好 203.json（forked_from）
cd my-synth && git submodule update --init --recursive
# 改 203.json 的 name 和 summary，改代码，提高 version.txt，然后
make DEVICE=Cardenza build
203c publish --dry-run
203c publish
```

工程规则（环境、必需的文件、HAL 的用法、社区发布前检查什么、版本与渠道、Fork、绝不能提交的东西）只有一份：CardenzaHAL 的 [docs/FIRMWARE-PROJECT.md](https://github.com/203-Systems/CardenzaHAL/blob/main/docs/FIRMWARE-PROJECT.md)，社区的[开发指引](https://cardenza.203.io/community/develop)也从它开始。根目录的 `203.json` 是这个固件在社区里的信息（Cardenza Synth，代码 `V6QK28`）；版本号就是 `version.txt`，所以 `203.json` 里没有 `version`。官方版本由 CI 经 Launcher hub 发布，不用 203c。

## 致谢

- **[M5Stack Cardputer](https://docs.m5stack.com/en/core/Cardputer)**：一个很棒的项目。它是鸣片的灵感来源，也是我们动手做鸣片的初衷，谢谢 M5Stack 团队。鸣片不兼容 M5Stack 的设备，这里的固件也只为鸣片构建。
- **[AMY](https://github.com/shorepine/amy)**（Brian Whitman 与 Daniel PW Ellis，MIT）：四种音源背后的合成引擎。我们在它之上维护分支 [203-Systems/amy](https://github.com/203-Systems/amy)。
- **[ESP-IDF](https://github.com/espressif/esp-idf)** 与 **[TinyUSB](https://github.com/hathach/tinyusb)**：固件和 USB 所基于的框架。
- **[Emscripten](https://emscripten.org)**：让同一份代码在浏览器里运行，网页模拟器靠它。
- **[Docusaurus](https://docusaurus.io)**：教程与手册网站。
