/**
 * @file    led.h
 * @brief   支持 PWM 的 LED 控制模块
 */

#ifndef __LED_H
#define __LED_H

#include "stm32f1xx_hal.h"
#include "config.h"   /* LED_CH1..LED_CH4 */

/**
 * @brief  初始化 LED GPIO 与 PWM 定时器
 */
void LED_Init(void);

/**
 * @brief  设置 LED 亮度（0–100%）
 * @param  channel: LED 通道（1–4）
 * @param  duty: 亮度占空比（0–100）
 */
void LED_SetBrightness(uint8_t channel, uint8_t duty);

/**
 * @brief  切换 LED 开/关
 * @param  channel: LED 通道（1–4）
 */
void LED_Toggle(uint8_t channel);

/**
 * @brief  点亮 LED
 * @param  channel: LED 通道（1–4）
 */
void LED_On(uint8_t channel);

/**
 * @brief  熄灭 LED
 * @param  channel: LED 通道（1–4）
 */
void LED_Off(uint8_t channel);

/**
 * @brief  关闭全部 LED
 */
void LED_AllOff(void);

/**
 * @brief  读取 LED 亮度
 * @param  channel: LED 通道（1–4）
 * @return 当前亮度（0–100）
 */
uint8_t LED_GetBrightness(uint8_t channel);

/**
 * @brief  读取 LED 开关状态
 * @param  channel: LED 通道（1–4）
 * @return 1=亮，0=灭
 */
uint8_t LED_GetState(uint8_t channel);

#endif /* __LED_H */
