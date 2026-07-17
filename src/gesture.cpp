/**
 * @file    gesture.cpp
 * @brief   手势识别模块实现
 *
 *  TTP223：单击 / 长按 / 双击；PA1/PA2 左右紧挨滑动调光（与 APDS-9960 上/下滑并存）
 */

#include "gesture.h"
#include "config.h"

/* 私有状态枚举 */
typedef enum {
    GEST_STATE_IDLE,
    GEST_STATE_FIRST_TOUCH,
    GEST_STATE_WAIT_SECOND,
    GEST_STATE_LONG_PRESS_LATCH
} GestureState;

/* 私有变量 */
static GestureState state = GEST_STATE_IDLE;
static TouchChannel first_channel = TOUCH_NONE;
static uint32_t first_press_time = 0;
static uint32_t first_release_time = 0;

void Gesture_Init(void) {
    state = GEST_STATE_IDLE;
    first_channel = TOUCH_NONE;
    first_press_time = 0;
    first_release_time = 0;
}

GestureResult Gesture_ProcessAPDS(GestureType apds_gesture) {
    GestureResult res = { .type = apds_gesture, .channel = TOUCH_NONE };
    return res;
}

/**
 * 状态机：
 *   IDLE —按下─► FIRST_TOUCH —释放─► WAIT_SECOND —超时(GESTURE_SLIDE_TIME)—► 单击
 *                                          │ DOUBLE_TIME 内同通道再按 → 双击 → LONG_PRESS_LATCH
 *                                          │ TOUCH_SLIDE_TIME 内 LEFT↔RIGHT（异键）→ 触摸滑动（含换键 0ms 间隔）
 *                                          │ 其它再按 → 补发单击后以新按下进入 FIRST_TOUCH
 *   LONG_PRESS_LATCH —释放─► IDLE
 */
GestureResult Gesture_Process(TouchEvent *event) {
    GestureResult res = { .type = GESTURE_NONE, .channel = TOUCH_NONE };
    GestureType result = GESTURE_NONE;
    uint32_t current_time = HAL_GetTick();
    uint8_t use_explicit_channel = 0;
    TouchChannel explicit_channel = TOUCH_NONE;

    if (event->channel == TOUCH_NONE) {
        switch (state) {
            case GEST_STATE_LONG_PRESS_LATCH:
                state = GEST_STATE_IDLE;
                break;
            case GEST_STATE_WAIT_SECOND:
                if ((uint32_t)(current_time - first_release_time) > GESTURE_SLIDE_TIME) {
                    result = GESTURE_TAP;
                    state = GEST_STATE_IDLE;
                }
                break;
            default:
                break;
        }
        res.type = result;
        res.channel = (result != GESTURE_NONE) ? first_channel : TOUCH_NONE;
        return res;
    }

    switch (state) {
        case GEST_STATE_IDLE:
            if (event->is_pressed) {
                first_channel = event->channel;
                first_press_time = event->press_time;
                state = GEST_STATE_FIRST_TOUCH;
            }
            break;

        case GEST_STATE_FIRST_TOUCH:
            if (event->is_pressed) {
                if ((uint32_t)(current_time - first_press_time) >= GESTURE_LONGPRESS_TIME) {
                    result = GESTURE_LONG_PRESS;
                    state = GEST_STATE_LONG_PRESS_LATCH;
                }
            } else {
                first_release_time = event->release_time;
                uint32_t dur = (uint32_t)(first_release_time - first_press_time);
                if (dur >= GESTURE_LONGPRESS_TIME) {
                    result = GESTURE_LONG_PRESS;
                    state = GEST_STATE_IDLE;
                } else {
                    state = GEST_STATE_WAIT_SECOND;
                }
            }
            break;

        case GEST_STATE_WAIT_SECOND:
            if (event->is_pressed) {
                uint32_t interval = (uint32_t)(event->press_time - first_release_time);

                /* 优先：PA1/PA2 紧挨滑动（touch 换键路径 interval 可为 0，避免误补发单击） */
                if (interval < GESTURE_TOUCH_SLIDE_TIME &&
                    event->channel != first_channel &&
                    ((first_channel == TOUCH_LEFT && event->channel == TOUCH_RIGHT) ||
                     (first_channel == TOUCH_RIGHT && event->channel == TOUCH_LEFT))) {
                    result = (event->channel == TOUCH_RIGHT)
                                 ? GESTURE_SLIDE_RIGHT
                                 : GESTURE_SLIDE_LEFT;
                    state = GEST_STATE_LONG_PRESS_LATCH;
                } else if (interval < GESTURE_DOUBLE_TIME && event->channel == first_channel) {
                    result = GESTURE_DOUBLE_TAP;
                    state = GEST_STATE_LONG_PRESS_LATCH;
                } else {
                    /* 其它再按：双击窗口已过则补发上一轮单击 */
                    if (interval >= GESTURE_DOUBLE_TIME) {
                        result = GESTURE_TAP;
                        explicit_channel = first_channel;
                        use_explicit_channel = 1;
                    }
                    first_channel = event->channel;
                    first_press_time = event->press_time;
                    state = GEST_STATE_FIRST_TOUCH;
                }
            }
            break;

        case GEST_STATE_LONG_PRESS_LATCH:
            if (!event->is_pressed) {
                state = GEST_STATE_IDLE;
            }
            break;

        default:
            state = GEST_STATE_IDLE;
            break;
    }

    res.type = result;
    if (result != GESTURE_NONE) {
        res.channel = use_explicit_channel ? explicit_channel : first_channel;
    } else {
        res.channel = TOUCH_NONE;
    }
    return res;
}
