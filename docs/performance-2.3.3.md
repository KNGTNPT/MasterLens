# MasterLens 2.3.3：交互回归记录

2026-11-03，基于 `v2.3.2` / `6013843`。2.3.3 是交互修正版，未改变响度、真峰值、M/S 或默认旁路算法；2.3.2 的测量误差、EBU 素材、编码往返和实时安全证据继续有效，详见 [2.3.2 验证记录](performance-2.3.2.md)。

## 本轮改动

- 独立版 File QC 可播放当前选中文件，播放结束后恢复旁路。
- 信号卡实时试听增加卡片高亮、页面头部 `LIVE` 提示和 `Esc` 停止提示。
- 修复横向标签栏下划线不跟随当前页面：点击标签时通过 `setSelected` 更新视觉指示线。
- 页头 `MASTERLENS` 字标垂直压缩 15%、字距增加 30%、字标到 byline 的留白减少 20%。
- 关于页直接渲染提供的原始 SVG，不重绘、不识别、不改色；SHA-256 为 `ce185364ec34c4e1bb8e79c36f0da9ae59017c9873d5dff62ba9e182f2c278ee`。

## 验证

- **144 项 DSP / 可靠性断言通过**（继承 2.3.2 的测量与实时检查）。
- **66 项 CLI 集成检查通过**。
- `scripts/check_ui.py --build-dir build`：**35/35 布局场景通过**。
- macOS Universal AU / VST3 / Standalone / CLI / UiShot 构建通过。
- `auval -v aufx Maln OrcA`：通过。
- 独立版播放使用现有 `FilePreviewPlayer` 输出路径；缓存上限为 900 秒，长于该值的文件会截断，不能作为完整离线分析替代。
- 播放文件、证据预听与参考 A/B 都是显式输出覆盖；停止后恢复原旁路，不改写源文件。

## 发行限制

macOS 继续 ad-hoc 签名、未公证；Windows 未做商业代码签名。Windows ARM64 为交叉构建，尚未在 Windows on ARM 实机验证。真实 DAW、音频设备压力和所有显示器缩放组合仍需继续覆盖。
