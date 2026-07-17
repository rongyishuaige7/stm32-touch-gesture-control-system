# 本地热点与 HTTP 协议

## 网络模型

```text
手机/电脑 ── Wi-Fi ── ESP-01S SoftAP
                         │ AT +IPD / CIPSEND
                         ▼
                    STM32F103 固件
```

固件不连接外部云服务，也不使用用户真实 Wi-Fi。它要求 ESP-01S 运行兼容 AT 固件并创建本地 SoftAP。

## HTTP 端点

### `GET /`

返回嵌入式 HTML 状态页。页面每约 700 ms 请求 `/api/state`，仅展示当前事实，不提供 LED 控制按钮。

### `GET /api/state`

示例结构：

```json
{
  "t": 12345,
  "touch": [0, 1, 0, 0],
  "gesture": 1,
  "channel": 2,
  "last_g": 1,
  "last_gt": 12200,
  "led": [1, 0, 0, 0],
  "bri": [50, 50, 0, 0],
  "apds": {"ok": 1, "id": "0xAB", "gv": 2, "gc": 1}
}
```

字段来自当前内存快照，不是远程服务健康状态，也不证明物理亮度或传感器准确率。

### `GET /api/diag`

返回 APDS 初始化、ID、FIFO、方向差分和累计计数等诊断字段。字段用于教学排障，不是校准或识别准确率报告。

### 未知路径

当前公开候选返回：

```text
HTTP/1.1 404 Not Found
```

## 安全与实现边界

- 无认证、TLS、CSRF 保护、会话或设备身份；
- `Access-Control-Allow-Origin: *`；
- 使用字符串匹配处理有限的 GET 路径，不是通用 HTTP 服务器；
- 环形缓冲区和请求 scratch 都有固定上限；
- HTTP 发送会等待 ESP AT 响应并可能阻塞主循环；
- 只能用于隔离可信的桌面教学环境。
