# MasterLens 2.5.0：文件 QC 与监听来源改进记录

2026-11-05，基于 `v2.4.1` / `1534769`。2.5.0 只改文件 QC 的信息层级、监听来源状态和读数排版；DSP、音频回调、项目格式和默认旁路逻辑不变。

## 1. 改动

### 文件 QC

- 文件表优先保留高度，默认至少可见三行；证据详情可收起 / 展开。
- 选中文件时立即显示总时长，并区分未加载、可播放、播放中。
- 样本证据行单击回听事件前后片段；右侧圆点仍负责复核，双击定位保留。
- 文件状态列先显示「建议回听 / 需修复 / 已复核 / 通过」，风险分数保留在排序逻辑中。

### 监听来源与 A/B

- 底部常驻「监听 · 工程 / 参考 / 问题片段 / 文件」，与输出状态分开。
- 文件证据回听时左侧显示文件名与时间段。
- 参考按钮改为「监听参考 / 监听工程」；A/B 标注为「读数快照」。

### 读数

- 响度读数把单位放回数值旁边。
- 信号卡片的风险数字改为状态标签；风险分数仍用于排序和顶部摘要。

## 2. 验证

- macOS Universal AU / VST3 / Standalone / CLI / UiShot 构建通过。
- `MasterLens-2.5.0-macOS.dmg` 构建通过，`hdiutil verify` 通过；SHA-256 `bb75f8f587067ef2b089e82bbe840cf70667806c50218ebbba1aa7696418fbeb`。
- 版本字符串：2.5.0。
- `auval -v aufx Maln OrcA`：PASS。
- DSP / 可靠性断言：**151 项通过**。
- CLI 集成检查：**66 项通过**。
- UI 布局：**35/35 通过**。
- 2.5.0 README 截图：6 张，2240×1400，记录在 `docs/screenshots-2.5.0.json`。
- 文件 QC 截图确认两首曲目同屏、`可播放 00:00 / 00:04`、状态列与监听来源可见。
- GitHub Actions run `41994558587`：macOS、Windows x64、Windows ARM64 构建与 Release 发布全部 success。
- Windows x64 安装冒烟测试通过：静默安装后 VST3 位于 `C:\Program Files\Common Files\VST3\MasterLens.vst3`，独立版位于 `C:\Program Files\MasterLens\MasterLens.exe`。
- GitHub Release `v2.5.0` 资产：macOS DMG SHA-256 `5548103ad15b1ed4cf4c24a8703b4c4bc9855137dbe3d287d7de6ea51d137b87`，Windows x64 `1d51f7fa299eeea4e5fc407814301218f880b55121b64d1ead2fb69313af3d27`，Windows ARM64 `53c5d600ede7daaa6785d141d46179e570537c56c54c4856bcc0d17a01455091`。

## 3. 范围与限制

- 单击回听、证据展开 / 收起和监听来源切换属于 UI 交互；真实声卡、长文件、不同宿主中的听感与焦点仍需实机验收。
- 多参考逐段比较、选区等响、频段独听、900 秒以上流式播放和编码后 A/B 不在 2.5.0 范围。
- macOS 继续 ad-hoc 签名、未公证；Windows 未做商业代码签名。Windows ARM64 仍为交叉构建，未在 Windows on ARM 实机验证。
