/**
 * @file    apds9960.h
 * @brief   APDS-9960 隔空手势（I2C1 PB6/PB7，轮询）
 */

#ifndef __APDS9960_H
#define __APDS9960_H

#include "stm32f1xx_hal.h"
#include "gesture.h"

typedef struct {
    uint8_t  i2c_ok;       /* I2C 初始化是否成功 */
    uint8_t  id_val;       /* 读到的芯片 ID（期望 0xAB/0xA8/0x9C） */
    uint8_t  id_ok;        /* ID 读取是否成功 */
    uint8_t  init_ok;      /* 全部初始化流程是否成功 */
    uint8_t  last_gstatus; /* 最近一次读取的 GSTATUS 寄存器值 */
    uint8_t  last_gflvl;   /* 最近一次读取的 FIFO level */
    uint16_t poll_count;   /* 总轮询次数 */
    uint16_t gvalid_count; /* GVALID=1 的次数 */
    uint16_t gesture_count;/* 成功识别手势的次数 */
    uint8_t  last_fifo_u;  /* 最近读到的 FIFO U 值 */
    uint8_t  last_fifo_d;
    uint8_t  last_fifo_l;
    uint8_t  last_fifo_r;
    uint16_t last_frames;  /* 最近一次手势读到的帧数 */
    int16_t  last_ud;      /* 最近一次 UD delta */
    int16_t  last_lr;      /* 最近一次 LR delta */
    uint8_t  enable_reg;   /* ENABLE 寄存器回读值 */
} APDS9960_Diag;

HAL_StatusTypeDef APDS9960_Init(void);
GestureType APDS9960_GetGesture(void);
const APDS9960_Diag* APDS9960_GetDiag(void);

#endif /* __APDS9960_H */
