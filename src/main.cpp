/**
 * @file    main.cpp
 * @brief   STM32F103 触摸手势控制 — 主程序
 * @description
 *   基于STM32F103C8T6的非接触式智能家电控制原型
 *   使用TTP223电容触摸模块实现手势识别，控制LED开关及PWM调光
 */

#include "stm32f1xx_hal.h"
#include "touch.h"
#include "gesture.h"
#include "led.h"
#include "esp8266.h"
#include "apds9960.h"
#include "config.h"

/* 私有函数声明 */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);

/* 长按后自动手势演示（非阻塞状态机，覆盖 APDS 不稳定时的展示） */
typedef enum {
    DEMO_IDLE = 0,
    DEMO_WAIT,         /* 长按后等待 3s */
    DEMO_SLIDE_UP,     /* 上滑：CH1 渐亮 */
    DEMO_PAUSE_1,
    DEMO_SLIDE_DOWN,   /* 下滑：CH1 渐暗 */
    DEMO_PAUSE_2,
    DEMO_SLIDE_RIGHT,  /* 右滑：CH2 渐亮 */
    DEMO_PAUSE_3,
    DEMO_SLIDE_LEFT,   /* 左滑：CH2 渐暗 */
    DEMO_FINISH        /* 全灭后短暂结束 */
} DemoState;

#define DEMO_WAIT_MS      3000U
#define DEMO_PAUSE_MS     2000U
#define DEMO_RAMP_STEP_MS 80U
#define DEMO_FINISH_MS    500U

extern "C" void SysTick_Handler(void) {
    HAL_IncTick();
}

/**
 * @brief  应用入口
 * @retval int
 */
int main(void) {
    /* 初始化 HAL 库 */
    HAL_Init();

    /* 配置系统时钟 */
    SystemClock_Config();

    /* 初始化已配置的外设 */
    MX_GPIO_Init();

    /* 初始化应用模块 */
    Touch_Init();
    LED_Init();
    Gesture_Init();
    (void)APDS9960_Init();
    ESP_Init();

    /* 主循环变量 */
    TouchEvent touch_evt;
    GestureResult gesture;
    uint8_t brightness;
    DemoState demo_state = DEMO_IDLE;
    uint32_t demo_tick = 0;
    uint8_t demo_step = 0;

    /* 主循环 */
    while (1) {
        /* 读取触摸事件 */
        touch_evt = Touch_GetEvent();

        /* 演示已过等待阶段后，任意新按下可中断演示（WAIT 内不判，避免长按未松手误取消） */
        if (demo_state > DEMO_WAIT &&
            touch_evt.is_pressed != 0U &&
            touch_evt.channel != TOUCH_NONE) {
            demo_state = DEMO_IDLE;
        }

        /* 手势处理：TTP223（单击/长按/双击） */
        gesture = Gesture_Process(&touch_evt);

        /* APDS-9960：演示进行中不轮询，避免 FIFO 读最多阻塞 ~300ms 打乱节奏 */
        if (demo_state == DEMO_IDLE) {
            GestureType apds_g = APDS9960_GetGesture();
            if (apds_g != GESTURE_NONE && gesture.type == GESTURE_NONE) {
                gesture = Gesture_ProcessAPDS(apds_g);
            }
        }

        /* 根据手势执行动作 */
        switch (gesture.type) {
            case GESTURE_TAP:
                if (gesture.channel == TOUCH_UP || gesture.channel == TOUCH_DOWN) {
                    LED_Toggle(LED_CH1);
                } else if (gesture.channel == TOUCH_LEFT || gesture.channel == TOUCH_RIGHT) {
                    LED_Toggle(LED_CH2);
                }
                break;

            case GESTURE_SLIDE_UP:
                LED_On(LED_CH1);
                brightness = LED_GetBrightness(LED_CH1);
                if (brightness < 100) {
                    LED_SetBrightness(LED_CH1, brightness + PWM_STEP);
                }
                break;

            case GESTURE_SLIDE_DOWN:
                LED_On(LED_CH1);
                brightness = LED_GetBrightness(LED_CH1);
                if (brightness > 0) {
                    LED_SetBrightness(LED_CH1, brightness - PWM_STEP);
                }
                break;

            case GESTURE_SLIDE_LEFT:
                LED_On(LED_CH1);
                brightness = LED_GetBrightness(LED_CH1);
                if (brightness >= PWM_STEP) {
                    LED_SetBrightness(LED_CH1, (uint8_t)(brightness - PWM_STEP));
                } else {
                    LED_SetBrightness(LED_CH1, PWM_MIN_DUTY);
                }
                break;

            case GESTURE_SLIDE_RIGHT:
                LED_On(LED_CH1);
                brightness = LED_GetBrightness(LED_CH1);
                if (brightness <= (PWM_MAX_DUTY - PWM_STEP)) {
                    LED_SetBrightness(LED_CH1, (uint8_t)(brightness + PWM_STEP));
                } else {
                    LED_SetBrightness(LED_CH1, PWM_MAX_DUTY);
                }
                break;

            case GESTURE_LONG_PRESS:
                LED_AllOff();
                if (demo_state == DEMO_IDLE) {
                    demo_state = DEMO_WAIT;
                    demo_tick = HAL_GetTick();
                } else {
                    /* 演示中途再次长按：关断并结束演示 */
                    demo_state = DEMO_IDLE;
                }
                break;

            case GESTURE_DOUBLE_TAP:
                if (gesture.channel == TOUCH_UP || gesture.channel == TOUCH_DOWN) {
                    LED_Toggle(LED_CH3);
                } else {
                    LED_Toggle(LED_CH4);
                }
                break;

            default:
                break;
        }

        /* 演示状态机：非阻塞渐变 + 向 ESP 上报对应滑动手势 */
        if (demo_state != DEMO_IDLE) {
            uint32_t now = HAL_GetTick();
            switch (demo_state) {
                case DEMO_WAIT:
                    if ((uint32_t)(now - demo_tick) >= DEMO_WAIT_MS) {
                        demo_state = DEMO_SLIDE_UP;
                        demo_tick = now;
                        demo_step = 0;
                        LED_On(LED_CH1);
                        LED_SetBrightness(LED_CH1, 0);
                    }
                    break;

                case DEMO_SLIDE_UP:
                    if ((uint32_t)(now - demo_tick) >= DEMO_RAMP_STEP_MS) {
                        demo_step++;
                        {
                            uint8_t bri = (uint8_t)(demo_step * PWM_STEP);
                            if (bri > PWM_MAX_DUTY) {
                                bri = PWM_MAX_DUTY;
                            }
                            LED_SetBrightness(LED_CH1, bri);
                        }
                        gesture.type = GESTURE_SLIDE_UP;
                        gesture.channel = TOUCH_NONE;
                        demo_tick = now;
                        if (demo_step >= 10U) {
                            demo_state = DEMO_PAUSE_1;
                            demo_tick = now;
                        }
                    }
                    break;

                case DEMO_PAUSE_1:
                    gesture.type = GESTURE_NONE;
                    gesture.channel = TOUCH_NONE;
                    if ((uint32_t)(now - demo_tick) >= DEMO_PAUSE_MS) {
                        demo_state = DEMO_SLIDE_DOWN;
                        demo_tick = now;
                        demo_step = 0;
                        LED_On(LED_CH1);
                        LED_SetBrightness(LED_CH1, PWM_MAX_DUTY);
                    }
                    break;

                case DEMO_SLIDE_DOWN:
                    if ((uint32_t)(now - demo_tick) >= DEMO_RAMP_STEP_MS) {
                        demo_step++;
                        {
                            uint8_t bri = (uint8_t)(PWM_MAX_DUTY - demo_step * PWM_STEP);
                            LED_SetBrightness(LED_CH1, bri);
                        }
                        gesture.type = GESTURE_SLIDE_DOWN;
                        gesture.channel = TOUCH_NONE;
                        demo_tick = now;
                        if (demo_step >= 10U) {
                            demo_state = DEMO_PAUSE_2;
                            demo_tick = now;
                        }
                    }
                    break;

                case DEMO_PAUSE_2:
                    gesture.type = GESTURE_NONE;
                    gesture.channel = TOUCH_NONE;
                    if ((uint32_t)(now - demo_tick) >= DEMO_PAUSE_MS) {
                        demo_state = DEMO_SLIDE_RIGHT;
                        demo_tick = now;
                        demo_step = 0;
                        LED_Off(LED_CH1);
                        LED_On(LED_CH2);
                        LED_SetBrightness(LED_CH2, 0);
                    }
                    break;

                case DEMO_SLIDE_RIGHT:
                    if ((uint32_t)(now - demo_tick) >= DEMO_RAMP_STEP_MS) {
                        demo_step++;
                        {
                            uint8_t bri = (uint8_t)(demo_step * PWM_STEP);
                            if (bri > PWM_MAX_DUTY) {
                                bri = PWM_MAX_DUTY;
                            }
                            LED_SetBrightness(LED_CH2, bri);
                        }
                        gesture.type = GESTURE_SLIDE_RIGHT;
                        gesture.channel = TOUCH_NONE;
                        demo_tick = now;
                        if (demo_step >= 10U) {
                            demo_state = DEMO_PAUSE_3;
                            demo_tick = now;
                        }
                    }
                    break;

                case DEMO_PAUSE_3:
                    gesture.type = GESTURE_NONE;
                    gesture.channel = TOUCH_NONE;
                    if ((uint32_t)(now - demo_tick) >= DEMO_PAUSE_MS) {
                        demo_state = DEMO_SLIDE_LEFT;
                        demo_tick = now;
                        demo_step = 0;
                        LED_On(LED_CH2);
                        LED_SetBrightness(LED_CH2, PWM_MAX_DUTY);
                    }
                    break;

                case DEMO_SLIDE_LEFT:
                    if ((uint32_t)(now - demo_tick) >= DEMO_RAMP_STEP_MS) {
                        demo_step++;
                        {
                            uint8_t bri = (uint8_t)(PWM_MAX_DUTY - demo_step * PWM_STEP);
                            LED_SetBrightness(LED_CH2, bri);
                        }
                        gesture.type = GESTURE_SLIDE_LEFT;
                        gesture.channel = TOUCH_NONE;
                        demo_tick = now;
                        if (demo_step >= 10U) {
                            demo_state = DEMO_FINISH;
                            demo_tick = now;
                            LED_AllOff();
                        }
                    }
                    break;

                case DEMO_FINISH:
                    gesture.type = GESTURE_NONE;
                    gesture.channel = TOUCH_NONE;
                    if ((uint32_t)(now - demo_tick) >= DEMO_FINISH_MS) {
                        /* 恢复亮度默认值，避免演示后亮度为 0 导致 LED 无法被点亮 */
                        LED_SetBrightness(LED_CH1, 50);
                        LED_SetBrightness(LED_CH2, 50);
                        demo_state = DEMO_IDLE;
                    }
                    break;

                default:
                    break;
            }
        }

        /* 更新 ESP8266 状态快照并尽量排空 UART 上的 +IPD/AT 行（HTTP 响应仍会阻塞主循环） */
        ESP_UpdateState(gesture.type, gesture.channel, Touch_GetRawMask());
        for (uint8_t esp_i = 0; esp_i < 24U; esp_i++) {
            ESP_Process();
        }

        /* 消抖延时（略短以便更快轮询 HTTP，仍保持触摸采样） */
        HAL_Delay(8);
    }
}

/**
 * @brief  系统时钟配置
 * @retval None
 */
void SystemClock_Config(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /* 使用内部 HSI 8MHz，无需外部晶振，稳定可靠 */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                  | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK) {
        Error_Handler();
    }
}

/**
 * @brief  GPIO 初始化
 * @retval None
 */
static void MX_GPIO_Init(void) {
    /* 使能所需 GPIO 时钟 */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_AFIO_CLK_ENABLE();

    /* 关闭 JTAG，释放 PB3、PB4、PA15 */
    __HAL_AFIO_REMAP_SWJ_NOJTAG();
}

/**
 * @brief  发生错误时执行
 * @retval None
 */
void Error_Handler(void) {
    __disable_irq();

    /* PB0/PB1 闪烁指示故障（与开关 LED 同脚；HAL_Init 之后安全） */
    __HAL_RCC_GPIOB_CLK_ENABLE();
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    while (1) {
        HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_0);
        HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_1);
        HAL_Delay(150);
    }
}

/*
 * HAL 库需要
 */
#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line) {
    (void)file;
    (void)line;
}
#endif
