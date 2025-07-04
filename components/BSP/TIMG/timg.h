/**
 ****************************************************************************************************
 * @file        timg.h

 */

#ifndef __TIMG_H
#define __TIMG_H


#include "driver/timer.h"
#include "esp_clk_tree.h"



/* 定时器配置结构体 */
typedef struct
{
    timer_src_clk_t clk_src;            /* 时钟源选择（GPTIMER_CLK_SRC_XTAL\GPTIMER_CLK_SRC_PLL_F80M(默认)\GPTIMER_CLK_SRC_RC_FAST） */
    int timer_group;                    /* 定时器组（TIMER_GROUP_0~TIMER_GROUP_1） */
    int timer_idx;                      /* 定时器组的那个定时器（TIMER_0~TIMER_1） */
    uint64_t timing_time;               /* 定时时间 */
    uint64_t alarm_value;               /* 警报值 */
    timer_autoreload_t auto_reload;     /* 启动自动装载 */
    uint64_t timer_count_value;         /* 计数器当前值 */
} timg_config_t;

/* 函数声明 */
void timg_init(timg_config_t *timgr_config);

#endif
