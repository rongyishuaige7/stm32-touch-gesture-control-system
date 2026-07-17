/**
 * @file    config.h
 * @brief   STM32F103 触摸手势控制系统配置
 */

#ifndef __CONFIG_H
#define __CONFIG_H

#include "stm32f1xx_hal.h"

// ============================================
// ESP-01S 教学热点配置
// ============================================
// 固件启动 ESP8266 SoftAP，不连接家庭/学校 Wi-Fi，也不需要用户真实凭据。
// 如需修改热点密码，请使用至少 8 位的本地教学用密码，并重新编译烧录。
#define ESP_WIFI_SSID             "TouchGesture"
#define ESP_WIFI_PASS             "12345678"
#define ESP_HTTP_PORT             80

// ============================================
// 触摸消抖（软件滤波）
// ============================================
#define TOUCH_DEBOUNCE_COUNT      2       // 连续相同读数才确认状态（约 20ms @ 10ms 轮询）

// ============================================
// TTP223 触摸引脚定义（PA0–PA3）
// 实际接线：PA0=上，PA1=左，PA2=右，PA3=下
// ============================================
#define TOUCH_PORT                GPIOA
#define TOUCH_PIN_UP              GPIO_PIN_0
#define TOUCH_PIN_LEFT            GPIO_PIN_1
#define TOUCH_PIN_RIGHT           GPIO_PIN_2
#define TOUCH_PIN_DOWN            GPIO_PIN_3
#define TOUCH_ALL_PINS            (TOUCH_PIN_UP | TOUCH_PIN_DOWN | TOUCH_PIN_LEFT | TOUCH_PIN_RIGHT)

// ============================================
// LED 引脚定义
// ============================================
// PWM LED（PA6、PA7 — TIM3 CH1、CH2）
#define LED_PWM_PORT              GPIOA
#define LED_PWM1_PIN              GPIO_PIN_6
#define LED_PWM2_PIN              GPIO_PIN_7

// 开关 LED（PB0、PB1）
#define LED_SW_PORT               GPIOB
#define LED_SW1_PIN               GPIO_PIN_0
#define LED_SW2_PIN               GPIO_PIN_1

// ============================================
// 手势时间参数（单位：毫秒）
// ============================================
#define GESTURE_SLIDE_TIME        500     // 首次释放后等待第二次触摸的超时（超时则判为单击），非 APDS 滑动间隔
#define GESTURE_TOUCH_SLIDE_TIME  300     // TTP223 左/右紧挨滑动：释放后到第二次按下的最大间隔（与双击窗口错开）
#define GESTURE_DOUBLE_TIME       200     // 双击两次触摸的最大间隔
#define GESTURE_LONGPRESS_TIME    1500    // 长按判定阈值

// ============================================
// PWM 参数
// ============================================
#define PWM_MAX_DUTY              100
#define PWM_MIN_DUTY              0
#define PWM_STEP                  10
#define PWM_FREQ                  3920    // 约 3.92kHz（TIM3 上 1MHz/255）

/* TIM3: HSI 8MHz，APB1不分频，定时器时钟=8MHz；目标tick 1MHz */
#define TIM3_PWM_TIMER_CLK_HZ     8000000UL
#define TIM3_PWM_TICK_HZ          1000000UL
#define TIM3_PWM_PRESCALER        ((uint32_t)(TIM3_PWM_TIMER_CLK_HZ / TIM3_PWM_TICK_HZ) - 1U)
/* HAL Period = ARR；计数器 0..254 共 255 级；CCR 使用 0..255 标度（与原版一致） */
#define TIM3_PWM_HAL_PERIOD       (254U)
#define PWM_COMPARE_FULLSCALE     255U

// ============================================
// APDS-9960（I2C1：PB6=SCL，PB7=SDA）
// ============================================
#define APDS9960_I2C_ADDR_7BIT    0x39U
#define APDS9960_I2C_PORT         GPIOB
#define APDS9960_I2C_SCL_PIN      GPIO_PIN_6
#define APDS9960_I2C_SDA_PIN      GPIO_PIN_7

// ============================================
// LED 通道定义
// ============================================
#define LED_CH1                   1       // 第1路 PWM 灯
#define LED_CH2                   2       // 第2路 PWM 灯
#define LED_CH3                   3       // 第1路开关灯
#define LED_CH4                   4       // 第2路开关灯

// ============================================
// 错误处理函数声明
// ============================================
#ifdef __cplusplus
extern "C" {
#endif
void Error_Handler(void);
#ifdef __cplusplus
}
#endif

#endif /* __CONFIG_H */
