/**
 * @file    gesture.h
 * @brief   手势识别模块
 */

#ifndef __GESTURE_H
#define __GESTURE_H

#include "stm32f1xx_hal.h"
#include "touch.h"

/**
 * @brief 手势类型枚举
 */
typedef enum {
    GESTURE_NONE = 0,
    GESTURE_TAP,           /* 单击 */
    GESTURE_SLIDE_UP,     /* 上滑 */
    GESTURE_SLIDE_DOWN,   /* 下滑 */
    GESTURE_SLIDE_LEFT,   /* 左滑 */
    GESTURE_SLIDE_RIGHT,  /* 右滑 */
    GESTURE_LONG_PRESS,   /* 长按 */
    GESTURE_DOUBLE_TAP    /* 双击 */
} GestureType;

/**
 * @brief 手势结果：类型与触发通道
 */
typedef struct {
    GestureType type;
    /** 对单击/双击有意义；滑动时方向仅体现在 @a type 中 */
    TouchChannel channel;
} GestureResult;

/**
 * @brief  初始化手势识别模块
 */
void Gesture_Init(void);

/**
 * @brief  处理触摸事件并识别手势
 * @param  event: 来自 Touch_GetEvent() 的触摸事件
 * @return 手势结果（类型 + 通道）
 */
GestureResult Gesture_Process(TouchEvent *event);

/**
 * @brief  将 APDS-9960 隔空滑动手势包装为 GestureResult（channel 恒为 TOUCH_NONE）
 */
GestureResult Gesture_ProcessAPDS(GestureType apds_gesture);

#endif /* __GESTURE_H */
