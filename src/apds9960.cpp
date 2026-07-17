/**
 * @file    apds9960.cpp
 * @brief   APDS-9960 隔空手势（I2C1 PB6/PB7，轮询 FIFO）+ 诊断信息
 */

#include "apds9960.h"
#include "config.h"
#include <cstdlib>
#include <string.h>

static I2C_HandleTypeDef hi2c1;
static uint8_t s_apds_ready = 0;
static APDS9960_Diag s_diag;

/* ---- 寄存器地址 ---- */
#define REG_ENABLE      0x80U
#define REG_ATIME       0x81U
#define REG_WTIME       0x83U
#define REG_PPULSE      0x8EU
#define REG_CONTROL     0x8FU
#define REG_ID          0x92U
#define REG_STATUS      0x93U
#define REG_GPENTH      0xA0U
#define REG_GEXTH       0xA1U
#define REG_GCONF1      0xA2U
#define REG_GCONF2      0xA3U
#define REG_GOFFSET_U   0xA4U
#define REG_GOFFSET_D   0xA5U
#define REG_GPULSE      0xA6U
#define REG_GOFFSET_L   0xA7U
#define REG_GOFFSET_R   0xA9U
#define REG_GCONF3      0xAAU
#define REG_GCONF4      0xABU
#define REG_GFLVL       0xAEU
#define REG_GSTATUS     0xAFU
#define REG_GFIFO_U     0xFCU

#define ENABLE_PON      0x01U
#define ENABLE_WEN      0x08U
#define ENABLE_PEN      0x04U
#define ENABLE_GEN      0x40U

#define GSTATUS_GVALID  0x01U

#define GESTURE_SENSITIVITY  20
#define GESTURE_NEAR_THRESH  10

extern "C" void HAL_I2C_MspInit(I2C_HandleTypeDef *hi2c) {
    if (hi2c->Instance != I2C1) {
        return;
    }
    __HAL_RCC_I2C1_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};
    g.Pin   = APDS9960_I2C_SCL_PIN | APDS9960_I2C_SDA_PIN;
    g.Mode  = GPIO_MODE_AF_OD;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(APDS9960_I2C_PORT, &g);
}

static HAL_StatusTypeDef apds_write8(uint8_t reg, uint8_t val) {
    return HAL_I2C_Mem_Write(&hi2c1, (uint16_t)(APDS9960_I2C_ADDR_7BIT << 1),
                             reg, I2C_MEMADD_SIZE_8BIT, &val, 1U, 50U);
}

static HAL_StatusTypeDef apds_read8(uint8_t reg, uint8_t *val) {
    return HAL_I2C_Mem_Read(&hi2c1, (uint16_t)(APDS9960_I2C_ADDR_7BIT << 1),
                            reg, I2C_MEMADD_SIZE_8BIT, val, 1U, 50U);
}

static HAL_StatusTypeDef apds_read_fifo4(uint8_t buf[4]) {
    return HAL_I2C_Mem_Read(&hi2c1, (uint16_t)(APDS9960_I2C_ADDR_7BIT << 1),
                            REG_GFIFO_U, I2C_MEMADD_SIZE_8BIT, buf, 4U, 50U);
}

static void apds_reset_gesture_engine(void) {
    uint8_t val = 0;
    apds_read8(REG_GCONF4, &val);
    val |= 0x04U;
    apds_write8(REG_GCONF4, val);
    HAL_Delay(2);
    val &= ~0x04U;
    apds_write8(REG_GCONF4, val);
}

const APDS9960_Diag* APDS9960_GetDiag(void) {
    return &s_diag;
}

HAL_StatusTypeDef APDS9960_Init(void) {
    memset(&s_diag, 0, sizeof(s_diag));

    hi2c1.Instance             = I2C1;
    hi2c1.Init.ClockSpeed      = 100000U;
    hi2c1.Init.DutyCycle       = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1     = 0;
    hi2c1.Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2     = 0;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode   = I2C_NOSTRETCH_DISABLE;

    if (HAL_I2C_Init(&hi2c1) != HAL_OK) {
        s_apds_ready = 0;
        s_diag.i2c_ok = 0;
        return HAL_ERROR;
    }
    s_diag.i2c_ok = 1;

    HAL_Delay(10);

    uint8_t id = 0;
    if (apds_read8(REG_ID, &id) != HAL_OK) {
        s_diag.id_ok = 0;
        s_diag.id_val = 0xFF;
        s_apds_ready = 0;
        return HAL_ERROR;
    }
    s_diag.id_val = id;
    s_diag.id_ok = 1;

    /* 放宽 ID 校验：只要能读到就继续 */
    if (id != 0xABU && id != 0xA8U && id != 0x9CU && id != 0xA1U) {
        s_apds_ready = 0;
        s_diag.init_ok = 0;
        return HAL_ERROR;
    }

    apds_write8(REG_ENABLE, 0x00U);
    HAL_Delay(25);

    apds_write8(REG_ENABLE, ENABLE_PON);
    HAL_Delay(10);

    apds_write8(REG_ATIME, 0xDBU);
    apds_write8(REG_WTIME, 0xF6U);
    apds_write8(REG_PPULSE, 0x87U);
    apds_write8(REG_CONTROL, 0x0EU);

    apds_write8(REG_GPENTH, 20U);
    apds_write8(REG_GEXTH, 10U);
    apds_write8(REG_GCONF1, 0x42U);
    apds_write8(REG_GCONF2, 0x61U);
    apds_write8(REG_GOFFSET_U, 0x00U);
    apds_write8(REG_GOFFSET_D, 0x00U);
    apds_write8(REG_GOFFSET_L, 0x00U);
    apds_write8(REG_GOFFSET_R, 0x00U);
    apds_write8(REG_GPULSE, 0x89U);
    apds_write8(REG_GCONF3, 0x00U);
    apds_write8(REG_GCONF4, 0x02U);

    apds_reset_gesture_engine();

    uint8_t en_val = (uint8_t)(ENABLE_PON | ENABLE_WEN | ENABLE_PEN | ENABLE_GEN);
    if (apds_write8(REG_ENABLE, en_val) != HAL_OK) {
        s_apds_ready = 0;
        s_diag.init_ok = 0;
        return HAL_ERROR;
    }

    HAL_Delay(10);

    /* 回读 ENABLE 寄存器验证写入 */
    uint8_t readback = 0;
    apds_read8(REG_ENABLE, &readback);
    s_diag.enable_reg = readback;

    s_apds_ready = 1;
    s_diag.init_ok = 1;
    return HAL_OK;
}

GestureType APDS9960_GetGesture(void) {
    s_diag.poll_count++;

    if (!s_apds_ready) {
        return GESTURE_NONE;
    }

    uint8_t gstatus = 0;
    if (apds_read8(REG_GSTATUS, &gstatus) != HAL_OK) {
        s_diag.last_gstatus = 0xEE;
        return GESTURE_NONE;
    }
    s_diag.last_gstatus = gstatus;

    if ((gstatus & GSTATUS_GVALID) == 0U) {
        return GESTURE_NONE;
    }

    s_diag.gvalid_count++;

    uint16_t total_frames = 0;
    uint8_t u_first = 0, d_first = 0, l_first = 0, r_first = 0;
    uint8_t u_last = 0, d_last = 0, l_last = 0, r_last = 0;
    uint8_t got_first = 0;

    uint32_t t0 = HAL_GetTick();
    while ((HAL_GetTick() - t0) < 300) {
        uint8_t fifo_level = 0;
        if (apds_read8(REG_GFLVL, &fifo_level) != HAL_OK) {
            break;
        }
        s_diag.last_gflvl = fifo_level;

        if (fifo_level == 0) {
            if (apds_read8(REG_GSTATUS, &gstatus) != HAL_OK) {
                break;
            }
            if ((gstatus & GSTATUS_GVALID) == 0) {
                break;
            }
            HAL_Delay(3);
            continue;
        }

        for (uint8_t i = 0; i < fifo_level; i++) {
            uint8_t buf[4];
            if (apds_read_fifo4(buf) != HAL_OK) {
                break;
            }

            uint8_t u = buf[0], d = buf[1], l = buf[2], r = buf[3];

            s_diag.last_fifo_u = u;
            s_diag.last_fifo_d = d;
            s_diag.last_fifo_l = l;
            s_diag.last_fifo_r = r;

            if (u < GESTURE_NEAR_THRESH && d < GESTURE_NEAR_THRESH &&
                l < GESTURE_NEAR_THRESH && r < GESTURE_NEAR_THRESH) {
                continue;
            }

            if (!got_first) {
                u_first = u; d_first = d; l_first = l; r_first = r;
                got_first = 1;
            }
            u_last = u; d_last = d; l_last = l; r_last = r;
            total_frames++;
        }
    }

    s_diag.last_frames = total_frames;

    if (total_frames < 3) {
        return GESTURE_NONE;
    }

    int32_t ud_first_d = (int32_t)u_first - (int32_t)d_first;
    int32_t ud_last_d  = (int32_t)u_last  - (int32_t)d_last;
    int32_t lr_first_d = (int32_t)l_first - (int32_t)r_first;
    int32_t lr_last_d  = (int32_t)l_last  - (int32_t)r_last;

    int32_t ud_delta = ud_last_d - ud_first_d;
    int32_t lr_delta = lr_last_d - lr_first_d;

    s_diag.last_ud = (int16_t)ud_delta;
    s_diag.last_lr = (int16_t)lr_delta;

    int32_t abs_ud = (ud_delta >= 0) ? ud_delta : -ud_delta;
    int32_t abs_lr = (lr_delta >= 0) ? lr_delta : -lr_delta;

    if (abs_ud < GESTURE_SENSITIVITY && abs_lr < GESTURE_SENSITIVITY) {
        return GESTURE_NONE;
    }

    s_diag.gesture_count++;

    if (abs_ud > abs_lr) {
        return (ud_delta > 0) ? GESTURE_SLIDE_UP : GESTURE_SLIDE_DOWN;
    } else {
        return (lr_delta > 0) ? GESTURE_SLIDE_LEFT : GESTURE_SLIDE_RIGHT;
    }
}
