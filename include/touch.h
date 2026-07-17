/**
 * @file    touch.h
 * @brief   TTP223 触摸模块驱动
 */

#ifndef __TOUCH_H
#define __TOUCH_H

#include "stm32f1xx_hal.h"

/**
 * @brief 触摸通道枚举
 */
typedef enum {
    TOUCH_NONE = 0,
    TOUCH_UP,
    TOUCH_DOWN,
    TOUCH_LEFT,
    TOUCH_RIGHT
} TouchChannel;

/**
 * @brief 触摸事件结构体
 */
typedef struct {
    TouchChannel channel;   // 触摸通道
    uint32_t press_time;    // 按下时刻（ms）
    uint32_t release_time;  // 释放时刻（ms）
    uint8_t is_pressed;     // 是否处于按下
} TouchEvent;

/**
 * @brief  初始化触摸 GPIO
 */
void Touch_Init(void);

/**
 * @brief  单次原始扫描并解析多键（调试用；应用层请用 Touch_GetEvent）
 * @return 解析出的通道，无触摸则为 TOUCH_NONE
 */
TouchChannel Touch_Scan(void);

/**
 * @brief  获取完整触摸事件
 * @return 含按下/释放时间戳的触摸事件
 */
TouchEvent Touch_GetEvent(void);

/**
 * @brief  获取消抖后的原始引脚掩码
 * @return bit0=UP(PA0), bit1=LEFT(PA1), bit2=RIGHT(PA2), bit3=DOWN(PA3)
 */
uint8_t Touch_GetRawMask(void);

#endif /* __TOUCH_H */
