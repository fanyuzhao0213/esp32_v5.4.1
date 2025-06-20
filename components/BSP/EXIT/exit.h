/**
 ****************************************************************************************************
 * @file        exit.h
 */

#ifndef __EXIT_H
#define __EXIT_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"


/* 引脚定义 */
#define MY_KEY      GPIO_NUM_0

/* IO操作 */
#define KEY_INT    gpio_get_level(MY_KEY)

/* 函数声明 */
void exit_init(void);   /* 外部中断初始化程序 */

#endif
