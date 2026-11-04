# MasterLens 2.4.1：Windows x64 安装器修复记录

2026-11-04，基于 `v2.4.0` / `c0804aa`。2.4.1 只修 Windows 安装器的 VST3 目录变量，不改音频、测量、界面和项目格式。

## 1. 根因

`packaging/installer.nsi` 原来写的是：

```nsis
SetOutPath "$COMMONPROGRAMFILES64\VST3\MasterLens.vst3"
```

NSIS 3 没有 `$COMMONPROGRAMFILES64` 这个变量。可用的是：

- `$PROGRAMFILES64` → `C:\Program Files`
- `$COMMONFILES64` → `C:\Program Files\Common Files`

旧脚本编译时只产生：

```text
warning 6000: unknown variable/constant "COMMONPROGRAMFILES64\VST3\MasterLens.vst3" detected, ignoring
```

安装器仍然生成，但 VST3 的 `SetOutPath` 解析到了错误位置。Windows x64 下用户看起来完成了安装，DAW 却扫描不到插件。

## 2. 修复

- 安装路径改为 `$COMMONFILES64\VST3\MasterLens.vst3`。
- 卸载路径同步改为 `$COMMONFILES64\VST3\MasterLens.vst3`。
- 安装器版本默认值改为 2.4.1。
- CI 的 `makensis` 增加 `-WX`：warning 6000 现在直接让 Windows 安装包构建失败。
- CI 增加 native x64 安装冒烟测试：静默安装后检查
  `C:\Program Files\Common Files\VST3\MasterLens.vst3\Contents\x86_64-win\MasterLens.vst3`
  和 `C:\Program Files\MasterLens\MasterLens.exe` 是否存在。

## 3. 本地验证

环境：Apple Silicon macOS，JUCE 8.0.15，NSIS 3.13。

- **修复前**：`makensis -WX` 在 `installer.nsi:37` 因 warning 6000 失败，复现了错误变量。
- **修复后**：使用 stub stage 执行 `makensis -WX -DVERSION=2.4.1 -DARCH=x64 ...`，退出码 0，无 warning；测试安装包 81 KB。
- macOS Universal AU / VST3 / Standalone / CLI / UiShot 构建通过。
- `MasterLens-2.4.1-macOS.dmg`：`hdiutil verify` 通过，SHA-256 `8feb9b0f1d2a5a18644a4ca6b9d8e8d3066ccf55e580022824c3c4999e0002cf`。
- VST3 / Standalone 版本字符串：2.4.1。
- `auval -v aufx Maln OrcA`：PASS。
- DSP / 可靠性断言：**151 项通过**。
- CLI 集成检查：**66 项通过**。
- UI 布局：**35/35 通过**。
- 2.4.1 README 截图：6 张，2240×1400，记录在 `docs/screenshots-2.4.1.json`。

## 4. 发行限制

- Windows x64 / ARM64 安装包由 GitHub Actions 的 MSVC 构建；本机是 macOS，不能替代 Windows 实机听音或所有 DAW 扫描行为。
- macOS 继续 ad-hoc 签名、未公证；Windows 未做商业代码签名。
- 2.4.1 没有新增测量功能，因此不重新论证 2.4.0 的算法与 DSP 范围。
