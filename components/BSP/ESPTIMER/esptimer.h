/**
 ****************************************************************************************************
 * @file        esptim.h
 ****************************************************************************************************
 */

#ifndef __ESPTIMER_H
#define __ESPTIMER_H

#include "esp_timer.h"
#include "esp_log.h"

/* 函数声明 */
void esptimer_init(uint64_t tps);   /* 初始化高分辨率定时器 */
void once_timer_init(uint64_t tps);
#endif
