# Cardenza Synth

若本仓位于 203 工作区，先读取上级目录的 `../AGENTS.md`，再按本文件和仓内维护文档处理项目细节。

本仓负责鸣片官方四轨 Groovebox 及真机、桌面和网页目标。修改前先读 `AGENT.md` 中与任务相关的章节，里面记录了存储、内存、声音、按键、页面和测试约定。[详细维护规则](AGENT.md) · [中文 README](README.zh-CN.md)

- `Applications/Synth/` 呈现 Groovebox 并管理音序、采样及工程；`Library/Synth/` 承担合成与效果；`OS/` 提供应用框架、UI、MIDI 和存储。共用界面通过设备抽象访问硬件，真机驱动放在 `Devices/Cardenza/`。
- 通用硬件修正从 `CardenzaHAL` 维护，AMY 核心从 `amy` 维护。在本仓使用固定子模块。工程、音色和采样改动按存储协议兼容旧数据，并检查无卡行为。[存储协议](docs/app-storage-protocol.md)
- 从 `GrooveApp` 跟到各页面控制器，音序改动进 `Applications/Synth/Runtime/Sequencer/`。跨层调整用 CMake 的 `ValidateArchitecture` 目标和相关主机测试验证。
- 用 `cmake -S . -B build/host -DDEVICE=CardenzaSim -DBUILD_TESTING=ON` 配置主机目标，再构建与运行 CTest。界面用 `CardenzaSimShot` 和 `tests/ui-review.script`，给 `--root` 一个独立目录，按脚本需要放置 SD 卡样本，并核对每张渲染帧对应的页面及末尾的延迟统计。对照[页面图册](docs/readme/scenes.png)、[音源图册](docs/readme/engines.png)及[模拟器说明](Devices/CardenzaSim/README.md)，按影响范围验证真机、网页或教程。
