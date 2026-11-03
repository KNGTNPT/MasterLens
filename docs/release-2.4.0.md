# MasterLens 2.4.0

导入文件真正进入测量链路，并处理三处底层可靠性问题喵。

- 独立版 File QC 播放文件时，文件音频先进入响度、真峰值、LRA、频谱、声场与诊断分析，再送到输出；切换任意测量页面都能看到实时读数。
- 文件页增加播放进度条和时间显示，可拖动定位；播放 / 停止逻辑保留。
- Logic Pro / 宿主前台时，普通空格不再被 MasterLens 的 Hold 快捷键消费，交回宿主 transport；Hold 改为 `Shift+Space`。
- `SpectrumRing` 的共享样本改为 atomic float，消除音频线程与 UI 线程的 C++ data race。
- `TpEventRing` 的事件槽改为 atomic，读取端执行版本校验和重试；不再只是注释承诺。
- 真峰值插值按 192 kHz BS.1770 基准工作：48 kHz 4×、96 kHz 2×、192 kHz 1×。
- 关于页保留原始 SVG 字节与几何，位置改到卡片左下角；暗色模式只把黑色 ink 变为白色，红色保持原色。
- 保留横向标签栏下划线跟随当前页面的修复。

macOS Universal、Windows x64 / ARM64 安装包。macOS 仍为 ad-hoc 签名、未公证；Windows ARM64 仍为交叉构建，未做该环境实机测试。详细验证见 `docs/performance-2.4.0.md`。
