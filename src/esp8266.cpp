/**
 * @file    esp8266.cpp
 * @brief   ESP8266-01S AT 指令驱动实现
 *
 *   USART1 PA9(TX)/PA10(RX) 与 ESP8266-01S 通信。
 *   启动后配置为 WiFi AP 模式 + TCP Server :80。
 *   解析浏览器 HTTP GET 请求，返回 HTML 页面或 JSON 状态。
 */

#include "esp8266.h"
#include "apds9960.h"
#include "config.h"
#include <ctype.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

static uint16_t snprintf_safe_len(char *buf, size_t bufsize, int ret) {
    if (bufsize == 0U) {
        return 0U;
    }
    if (ret < 0) {
        buf[0] = '\0';
        return 0U;
    }
    if ((size_t)ret >= bufsize) {
        return (uint16_t)(bufsize - 1U);
    }
    return (uint16_t)ret;
}

/* ---- UART 句柄 ---- */
static UART_HandleTypeDef huart1;

/* ---- 接收环形缓冲区（中断驱动） ---- */
static volatile uint8_t  rx_buf[ESP_RX_BUF_SIZE];
static volatile uint16_t rx_head = 0;
static volatile uint16_t rx_tail = 0;

/* ---- 设备状态快照 ---- */
static volatile GestureType  last_gesture  = GESTURE_NONE;
static volatile TouchChannel last_channel  = TOUCH_NONE;
static volatile uint8_t      last_touch_mask = 0;
static volatile GestureType  last_latched_gesture = GESTURE_NONE;
static volatile uint32_t     last_latched_gesture_tick = 0;

/* HTTP 分块发送工作区（放 BSS，避免 2048 字节栈占用导致溢出） */
static char s_esp_http_sendbuf[2048];

/* ---- 前向声明 ---- */
static void ESP_UART_Init(void);
static void ESP_SendRaw(const char *data, uint16_t len);
static void ESP_DrainRx(uint16_t ms);
static uint8_t ESP_WaitFor(const char *target, uint16_t timeout_ms);
static uint8_t ESP_RxAvailable(void);
static uint8_t ESP_RxRead(void);
static void ESP_HandleRequest(uint8_t conn_id, const char *req);
static void ESP_SendHTTPResponseStatus(uint8_t conn_id, uint16_t status_code,
                                       const char *reason,
                                       const char *content_type,
                                       const char *body, uint16_t body_len);
static void ESP_SendHTTPResponse(uint8_t conn_id, const char *content_type,
                                  const char *body, uint16_t body_len);
typedef struct {
    const char *data;
    uint16_t    len;
} ESP_ChunkDesc;

static void ESP_SendHTTPResponseChunked(uint8_t conn_id, const char *content_type,
                                        const ESP_ChunkDesc *chunks, uint8_t chunk_count);
static void ESP_CloseConn(uint8_t conn_id);
static uint8_t ESP_CipsendAndWaitPrompt(uint8_t conn_id, uint16_t nbytes);

extern const char html_page[];
extern const uint16_t html_page_len;

/* ================================================================
 *  UART 中断接收
 * ================================================================ */

extern "C" void USART1_IRQHandler(void) {
    if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_RXNE)) {
        uint8_t ch = (uint8_t)(huart1.Instance->DR & 0xFF);
        uint16_t next = (rx_head + 1) % ESP_RX_BUF_SIZE;
        if (next != rx_tail) {
            rx_buf[rx_head] = ch;
            rx_head = next;
        }
    }
    if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_ORE)) {
        __HAL_UART_CLEAR_OREFLAG(&huart1);
    }
}

static uint8_t ESP_RxAvailable(void) {
    uint16_t h, t;
    __disable_irq();
    h = rx_head;
    t = rx_tail;
    __enable_irq();
    return (h != t) ? 1U : 0U;
}

static uint16_t ESP_RxCount(void) {
    uint16_t h, t;
    __disable_irq();
    h = rx_head;
    t = rx_tail;
    __enable_irq();
    return (uint16_t)((h - t + ESP_RX_BUF_SIZE) % ESP_RX_BUF_SIZE);
}

static uint8_t ESP_RxRead(void) {
    __disable_irq();
    uint8_t ch = rx_buf[rx_tail];
    rx_tail = (rx_tail + 1U) % ESP_RX_BUF_SIZE;
    __enable_irq();
    return ch;
}

static uint8_t ESP_RxPeekAt(uint16_t offset) {
    uint16_t t;
    __disable_irq();
    t = rx_tail;
    __enable_irq();
    return rx_buf[(t + offset) % ESP_RX_BUF_SIZE];
}

static void ESP_RxSkip(uint16_t count) {
    __disable_irq();
    rx_tail = (rx_tail + count) % ESP_RX_BUF_SIZE;
    __enable_irq();
}

static uint8_t ESP_CipsendAndWaitPrompt(uint8_t conn_id, uint16_t nbytes) {
    char cmd[40];
    int ci = snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%u,%u\r\n", (unsigned)conn_id, (unsigned)nbytes);
    uint16_t clen = snprintf_safe_len(cmd, sizeof(cmd), ci);
    if (clen == 0U) {
        return 0U;
    }
    ESP_SendRaw(cmd, clen);
    return ESP_WaitFor(">", 1500);
}

/* ================================================================
 *  UART 初始化
 * ================================================================ */

static void ESP_UART_Init(void) {
    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};

    gpio.Pin   = GPIO_PIN_9;
    gpio.Mode  = GPIO_MODE_AF_PP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &gpio);

    gpio.Pin  = GPIO_PIN_10;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &gpio);

    huart1.Instance          = USART1;
    huart1.Init.BaudRate     = ESP_UART_BAUDRATE;
    huart1.Init.WordLength   = UART_WORDLENGTH_8B;
    huart1.Init.StopBits     = UART_STOPBITS_1;
    huart1.Init.Parity       = UART_PARITY_NONE;
    huart1.Init.Mode         = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart1);

    __HAL_UART_ENABLE_IT(&huart1, UART_IT_RXNE);
    HAL_NVIC_SetPriority(USART1_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
}

/* ================================================================
 *  AT 指令工具函数
 * ================================================================ */

static void ESP_SendRaw(const char *data, uint16_t len) {
    HAL_UART_Transmit(&huart1, (uint8_t *)data, len, 500);
}

static void ESP_DrainRx(uint16_t ms) {
    uint32_t start = HAL_GetTick();
    while ((HAL_GetTick() - start) < ms) {
        while (ESP_RxAvailable()) {
            ESP_RxRead();
        }
    }
}

static uint8_t ESP_WaitFor(const char *target, uint16_t timeout_ms) {
    uint32_t start = HAL_GetTick();
    char buf[128];
    uint16_t pos = 0;
    uint16_t tlen = (uint16_t)strlen(target);

    while ((HAL_GetTick() - start) < timeout_ms) {
        while (ESP_RxAvailable()) {
            char c = (char)ESP_RxRead();
            if (pos < sizeof(buf) - 1) {
                buf[pos++] = c;
                buf[pos] = '\0';
                if (pos >= tlen && memcmp(&buf[pos - tlen], target, tlen) == 0) {
                    return 1;
                }
            } else {
                memmove(buf, buf + pos - tlen, tlen);
                pos = tlen;
                buf[pos++] = c;
                buf[pos] = '\0';
                if (pos >= tlen && memcmp(&buf[pos - tlen], target, tlen) == 0) {
                    return 1;
                }
            }
        }
    }
    return 0;
}

/* ================================================================
 *  ESP_Init — 配置 WiFi AP + TCP Server（带重试）
 * ================================================================ */

static uint8_t ESP_SendCmdOK(const char *cmd, uint16_t timeout_ms) {
    uint16_t len = (uint16_t)strlen(cmd);
    ESP_SendRaw(cmd, len);
    ESP_SendRaw("\r\n", 2);
    return ESP_WaitFor("OK", timeout_ms);
}

void ESP_Init(void) {
    ESP_UART_Init();

    HAL_Delay(1500);
    ESP_DrainRx(200);

    ESP_SendRaw("AT+RST\r\n", 8);
    ESP_WaitFor("ready", 5000);
    HAL_Delay(1000);
    ESP_DrainRx(200);

    for (uint8_t retry = 0; retry < 3; retry++) {
        if (ESP_SendCmdOK("ATE0", 1000)) break;
        HAL_Delay(500);
    }

    for (uint8_t retry = 0; retry < 3; retry++) {
        if (ESP_SendCmdOK("AT+CWMODE=2", 1000)) break;
        HAL_Delay(500);
    }

    {
        const char *cmd = "AT+CWSAP=\"" ESP_WIFI_SSID "\",\"" ESP_WIFI_PASS "\",1,3";
        for (uint8_t retry = 0; retry < 3; retry++) {
            if (ESP_SendCmdOK(cmd, 2000)) break;
            HAL_Delay(500);
        }
    }

    for (uint8_t retry = 0; retry < 3; retry++) {
        if (ESP_SendCmdOK("AT+CIPMUX=1", 1000)) break;
        HAL_Delay(500);
    }

    for (uint8_t retry = 0; retry < 3; retry++) {
        if (ESP_SendCmdOK("AT+CIPSERVER=1,80", 1000)) break;
        HAL_Delay(500);
    }

    ESP_DrainRx(300);
}

/* ================================================================
 *  +IPD 扫描：直接在 ring buffer 中寻找 "+IPD,"
 *  避免依赖换行分割导致丢失请求
 * ================================================================ */

static uint8_t ESP_ScanForIPD(void) {
    uint16_t avail = ESP_RxCount();
    if (avail < 10) {
        return 0;
    }

    /* 在 ring buffer 中搜索 "+IPD," */
    uint16_t ipd_start = 0xFFFF;
    for (uint16_t i = 0; i + 4 < avail; i++) {
        if (ESP_RxPeekAt(i) == '+' &&
            ESP_RxPeekAt(i + 1) == 'I' &&
            ESP_RxPeekAt(i + 2) == 'P' &&
            ESP_RxPeekAt(i + 3) == 'D' &&
            ESP_RxPeekAt(i + 4) == ',') {
            ipd_start = i;
            break;
        }
    }

    if (ipd_start == 0xFFFF) {
        /* 没找到 +IPD，丢弃缓冲区前面的非 +IPD 数据（保留末尾几字节防截断） */
        if (avail > 8) {
            ESP_RxSkip(avail - 8);
        }
        return 0;
    }

    /* 跳过 +IPD 之前的杂数据 */
    if (ipd_start > 0) {
        ESP_RxSkip(ipd_start);
        avail -= ipd_start;
    }

    /* 解析 +IPD,<conn>,<len>: */
    /* 先把头部读到本地缓冲（最多 30 字节足够） */
    if (avail < 10) {
        return 0;
    }

    char hdr[32];
    uint16_t hdr_len = (avail < 31) ? avail : 31;
    for (uint16_t i = 0; i < hdr_len; i++) {
        hdr[i] = (char)ESP_RxPeekAt(i);
    }
    hdr[hdr_len] = '\0';

    /* 找冒号位置 */
    const char *colon = strchr(hdr, ':');
    if (!colon) {
        if (hdr_len >= 30) {
            ESP_RxSkip(5);
        }
        return 0;
    }

    /* 解析 conn_id 和 data_len */
    const char *p = hdr + 5; /* skip "+IPD," */
    uint32_t conn_id = 0;
    while (isdigit((unsigned char)*p)) {
        conn_id = conn_id * 10 + (uint32_t)(*p - '0');
        p++;
    }
    if (*p != ',' || conn_id >= ESP_MAX_CONNECTIONS) {
        ESP_RxSkip(5);
        return 0;
    }
    p++;
    uint32_t data_len = 0;
    while (isdigit((unsigned char)*p)) {
        data_len = data_len * 10 + (uint32_t)(*p - '0');
        p++;
    }
    if (*p != ':') {
        ESP_RxSkip(5);
        return 0;
    }

    uint16_t header_bytes = (uint16_t)(colon - hdr + 1); /* "+IPD,x,yyy:" */
    uint16_t total_needed = header_bytes + (uint16_t)((data_len > 300) ? 300 : data_len);

    /* 等待足够数据到达 */
    {
        uint32_t t0 = HAL_GetTick();
        while (ESP_RxCount() < total_needed) {
            if ((HAL_GetTick() - t0) > 200) {
                break;
            }
        }
    }

    /* 跳过 header 部分 */
    ESP_RxSkip(header_bytes);

    /* 读取 payload 到 scratch */
    char scratch[200];
    uint16_t to_read = (data_len > 199) ? 199 : (uint16_t)data_len;
    uint16_t got = 0;
    uint32_t t0 = HAL_GetTick();
    while (got < to_read) {
        if (ESP_RxAvailable()) {
            scratch[got++] = (char)ESP_RxRead();
        } else if ((HAL_GetTick() - t0) > 100) {
            break;
        }
    }
    scratch[got] = '\0';

    /* 丢弃 payload 剩余部分 */
    if (data_len > to_read) {
        uint32_t remain = data_len - to_read;
        t0 = HAL_GetTick();
        while (remain > 0) {
            if (ESP_RxAvailable()) {
                ESP_RxRead();
                remain--;
            } else if ((HAL_GetTick() - t0) > 100) {
                break;
            }
        }
    }

    ESP_HandleRequest((uint8_t)conn_id, scratch);
    return 1;
}

/* ================================================================
 *  HTTP 请求路由
 * ================================================================ */

static void ESP_HandleRequest(uint8_t conn_id, const char *req) {
    if (strstr(req, "GET /api/diag")) {
        const APDS9960_Diag *dg = APDS9960_GetDiag();
        char json[420];
        int ji = snprintf(json, sizeof(json),
            "{\"i2c\":%d,\"id\":\"0x%02X\",\"id_ok\":%d,\"init\":%d,"
            "\"en\":\"0x%02X\","
            "\"gst\":\"0x%02X\",\"gflvl\":%d,"
            "\"poll\":%u,\"gvalid\":%u,\"gest_ok\":%u,"
            "\"fifo\":[%d,%d,%d,%d],"
            "\"frames\":%u,\"ud\":%d,\"lr\":%d}",
            dg->i2c_ok, dg->id_val, dg->id_ok, dg->init_ok,
            dg->enable_reg,
            dg->last_gstatus, dg->last_gflvl,
            (unsigned)dg->poll_count, (unsigned)dg->gvalid_count,
            (unsigned)dg->gesture_count,
            dg->last_fifo_u, dg->last_fifo_d, dg->last_fifo_l, dg->last_fifo_r,
            (unsigned)dg->last_frames, (int)dg->last_ud, (int)dg->last_lr);
        uint16_t jlen = snprintf_safe_len(json, sizeof(json), ji);
        ESP_SendHTTPResponse(conn_id, "application/json", json, jlen);
    } else if (strstr(req, "GET /api/state")) {
        const APDS9960_Diag *dg = APDS9960_GetDiag();
        char json[420];
        int ji = snprintf(json, sizeof(json),
            "{\"t\":%u,\"touch\":[%d,%d,%d,%d],"
            "\"gesture\":%d,\"channel\":%d,"
            "\"last_g\":%d,\"last_gt\":%u,"
            "\"led\":[%d,%d,%d,%d],"
            "\"bri\":[%d,%d,%d,%d],"
            "\"apds\":{\"ok\":%d,\"id\":\"0x%02X\",\"gv\":%u,\"gc\":%u}}",
            (unsigned)HAL_GetTick(),
            (last_touch_mask >> 0) & 1,
            (last_touch_mask >> 1) & 1,
            (last_touch_mask >> 2) & 1,
            (last_touch_mask >> 3) & 1,
            (int)last_gesture, (int)last_channel,
            (int)last_latched_gesture, (unsigned)last_latched_gesture_tick,
            LED_GetState(1), LED_GetState(2),
            LED_GetState(3), LED_GetState(4),
            (int)LED_GetBrightness(1), (int)LED_GetBrightness(2),
            LED_GetState(3) ? 100 : 0,
            LED_GetState(4) ? 100 : 0,
            dg->init_ok, dg->id_val,
            (unsigned)dg->gvalid_count, (unsigned)dg->gesture_count);
        uint16_t jlen = snprintf_safe_len(json, sizeof(json), ji);
        ESP_SendHTTPResponse(conn_id, "application/json", json, jlen);
    } else if (strstr(req, "GET / ") || strstr(req, "GET /index") || strstr(req, "GET / HTTP")) {
        const ESP_ChunkDesc chunks[] = {
            { html_page, html_page_len }
        };
        ESP_SendHTTPResponseChunked(conn_id, "text/html", chunks, 1);
    } else if (strstr(req, "GET /favicon")) {
        ESP_SendHTTPResponse(conn_id, "text/plain", "", 0);
    } else {
        ESP_SendHTTPResponseStatus(conn_id, 404, "Not Found", "text/plain", "404", 3);
    }
}

/* ================================================================
 *  HTTP 响应发送
 * ================================================================ */

static void ESP_SendHTTPResponseStatus(uint8_t conn_id, uint16_t status_code,
                                       const char *reason,
                                       const char *content_type,
                                       const char *body, uint16_t body_len) {
    char header[192];
    int hlen_i = snprintf(header, sizeof(header),
        "HTTP/1.1 %u %s\r\n"
        "Content-Type: %s\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Connection: close\r\n"
        "Content-Length: %u\r\n\r\n",
        (unsigned)status_code, reason, content_type, (unsigned)body_len);
    uint16_t hlen = snprintf_safe_len(header, sizeof(header), hlen_i);
    if (hlen == 0U) {
        ESP_CloseConn(conn_id);
        return;
    }

    uint32_t total_u = (uint32_t)hlen + (uint32_t)body_len;
    if (total_u > 65535U) {
        ESP_CloseConn(conn_id);
        return;
    }
    uint16_t total = (uint16_t)total_u;

    if (!ESP_CipsendAndWaitPrompt(conn_id, total)) {
        ESP_CloseConn(conn_id);
        return;
    }

    ESP_SendRaw(header, hlen);
    if (body_len > 0) {
        ESP_SendRaw(body, body_len);
    }
    if (!ESP_WaitFor("SEND OK", 1500)) {
        ESP_CloseConn(conn_id);
        return;
    }

    ESP_CloseConn(conn_id);
}

static void ESP_SendHTTPResponse(uint8_t conn_id, const char *content_type,
                                 const char *body, uint16_t body_len) {
    ESP_SendHTTPResponseStatus(conn_id, 200, "OK", content_type, body, body_len);
}

static void ESP_SendHTTPResponseChunked(uint8_t conn_id, const char *content_type,
                                        const ESP_ChunkDesc *chunks, uint8_t chunk_count) {
    uint32_t body_len_u = 0;
    for (uint8_t i = 0; i < chunk_count; i++) {
        body_len_u += chunks[i].len;
    }
    if (body_len_u > 65535U) {
        ESP_CloseConn(conn_id);
        return;
    }
    uint16_t body_len = (uint16_t)body_len_u;

    char header[220];
    int hlen_i = snprintf(header, sizeof(header),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Connection: close\r\n"
        "Content-Length: %u\r\n\r\n",
        content_type, (unsigned)body_len);
    uint16_t hlen = snprintf_safe_len(header, sizeof(header), hlen_i);
    if (hlen == 0U) {
        ESP_CloseConn(conn_id);
        return;
    }

    /* ESP8266 AT+CIPSEND 单包上限约 2048：首包必须含完整 HTTP 头 + 部分 body，避免浏览器只收到头无 body */
    char *sendbuf = s_esp_http_sendbuf;
    uint8_t ci = 0;
    uint16_t co = 0;

    memcpy(sendbuf, header, hlen);
    uint16_t piece = hlen;
    while (piece < 2048U && ci < chunk_count) {
        while (ci < chunk_count && chunks[ci].len == 0U) {
            ci++;
            co = 0;
        }
        if (ci >= chunk_count) {
            break;
        }
        uint16_t avail = (uint16_t)(chunks[ci].len - co);
        uint16_t room = (uint16_t)(2048U - piece);
        uint16_t take = (avail < room) ? avail : room;
        memcpy(sendbuf + piece, chunks[ci].data + co, take);
        piece += take;
        co += take;
        if (co >= chunks[ci].len) {
            ci++;
            co = 0;
        }
    }

    if (!ESP_CipsendAndWaitPrompt(conn_id, piece)) {
        ESP_CloseConn(conn_id);
        return;
    }
    ESP_SendRaw(sendbuf, piece);
    if (!ESP_WaitFor("SEND OK", 2000)) {
        ESP_CloseConn(conn_id);
        return;
    }

    while (ci < chunk_count) {
        while (ci < chunk_count && chunks[ci].len == 0U) {
            ci++;
            co = 0;
        }
        if (ci >= chunk_count) {
            break;
        }
        piece = 0;
        while (piece < 2048U && ci < chunk_count) {
            while (ci < chunk_count && chunks[ci].len == 0U) {
                ci++;
                co = 0;
            }
            if (ci >= chunk_count) {
                break;
            }
            uint16_t avail = (uint16_t)(chunks[ci].len - co);
            uint16_t room = (uint16_t)(2048U - piece);
            uint16_t take = (avail < room) ? avail : room;
            memcpy(sendbuf + piece, chunks[ci].data + co, take);
            piece += take;
            co += take;
            if (co >= chunks[ci].len) {
                ci++;
                co = 0;
            }
        }
        if (piece == 0U) {
            break;
        }
        if (!ESP_CipsendAndWaitPrompt(conn_id, piece)) {
            ESP_CloseConn(conn_id);
            return;
        }
        ESP_SendRaw(sendbuf, piece);
        if (!ESP_WaitFor("SEND OK", 2000)) {
            ESP_CloseConn(conn_id);
            return;
        }
    }

    ESP_CloseConn(conn_id);
}

static void ESP_CloseConn(uint8_t conn_id) {
    char cmd[28];
    int len_i = snprintf(cmd, sizeof(cmd), "AT+CIPCLOSE=%u\r\n", (unsigned)conn_id);
    uint16_t len = snprintf_safe_len(cmd, sizeof(cmd), len_i);
    if (len > 0U) {
        ESP_SendRaw(cmd, len);
    }
    ESP_DrainRx(50);
}

/* ================================================================
 *  公开 API
 * ================================================================ */

void ESP_UpdateState(GestureType gesture, TouchChannel channel, uint8_t touch_mask) {
    last_gesture = gesture;
    last_channel = channel;
    last_touch_mask = touch_mask;
    if (gesture != GESTURE_NONE) {
        last_latched_gesture = gesture;
        last_latched_gesture_tick = HAL_GetTick();
    }
}

void ESP_Process(void) {
    ESP_ScanForIPD();
}
