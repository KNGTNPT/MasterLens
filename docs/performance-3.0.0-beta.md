# MasterLens 3.0.0-beta：参考槽位与 UI 对齐记录

2026-11-06，基于 `v3.0.0-alpha`。3.0-beta 增加参考槽位基础、参考偏移/增益，并修复六项界面排版问题；DSP、默认旁路和文件 QC 行为不变。

## 改动

- 频谱页参考槽位下拉；多参考路径、当前槽位、OFS 和 GAIN 写入插件状态，重开工程恢复。
- 参考播放器增加逐槽增益；切换槽位重新加载参考并应用偏移/增益。
- 设置页底部 logo、BERRYSTUDIO、开发者、关闭按钮组成一个居中水平组。
- 读数单位跟随数值基线；平台归一化下拉与 REF / CEIL 读数对齐。
- 快照 A/B 字母居中；时间页按钮增加上下留白；目标页两栏按高度分散行距。

## 验证

- DSP / 可靠性断言：**151 项通过**。
- CLI 集成检查：**66 项通过**。
- UI 布局：**35/35 通过**。
- 11 个页面逐页截图检查；6 张 README 截图（2240×1400）记录在 `docs/screenshots-3.0.0-beta.json`，SHA-256 复核一致。
- macOS Universal 构建、DMG 校验、`auval` 通过；本地 DMG SHA-256 `4cc28be37a2bfa4bef4f57ecfd80a05fd72da3380221843b3ba9bc007bed0281`。
- GitHub Actions run `42159603430`：macOS、Windows x64、Windows ARM64 与 Publish verified installers 全部 success。
- Windows x64 安装冒烟测试通过；VST3 与独立版安装到正确目录。
- GitHub Release `v3.0.0-beta`（pre-release）：macOS DMG `a059cb69e2a25f7b7a3f23f53eb91e9402f6813087980c2f61a4a6fe5c1b108e`，Windows x64 `e5a4c57a7d57691ef234690ae3dbb0ffaf557f8fed73e4635a48bbb54ea7b0b0`，Windows ARM64 `1f9dba25db468ef36b974834d448caad4a49bd616108a953801786f9730cef19`；下载后 `shasum -a 256 -c SHA256SUMS.txt` 三个文件全部 OK。Release 的 DMG 由 CI 重新构建，哈希与本地构建不同，校验一律以 `SHA256SUMS.txt` 为准。

## 范围

2.7 的长文件流式、编码 A/B、选区等响和频段独听仍未完成；本 beta 不能当作完整 3.0。macOS 继续 ad-hoc 签名、未公证；Windows ARM64 仍为交叉构建，未在 Windows on ARM 实机验证。
