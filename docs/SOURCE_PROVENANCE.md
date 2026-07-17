# 源码来源与权威副本裁决

> 状态日期：2026-07-17

## 只读来源

```text
/home/rongyi/桌面/stm32-touch-gesture-pio
/mnt/shared/2026项目/stm32-touch-gesture-pio.zip
```

历史 ZIP SHA-256：

```text
aeadcca953d972baa461423ab2f22426d6c738a62947dccbefff62085b5580c2
```

整理前桌面目录共 15 个文件、81,428 字节。逐文件清单内容计算得到的 manifest SHA-256：

```text
a637103fda2aaf1171651d96a4cebc5af42cedd160680ade3857c10045dd0fca
```

这些路径只用于来源审计，不是公开仓库的构建依赖。原目录和 ZIP 均保持只读，不由公开候选反向覆盖或删除。

## 对比结果

桌面目录与 ZIP 的 15 个对应文件中：

- 14 个源码、头文件和 `platformio.ini` 逐字节一致；
- 仅 `README.md` 不同；
- 桌面 README 比 ZIP 增加了 PlatformIO、STM32Cube HAL 与“不依赖 `.ioc`”的构建说明；
- 桌面目录因此作为整理时的较新文档来源，ZIP 作为历史封存基线。

该裁决只说明来源和文档新旧，不证明桌面版本已经在当前实物上重新烧录验证。

## 公开候选的最小修复

公开候选目录：

```text
/home/rongyi/桌面/stm32-touch-gesture-control-system
```

在不反向修改来源目录的前提下完成：

1. 将 PlatformIO 平台固定为 `ststm32@19.5.0`；
2. 把教学 SoftAP 配置集中到 `include/config.h`，明确它不是用户真实凭据；
3. 修复 LED 亮度在非 10 倍数情况下可能发生 8 位加减溢出的边界；
4. 让未知 HTTP 路径返回真实 `404 Not Found`，不再用 `200 OK` 包装 404 文本；
5. 新增中文 README、硬件边界、BOM、接线图、协议、验证、许可证、第三方声明和 CI；
6. 不创建没有真实素材的照片、视频或 EDA 占位入口。

以上属于公开前的可审计加固，不得写成当前硬件已经重新验证。
