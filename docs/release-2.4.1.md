# MasterLens 2.4.1

Windows x64 安装器的小补丁喵。测量、音频和界面代码与 2.4.0 相同，只把 VST3 放回 DAW 会扫描的目录。

- **根因**：NSIS 没有 `$COMMONPROGRAMFILES64` 这个变量，实际应为 `$COMMONFILES64`。旧脚本编译时只报 warning 6000，安装器继续运行，但 VST3 的 `SetOutPath` 解析到了错误位置，Windows x64 下装完看不到插件。
- **修复**：安装和卸载路径统一改为 `$COMMONFILES64\VST3\MasterLens.vst3`，即 `C:\Program Files\Common Files\VST3\MasterLens.vst3`。
- **防回归**：CI 的 `makensis` 增加 `-WX`，同类未知变量 warning 直接让 Windows 安装包构建失败，不再静默产出坏安装器。
- **最小范围**：没有改 DSP、旁路、文件 QC、界面布局或项目格式；macOS 安装包与 2.4.0 功能一致。
- **升级**：Windows 2.4.0 用户请重新下载 2.4.1 安装包，安装后重启 DAW；如果之前把 VST3 装到了错误位置，可以手动清理旧的错位目录。

macOS Universal、Windows x64 / ARM64 安装包。macOS 仍为 ad-hoc 签名、未公证；Windows ARM64 仍为交叉构建，未做该环境实机测试。详细验证见 `docs/performance-2.4.1.md`。
