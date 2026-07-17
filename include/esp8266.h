/**
 * @file    esp8266.h
 * @brief   ESP8266-01S AT 指令驱动（USART1, WiFi AP + HTTP Server）
 */

#ifndef __ESP8266_H
#define __ESP8266_H

#include "stm32f1xx_hal.h"
#include "gesture.h"
#include "led.h"
#include "touch.h"

#define ESP_UART_BAUDRATE     115200
#define ESP_RX_BUF_SIZE       2048
#define ESP_TX_BUF_SIZE       512
#define ESP_MAX_CONNECTIONS   5

/**
 * @brief  初始化 ESP8266（USART1 + AT 配置 + 启动 TCP Server）
 * @note   会阻塞数秒等待 AT 响应
 */
void ESP_Init(void);

/**
 * @brief  在主循环中调用，处理收到的 HTTP 请求并返回响应
 */
void ESP_Process(void);

/**
 * @brief  更新设备状态快照（手势 + 触摸 + LED），供 HTTP JSON 接口使用
 * @note   非 NONE 手势会同时更新“最近一次手势”锁存，便于慢轮询网页仍能显示
 */
void ESP_UpdateState(GestureType gesture, TouchChannel channel, uint8_t touch_mask);

#endif /* __ESP8266_H */
