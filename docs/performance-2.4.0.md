# MasterLens 2.4.0：交互与底层可靠性记录

2026-11-04，基于 `v2.3.3` / `306c4fd`。2.4.0 修改了独立版文件播放链路、宿主键盘处理、三处 DSP 共享状态和关于页 SVG 呈现；默认旁路行为与输出位深不变。

## 本轮改动

### 1. 独立版文件播放进入测量

`FilePreviewPlayer` 新增分析馈送模式。播放文件时先渲染到音频缓冲区，再进入 `SpectrumRing`、`AnalysisEngine` 和输出；证据预听仍保持“分析原始总线、只覆盖输出”的旧语义。文件页新增进度条、时间显示和拖动定位。

限制：文件缓存上限为 900 秒，超过该长度的文件会截断；这是播放缓存，不是离线文件分析的替代。

### 2. Logic Pro 空格键

普通空格不再触发本地 Hold，而是返回 `false` 交给宿主；Hold 改为 `Shift+Space`。独立版在没有宿主时可用空格停止当前文件播放。真实 Logic Pro 的键盘焦点行为仍需宿主实机确认。

### 3. 三处技术债

- `SpectrumRing` 的样本存储从 `std::vector<float>` 改为 `std::unique_ptr<std::atomic<float>[]>`，音频写入和 UI 读取不再构成普通 float data race。显示帧仍可能被覆盖，这是可接受的视觉撕裂。
- `TpEventRing` 的 `write`、`count` 和每个事件槽均为 atomic；`get()` 在复制前后读取版本，版本变化会重试，最多四次。
- `TruePeakDetector` 按 192 kHz 目标采样率选择相位：48 kHz → 4×，96 kHz → 2×，192 kHz → 1×；不再无条件把宿主采样率乘四。

### 4. 关于页

原始 SVG 字节与几何不变，只改变布局到卡片左下角。暗色模式将黑色 ink 替换为白色；红色保持原色。没有重画或解析图形语义。

## 验证

- **151 项 DSP / 可靠性断言通过**（新增 96 kHz / 192 kHz TP 倍率、TpEventRing 版本和事件快照检查）。
- **66 项 CLI 集成检查通过**。
- `scripts/check_ui.py --build-dir build`：**35/35 布局场景通过**。
- macOS Universal AU / VST3 / Standalone / CLI / UiShot 构建通过。
- `auval -v aufx Maln OrcA`：通过。
- 文件播放使用 `FilePreviewPlayer` 的 900 秒缓存上限；播放结束或停止后恢复旁路。
- 证据预听、参考 A/B 和文件播放都是显式输出覆盖；停止后恢复原旁路，不改写源文件。

## 发行限制

macOS 继续 ad-hoc 签名、未公证；Windows 未做商业代码签名。Windows ARM64 为交叉构建，尚未在 Windows on ARM 实机验证。真实 DAW、音频设备压力、Logic Pro 键盘焦点和所有显示器缩放组合仍需继续覆盖。离屏 UI 检查不能替代真实宿主中的听音与键盘验证。
