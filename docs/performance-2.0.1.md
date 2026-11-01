# MasterLens 2.0.1 — UI 重做与性能记录

日期：2026-11-01。机器：Apple Silicon MacBook Air（macOS）。所有数字来自仓库自带的 `MasterLensUiShot`，不是估算。

## 方法

- 基准：`MasterLensUiShot bench pageN size1120` 与 `minsize`，离屏运行 5 秒，统计 UI 定时器 CPU。
- paint：把整棵组件树用 `paintEntireComponent` 画进一张复用的 1120×700 图像，连续 30 次，取 p50 / p95 / max。
- 旧界面：`git worktree` 检出 tag `v2.0.0`，使用同一份 `Tools/UiShot.cpp`、同一台机器、同一尺寸，避免工具差异。
- 音频路径：`MasterLensUiShot closedbench`，编辑器和窗口都不创建，直接跑 30 秒 `processBlock`（48 kHz / 512）。
- 文件 QC：12 个 4 分钟、48 kHz / 24-bit 立体声 WAV，文件夹递归分析。
- 内存：`/usr/bin/time -l` 读取 peak memory footprint。

## CPU（UI 定时器负载，单核百分比）

| 页面 | 2.0.0 旧界面 | 2.0.1 新界面 | 预算 | 结论 |
| --- | ---: | ---: | ---: | --- |
| 响度 | 1.41% | 1.37% | ≤ 3% | 通过 |
| 频谱（60 fps） | 2.13% | 2.41% | ≤ 6% | 通过 |
| 立体声 | 1.41% | 1.33% | ≤ 3% | 通过 |
| 文件 QC | 1.24% | 1.40% | ≤ 3% | 通过 |
| 诊断 | 1.28% | 1.52% | ≤ 3% | 通过 |
| 曲风交付 | 1.41% | 1.53% | ≤ 3% | 通过 |

新界面多了暗角 / 噪点 / 纹理，频谱页从 2.13% 涨到约 2.4% 的中位，仍低于 6% 预算。没有用降 FFT、降过采样或减少测量频率来换。

## paint（整棵组件树，1120×700）

| 页面 | 2.0.0 旧界面 p95 | 2.0.1 新界面 p95 | 新界面缓存前 p95 | 预算 |
| --- | ---: | ---: | ---: | ---: |
| 响度 | 2.6 ms | **2.5 ms** | 约 14 ms | ≤ 10 ms |
| 频谱 | 8.2 ms | **8.6 ms** | 约 18 ms | ≤ 10 ms |
| 文件 QC | 1.7 ms | **1.4 ms** | 约 11 ms | ≤ 10 ms |

880×560 下响度页 p95 为 **2.1 ms**。“缓存前”是在 2.0.1 视觉层已经存在、但详情底 / 历史图 / 频谱网格还没有离屏缓存时的数字；把这三块缓存后，paint 时间才回到个位数。

## 音频线程

```
CLOSED_BENCH wall=0.132–0.137 s load=0.44–0.46 %
```

30 秒立体声，48 kHz / 512；编辑器和窗口均未创建。目标 ≤ 0.6%，通过。`processBlock` 本次没有新增堆分配、锁、字符串或文件写入；DSP / 测量 / 门禁 / 导出代码未改。

## 文件 QC 批量

12 × 4 分钟 48 kHz / 24-bit 立体声 WAV：

```
real 14.11 s
```

目标 ≤ 25 s（M1），通过。文件按原生采样率分块读取，不整张专辑读入内存。

## 内存

- UiShot（编辑器打开、1120×700）peak memory footprint：**48.8 MB**，插件整体预算 ≤ 250 MB。
- 离屏缓存估算约 6 MB：详情底约 3.1 MB、历史图约 0.9 MB、频谱网格约 1.4 MB；按尺寸 / 主题 revision 失效，切页不叠加缓存。预算 ≤ 48 MB。

## 回归

- `MasterLensDspTest`：**73 项全部通过**，CPU 0.28 s / 60 s。
- `auval -v aufx Maln OrcA`：**PASS**。
- 固定 WAV 的 JSON：2.0.0 与 2.0.1 只差 `version` 和 `generatedAt`；`sampleRate`、`bitDepth`、LUFS、TP、LRA、PLR、频段、风险逐项相同。
- 三处版本号：`CMakeLists.txt` project / plugin、`Source/Version.h` 均为 **2.0.1**。
- 截图：`docs/screenshot-2.0.1.png`、`docs/fileqc-2.0.1.png`、`docs/stereo-2.0.1.png`，另附 `-880.png`。

## 备注

- `processBlock` 的零分配是代码审查 + 既有 1.4.3 分配计数器结论；本次没有改音频路径，未新增分配点。
- “减少动态效果”：当前 2.0.1 的动效只有 hover / 状态色切换，没有循环动画、粒子、模糊或 spring；在系统减少动态效果时，这些静态状态切换不需要额外关闭。没有为了动效牺牲可读性。
- 所有特效均缓存或使用渐变 / 半透明矩形，paint 中没有高斯模糊。
