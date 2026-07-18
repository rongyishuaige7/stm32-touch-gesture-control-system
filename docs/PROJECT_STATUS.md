# 项目状态

> 状态日期：2026-07-17

| 层级 | 状态 | 当前证据 |
| :-- | :-- | :-- |
| 源码来源 | 已确认 | 桌面目录与历史 ZIP 已逐文件比较，权威裁决已记录 |
| 仓库门禁 | 已通过 | 敏感信息、结构、生成物、SVG/BOM 和声明契约可自动检查 |
| 源码契约 | 已通过 | 关键引脚、阈值、HTTP 状态和边界修复由无硬件测试核对 |
| 固件构建 | 已验证 | PlatformIO 6.1.19 + `ststm32@19.5.0` 干净构建通过 |
| 当前真机复测 | 未执行 | 未烧录当前提交，未重新验证触摸、APDS、ESP、网页和 LED |
| 媒体与 EDA | 未提供 | 当前没有真实照片、视频、原理图、PCB 或制造文件 |

## 权威状态摘要

```text
源码来源已确认
硬件无关源码契约已通过
PlatformIO 干净构建已验证
当前 STM32、TTP223、APDS-9960、ESP-01S 与 LED 整机尚未重新真机复测
```

## 已知限制

- TTP223 模块版本、极性焊盘和真实接线尚未由当前实物确认；
- APDS 手势方向、阈值、距离、环境光与误触率尚未实测；
- ESP-01S AT 固件版本和 HTTP 稳定性尚未重新验证；
- HTTP 无认证、TLS、会话和设备身份，只适合隔离教学热点；
- Web 页面只展示状态，不能远程控制 LED；
- APDS FIFO 轮询和 HTTP 发送可能阻塞主循环；
- 当前没有 EDA/PCB，接线图不是原理图；
- PlatformIO 输出中的 72 MHz 是板卡能力描述，源码实际配置 HSI 8 MHz；
- 没有功耗、长稳、识别准确率和可靠性证据。

在当前提交完成日期化实物复测以前，不得使用“硬件已验证”“识别准确”“生产就绪”“家电可用”或“系统在线”等标签。

## Historical media and EDA added on 2026-07-18

sanitized historical photo(s), historical EDA derivative(s). See [MEDIA_EVIDENCE](MEDIA_EVIDENCE.md) for dates, sanitization, omissions, and evidence limits.

This publication update adds historical evidence only. Current hardware re-test not run.
