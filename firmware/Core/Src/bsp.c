#include "bsp.h"
#include "pin_map.h"
#include "shiftreg.h"

static GPIO_TypeDef *const RELAY_PORT[3] = { RELAY1_PORT, RELAY2_PORT, RELAY3_PORT };
static const uint16_t      RELAY_PIN [3] = { RELAY1_PIN,  RELAY2_PIN,  RELAY3_PIN  };

static GPIO_TypeDef *const IR_PORT[IR_COUNT] = { IR1_PORT, IR2_PORT, IR3_PORT, IR4_PORT, IR5_PORT };
static const uint16_t      IR_PIN [IR_COUNT] = { IR1_PIN,  IR2_PIN,  IR3_PIN,  IR4_PIN,  IR5_PIN  };

void BSP_Init(void)
{
    /* Drivers disabled (EN active-low → drive HIGH = disabled). */
    HAL_GPIO_WritePin(TMC_EN_PORT, TMC_EN_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(TP_EN_PORT,  TP_EN_PIN,  GPIO_PIN_SET);
    HAL_GPIO_WritePin(TMC_DIR_PORT, TMC_DIR_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(TP_DIR_PORT,  TP_DIR_PIN,  GPIO_PIN_RESET);

    /* Relays off. */
    for (int i = 0; i < 3; ++i)
        HAL_GPIO_WritePin(RELAY_PORT[i], RELAY_PIN[i], GPIO_PIN_RESET);

    SR595_Init();         /* no motor selected */
    SR165_Init();
}

/* ── Relays ─────────────────────────────────────────────────────────── */
void BSP_Relay(uint8_t idx1, bool on)
{
    if (idx1 < 1 || idx1 > 3) return;
    HAL_GPIO_WritePin(RELAY_PORT[idx1 - 1], RELAY_PIN[idx1 - 1],
                      on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* ── IR sensors (active-low DOUT) ───────────────────────────────────── */
bool BSP_IR(uint8_t idx1)
{
    if (idx1 < 1 || idx1 > IR_COUNT) return false;
    return HAL_GPIO_ReadPin(IR_PORT[idx1 - 1], IR_PIN[idx1 - 1]) == GPIO_PIN_RESET;
}

uint8_t BSP_IR_Mask(void)
{
    uint8_t m = 0;
    for (int i = 0; i < IR_COUNT; ++i)
        if (BSP_IR((uint8_t)(i + 1))) m |= (uint8_t)(1u << i);
    return m;
}

/* ── Reed (door) ────────────────────────────────────────────────────── */
bool BSP_DoorClosed(void)
{
    return HAL_GPIO_ReadPin(REED_PORT, REED_PIN) == GPIO_PIN_RESET;
}

/* ── TMC2209 discrete lines ─────────────────────────────────────────── */
void BSP_TMC_Enable(bool en)
{   /* EN is active-low */
    HAL_GPIO_WritePin(TMC_EN_PORT, TMC_EN_PIN, en ? GPIO_PIN_RESET : GPIO_PIN_SET);
}
void BSP_TMC_Dir(bool cw)
{
    HAL_GPIO_WritePin(TMC_DIR_PORT, TMC_DIR_PIN, cw ? GPIO_PIN_SET : GPIO_PIN_RESET);
}
bool BSP_TMC_Diag(void)
{
    return HAL_GPIO_ReadPin(TMC_DIAG_PORT, TMC_DIAG_PIN) == GPIO_PIN_SET;
}
bool BSP_TMC_Index(void)
{
    return HAL_GPIO_ReadPin(TMC_INDEX_PORT, TMC_INDEX_PIN) == GPIO_PIN_SET;
}

/* ── TP6600 discrete lines ──────────────────────────────────────────── */
void BSP_TP_Enable(bool en)
{
    HAL_GPIO_WritePin(TP_EN_PORT, TP_EN_PIN, en ? GPIO_PIN_RESET : GPIO_PIN_SET);
}
void BSP_TP_Dir(bool cw)
{
    HAL_GPIO_WritePin(TP_DIR_PORT, TP_DIR_PIN, cw ? GPIO_PIN_SET : GPIO_PIN_RESET);
}
