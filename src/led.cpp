/**
 * @file    led.c
 * @brief   支持 PWM 的 LED 控制模块实现
 */

#include "led.h"
#include "config.h"

#define LED_NUM_CHANNELS  4U

/* LED_CH1..4（1..4）映射到数组下标 0..3 */
#define LED_IDX(ch)       ((uint8_t)((ch)-1U))

/* 私有变量：下标从 0 起 */
static uint8_t  led_brightness[LED_NUM_CHANNELS] = { 50, 50, 0, 0 };
static uint8_t  led_state[LED_NUM_CHANNELS]      = { 0, 0, 0, 0 };
/* 缓存 TIM3 的 CCR（CH1、CH2），避免重复换算占空比 */
static uint16_t pwm_ccr[2] = { 0, 0 };

/* PWM 用定时器句柄 */
static TIM_HandleTypeDef htim3;

/* 私有函数声明 */
static void LED_PWM_Init(void);
static void LED_GPIO_Init(void);

/**
 * @brief  将占空比（0–100）转为比较寄存器值（0–255）
 */
#define DUTY_TO_COMPARE(duty)     ((uint16_t)((uint32_t)(duty) * PWM_COMPARE_FULLSCALE / PWM_MAX_DUTY))

static void LED_UpdatePwmCompare(uint8_t idx01, uint8_t duty) {
    uint16_t ccr = DUTY_TO_COMPARE(duty);
    pwm_ccr[idx01] = ccr;
    if (idx01 == 0U) {
        htim3.Instance->CCR1 = ccr;
    } else {
        htim3.Instance->CCR2 = ccr;
    }
}

/**
 * @brief  初始化 LED GPIO 与 PWM 定时器
 */
void LED_Init(void) {
    LED_GPIO_Init();
    LED_PWM_Init();

    /* 与默认 50% 亮度同步缓存（PWM 通道） */
    LED_UpdatePwmCompare(0U, led_brightness[LED_IDX(LED_CH1)]);
    LED_UpdatePwmCompare(1U, led_brightness[LED_IDX(LED_CH2)]);

    /* 上电先关闭所有 LED */
    LED_AllOff();
}

/**
 * @brief  初始化 LED 所用 GPIO
 */
static void LED_GPIO_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* 使能时钟 */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* PA6、PA7 配置为 PWM 输出（TIM3 CH1、CH2） */
    GPIO_InitStruct.Pin = LED_PWM1_PIN | LED_PWM2_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_PWM_PORT, &GPIO_InitStruct);

    /* PB0、PB1 推挽输出 */
    GPIO_InitStruct.Pin = LED_SW1_PIN | LED_SW2_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_SW_PORT, &GPIO_InitStruct);
}

/**
 * @brief  初始化 TIM3 用于 PWM 输出
 */
static void LED_PWM_Init(void) {
    TIM_ClockConfigTypeDef sClockSourceConfig = {0};
    TIM_MasterConfigTypeDef sMasterConfig = {0};
    TIM_OC_InitTypeDef sConfigOC = {0};

    /* 使能 TIM3 时钟 */
    __HAL_RCC_TIM3_CLK_ENABLE();

    /* 初始化 TIM3 */
    htim3.Instance = TIM3;
    htim3.Init.Prescaler = (uint16_t)TIM3_PWM_PRESCALER;
    htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim3.Init.Period = (uint16_t)TIM3_PWM_HAL_PERIOD;
    htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

    if (HAL_TIM_Base_Init(&htim3) != HAL_OK) {
        Error_Handler();
    }

    sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK) {
        Error_Handler();
    }

    if (HAL_TIM_PWM_Init(&htim3) != HAL_OK) {
        Error_Handler();
    }

    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK) {
        Error_Handler();
    }

    /* 配置 PWM 通道 */
    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = DUTY_TO_COMPARE(50);  /* 初始 50% 占空比 */
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;

    /* 通道 1 — PA6 */
    if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1) != HAL_OK) {
        Error_Handler();
    }

    /* 通道 2 — PA7 */
    if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_2) != HAL_OK) {
        Error_Handler();
    }

    /* 启动 PWM 输出 */
    if (HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1) != HAL_OK) {
        Error_Handler();
    }
    if (HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2) != HAL_OK) {
        Error_Handler();
    }
}

/**
 * @brief  设置 LED 亮度
 */
void LED_SetBrightness(uint8_t channel, uint8_t duty) {
    if (duty > PWM_MAX_DUTY) {
        duty = PWM_MAX_DUTY;
    }

    if (channel >= 1U && channel <= LED_NUM_CHANNELS) {
        uint8_t idx = LED_IDX(channel);
        led_brightness[idx] = duty;

        /* 仅当 LED 为亮时才更新 PWM；否则只保存亮度值 */
        if (led_state[idx]) {
            if (channel == LED_CH1) {
                LED_UpdatePwmCompare(0U, duty);
            } else if (channel == LED_CH2) {
                LED_UpdatePwmCompare(1U, duty);
            }
        }
    }
}

/**
 * @brief  切换 LED 状态
 */
void LED_Toggle(uint8_t channel) {
    if (channel >= 1U && channel <= LED_NUM_CHANNELS) {
        uint8_t idx = LED_IDX(channel);
        if (led_state[idx]) {
            LED_Off(channel);
        } else {
            LED_On(channel);
        }
    }
}

/**
 * @brief  点亮 LED
 */
void LED_On(uint8_t channel) {
    if (channel >= 1U && channel <= LED_NUM_CHANNELS) {
        uint8_t idx = LED_IDX(channel);
        led_state[idx] = 1;

        switch (channel) {
            case LED_CH1:
                LED_UpdatePwmCompare(0U, led_brightness[idx]);
                break;
            case LED_CH2:
                LED_UpdatePwmCompare(1U, led_brightness[idx]);
                break;
            case LED_CH3:
                HAL_GPIO_WritePin(LED_SW_PORT, LED_SW1_PIN, GPIO_PIN_SET);
                break;
            case LED_CH4:
                HAL_GPIO_WritePin(LED_SW_PORT, LED_SW2_PIN, GPIO_PIN_SET);
                break;
            default:
                break;
        }
    }
}

/**
 * @brief  熄灭 LED
 */
void LED_Off(uint8_t channel) {
    if (channel >= 1U && channel <= LED_NUM_CHANNELS) {
        uint8_t idx = LED_IDX(channel);
        led_state[idx] = 0;

        switch (channel) {
            case LED_CH1:
                htim3.Instance->CCR1 = 0;
                pwm_ccr[0] = 0;
                break;
            case LED_CH2:
                htim3.Instance->CCR2 = 0;
                pwm_ccr[1] = 0;
                break;
            case LED_CH3:
                HAL_GPIO_WritePin(LED_SW_PORT, LED_SW1_PIN, GPIO_PIN_RESET);
                break;
            case LED_CH4:
                HAL_GPIO_WritePin(LED_SW_PORT, LED_SW2_PIN, GPIO_PIN_RESET);
                break;
            default:
                break;
        }
    }
}

/**
 * @brief  关闭全部 LED
 */
void LED_AllOff(void) {
    for (uint8_t ch = 1U; ch <= LED_NUM_CHANNELS; ch++) {
        LED_Off(ch);
    }
}

/**
 * @brief  读取 LED 亮度
 */
uint8_t LED_GetBrightness(uint8_t channel) {
    if (channel >= 1U && channel <= LED_NUM_CHANNELS) {
        return led_brightness[LED_IDX(channel)];
    }
    return 0;
}

uint8_t LED_GetState(uint8_t channel) {
    if (channel >= 1U && channel <= LED_NUM_CHANNELS) {
        return led_state[LED_IDX(channel)];
    }
    return 0;
}

/* Error_Handler 在 main.c 中定义 */
extern void Error_Handler(void);
