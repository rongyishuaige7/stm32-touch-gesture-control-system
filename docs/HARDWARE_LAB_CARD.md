# Hardware Lab 索引卡片

```yaml
name: STM32 触摸与隔空手势控制系统
platform: STM32F103 · C++ · PlatformIO · STM32Cube HAL · TTP223 · APDS-9960 · ESP-01S
summary: 使用四路电容触摸和 APDS-9960 隔空手势控制四路 LED，并通过 ESP-01S 本地热点展示状态与诊断的教学原型。
media_scope: 当前没有实物照片、演示视频、EDA、PCB 或制造文件；公开 BOM、接线边界图、协议、来源和验证说明。
known_boundaries:
  - HTTP 无认证和 TLS，只面向隔离可信的教学热点。
  - GPIO 不得直接驱动继电器、灯带、电机或市电负载。
  - Actions Artifact 仅保留 14 天。
```

- **项目素材：** 已补充项目照片、界面截图和相关资料；范围和版本差异见 [`MEDIA_EVIDENCE.md`](MEDIA_EVIDENCE.md)。
