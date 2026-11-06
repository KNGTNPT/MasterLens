# MasterLens 3.0.0-rc.1：四项监听能力验证记录

2026-11-06，基于 `v3.0.0-beta` / `aed9e86`。3.0-rc.1 补齐频段独听、选区等响、长文件流式播放、编码 A/B；参考槽位与排版沿用 beta。

## 改动

- **频段独听**：`AuditionEngine` 新增 4 阶 Butterworth 实时带通（Q 0.5412 / 1.3066，24 dB/oct），`compensate=false` 时不做电平补偿；频谱页 10 段热力图与低 / 中 / 高三分段都可点击，`Esc` 停止。
- **选区等响**：新增 `RegionLoudness`（BS.1770-4 门限积分：400 ms 窗、100 ms hop、−70 LUFS 绝对门、−10 LU 相对门）与 `ReferenceRangeMeter`（后台只解码选区）；时间页「选区等响」把差值写进参考槽位 GAIN，工程侧不动。
- **长文件流式播放**：新增 `StreamingPreviewPlayer`——读取线程 8192 帧预读、无锁环形缓冲（4 s × 192 kHz，约 8 MB）、4 点 Catmull-Rom；独立版文件传输改用它，欠载输出静音并显示「缓冲中」。
- **编码 A/B**：新增 `CodecABJob` / `createCodecPreview`（本机 ffmpeg AAC 256k / MP3 320k → float WAV）与处理器内的 A/B 双流切换；文件 QC 显示 ΔLUFS / ΔTP，CSV / JSON 增加 `codecAB` 字段。

## 验证

- DSP / 可靠性断言：**178 项通过**（新增 27 项）。
  - 频段独听：通带增益 −0.03 dB（容差 ±1.5 dB），相邻两倍频程泄漏 **≤ −24 dB**；回调内 `new/delete` 为 0。
  - 选区等响：稳态 −23.0 LUFS 门限积分不变；−95 LUFS 窗口被绝对门剔除；−50 LUFS 段按相对门剔除后回到 −20.0 LUFS；文件选段测量 −20.0 LUFS（双声道 −20 dBFS 正弦，±0.2），窗口数 37；短于 0.4 s 的选段明确拒绝。
  - 长文件流式：2 小时虚拟文件（测试用按需 `AudioFormatReader`）可打开、播放、seek 到 3600.01 s、EOF 停止；回调内 `new/delete` 为 0。
  - 编码 A/B：AAC 往返成功，解码 WAV 可测量，与源的 LUFS 差 ≤ 0.6 LU；机器上没有 ffmpeg 时用例**显式跳过并打印原因**（`encode: ffmpeg unavailable`），不冒充通过。
- CLI 集成检查：**66 项通过**。
- UI 布局：**35/35 通过**（文件页新增编码按钮、时间页新增选区等响按钮后仍无越界）。
- 11 个页面逐页截图检查；6 张 README 截图（2240×1400）记录在 `docs/screenshots-3.0.0-rc.1.json`，SHA-256 复核一致。
- macOS Universal 构建（x86_64 + arm64）、DMG 校验、`auval -v aufx Maln OrcA` 通过；本地 DMG SHA-256 `435133e2d4284bbbe404ac955069c520720b7bda8b2aa1fec9250991754b32b5`。

## 未验证 / 限制

- 频段独听、选区等响、编码 A/B 的真实听感与宿主行为仍需实机走查；本轮只验证了测量、线程安全与界面布局。
- 「选区」是 MasterLens 时间线拖选，不是宿主 loop；长选区循环交给宿主，≤ 10 s 可用 12 s 历史环。
- 参考轨仍整段解码，超过 1 小时或 512 MB 会明确拒绝；逐槽流式参考不在本版。
- 编码 A/B 依赖本机 ffmpeg（与 2.3 CLI 一致）；Windows 实机需自行安装 ffmpeg，macOS 继续 ad-hoc 签名、未公证。
- Windows ARM64 仍为交叉构建，未在 Windows on ARM 实机验证。
