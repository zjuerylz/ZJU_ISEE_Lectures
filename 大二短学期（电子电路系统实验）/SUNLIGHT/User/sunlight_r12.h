/* USER CODE BEGIN SUNLIGHT_R12_INTERFACE */
#ifndef SUNLIGHT_R12_H
#define SUNLIGHT_R12_H

#include "main.h"

/* 设置充电和 LED 功率通路的安全初值。 */
void SunlightR12_PreClockInit(void);

/* 进入 R12 主控制循环。 */
__NO_RETURN void SunlightR12_Run(void);

/* 驱动 PC13 系统呼吸灯。 */
void SunlightR12_HeartbeatTick1ms(void);

#endif
/* USER CODE END SUNLIGHT_R12_INTERFACE */
