# MasterLens 2.0.2 — 光影、Logo 与帧率记录

测试机：Apple Silicon，macOS，Release，1120×700（除非注明）。
音频：48 kHz / 512 缓冲，UiShot 合成音乐信号 200 s。
`HighResolutionTicks` 采样 10 s；paint 是把整棵组件树画进复用图像的时间。

## 1. 2.0.1 的帧率诊断

临时构建放在 `/tmp`，未改仓库。10 s 样本：

| 页面 | 定时器 | Detail setState | M/S 值/repaint | 历史数据 | paint p50 / p95 / max |
|---|---:|---:|---:|---:|---:|
| 响度 | 53.1 Hz / p50 18.13 / p95 20.36 | 26.5 Hz | 8.8 Hz | 8.8 Hz | 2.62 / 3.78 / 74.68 ms |
| 立体声 | 52.9 Hz / p50 18.55 / p95 20.61 | 26.4 Hz | 8.8 Hz | — | 1.46 / 2.79 / 63.41 ms |
| 频谱 | 53.2 Hz / p50 18.32 / p95 20.41 | 53.2 Hz | — | — | 9.14 / 10.99 / 76.43 ms |
| 文件 QC | 53.0 Hz / p50 18.61 / p95 20.23 | 26.5 Hz | — | — | 1.25 / 2.65 / 62.35 ms |

结论：

- 低帧不是 GPU。响度页 p95 只有 3.78 ms，预算 10 ms。
- 定时器名义 60 Hz，实测 53 Hz；非频谱页 `frameCounter & 1` 后只有 26.5 Hz。
- `DetailArea::setState` 再 `tick % 3`，M/S 与历史更新落到 8.8 Hz。
- 历史数据 `frameCounter % 6`，同样 8.8 Hz。
- `setValues` / `setData` 每次都 `repaint()` 整块组件；`HistoryGraph::setData` 还按值复制 1800 个 float。
- 没有每帧 `invalidateDisplays`；没有 `setBufferedToImage`；背景、历史、频谱网格已有离屏缓存。
- 频谱页 p95 10.99 ms，超过新预算 8 ms，是另一条要单独处理的路径。

## 2. 2.0.2 修复

- 单计时器 `startTimer(12)`；响度页实测 **63.2 Hz**（p50 15.7 ms，p95 18.1 ms），其他页面约 30 Hz，隐藏时直接返回。
- 数据仍按 10 Hz 读取。M/S 在 `LevelMeters::setValues` 内做指数平滑（attack 35 ms / release 300 ms），峰值保持按 10 dB/s 衰减。
- 历史数据 10.5 Hz 追加；`setData(const vector&)` 用 `assign` 复用容量，不再按值窃取调用方缓冲区。
- 历史 playhead 63.2 Hz 重绘；曲线 Path / fill Path / chip bounds 全部成员化并预分配，`setPlayhead` 只重绘 plot 矩形。
- 频谱采样点从按宽度 760 收到 420；`values` / `holds` 固定数组，曲线 Path 预分配；频谱页 CPU 3.8% → **2.82%**，p95 11.2 → **6.97 ms**。
- 详情底、历史底、频谱网格继续缓存；阴影/柔光只画固定像素，不做每帧 blur。

## 3. 2.0.2 实测

| 场景 | UI CPU | paint p50 | paint p95 | paint max |
|---|---:|---:|---:|---:|
| 响度页 | 1.74% | 2.56 ms | **2.85 ms** | 61.22 ms |
| 频谱页 | 2.82% | 5.54 ms | **6.97 ms** | 67.07 ms |
| 立体声页 | 1.72% | 1.23 ms | **1.78 ms** | 45.52 ms |
| 文件 QC | 1.68% | 1.15 ms | **1.58 ms** | 64.46 ms |
| 关闭编辑器 | **0.41%** | — | — | — |

刷新率（10 s 计数）：

- 响度页定时器 / Detail / M/S：**63.2 Hz**；
- 历史数据：**10.5 Hz**；历史 playhead：**63.2 Hz**（`repaint(plotArea)`）；
- 立体声 / 频谱 / 文件 QC：约 30 Hz 分析节拍。

`max` 是首次打开时纹理缓存构建的一帧；稳定帧用 p95。

## 4. 其它验证

- 73 项 DSP 断言通过；`auval -v aufx Maln OrcA` PASS。
- 固定 WAV 的 JSON：2.0.1 vs 2.0.2 只差 `version` / `generatedAt`。
- VST3 2.0.2 / BerryStudio；AU `com.berrystudio.masterlens`。
- 缓存约 6 MB；UiShot 峰值 RSS 约 49 MB。
- 12 × 4 分钟 48 kHz / 24-bit 批量 QC 仍按原路径，未改 DSP。

## 5. 未做

- 没有加入真正的 processBlock 分配计数器；沿用 1.4.3 的代码审查与既有结论。
- 冷缓存首帧 max 仍高于 25 ms；不是稳态。
- Windows 实机 DAW 回归、AAX、ARA2、DDP、公证/签名仍未做。
