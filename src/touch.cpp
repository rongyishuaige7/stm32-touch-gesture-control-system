/**
 * @file    touch.c
 * @brief   TTP223 触摸模块驱动实现
 */

#include "touch.h"
#include "config.h"

/* 私有变量 */
static TouchEvent last_event = { .channel = TOUCH_NONE, .press_time = 0, .release_time = 0, .is_pressed = 0 };
static uint16_t debounce_raw_mask = 0;
static uint16_t debounce_confirmed_mask = 0;
static uint8_t debounce_count = 0;
/** 上一拍稳定消抖后的掩码（用于多键解析） */
static uint16_t s_prev_confirmed_mask = 0;
/** 换键后，下一次 GetEvent() 直接返回此次按下，不再等消抖 */
static TouchChannel s_pending_press_channel = TOUCH_NONE;

/**
 * @brief 读取当前哪些 TTP223 焊盘有效（TOUCH_ALL_PINS 上的位掩码）
 */
static uint16_t Touch_ReadPinMask(void) {
    return (uint16_t)(TOUCH_PORT->IDR & TOUCH_ALL_PINS);
}

/**
 * @brief 将单个置位比特映射为 TouchChannel
 */
static TouchChannel Touch_ChannelFromOnePin(uint16_t one_bit) {
    if (one_bit & TOUCH_PIN_UP) {
        return TOUCH_UP;
    }
    if (one_bit & TOUCH_PIN_DOWN) {
        return TOUCH_DOWN;
    }
    if (one_bit & TOUCH_PIN_LEFT) {
        return TOUCH_LEFT;
    }
    if (one_bit & TOUCH_PIN_RIGHT) {
        return TOUCH_RIGHT;
    }
    return TOUCH_NONE;
}

static uint8_t Touch_PopCount16(uint16_t v) {
    return (uint8_t)__builtin_popcount((unsigned int)v);
}

/**
 * @brief 由引脚掩码解析逻辑通道（多键同时按下时避免总选 UP 的问题）
 * @param m      当前消抖后的引脚掩码
 * @param prev_m 上一拍消抖后的引脚掩码
 */
static TouchChannel Touch_ResolveChannel(uint16_t m, uint16_t prev_m) {
    uint8_t n;

    if (m == 0U) {
        return TOUCH_NONE;
    }

    n = Touch_PopCount16(m);
    if (n == 1U) {
        return Touch_ChannelFromOnePin(m);
    }

    /* 多键：优先新按下的焊盘 */
    {
        uint16_t rising = (uint16_t)(m & (uint16_t)(~prev_m));
        uint8_t rn = Touch_PopCount16(rising);

        if (rn == 1U) {
            return Touch_ChannelFromOnePin(rising);
        }
        if (rn > 1U) {
            /* 多键同时首次接触 — 固定顺序 */
            if (rising & TOUCH_PIN_DOWN) {
                return TOUCH_DOWN;
            }
            if (rising & TOUCH_PIN_UP) {
                return TOUCH_UP;
            }
            if (rising & TOUCH_PIN_LEFT) {
                return TOUCH_LEFT;
            }
            if (rising & TOUCH_PIN_RIGHT) {
                return TOUCH_RIGHT;
            }
        }
    }

    /* 持续按住多键且掩码与上次相同：推断滑动时的手指位置 */
    if (m == (uint16_t)(TOUCH_PIN_UP | TOUCH_PIN_DOWN)) {
        if (prev_m == TOUCH_PIN_DOWN) {
            return TOUCH_UP;
        }
        if (prev_m == TOUCH_PIN_UP) {
            return TOUCH_DOWN;
        }
        return TOUCH_UP;
    }
    if (m == (uint16_t)(TOUCH_PIN_LEFT | TOUCH_PIN_RIGHT)) {
        if (prev_m == TOUCH_PIN_RIGHT) {
            return TOUCH_LEFT;
        }
        if (prev_m == TOUCH_PIN_LEFT) {
            return TOUCH_RIGHT;
        }
        return TOUCH_LEFT;
    }

    /* 3 键以上或对角：回退 — 避免总选 UP */
    if (m & TOUCH_PIN_DOWN) {
        return TOUCH_DOWN;
    }
    if (m & TOUCH_PIN_UP) {
        return TOUCH_UP;
    }
    if (m & TOUCH_PIN_LEFT) {
        return TOUCH_LEFT;
    }
    if (m & TOUCH_PIN_RIGHT) {
        return TOUCH_RIGHT;
    }

    return TOUCH_NONE;
}

/**
 * @brief  初始化触摸 GPIO
 */
void Touch_Init(void) {
    GPIO_InitTypeDef GPIO_InitStructure;

    /* 使能 GPIO 时钟 */
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* PA0–PA3 配置为输入下拉
     * TTP223 高电平有效（AHLB未焊），下拉防飞线悬空误触发 */
    GPIO_InitStructure.Pin = TOUCH_ALL_PINS;
    GPIO_InitStructure.Mode = GPIO_MODE_INPUT;
    GPIO_InitStructure.Pull = GPIO_PULLDOWN;
    GPIO_InitStructure.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(TOUCH_PORT, &GPIO_InitStructure);

    last_event = (TouchEvent){ .channel = TOUCH_NONE, .press_time = 0, .release_time = 0, .is_pressed = 0 };
    debounce_raw_mask = 0;
    debounce_confirmed_mask = 0;
    debounce_count = 0;
    s_prev_confirmed_mask = 0;
    s_pending_press_channel = TOUCH_NONE;
}

/**
 * @brief  扫描触摸通道（单次原始采样，结合跳变提示解析）
 * @note   内部保存上一拍原始掩码；手势逻辑请优先用 Touch_GetEvent()
 */
TouchChannel Touch_Scan(void) {
    static uint16_t s_scan_prev_mask = 0;
    uint16_t m = Touch_ReadPinMask();
    TouchChannel ch = Touch_ResolveChannel(m, s_scan_prev_mask);
    s_scan_prev_mask = m;
    return ch;
}

/**
 * @brief 获取消抖后的引脚掩码
 */
static uint16_t Touch_GetDebouncedMask(void) {
    uint16_t raw = Touch_ReadPinMask();

    if (raw == debounce_raw_mask) {
        if (debounce_count < TOUCH_DEBOUNCE_COUNT) {
            debounce_count++;
        }
        if (debounce_count >= TOUCH_DEBOUNCE_COUNT) {
            debounce_confirmed_mask = debounce_raw_mask;
        }
    } else {
        debounce_raw_mask = raw;
        debounce_count = 1;
    }

    return debounce_confirmed_mask;
}

/**
 * @brief  获取完整触摸事件（含时间戳，已消抖）
 * @return 触摸事件结构体
 */
TouchEvent Touch_GetEvent(void) {
    TouchEvent evt = { .channel = TOUCH_NONE, .press_time = 0, .release_time = 0, .is_pressed = 0 };
    uint32_t current_time = HAL_GetTick();

    /* 换键后立即投递合成按下事件 */
    if (s_pending_press_channel != TOUCH_NONE) {
        evt.channel = s_pending_press_channel;
        evt.press_time = current_time;
        evt.is_pressed = 1;
        last_event = evt;
        s_pending_press_channel = TOUCH_NONE;
        return evt;
    }

    {
        uint16_t m = Touch_GetDebouncedMask();
        TouchChannel ch = Touch_ResolveChannel(m, s_prev_confirmed_mask);
        s_prev_confirmed_mask = m;

        if (ch != TOUCH_NONE) {
            /* 有触摸按下 */
            if (!last_event.is_pressed) {
                /* 从空闲进入新按下 */
                evt.channel = ch;
                evt.press_time = current_time;
                evt.is_pressed = 1;
                last_event = evt;
            } else if (ch != last_event.channel) {
                /* 滑动重叠未完全抬起就换键：先释放旧键，排队新按下 */
                evt = last_event;
                evt.release_time = current_time;
                evt.is_pressed = 0;
                s_pending_press_channel = ch;
                last_event = (TouchEvent){ .channel = TOUCH_NONE, .press_time = 0, .release_time = 0, .is_pressed = 0 };
            } else {
                /* 持续按住 — 同一通道 */
                evt = last_event;
            }
        } else {
            /* 触摸释放（消抖后为空闲） */
            if (last_event.is_pressed) {
                evt = last_event;
                evt.release_time = current_time;
                evt.is_pressed = 0;
                last_event = (TouchEvent){ .channel = TOUCH_NONE, .press_time = 0, .release_time = 0, .is_pressed = 0 };
            }
        }
    }

    return evt;
}

uint8_t Touch_GetRawMask(void) {
    uint16_t m = debounce_confirmed_mask;
    uint8_t result = 0;
    if (m & TOUCH_PIN_UP)    result |= 0x01;
    if (m & TOUCH_PIN_LEFT)  result |= 0x02;
    if (m & TOUCH_PIN_RIGHT) result |= 0x04;
    if (m & TOUCH_PIN_DOWN)  result |= 0x08;
    return result;
}
