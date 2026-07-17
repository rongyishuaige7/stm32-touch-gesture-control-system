# Hardware Lab 索引卡片

```yaml
name: STM32 触摸与隔空手势控制系统
platform: STM32F103 · C++ · PlatformIO · STM32Cube HAL · TTP223 · APDS-9960 · ESP-01S
summary: 使用四路电容触摸和 APDS-9960 隔空手势控制四路 LED，并通过 ESP-01S 本地热点展示状态与诊断的教学原型。
status: 源码来源已确认 · 硬件无关源码契约已通过 · PlatformIO 干净构建已验证 · 当前整机尚未重新真机复测
media_scope: 当前没有实物照片、演示视频、EDA、PCB 或制造文件；公开 BOM、接线边界图、协议、来源和验证说明。
known_boundaries:
  - 构建和源码契约不证明触摸可靠性、APDS 识别率、ESP AT 兼容性或实际 LED 行为。
  - HTTP 无认证和 TLS，只面向隔离可信的教学热点。
  - GPIO 不得直接驱动继电器、灯带、电机或市电负载。
  - Actions Artifact 仅保留 14 天。
```
