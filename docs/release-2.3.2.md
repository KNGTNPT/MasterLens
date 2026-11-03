# MasterLens 2.3.2

每一个「通过」，都再认真一点喵。

- 校准 M 400 ms 滑动窗取峰、LRA 采样与文件尾部真峰值；加入 EBU v5、独立 FFmpeg、动态 / 边界 / 随机缓冲区验证。89 个文件通过本轮验收，误差表与测试边界公开。
- 未知 / 未完整覆盖不再盖 PASS；报告写入、编码失败、超时与交付规格失败都有明确结果和 CLI 退出码。
- 每个实例独立保存项目；复核绑定源文件指纹、分析版本与事件，旧复核不会替新内容背书。
- AAC / MP3 真正编码、解码后重测，并保存源 / 编码 / 解码内容指纹。
- 参考与证据试听移除音频回调锁和 owning 对象释放，参考 A/B 加入 5 ms 淡变；修复 Debug 退出时 DynamicObject 生命周期断言。
- 浅色频谱增加对比，按钮光晕和回弹更有一点手感；关于页工作室 SVG 原封不动。

本地 144 项 DSP / 可靠性检查、66 项 CLI 集成检查、pluginval Level 5 与 auval 通过。详细误差、复现和限制见[验证记录](https://github.com/KNGTNPT/MasterLens/blob/v2.3.2/docs/performance-2.3.2.md)。EBU 素材仅内部验证，不随软件分发；测试通过不代表 EBU 认证。

提供 macOS Universal DMG、Windows x64 / ARM64 安装包。**手头没有 Windows on ARM 设备，ARM64 包尚未经该环境实机测试。** macOS 为 ad-hoc 签名、未公证；Windows 未做商业代码签名。真实 DAW、硬件及长时间运行覆盖仍需继续补齐。

不需要账号、数据集或模型，音频留在本地。
