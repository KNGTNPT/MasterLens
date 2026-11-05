# MasterLens 3.0.0-alpha：排版与数字单位记录

2026-11-05，基于 `v2.5.0` / `4a2156b`。3.0.0-alpha 只做排版、数字与单位校正；默认旁路、DSP、项目格式和文件 QC 行为不变。

## 改动

- 响度页综合、M/S 读数与历史曲线：单位跟随数值，历史曲线末端显示 `−17.7 LUFS`。
- 真峰值页：`−1.5 dBTP` 合并为一个读数单元，移除重复单位标签。
- 信号卡片：`dB / % / /100` 紧跟数值；风险数字改为状态标签。
- 平台归一化表：目标、安全增益、预测 TP、差额加单位并重新分配列宽。
- 目标页：当前综合响度与 `LUFS` 合并成同一行。
- 保留 Geist 14 / 16 / 26px、tabular numerals、统一 tracking 和 4px 网格。

## 验证

- 11 个页面逐页截图检查；中英文与最小窗口由 UI 布局检查覆盖。
- DSP / 可靠性断言：**151 项通过**。
- CLI 集成检查：**66 项通过**。
- UI 布局：**35/35 通过**。
- macOS Universal 构建、DMG 校验、`auval` 通过；本地 DMG SHA-256 `99281c90b3cdb3b67d0de980b2b1f9068c1d68876100b889798c76cb61ca9589`。
- 3.0.0-alpha 截图：6 张，2240×1400，记录在 `docs/screenshots-3.0.0-alpha.json`。
- GitHub Actions run `42001151245`：macOS、Windows x64、Windows ARM64 与 Release 发布全部 success。
- Windows x64 安装冒烟测试通过；VST3 与独立版安装到正确目录。
- GitHub Release `v3.0.0-alpha`（pre-release）：macOS DMG `d825fe2187b86fa530f23bfbfa1fc94f424f923d45b3f71409f5b983857e33f9`，Windows x64 `ea97fbfd29f08e9b57d0c436a6ac840fc429c06438f2a91fa3c63d543a40d1c8`，Windows ARM64 `9d59eaa9cd306093379e3caf6d454255e7552596f34bbfaef9f1242e0740780a`。

## 范围

本版不包含 2.6 / 2.7 的参考工作区、选区等响、频段独听、长文件流式播放和编码 A/B；这些仍未完成，不应把 3.0.0-alpha 当作完整 3.0。macOS 继续 ad-hoc 签名、未公证；Windows ARM64 仍为交叉构建，未做该环境实机验证。
